# SPDX-License-Identifier: GPL-3.0-or-later
"""Shared boss-damage ledger for the authenticated private room bridge.

Only COST_BOSS_HP uses the locally recovered CN parameter order. This is not
an official ActionUp transport, StepDn clock or native battlefield simulator.
See docs/multiplayer-boss-domain.md for the exact static evidence and policies.
"""
import copy
import hashlib
from fractions import Fraction

from .engine import DomainError, integer
from .storage import dump

VERSION = 1
MODES = ('mode_multi_funny', 'mode_multi_normal', 'mode_multi_hard', 'mode_multi_abyss')


def _players(players):
    if type(players) is not list or len(players) > 4:
        raise DomainError('invalid_multiplayer_boss_players')
    result, seats = [], set()
    for item in players:
        if type(item) is not dict:
            raise DomainError('invalid_multiplayer_boss_players')
        wrapped = 'state' in item
        state = item.get('state') if wrapped else item
        if wrapped and (item.get('left') or item.get('quit')):
            continue
        if type(state) is not dict or type(state.get('player')) is not dict:
            raise DomainError('invalid_multiplayer_boss_players')
        seat = integer(item.get('seat') if wrapped else state['player'].get('uidIndex'), 'seat', 0, 3)
        if seat in seats:
            raise DomainError('duplicate_multiplayer_boss_seat')
        seats.add(seat)
        hp = integer(state['player'].get('hp'), 'playerHp')
        if hp > 0:
            result.append((seat, state))
    return sorted(result, key=lambda row: row[0])


def _thresholds(catalog):
    """Use CN's existing radio IDs/ratios without a second message template."""
    result = []
    for row in catalog.common['BroadcastList']:
        if row['Type'] != 3:
            continue
        params = row.get('ParamList')
        try:
            if not isinstance(params, list) or len(params) != 1:
                raise ValueError()
            ratio = Fraction(params[0])
        except (ValueError, TypeError, ZeroDivisionError):
            raise DomainError('invalid_multiplayer_boss_broadcast_configuration')
        if not 0 < ratio <= 1 or not isinstance(row.get('Id'), str):
            raise DomainError('invalid_multiplayer_boss_broadcast_configuration')
        result.append({'broadcastId': row['Id'], 'numerator': ratio.numerator, 'denominator': ratio.denominator})
    if not result or len({row['broadcastId'] for row in result}) != len(result):
        raise DomainError('invalid_multiplayer_boss_broadcast_configuration')
    return sorted(result, key=lambda row: Fraction(row['numerator'], row['denominator']))


def create(catalog, players, round_id, epoch, boss_id):
    """Freeze participants and one CN-configured shared HP pool for an epoch."""
    integer(round_id, 'round', 1, 15)
    integer(epoch, 'epoch', 1)
    active = _players(players)
    if not active:
        raise DomainError('multiplayer_boss_requires_active_players')
    modes = {state.get('modeId') for _, state in active}
    if len(modes) != 1 or next(iter(modes)) not in MODES:
        raise DomainError('multiplayer_boss_mode_mismatch')
    mode = next(iter(modes))
    if catalog.mode(mode)['ModeType'] != 2:
        raise DomainError('multiplayer_boss_mode_required')
    turn = catalog.turns(mode).get(round_id)
    if not turn or not turn['IsBossTurn']:
        raise DomainError('multiplayer_boss_round_required')
    if any(state.get('round') != round_id for _, state in active):
        raise DomainError('multiplayer_boss_player_round_mismatch')
    variants = {row.get('BossId') for row in catalog.rounds(mode).get(round_id, [])}
    definition = catalog.season['BossInfoDict'].get(boss_id) if isinstance(boss_id, str) else None
    if definition is None or boss_id not in variants:
        raise DomainError('multiplayer_boss_configuration_mismatch')
    field = {1: 'BloodPoint', 2: 'BloodPointNormal', 3: 'BloodPointHard', 4: 'BloodPointAbyss'}[catalog.mode(mode)['ModeDifficulty']]
    maximum = integer(definition.get(field), 'initialBossHp', 1)
    seats = [seat for seat, _ in active]
    identity = {'catalogHash': catalog.hash, 'modeId': mode, 'round': round_id, 'epoch': epoch,
                'bossId': boss_id, 'participants': [[seat, state.get('runId')] for seat, state in active]}
    timeline_id = hashlib.sha256(dump(identity).encode()).hexdigest()
    return {'version': VERSION, 'timelineId': timeline_id, 'catalogHash': catalog.hash,
            'modeId': mode, 'round': round_id, 'epoch': epoch, 'bossId': boss_id,
            'playerSeats': seats, 'initialBossHp': maximum, 'bossHp': maximum,
            'bossHpPolicy': 'cn-configured-shared-pool-v1', 'officialScalingVerified': False,
            'contributions': {str(seat): 0 for seat in seats}, 'unattributedDamage': 0,
            'reportedDamage': {str(seat): 0 for seat in seats},
            'lastSeqBySeat': {str(seat): -1 for seat in seats}, 'reports': {},
            'thresholds': _thresholds(catalog), 'broadcastSeq': 0,
            'broadcastSeen': {str(seat): [] for seat in seats}, 'broadcasts': [],
            'stepTimingImplemented': False, 'officialWireCompatible': False,
            'transport': 'private authenticated COST_BOSS_HP bridge'}


