# SPDX-License-Identifier: GPL-3.0-or-later
"""Authenticated Help field plans and persistent loading barriers.

The existing room coordinator still owns transactions, timers and settlement.
Unity remains the battle executor; this module never simulates combat.
"""
import copy
import hashlib
import json
import re

from .engine import DomainError, integer
from .joint_defense import finish
from .multiplayer import MultiplayerRooms

VERSION = 1
LOAD_TIMEOUT_SECONDS = 60  # Explicit private policy, not an official value.
FAILURES = frozenset(('resource', 'composition', 'route', 'terrain', 'carry', 'runtime'))


def digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'),
                                     ensure_ascii=False, allow_nan=False).encode()).hexdigest()


def source(catalog, assets, stage):
    asset = assets.get(stage)
    level_key = 'level_' + stage
    level = catalog.data['levelTemplates'].get(level_key)
    if not asset or not level:
        raise DomainError('native_field_stage_unavailable', stage)
    if (set(asset) != {'assetKey', 'sha256'} or
            not re.fullmatch(r'[0-9a-f]{64}', asset['sha256']) or
            asset['assetKey'] != 'private/autochess/mixed-map-v1/' + stage + '-graphic.prefab'):
        raise DomainError('invalid_native_field_asset')
    frame = level['MapData']['Map']
    if frame['ColumnSize'] != 21 or frame['RowSize'] != 19:
        raise DomainError('native_field_frame_unavailable')
    return {'stageId': stage, 'levelTemplateKey': level_key,
            'mapDataSha256': digest(level['MapData']),
            'predefinesSha256': digest({key: level.get(key) for key in ('Predefines', 'HardPredefines')}),
            'graphic': copy.deepcopy(asset)}


def arm(rooms, room, assets):
    """Called once from the existing help phase transition, in its transaction."""
    plan = room['help']
    if plan.get('fieldLoad') is not None:
        raise DomainError('native_field_already_armed')
    states = {row['seat']: row['state'] for row in room['seats'] if row.get('state') and not row.get('left')}
    defenders = plan['defenderSeats']
    if not 1 <= len(defenders) <= 2 or len(set(defenders)) != len(defenders):
        raise DomainError('invalid_native_field_defenders')
    segments = []
    for row in plan['segments']:
        state = states[row['seat']]
        if state['stageId'] != row['stageId']:
            raise DomainError('native_field_stage_changed_after_report')
        segments.append({'seat': row['seat'], 'side': row['side'],
                         'source': source(rooms.c, assets, row['stageId']),
                         'specialEffects': copy.deepcopy(row['battlefield']['specialEffects']),
                         'deploymentsSha256': digest(row['deployments']),
                         'bondsSha256': digest(row['bonds'])})
    route_key = 'EscapedBattleTemplateMapMultiPlayer' if len(defenders) == 2 else 'EscapedBattleTemplateMapSinglePlayer'
    route = rooms.c.season['ConstData'][route_key]
    route_level_key = route.rsplit('/', 1)[-1].lower()
    if route_level_key not in rooms.c.data['levelTemplates']:
        raise DomainError('native_field_route_unavailable')
    immutable = {'version': VERSION, 'roomId': room['roomId'],
        'matchSequence': room.get('matchSequence', 1), 'round': room['round'],
        'compositionGeneration': plan['generation'], 'catalogHash': rooms.c.hash,
        'kind': 'serial-two-owner' if len(defenders) == 2 else 'single-owner',
        'nativePlayerOrder': list(reversed(defenders)) if len(defenders) == 2 else list(defenders),
        'rightSeat': plan['rightSeat'], 'segments': segments,
        'routeLevelPath': route, 'routeLevelSha256': digest(rooms.c.data['levelTemplates'][route_level_key]),
        'enemyIssueSha256': digest(plan['enemies']), 'originBattlefieldsSha256': digest(plan['originBattlefields']),
        'sourceCountsSha256': digest(plan['sourceCounts']),
        'restoreBySeat': {str(seat): {'runId': state['runId'], 'source': source(rooms.c, assets, state['stageId'])}
                          for seat, state in sorted(states.items())},
        'allowBondLayerGrowth': False}
    from .multiplayer_reconstruction import waiting
    available = {row['seat'] for row in room['seats']
        if row.get('connected') and not row.get('left') and not row.get('quit') and
           row.get('state') and not waiting(room, row)}
    available_defenders = sorted(set(defenders) & available)
    if available_defenders:
        plan['authoritySeat'] = min(available_defenders)
    required = sorted((set(defenders) & available) | available)
    if not available_defenders:
        required = sorted(set(required) | {plan.get('authoritySeat', plan['rightSeat'])})
    field = {'version': VERSION, 'plan': immutable, 'planHash': digest(immutable),
        'status': 'loading', 'requiredSeats': required, 'readySeats': [], 'failedSeats': {},
        'loadAttemptBySeat': {str(value): 0 for value in states}, 'recoveringSeats': [],
        'worldLostSeats': [],
        'loadStartedAtTs': int(rooms.clock()), 'loadDeadlineTs': int(rooms.clock()) + LOAD_TIMEOUT_SECONDS,
        'battleStartedAtTs': None, 'battleDeadlineTs': None, 'skippedObserverSeats': [],
        'nativeRuntimeValidated': False}
    plan['fieldLoad'] = field
    # No simulation time is consumed by resource loading. The coordinator's
    # deadline is a loading limit, projected separately from native forceEndTs.
    plan['startedAtTs'] = plan['deadlineTs'] = None
    room['deadlineTs'] = field['loadDeadlineTs']; room['settleAfterTs'] = None
    rooms._event(room, 'native-field-loading', {'planHash': field['planHash'], 'requiredSeats': required,
                                              'loadDeadlineTs': field['loadDeadlineTs']})


