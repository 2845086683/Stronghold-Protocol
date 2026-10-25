# SPDX-License-Identifier: GPL-3.0-or-later
"""CN native final-assault adapter: deterministic private clock, no combat code.

The Unity StepModeManager executes all frames, deployment, enemies and Runes.
This module authenticates its exact recovered ActionUp and supplies server-only
actions 5..10. See docs/multiplayer-native-boss-0157.md for deliberate policies.
"""
import copy
import hashlib

from .engine import DomainError, integer
from .storage import dump
from . import multiplayer_boss

VERSION = 1
DURATION_MS = 100
MAX_STEPS = 18000
ACK_WINDOW = 128


def groups(players):
    seats = sorted(row['seat'] for row in players if not row.get('left') and not row.get('quit')
                   and row.get('state') and row['state']['player']['hp'] > 0)
    return [{'group': index // 2 + 1, 'players': seats[index:index + 2]}
            for index in range(0, len(seats), 2)]


def _group(native, seat):
    return next((row for row in native['groups'] if seat in row['players']), None)


def authority(native, members, group):
    seats = next(row['players'] for row in native['groups'] if row['group'] == group)
    connected = [row['seat'] for row in members if row['seat'] in seats and row.get('connected', True)
                 and not row.get('quit') and not row.get('left')]
    return min(connected) if connected else min(seats)


def _down(operation, params):
    # CN _ParseApplyStepAction accepts server actions only with playerUidIndex=0.
    return {'playerUidIndex': 0, 'operate': operation, 'paramList': params}


def _team_hp(room):
    return sum(row['state']['player']['hp'] for row in room['seats'] if row.get('state') and not row.get('left'))


def _status(room, acknowledgements=None):
    native, timeline = room['nativeBoss'], room['bossTimeline']
    result = [_down(8, [timeline['bossHp'], row['group']] + (acknowledgements or {}).get(row['group'], []))
              for row in native['groups']]
    # CN BossBattleRoundStatus.get_curEnemyCnt returns battleProcess directly:
    # this is a completed-enemy count, never a percentage or an invented timer.
    killed = sum(row['dead'] for row in native['enemies'].values() if not row['summoned'])
    result.append(_down(9, [_team_hp(room), killed]))
    return result


def create(room, now, catalog):
    timeline = room['bossTimeline']; paired = groups(room['seats'])
    if not paired:
        raise DomainError('multiplayer_boss_requires_active_players')
    native = {'version': VERSION, 'epoch': timeline['epoch'], 'round': room['round'],
              'startedAtMs': int(now * 1000), 'durationMs': DURATION_MS,
              'groups': paired, 'enemies': {}, 'steps': [], 'pending': [],
              'lastSeq': -1, 'reports': {}, 'lastSeqByGroup': {str(row['group']): -1 for row in paired},
              'initialBossHp': timeline['initialBossHp'], 'policy': 'private-100ms-paired-authority-v1',
              'initialEnemyGroup': [], 'acceptedSeqsByGroup': {}, 'nativeBossCountByGroup': {}}
    room['nativeBoss'] = native
    # GenerateBossBattleEnemyData calls the original _InsertLocalActions;
    # MainGameInBossBattle also starts boss_battle_multi_player for an occupied
    # pair. Those actions already create the original boss(es), as in LOCAL.
    # Issuing the same keys again creates extra bosses, not extra teammates.
    from .native_combat import local_boss_keys
    boss_keys = local_boss_keys(catalog)
    alias = 0
    for paired_group in paired:
        by_action = {}
        representative = next(row['state']['battle'] for row in room['seats']
                              if row['seat'] == paired_group['players'][0])
        level = catalog.level(representative['levelId'])
        local_count = sum(action['Count'] for wave in level['Waves'] for fragment in wave['Fragments']
                          for action in fragment['Actions'] if action['Key'] in boss_keys)
        if len(paired_group['players']) > 1:
            local_count += sum(action['Count'] for phase in (level.get('Branches') or {}).get(
                'boss_battle_multi_player', {}).get('Phases', []) for action in phase['Actions']
                if action['Key'] in boss_keys)
        native['nativeBossCountByGroup'][str(paired_group['group'])] = local_count
        for seat in paired_group['players']:
            member = next(row for row in room['seats'] if row['seat'] == seat)
            for unit in member['state']['battle']['units']:
                if unit['enemyId'] in boss_keys:
                    continue
                alias += 1
                if alias >= 65536:
                    raise DomainError('multiplayer_boss_native_enemy_namespace_exhausted')
                native['enemies'][str(alias)] = {'instId': alias, 'ownerSeat': seat,
                    'domainInstId': unit['instId'], 'enemyId': unit['enemyId'], 'actionIndex': unit['actionIndex'],
                    'initialGroup': paired_group['group'], 'currentGroup': paired_group['group'],
                    'escapeCount': 0, 'dead': False, 'summoned': False}
                by_action.setdefault((unit['enemyId'], unit['actionIndex']), []).append(alias)
        for (enemy_id, action_index), ids in by_action.items():
            native['initialEnemyGroup'].append({'playerGroup': paired_group['group'],
                'enemyId': enemy_id, 'actionIndex': action_index, 'instIdList': ids})
    timeline.update(stepTimingImplemented=True, transport='private authenticated CN ActionUp / native RevStep',
                    nativeStepPolicy=native['policy'])
    if not all(native['nativeBossCountByGroup'].values()):
        raise DomainError('multiplayer_native_boss_source_missing')
    # TC R14 (events-003:2147): 42 issued enemies across groups 1/2, enemyCount
    # 43. Shared HP is one objective even when native branches create multiple
    # physical boss bodies. Do not count the same shared objective per group.
    native['initialEnemyCount'] = alias + 1
    native['lastTeamHp'] = _team_hp(room); native['initialTeamHp'] = native['lastTeamHp']
    native['pending'] = _status(room)
    advance(room, now)
    return native


def advance(room, now):
    """Persist elapsed fixed steps; GET/projection never advances the clock."""
    native = room.get('nativeBoss')
    if room.get('phase') != 'boss' or not native:
        return False
    target = max(0, (int(now * 1000) - native['startedAtMs']) // DURATION_MS)
    if target >= MAX_STEPS:
        raise DomainError('multiplayer_boss_native_clock_limit')
    team_hp = _team_hp(room)
    if team_hp != native['lastTeamHp']:
        native['pending'].extend(_status(room)); native['lastTeamHp'] = team_hp
    changed = False
    while native['lastSeq'] < target:
        seq = native['lastSeq'] + 1
        actions = native['pending']; native['pending'] = []
        native['steps'].append({'seq': seq, 'duration': DURATION_MS, 'actions': actions})
        native['lastSeq'] = seq; changed = True
    return changed


def view(room, seat, after_step=-1, step_epoch=0):
    native = room.get('nativeBoss')
    if room.get('phase') != 'boss' or not native:
        return None
    own = _group(native, seat)
    watched = next((row.get('watchSeat', seat) for row in room['seats'] if row['seat'] == seat), seat)
    observed = _group(native, watched) or own or native['groups'][0]
    integer(after_step, 'afterStep', -1)
    integer(step_epoch, 'stepEpoch', 0)
    if step_epoch != native['epoch']:
        after_step = -1
    if after_step > native['lastSeq']:
        raise DomainError('multiplayer_boss_step_cursor_ahead')
    return {'version': VERSION, 'epoch': native['epoch'], 'round': native['round'],
            'durationMs': DURATION_MS, 'group': observed['group'],
            'reportGroup': own['group'] if own else 0,
            'reportAuthority': authority(native, room['seats'], own['group']) if own else -1,
            'initialBossHp': native['initialBossHp'], 'firstSeq': after_step + 1,
            'timelineId': room['bossTimeline']['timelineId'],
            # An accepted native cost may belong to a much earlier played step
            # than the wall-clock tail. Remove its original prediction cache
            # immediately; waiting for the queued StepDn ACK can exceed CN's
            # 30-step prediction expiry and incorrectly recover confirmed HP.
            'damageAcknowledgements': {'roomId': room['roomId'],
                'timelineId': room['bossTimeline']['timelineId'], 'epoch': native['epoch'],
                'round': native['round'], 'group': observed['group'],
                'seqs': list(native.get('acceptedSeqsByGroup', {}).get(str(observed['group']), []))},
            'availableThrough': native['lastSeq'],
            # A bounded replay window; cursor is acknowledged by Unity RevStep,
            # never by merely receiving an HTTP response on a worker thread.
            'steps': copy.deepcopy(native['steps'][after_step + 1:after_step + 129]), 'policy': native['policy'],
            'nativeAccepted': False, 'officialWireCompatible': False}


def apply_actions(room, seat, payload, catalog):
    native, timeline = room.get('nativeBoss'), room.get('bossTimeline')
    if not native or not timeline or native['version'] != VERSION:
        raise DomainError('multiplayer_boss_native_clock_required')
    if type(payload) is not dict or set(payload) != {'seq', 'actions'}:
        raise DomainError('invalid_multiplayer_boss_native_actions')
    seq = integer(payload['seq'], 'seq', 0, native['lastSeq'])
    group = _group(native, seat)
    if group is None or authority(native, room['seats'], group['group']) != seat:
        raise DomainError('multiplayer_boss_report_authority_required')
    actions = payload['actions']
    if type(actions) is not list or len(actions) > 4096:
        raise DomainError('invalid_multiplayer_boss_native_actions')
    group_id = group['group']; key = str(group_id) + ':' + str(seq)
    # Group-scoped dedup survives reporter failover. The native sender index
    # changes on failover but cannot create a second copy of the same effects.
    canonical = copy.deepcopy(actions)
    for action in canonical:
        if type(action) is not dict or set(action) != {'playerUidIndex', 'operate', 'paramList'}:
            raise DomainError('invalid_multiplayer_boss_native_actions')
        if type(action['playerUidIndex']) is not int or action['playerUidIndex'] != seat:
            raise DomainError('wrong_multiplayer_boss_player')
        action['playerUidIndex'] = 0
    fingerprint = hashlib.sha256(dump(canonical).encode()).hexdigest()
    previous = native['reports'].get(key)
    if previous is not None:
        from .multiplayer_instance_ids import historical_fingerprints
        if previous not in historical_fingerprints(room, canonical):
            raise DomainError('multiplayer_boss_group_seq_conflict')
        native['pending'].extend(_status(room, {group_id: [seq]}))
        _remember_ack(native, group_id, seq)
        return {'duplicateNativeSeq': True, 'broadcasts': [], 'bossDefeated': timeline['bossHp'] == 0}
    if seq <= native['lastSeqByGroup'][str(group_id)]:
        raise DomainError('stale_multiplayer_boss_group_seq')
    # Validation/mutation use private copies, including mixed enemy/damage batches.
    pending = copy.copy(room)
    pending['nativeBoss'] = dict(native); pending['bossTimeline'] = dict(timeline)
    # Only these structures are mutated by ActionUp. Committed steps, group
    # definitions and historical receipts remain immutable, including on a
    # mixed enemy/damage batch that fails validation halfway through.
    for name in ('enemies', 'acceptedSeqsByGroup'):
        pending['nativeBoss'][name] = copy.deepcopy(native.get(name, {}))
    for name in ('reports', 'lastSeqByGroup'):
        pending['nativeBoss'][name] = dict(native[name])
    pending['nativeBoss']['pending'] = list(native['pending'])
    pn = pending['nativeBoss']; pt = pending['bossTimeline']; down = []; costs = []
    owned = {unit['instId'] for member in room['seats'] if member['seat'] in group['players']
             for field in ('charChess', 'trapChess') for unit in member['state']['player'][field]}
    sources = set(catalog.constants['enums']['Torappu.AutoChessBattleDamageSrcType'].values())
    summoned = {row['ExtraEnemyIdentifier']: row['EnemyKey'] for row in catalog.common['RandomEnemyAttributeDict'].values()
                if row['ExtraEnemyIdentifier'] > 0}
    for action in actions:
        operation = action['operate']; params = action['paramList']
        if type(operation) is not int or operation not in (1, 2, 3, 4) or type(params) is not list or any(type(v) is not int for v in params):
            raise DomainError('invalid_multiplayer_boss_native_action')
        if operation == 3:
            costs.append(action); continue
        if len(params) != (5 if operation == 1 else 2):
            raise DomainError('invalid_multiplayer_boss_enemy_params')
        ident = integer(params[0], 'enemyInstId', 1, 2147483647)
        escaped = integer(params[1], 'escapedCount', 0)
        record = pn['enemies'].get(str(ident))
        if operation == 4 and record is None:
            identifier = ident >> 16; count = (ident >> 8) & 255; origin = ident & 255
            if identifier not in summoned or count < 1 or origin != group_id or escaped != 0:
                raise DomainError('invalid_multiplayer_boss_summoned_identifier')
            record = {'instId': ident, 'enemyId': summoned[identifier], 'actionIndex': -1,
                'initialGroup': origin, 'currentGroup': origin, 'escapeCount': 0,
                'dead': False, 'summoned': True, 'ownerSeat': seat, 'domainInstId': None}
            pn['enemies'][str(ident)] = record
        if not record or record['dead'] or record['currentGroup'] != group_id or record['escapeCount'] != escaped:
            raise DomainError('multiplayer_boss_enemy_state_mismatch')
        if operation == 1:
            attacker = integer(params[2], 'attackerInstId', -2147483648)
            if attacker >= 0 and attacker not in owned:
                raise DomainError('unknown_multiplayer_boss_attacker')
            if params[3] not in sources or params[4] not in [-1] + group['players']:
                raise DomainError('invalid_multiplayer_boss_kill_attribution')
            record['dead'] = True; down.append(_down(7, [ident]))
        else:
            if (operation == 2) != (not record['summoned']):
                raise DomainError('multiplayer_boss_enemy_kind_mismatch')
            target = next((row['group'] for row in pn['groups'] if row['group'] != group_id), group_id)
            record['currentGroup'] = target; record['escapeCount'] += 1
            down.append(_down(6 if record['summoned'] else 5, [ident, record['escapeCount'], target]))
    from .multiplayer_instance_ids import historical_fingerprints
    details = multiplayer_boss.apply_actions(pt, {'seq': seq, 'round': pt['round'], 'epoch': pt['epoch'], 'actions': costs},
        seat=seat, owned_ids=owned, credited_seats=group['players'], catalog=catalog,
        compatible_fingerprints=historical_fingerprints(room, costs))
    pn['reports'][key] = fingerprint; pn['lastSeqByGroup'][str(group_id)] = seq
    _remember_ack(pn, group_id, seq)
    pn['pending'].extend(down + _status(pending, {group_id: [seq]}))
    room['nativeBoss'] = pn; room['bossTimeline'] = pt
    # Domain settlement keeps the original seat namespace. A transfer kill is
    # credited to its enemy owner's run exactly once, regardless of killer group.
    for record in pn['enemies'].values():
        if record['dead'] and not record['summoned']:
            member = next(row for row in room['seats'] if row['seat'] == record['ownerSeat'])
            killed = member['state']['battle'].setdefault('killed', [])
            if record['domainInstId'] not in killed:
                killed.append(record['domainInstId'])
    return details


def _remember_ack(native, group, seq):
    """Bounded explicit accepted seqs, never a cumulative guessed watermark."""
    values = native.setdefault('acceptedSeqsByGroup', {}).setdefault(str(group), [])
    if seq not in values:
        values.append(seq)
        values.sort()
        del values[:-ACK_WINDOW]