def _details(timeline, seq, *, applied=0, reported=0, broadcasts=None, duplicate=False):
    return {'acknowledgedNativeSeq': seq, 'duplicateNativeSeq': duplicate,
            'epoch': timeline['epoch'], 'round': timeline['round'], 'bossHp': timeline['bossHp'],
            'initialBossHp': timeline['initialBossHp'], 'bossDefeated': timeline['bossHp'] == 0,
            'appliedDamage': applied, 'reportedDamage': reported,
            'contributions': dict(timeline['contributions']), 'unattributedDamage': timeline['unattributedDamage'],
            'broadcasts': copy.deepcopy(broadcasts or []), 'stepTimingImplemented': False,
            'officialWireCompatible': False}


def apply_actions(timeline, payload, *, seat, owned_ids, catalog, credited_seats=None,
                  compatible_fingerprints=None):
    """Apply one atomic authenticated batch; each seat owns its seq namespace.

    The outer private bridge supplies round/epoch. They are not added to the
    official ActionUp DTO. -1 attribution stays unattributed; one seat cannot
    fabricate another seat's contribution or attacker ownership.
    """
    if type(timeline) is not dict or timeline.get('version') != VERSION or timeline.get('catalogHash') != catalog.hash:
        raise DomainError('multiplayer_boss_timeline_version_mismatch')
    integer(seat, 'seat', 0, 3)
    if seat not in timeline['playerSeats']:
        raise DomainError('multiplayer_boss_participant_required')
    if type(payload) is not dict:
        raise DomainError('invalid_multiplayer_boss_actions')
    if set(payload) & {'duration', 'stepData', 'stepDuration'}:
        raise DomainError('multiplayer_boss_step_timing_pending')
    if set(payload) != {'round', 'epoch', 'seq', 'actions'}:
        raise DomainError('invalid_multiplayer_boss_actions')
    if integer(payload['round'], 'round', 1, 15) != timeline['round']:
        raise DomainError('multiplayer_boss_round_mismatch')
    if integer(payload['epoch'], 'epoch', 1) != timeline['epoch']:
        raise DomainError('multiplayer_boss_epoch_mismatch')
    seq = integer(payload['seq'], 'seq')
    actions = payload['actions']
    if type(actions) is not list or len(actions) > 4096:
        raise DomainError('invalid_multiplayer_boss_actions')
    fingerprint = hashlib.sha256(dump(actions).encode()).hexdigest()
    key = str(seat) + ':' + str(seq)
    previous = timeline['reports'].get(key)
    if previous is not None:
        # Only the authenticated room caller may supply its exact current/old
        # instance-bijection alternatives. Payloads cannot supply this argument.
        accepted = {fingerprint} if compatible_fingerprints is None else compatible_fingerprints
        if fingerprint not in accepted or previous['fingerprint'] not in accepted:
            raise DomainError('multiplayer_boss_seq_conflict')
        # A new bridge command retrying the same native batch gets current
        # authoritative state, and no old radio events to play a second time.
        return _details(timeline, seq, duplicate=True)
    if seq <= timeline['lastSeqBySeat'][str(seat)]:
        raise DomainError('stale_multiplayer_boss_seq')
    if not isinstance(owned_ids, (list, tuple, set, frozenset)) or len(owned_ids) > 4096:
        raise DomainError('invalid_multiplayer_boss_owned_ids')
    owned = {integer(value, 'ownedInstId') for value in owned_ids}
    # Only the private native paired-field coordinator supplies this set after
    # authenticating its sole reporter. The public domain bridge stays self-only.
    credits = {seat} if credited_seats is None else {integer(value, 'creditedSeat', 0, 3) for value in credited_seats}
    if not credits or not credits <= set(timeline['playerSeats']):
        raise DomainError('invalid_multiplayer_boss_credited_seats')
    normalized = []
    operation = catalog.enum('Torappu.AutoChessBattleStepActionOperate', 'COST_BOSS_HP')
    maximum = catalog.number('Torappu.Battle.AutoChessBattleConst', 'MAX_BATTLE_DAMAGE')
    damage_sources = catalog.constants['enums']['Torappu.AutoChessBattleDamageSrcType'].values()
    for action in actions:
        if type(action) is not dict or set(action) != {'playerUidIndex', 'operate', 'paramList'}:
            raise DomainError('invalid_multiplayer_boss_actions')
        if type(action['playerUidIndex']) is not int or action['playerUidIndex'] != seat:
            raise DomainError('wrong_multiplayer_boss_player')
        if type(action['operate']) is not int or action['operate'] != operation:
            raise DomainError('multiplayer_boss_operate_pending', str(action.get('operate')))
        params = action['paramList']
        if type(params) is not list or len(params) != 4 or any(type(value) is not int for value in params):
            raise DomainError('invalid_multiplayer_boss_damage_params')
        damage = integer(params[0], 'damage', 1, maximum - 1)
        attacker = integer(params[1], 'attackerInstId', -2147483648)
        if attacker >= 0 and attacker not in owned:
            raise DomainError('unknown_multiplayer_boss_attacker')
        if params[2] not in damage_sources:
            raise DomainError('invalid_damage_source')
        if params[3] != -1 and params[3] not in credits:
            raise DomainError('wrong_multiplayer_boss_credited_player')
        normalized.append((damage, attacker, params[2], params[3]))
    # All operations validate before the first HP/counter/radio mutation.
    # Historical report values and broadcasts are append-only. Copy their
    # containers, plus every mutable counter, without recopying the full fight.
    pending = dict(timeline)
    for name in ('reportedDamage', 'contributions', 'lastSeqBySeat', 'reports'):
        pending[name] = dict(timeline[name])
    pending['broadcasts'] = list(timeline['broadcasts'])
    pending['broadcastSeen'] = {k: list(v) for k, v in timeline['broadcastSeen'].items()}
    emitted, audit = [], []
    applied_total, reported_total = 0, 0
    for damage, attacker, source, credited in normalized:
        applied = min(damage, pending['bossHp'])
        pending['bossHp'] -= applied
        applied_total += applied
        reported_total += damage
        pending['reportedDamage'][str(seat)] += damage
        if credited == -1:
            pending['unattributedDamage'] += applied
        else:
            pending['contributions'][str(credited)] += applied
            cumulative = pending['contributions'][str(credited)]
            for threshold in pending['thresholds']:
                broadcast_id = threshold['broadcastId']
                crossed = cumulative * threshold['denominator'] >= pending['initialBossHp'] * threshold['numerator']
                # One shared HP denominator. A high-contribution threshold
                # (>= 50%) can announce only one winner in this boss encounter,
                # including the exact 50/50 boundary. Lower thresholds remain
                # per credited player. Existing saved ledgers need no migration.
                seen = (any(broadcast_id in values for values in pending['broadcastSeen'].values())
                        if threshold['numerator'] * 2 >= threshold['denominator']
                        else broadcast_id in pending['broadcastSeen'][str(credited)])
                if crossed and not seen:
                    pending['broadcastSeq'] += 1
                    message = {'eventId': 'boss:' + pending['timelineId'] + ':' + str(pending['broadcastSeq']),
                               'round': pending['round'], 'epoch': pending['epoch'],
                               'uIdx': credited, 'broadcastId': broadcast_id, 'strParams': [], 'intParams': []}
                    pending['broadcasts'].append(message)
                    pending['broadcastSeen'][str(credited)].append(broadcast_id)
                    emitted.append(message)
        audit.append({'damage': damage, 'appliedDamage': applied, 'attackerInstId': attacker,
                      'damageSrc': source, 'creditedPlayer': credited})
    pending['lastSeqBySeat'][str(seat)] = seq
    pending['reports'][key] = {'fingerprint': fingerprint, 'actions': audit,
                               'appliedDamage': applied_total, 'reportedDamage': reported_total}
    timeline.clear()
    timeline.update(pending)
    return _details(timeline, seq, applied=applied_total, reported=reported_total, broadcasts=emitted)