def start_if_ready(rooms, room, *, timeout_release=False):
    plan = room['help']; field = plan['fieldLoad']
    validate_binding(room, field)
    now = int(rooms.clock())
    if field['status'] == 'loading' and now >= field['loadDeadlineTs'] and not timeout_release:
        raise DomainError('native_field_loading_expired')
    if field['status'] != 'loading' or not set(field['requiredSeats']) <= set(field['readySeats']):
        return False
    if any(not row.get('connected') or row.get('quit') or row.get('left') for row in room['seats']
           if row['seat'] in set(plan['defenderSeats']) & set(field['requiredSeats'])):
        return False
    field.update(status='running', battleStartedAtTs=now,
                 battleDeadlineTs=now + plan['durationSeconds'])
    plan['startedAtTs'] = now; plan['deadlineTs'] = field['battleDeadlineTs']
    room['deadlineTs'] = plan['deadlineTs']; room['settleAfterTs'] = plan['deadlineTs'] + 5
    rooms._event(room, 'native-field-started', {'planHash': field['planHash'],
        'startedAtTs': now, 'deadlineTs': plan['deadlineTs']})
    return True


def exclude_reconstruction(rooms, room):
    """An interrupted display/defender cannot hold a pre-combat load lease.

    The paired deployment remains frozen; one available original engine can
    execute both sides. A running outcome ledger never changes authority here.
    """
    plan = room.get('help') or {}; field = plan.get('fieldLoad')
    if room['phase'] != 'help' or not field or field['status'] != 'loading': return
    from .multiplayer_reconstruction import waiting
    unavailable = {row['seat'] for row in room['seats']
        if not row.get('connected') or row.get('quit') or row.get('left') or waiting(room, row)}
    available_defenders = sorted(set(plan['defenderSeats']) - unavailable)
    if not available_defenders: return  # Original loading timeout accounts for unexecuted enemies.
    excluded = set(field['requiredSeats']) & unavailable
    if excluded:
        field['requiredSeats'] = sorted(set(field['requiredSeats']) - excluded)
        field['readySeats'] = sorted(set(field['readySeats']) - excluded)
        field['worldLostSeats'] = sorted(set(field['worldLostSeats']) | excluded)
        rooms._event(room, 'native-field-reconstruction-excluded', {'seats': sorted(excluded)})
    if plan.get('authoritySeat') not in available_defenders:
        plan['authoritySeat'] = min(available_defenders)
    if int(rooms.clock()) < field['loadDeadlineTs']:
        start_if_ready(rooms, room)