def hidden_eligible(catalog, mode, players):
    """Team gate only; a prior baseline boss victory is the room's prerequisite."""
    if mode not in MODES or catalog.mode(mode)['ModeType'] != 2:
        raise DomainError('multiplayer_boss_mode_required')
    active = _players(players)
    if any(state.get('modeId') != mode for _, state in active):
        raise DomainError('multiplayer_boss_mode_mismatch')
    rounds = [number for number, row in catalog.turns(mode).items() if row['IsBossTurn']]
    if catalog.mode(mode)['ModeDifficulty'] < 2 or len(rounds) < 2:
        return False
    hp, layers = 0, 0
    for _, state in active:
        hp += state['player']['hp']
        bonds = state['player'].get('bonds', [])
        if type(bonds) is not list or len(bonds) > 4096:
            raise DomainError('invalid_multiplayer_boss_bonds')
        seen = set()
        for bond in bonds:
            if type(bond) is not dict or type(bond.get('isActive')) is not bool:
                raise DomainError('invalid_multiplayer_boss_bonds')
            ident = integer(bond.get('bondIdentifier'), 'bondIdentifier')
            count = integer(bond.get('stackCnt'), 'stackCnt')
            if ident in seen:
                raise DomainError('duplicate_multiplayer_boss_bond')
            seen.add(ident)
            if bond['isActive']:
                layers += count
    return layers > 1200 and hp > 1