def validate_binding(room, field):
    """The frozen client plan must still describe the authoritative room plan."""
    try:
        immutable = field['plan']; plan = room['help']
        valid = (digest(immutable) == field['planHash'] and
            immutable['roomId'] == room['roomId'] and immutable['round'] == room['round'] and
            immutable['matchSequence'] == room.get('matchSequence', 1) and
            immutable['compositionGeneration'] == plan['generation'] and
            immutable['enemyIssueSha256'] == digest(plan['enemies']) and
            immutable['originBattlefieldsSha256'] == digest(plan['originBattlefields']) and
            immutable['sourceCountsSha256'] == digest(plan['sourceCounts']) and
            immutable['rightSeat'] == plan['rightSeat'] and
            immutable['allowBondLayerGrowth'] is False and plan['allowBondLayerGrowth'] is False)
        original = {row['seat']: row for row in plan['segments']}
        valid = valid and len(immutable['segments']) == len(original)
        for row in immutable['segments']:
            actual = original[row['seat']]
            valid = valid and (row['side'] == actual['side'] and row['source']['stageId'] == actual['stageId'] and
                row['specialEffects'] == actual['battlefield']['specialEffects'] and
                row['deploymentsSha256'] == digest(actual['deployments']) and row['bondsSha256'] == digest(actual['bonds']))
        states = {str(row['seat']): row['state'] for row in room['seats'] if row.get('state')}
        valid = valid and all(value['runId'] == states[key]['runId'] and
                              value['source']['stageId'] == states[key]['stageId']
                              for key, value in immutable['restoreBySeat'].items())
    except (KeyError, TypeError, ValueError):
        valid = False
    if not valid: raise DomainError('native_field_plan_mismatch')


def abort(rooms, room, reason):
    plan = room['help']; field = plan['fieldLoad']
    validate_binding(room, field)
    field.update(status='aborted', abortReason=reason)
    # A failed composition grants no kills, growth or success. Reuse the
    # existing final partition and per-origin HP settlement atomically.
    finish(plan, {'round': room['round'], 'escapedEnemies': [], 'killInfos': []},
           reporter_seat=plan['rightSeat'], expired=True)
    rooms._event(room, 'native-field-aborted', {'planHash': field['planHash'], 'reason': reason})
    rooms._settle(room)


def acknowledge(rooms, room, seat, op, payload):
    if room['phase'] != 'help' or not room.get('help', {}).get('fieldLoad'):
        raise DomainError('native_field_not_loading')
    plan = room['help']; field = plan['fieldLoad']; actor = seat['seat']
    validate_binding(room, field)
    wanted = {'planHash', 'loadAttempt'} if op == 'field.ready' else {'planHash', 'loadAttempt', 'reason'}
    if (set(payload) != wanted or payload['planHash'] != field['planHash'] or
            digest(field['plan']) != field['planHash']):
        raise DomainError('native_field_plan_mismatch')
    if actor not in {int(value) for value in field['plan']['restoreBySeat']} or seat.get('quit') or seat.get('left'):
        raise DomainError('native_field_actor_unavailable')
    if type(payload['loadAttempt']) is not int or payload['loadAttempt'] != field['loadAttemptBySeat'][str(actor)]:
        raise DomainError('native_field_stale_load_attempt')
    if field['status'] == 'loading' and int(rooms.clock()) >= field['loadDeadlineTs']:
        raise DomainError('native_field_loading_expired')
    if field['status'] not in ('loading', 'running'):
        raise DomainError('native_field_not_loading')
    seat.update(connected=True, lastSeenTs=int(rooms.clock()))
    if op == 'field.ready':
        if actor in field['worldLostSeats']:
            raise DomainError('native_field_running_world_unavailable')
        if any(segment.get('unresolvedCarry') for segment in plan['segments']):
            raise DomainError('native_field_carry_incomplete')
        field['failedSeats'].pop(str(actor), None)
        field['recoveringSeats'] = [value for value in field['recoveringSeats'] if value != actor]
        field['skippedObserverSeats'] = [value for value in field['skippedObserverSeats'] if value != actor]
        if actor not in field['readySeats']:
            field['readySeats'].append(actor); field['readySeats'].sort()
            rooms._event(room, 'native-field-ready', {'seat': actor, 'planHash': field['planHash']})
        start_if_ready(rooms, room)
    else:
        if not isinstance(payload['reason'], str) or payload['reason'] not in FAILURES:
            raise DomainError('invalid_native_field_failure')
        field['failedSeats'][str(actor)] = payload['reason']
        field['readySeats'] = [value for value in field['readySeats'] if value != actor]
        rooms._event(room, 'native-field-failed', {'seat': actor, 'reason': payload['reason']})
        if field['status'] == 'loading':
            if actor in plan['defenderSeats']:
                abort(rooms, room, 'defender-' + payload['reason'])
            else:
                field['requiredSeats'] = [value for value in field['requiredSeats'] if value != actor]
                if actor not in field['skippedObserverSeats']: field['skippedObserverSeats'].append(actor)
                start_if_ready(rooms, room)
        elif actor in plan['defenderSeats']:
            mark_world_lost(field, actor)
            from .multiplayer_reconnect import recover_unity
            recover_unity(rooms, room, seat)


def expire_loading(rooms, room):
    plan = room['help']; field = plan['fieldLoad']
    validate_binding(room, field)
    required_defenders = set(plan['defenderSeats']) & set(field['requiredSeats'])
    if (not required_defenders <= set(field['readySeats']) or
            any(not row.get('connected') or row.get('quit') or row.get('left') for row in room['seats']
                if row['seat'] in required_defenders)):
        abort(rooms, room, 'defender-loading-timeout')
    else:
        field['skippedObserverSeats'] = sorted(set(field['requiredSeats']) - set(field['readySeats']))
        field['requiredSeats'] = sorted(required_defenders | set(field['readySeats']))
        rooms._event(room, 'native-field-observers-skipped', {'seats': field['skippedObserverSeats']})
        start_if_ready(rooms, room, timeout_release=True)


def mark_world_lost(field, actor):
    if actor not in field['worldLostSeats']: field['worldLostSeats'].append(actor)
    field['readySeats'] = [value for value in field['readySeats'] if value != actor]


def invalidate_ready(rooms, room, seat):
    field = room['help']['fieldLoad']; actor = seat['seat']
    validate_binding(room, field)
    field['readySeats'] = [value for value in field['readySeats'] if value != actor]
    if actor not in field['recoveringSeats']:
        field['recoveringSeats'].append(actor)
        field['loadAttemptBySeat'][str(actor)] += 1
        rooms._event(room, 'native-field-reload', {'seat': actor, 'planHash': field['planHash'],
            'loadAttempt': field['loadAttemptBySeat'][str(actor)]})


class NativeFieldRooms(MultiplayerRooms):
    """Production custom rooms with an authenticated native loading barrier."""
    def __init__(self, *args, field_assets=None, **kwargs):
        from .multiplayer_field_assets import ASSETS
        self.field_assets = copy.deepcopy(ASSETS if field_assets is None else field_assets)
        super().__init__(*args, **kwargs)
        from .multiplayer_field_assets import CATALOG_HASH
        if self.c.hash != CATALOG_HASH:
            raise DomainError('native_field_catalog_mismatch')

    def _room(self, room_id):
        room = super()._room(room_id)
        # Loading a saved, not-yet-running round can derive the new native
        # COMMON metadata without advancing its RNG, economy or deadline.
        # _room returns a freshly decoded value: reads do not write SQLite;
        # the next ordinary transactional command persists the projection.
        # A running Unity world must never acquire new multipliers this way.
        if room['round'] > 0 and room['phase'] in ('loading', 'preparation', 'special', 'battle_countdown'):
            from . import multiplayer_policy
            for seat in room['seats']:
                state = seat.get('state')
                if not state or state.get('multiplayerPolicyVersion') != multiplayer_policy.VERSION:
                    continue
                multiplayer_policy.project(state)
                battle = state.get('battle')
                # A seat may have readied before its peers. Its battle input
                # is built already, but the room has not started Unity combat.
                if isinstance(battle, dict) and battle.get('round') == state['round']:
                    battle['nativeModeEffectIds'] = state['nativeModeEffectIds'][:]
                    battle['nativeModeStrength'] = copy.deepcopy(state['multiplayerModeStrength'])
        return room

    def _phase(self, room, phase, seconds=None):
        super()._phase(room, phase, None if phase == 'help' else seconds)
        if phase == 'help':
            # A reconnected self-field loser can load this new immutable Help
            # world. Losing an already running Help world still waits for rest.
            for seat in room['seats']:
                lost = seat.get('reconstruction') or {}
                if (seat.get('connected') and not seat.get('transportSuspension') and
                        lost.get('round') == room['round'] and
                        lost.get('phase', 'battle') in ('battle', 'battle_countdown')):
                    seat.pop('reconstruction', None)
            arm(self, room, self.field_assets)

    def _apply(self, room, seat, op, payload, **kwargs):
        from .multiplayer_reconstruction import waiting
        reconstructing = waiting(room, seat)
        if op in ('field.ready', 'field.failed'):
            if reconstructing: return
            return acknowledge(self, room, seat, op, payload)
        field = (room.get('help') or {}).get('fieldLoad')
        loading = room['phase'] == 'help' and field and field['status'] == 'loading'
        name = payload.get('type', '') if op == 'protocol' else ''
        if isinstance(name, int):
            name = next((key for key, value in self.c.constants['protocolIds'].items() if value == name), '')
        native_rejoin = (op == 'protocol' and isinstance(name, str) and
            name.rsplit('+', 1)[-1] in ('AutoChessBattleSceneSelfBattleFinishUp', 'AutoChessBattleSceneHelpBattleFinishUp') and
            isinstance(payload.get('payload'), dict) and payload['payload'].get('fromRejoin') is True)
        if native_rejoin:
            reported_round = integer(payload['payload'].get('round'), 'round', 1, 15)
        if reconstructing and (op in ('reconnect', 'heartbeat') or native_rejoin or
                isinstance(op, str) and op.startswith('help.') or
                isinstance(name, str) and 'HelpBattle' in name):
            return super()._apply(room, seat, op, payload, **kwargs)
        if room['phase'] == 'help' and field:
            validate_binding(room, field)
        if loading:
            quit_message = isinstance(name, str) and name.rsplit('+', 1)[-1] == 'AutoChessBattleSceneQuitUp'
            if int(self.clock()) >= field['loadDeadlineTs'] and op not in ('leave', 'disconnect', 'scene.quit') and not quit_message:
                raise DomainError('native_field_loading_expired')
            if native_rejoin:
                if seat.get('quit') or seat.get('left'): raise DomainError('multiplayer_seat_quit')
                if reported_round != room['round']:
                    if str(reported_round) not in seat['state'].get('completedReports', {}):
                        raise DomainError('stale_round')
                    return
                invalidate_ready(self, room, seat)
                seat.update(connected=True, lastSeenTs=int(self.clock()))
                return
            if (isinstance(op, str) and op.startswith('help.')) or (isinstance(name, str) and 'HelpBattle' in name):
                raise DomainError('native_field_not_started')
            if op == 'reconnect' and payload.get('unityLost') is True:
                invalidate_ready(self, room, seat)
                # There has been no Help world to lose. Reload from the same
                # immutable plan instead of settling as a lost running battle.
                payload = {**payload, 'unityLost': False}
        elif room['phase'] == 'help' and field and field['status'] == 'running':
            if ((op == 'reconnect' and payload.get('unityLost') is True) or
                    (native_rejoin and type(payload['payload'].get('round')) is int and payload['payload']['round'] == room['round'])):
                mark_world_lost(field, seat['seat'])
            if (seat['seat'] in field['worldLostSeats'] and not native_rejoin and
                    ((isinstance(op, str) and op.startswith('help.')) or
                     (isinstance(name, str) and 'HelpBattle' in name))):
                raise DomainError('native_field_running_world_unavailable')
        result = super()._apply(room, seat, op, payload, **kwargs)
        if loading and room['phase'] == 'help' and seat.get('quit') and seat['seat'] in room['help']['defenderSeats']:
            abort(self, room, 'defender-left')
        elif loading and room['phase'] == 'help' and op in ('heartbeat', 'reconnect'):
            start_if_ready(self, room)
        return result

    def _expire(self, room):
        field = (room.get('help') or {}).get('fieldLoad')
        if room['phase'] == 'help' and field:
            validate_binding(room, field)
        if room['phase'] == 'help' and field and field['status'] == 'loading':
            expire_loading(self, room)
        else:
            super()._expire(room)

    def _view(self, room, actor, *args, **kwargs):
        field = (room.get('help') or {}).get('fieldLoad')
        if room['phase'] == 'help' and field:
            validate_binding(room, field)
        view = super()._view(room, actor, *args, **kwargs)
        if view.get('left') or view.get('nativeSettlementPending'): return view
        state = view['selfState']
        if state and room['round'] > 0 and room['phase'] != 'result':
            from . import multiplayer_policy
            current = (state['round'] == room['round'] and
                state.get('modeStrengthVersion') == multiplayer_policy.VERSION and
                state.get('multiplayerPolicyVersion') == multiplayer_policy.VERSION)
            observer = (state['player']['hp'] <= 0 and state['round'] < room['round'] and
                any(row.get('state') and not row.get('quit') and not row.get('left') and
                    row['state']['player']['hp'] > 0 and row['state']['round'] == room['round'] and
                    row['state'].get('modeStrengthVersion') == multiplayer_policy.VERSION
                    for row in room['seats']))
            if current or observer:
                # Eliminated participants retain their final personal round,
                # inventory and archive. COMMON effects describe the current
                # shared battle independently of that frozen personal state.
                strength = multiplayer_policy.strengths(state, room['round'])
                view['nativeCommonMode'] = {'version': 1, 'roomId': room['roomId'],
                    'matchSequence': room.get('matchSequence', 1), 'round': room['round'],
                    'seat': view['seat'], 'runId': state['runId'], 'modeId': room['modeId'],
                    'nativeEffectIds': strength['nativeEffectIds'][:],
                    'nativeModeStrength': strength, 'observer': observer}
        if room['phase'] == 'help' and field:
            from .multiplayer_reconstruction import waiting
            seat = self._seat(room, actor)
            view['nativeField'] = None if waiting(room, seat) else copy.deepcopy(field)
            if field['status'] == 'loading':
                for packet in ('allStateSync', 'sceneStatus'):
                    payload = view['dtoProjection'][packet]['payload']
                    scene = payload['allStateSyncData']['sceneDetail'] if packet == 'allStateSync' else payload['sceneStatus']
                    scene['forceEndTs'] = -1
        return view
