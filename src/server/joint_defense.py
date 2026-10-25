# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Planning/carry/attribution reviewed against server/match/unite.js in the local
# Stronghold-Protocol reference. See docs/joint-defense-domain.md for hashes,
# licence, native DTO evidence, and intentional differences from that reference.
"""Persistent joint-defense domain policy, without a second battle simulator.

Unity retains deployment, battle, runes, conditions and effects. This module
selects the participating fields and owns the enemy attribution ledger. Its
segment model is private JSON, not an asserted native map-composition protocol.
"""
import copy
import hashlib
import json

from .engine import DomainError, integer

VERSION = 1
MAX_PLAYERS = 4
MAX_ENEMIES = 16384

# Explicit private policy. Native occupied-volume/wire deployCount is a different
# metric; neither it nor arbitrary caller counts may silently change ranking.
# See docs/multiplayer-followup-0158.md and the count audit under research/.
# Official server TOKEN tie-breaking
# remains unresolved; current preparation-operator behavior is preserved.
COUNT_POLICY = 'private-preparation-operators-including-downed-v1'


def authority_seat(plan):
    """One selected native defender owns this shared field's outcome.

    Observers may run the same scene for display, but their unfinished local
    journals cannot close or overwrite the defender's enemy ledger. Persisted
    plans predating this policy use the same deterministic first defender.
    """
    defenders = plan.get('defenderSeats', [])
    if not defenders: raise DomainError('joint_defense_authority_unavailable')
    value = plan.get('authoritySeat', defenders[0])
    if type(value) is not int or value not in defenders:
        raise DomainError('invalid_joint_defense_authority')
    return value


def count_deployed_operators(positions):
    return sum(integer(row.get('chessType', 0), 'chessType', 0, 3) == 0
               for row in _rows(positions, 'positions'))


def _rows(value, name, limit=4096):
    if type(value) is not list or len(value) > limit or any(type(row) is not dict for row in value):
        raise DomainError('invalid_joint_defense_' + name)
    return value


def _text(value, name):
    if not isinstance(value, str) or not 1 <= len(value) <= 256:
        raise DomainError('invalid_joint_defense_' + name)
    return value


def _bool(value, name):
    if type(value) is not bool:
        raise DomainError('invalid_joint_defense_' + name)
    return value


def _digest(value):
    return hashlib.sha256(json.dumps(value, sort_keys=True, separators=(',', ':'),
                                     ensure_ascii=False, allow_nan=False).encode()).hexdigest()


def _position(value):
    if type(value) is not dict or set(value) != {'x', 'y'}:
        raise DomainError('invalid_joint_defense_position')
    return {'x': integer(value['x'], 'x', 0, 65535),
            'y': integer(value['y'], 'y', 0, 32767)}


def _placements(player):
    result = []
    seen = set()
    tiles = set()
    for row in _rows(player.get('positions', []), 'positions'):
        inst = integer(row.get('instId'), 'instId')
        kind = integer(row.get('chessType', 0), 'chessType', 0, 3)
        pos = _position(row.get('position'))
        # Tokens can share their owner's native instId. A token's tile is part
        # of its private identity because an owner may have several summons.
        identity = (kind, inst, pos['x'], pos['y']) if kind == 3 else (kind, inst)
        if identity in seen or (pos['x'], pos['y']) in tiles:
            raise DomainError('joint_defense_position_collision')
        seen.add(identity)
        tiles.add((pos['x'], pos['y']))
        result.append({'instId': inst, 'chessType': kind, 'position': pos,
                       'direction': integer(row.get('direction', 0), 'direction', 0, 3)})
    return result


def _statuses(player, field, *, token=False):
    result = {}
    for row in _rows(player.get(field, []), 'character_status'):
        inst = integer(row.get('instId'), 'instId')
        status = {'tech': integer(row.get('tech'), 'spRatio', 0, 100)}
        if not token:
            status['hp'] = integer(row.get('hp'), 'hpRatio', 0, 100)
        pos = _position(row['position']) if token and 'position' in row else None
        key = (inst, pos['x'], pos['y']) if pos else (inst,)
        if key in result and result[key] != status:
            raise DomainError('conflicting_joint_defense_character_status')
        result[key] = status
    return result


def _player(player):
    seat = integer(player.get('seat'), 'seat', 0, MAX_PLAYERS - 1)
    outcome = player.get('outcome')
    if outcome not in ('perfect', 'failed'):
        raise DomainError('joint_defense_self_report_required')
    positions = _placements(player)
    chars = _statuses(player, 'charBattleStatusList')
    tokens = _statuses(player, 'tokenBattleStatusList', token=True)
    terminal_statuses_complete = _bool(player.get('nativeTerminalStatusesComplete', False),
                                      'terminal_statuses_complete')
    token_counts = {}
    for row in positions:
        if row['chessType'] == 3:
            token_counts[row['instId']] = token_counts.get(row['instId'], 0) + 1
    deployed = []
    unresolved = []
    for row in positions:
        obj = copy.deepcopy(row)
        kind, inst = row['chessType'], row['instId']
        obj['applyAfterDeployment'] = True
        if kind == 0:
            status = chars.get((inst,))
            if status is None and terminal_statuses_complete:
                # Native Help _ApplyChessStatus calls FinishWithNoReason when
                # an operator is absent from the final status dictionary.
                # Only a validated, complete native FinishUp has that meaning;
                # timeout/recovery placeholders and old saves remain unknown.
                obj['carry'] = {'hp': 0, 'tech': 0, 'forceRetreat': True}
            elif status is None:
                obj['carry'] = None
                unresolved.append(copy.deepcopy(row))
            else:
                obj['carry'] = {'hp': status['hp'], 'tech': status['tech'],
                                'forceRetreat': status['hp'] == 0}
        elif kind == 3:
            pos = row['position']
            status = tokens.get((inst, pos['x'], pos['y']))
            if status is None and token_counts[inst] == 1:
                status = tokens.get((inst,))
            # The plain CharBattleStatus DTO does not disambiguate an owner
            # and a TOKEN sharing instId. Never apply the owner's SP to it.
            obj['carry'] = {'tech': status['tech']} if status else None
            if status is None:
                unresolved.append(copy.deepcopy(row))
        else:
            obj['carry'] = None
        deployed.append(obj)
    bonds = []
    seen_bonds = set()
    for row in _rows(player.get('activeBonds', []), 'bonds'):
        ident = integer(row.get('bondIdentifier'), 'bondIdentifier')
        if ident in seen_bonds:
            raise DomainError('duplicate_joint_defense_bond')
        seen_bonds.add(ident)
        active = _bool(row.get('isActive', True), 'active_bond')
        layer = integer(row.get('stackCnt', 0), 'stackCnt')
        if active:
            bonds.append({'bondIdentifier': ident, 'isActive': True, 'stackCnt': layer})
    # Preparation operators remain in this ranking even when their terminal HP
    # is zero. TOKEN occupancy and terminal TOKEN status do not change it.
    operators = [row for row in positions if row['chessType'] == 0]
    deployed_count = count_deployed_operators(positions)
    if 'deployedCount' in player and integer(player['deployedCount'], 'deployedCount', 0, 4096) != deployed_count:
        raise DomainError('joint_defense_deployed_count_mismatch')
    stats = {'deployedCount': deployed_count, 'countPolicy': COUNT_POLICY,
             'activeBondCount': len(bonds), 'activeBondLayers': sum(row['stackCnt'] for row in bonds),
             'standingCount': sum(chars.get((row['instId'],), {}).get('hp', 0) > 0 for row in operators)}
    field = player.get('battlefield', {})
    if type(field) is not dict:
        raise DomainError('invalid_joint_defense_battlefield')
    return {'seat': seat, 'outcome': outcome, 'stageId': _text(player.get('stageId'), 'stage'),
            'battlefield': copy.deepcopy(field), 'deployments': deployed, 'bonds': bonds,
            'stats': stats, 'unresolvedCarry': unresolved}


def _enemy(row):
    if type(row) is not dict:
        raise DomainError('invalid_joint_defense_enemy')
    token = _bool(row.get('isToken', False), 'enemy_token')
    # Native GenerateHelpBattleEnemyData decodes an indexed summon from the
    # signed high 16 bits of enemyInstId. Its official enemyId is empty; keep
    # that opaque identity for the original generator instead of inventing a
    # normal enemy key (TC 2026-10-20, events-001:3282).
    enemy_id = row.get('enemyId')
    if not (token and enemy_id == ''): enemy_id = _text(enemy_id, 'enemy_id')
    return {'ownerPlayerIndex': integer(row.get('ownerPlayerIndex'), 'ownerPlayerIndex', 0, MAX_PLAYERS - 1),
            'enemyId': enemy_id,
            'enemyInstId': integer(row.get('enemyInstId'), 'enemyInstId'),
            'isToken': token}


def _enemy_key(row):
    return f"{row['ownerPlayerIndex']}:{int(row['isToken'])}:{row['enemyInstId']}"


def build_plan(players, *, round_id, generation, started_at, duration_seconds):
    """Build a JSON snapshot once after the complete self-finish barrier.

    Returns None unless at least one perfect and one failed player coexist.
    `duration_seconds` is a fresh full-round budget supplied by the caller;
    elapsed self-battle time must not be subtracted from it.
    """
    _rows(players, 'players', MAX_PLAYERS)
    integer(round_id, 'round', 1, 15)
    integer(generation, 'generation', 1)
    integer(started_at, 'startedAtTs', 0, 0x7fffffffffffffff)
    integer(duration_seconds, 'durationSeconds', 1, 86400)
    normalized = [_player(player) for player in players]
    seats = [player['seat'] for player in normalized]
    if len(seats) != len(set(seats)):
        raise DomainError('duplicate_joint_defense_seat')
    perfects = [player for player in normalized if player['outcome'] == 'perfect']
    failed = [player for player in normalized if player['outcome'] == 'failed']
    if not perfects or not failed:
        return None

    def selection(player):
        stat = player['stats']
        return (-stat['deployedCount'], -stat['activeBondCount'], -stat['standingCount'], player['seat'])

    def order(player):
        stat = player['stats']
        return (-stat['deployedCount'], -stat['activeBondCount'], -stat['activeBondLayers'],
                -stat['standingCount'], player['seat'])

    defenders = sorted(sorted(perfects, key=selection)[:2], key=order)
    enemies = {}
    for original, player in zip(players, normalized):
        rows = _rows(original.get('remainingEnemies', []), 'enemies', MAX_ENEMIES)
        if player['outcome'] == 'perfect' and rows:
            raise DomainError('joint_defense_perfect_player_has_remaining_enemies')
        for row in rows:
            enemy = _enemy(row)
            if enemy['ownerPlayerIndex'] != player['seat']:
                raise DomainError('joint_defense_wrong_enemy_owner')
            key = _enemy_key(enemy)
            if key in enemies and enemies[key] != enemy:
                raise DomainError('conflicting_joint_defense_enemy')
            enemies[key] = enemy
    if len(enemies) > MAX_ENEMIES:
        raise DomainError('invalid_joint_defense_enemies')
    source = {str(player['seat']): 0 for player in failed}
    for enemy in enemies.values():
        source[str(enemy['ownerPlayerIndex'])] += 1
    segments = []
    for index, player in enumerate(defenders):
        # Explicit per-owner segments preserve differing terrain choices. The
        # native adapter must bind real composition code before materializing
        # coordinates; no fixed +8 guess is baked into the domain state.
        segments.append({'seat': player['seat'], 'side': 'right' if index == 0 else 'left',
                         'encounterOrder': index, 'stageId': player['stageId'],
                         'battlefield': player['battlefield'], 'deployments': player['deployments'],
                         'bonds': player['bonds'], 'stats': player['stats'],
                         'unresolvedCarry': player['unresolvedCarry']})
    return {'version': VERSION, 'round': round_id, 'generation': generation,
            'status': 'battle', 'startedAtTs': started_at, 'deadlineTs': started_at + duration_seconds,
            'durationSeconds': duration_seconds, 'defenderSeats': [p['seat'] for p in defenders],
            'rightSeat': defenders[0]['seat'], 'spectatorSeats': sorted(seats),
            'failedSeats': sorted(p['seat'] for p in failed), 'allowBondLayerGrowth': False,
            'composition': 'serial-owner-segments', 'nativeCompositionValidated': False,
            'originBattlefields': {str(p['seat']): {'stageId': p['stageId'], 'battlefield': p['battlefield']}
                                   for p in normalized},
            'segments': segments, 'enemies': [enemies[key] for key in sorted(enemies)],
            'sourceCounts': source, 'kills': {}, 'escapes': {}, 'completedReport': None}


def _issued(plan):
    if type(plan) is not dict or plan.get('version') != VERSION:
        raise DomainError('invalid_joint_defense_plan')
    return {_enemy_key(row): row for row in plan['enemies']}


def _check_reporter(plan, reporter_seat, *, shared_observation=False):
    integer(reporter_seat, 'reporterSeat', 0, MAX_PLAYERS - 1)
    allowed = plan['spectatorSeats'] if shared_observation else plan['defenderSeats']
    if reporter_seat not in allowed:
        raise DomainError('joint_defense_reporter_not_defender')


def _kill(plan, record):
    if type(record) is not dict:
        raise DomainError('invalid_joint_defense_kill')
    enemy = _enemy(record.get('killedEnemyInfo'))
    killer = integer(record.get('killedByPlayer'), 'killedByPlayer', 0, MAX_PLAYERS - 1)
    if killer not in plan['defenderSeats']:
        raise DomainError('joint_defense_killer_not_defender')
    attacker = integer(record.get('attackerInstId', -1), 'attackerInstId', -1)
    segment = next(row for row in plan['segments'] if row['seat'] == killer)
    if attacker != -1 and attacker not in {row['instId'] for row in segment['deployments']}:
        raise DomainError('unknown_joint_defense_attacker')
    return {'killedByPlayer': killer, 'killedEnemyInfo': enemy, 'attackerInstId': attacker,
            'damageSrc': integer(record.get('damageSrc', 0), 'damageSrc', 0, 3)}


def _known_enemy(plan, enemy):
    key = _enemy_key(enemy)
    if _issued(plan).get(key) != enemy:
        raise DomainError('unknown_joint_defense_enemy')
    return key


def remaining_by_owner(plan):
    """HP settlement counts: escaped enemies remain unvanquished and billable."""
    result = dict(plan['sourceCounts'])
    for record in plan['kills'].values():
        owner = str(record['killedEnemyInfo']['ownerPlayerIndex'])
        result[owner] -= 1
    return result


def _result(plan, duplicate=False):
    return {'duplicateReport': duplicate, 'completed': plan['status'] == 'completed',
            'remainingByOwner': remaining_by_owner(plan), 'sourceCounts': dict(plan['sourceCounts'])}


def record_kill(plan, record, *, reporter_seat):
    _issued(plan)
    _check_reporter(plan, reporter_seat, shared_observation=True)
    normalized = _kill(plan, record)
    key = _known_enemy(plan, normalized['killedEnemyInfo'])
    if key in plan['escapes'] or (key in plan['kills'] and plan['kills'][key] != normalized):
        raise DomainError('conflicting_joint_defense_enemy_result')
    duplicate = key in plan['kills']
    if plan['status'] != 'battle' and not duplicate:
        raise DomainError('joint_defense_already_completed')
    plan['kills'][key] = normalized
    return _result(plan, duplicate)


def record_escape(plan, enemy, *, reporter_seat):
    _issued(plan)
    _check_reporter(plan, reporter_seat, shared_observation=True)
    normalized = _enemy(enemy)
    key = _known_enemy(plan, normalized)
    if key in plan['kills']:
        raise DomainError('conflicting_joint_defense_enemy_result')
    duplicate = key in plan['escapes']
    if plan['status'] != 'battle' and not duplicate:
        raise DomainError('joint_defense_already_completed')
    plan['escapes'][key] = normalized
    return _result(plan, duplicate)


def finish(plan, payload, *, reporter_seat, expired=False):
    """Validate one shared-field final partition and atomically close its ledger.

    A retransmission/fromRejoin never charges HP itself. The room commits the
    returned per-owner survivors and round settlement in the same transaction.
    `expired` is server authority, never copied from a client payload.
    """
    issued = _issued(plan)
    # Every original client observes the shared field and can submit its full
    # terminal partition. Reporting never makes that observer a defender or
    # credits their seat: _kill still validates killedByPlayer and attacker
    # against the selected defender's actual deployed inventory.
    _check_reporter(plan, reporter_seat, shared_observation=True)
    _bool(expired, 'expired')
    if type(payload) is not dict:
        raise DomainError('invalid_joint_defense_report')
    if integer(payload.get('round'), 'round', 1, 15) != plan['round']:
        raise DomainError('joint_defense_round_mismatch')
    if 'generation' in payload and integer(payload['generation'], 'generation', 1) != plan['generation']:
        raise DomainError('joint_defense_generation_mismatch')
    _bool(payload.get('fromRejoin', False), 'from_rejoin')
    kills, escapes = {}, {}
    for row in _rows(payload.get('killInfos', []), 'kills', MAX_ENEMIES):
        normalized = _kill(plan, row)
        key = _known_enemy(plan, normalized['killedEnemyInfo'])
        if key in kills and kills[key] != normalized:
            raise DomainError('conflicting_joint_defense_enemy_result')
        kills[key] = normalized
    for row in _rows(payload.get('escapedEnemies', []), 'escapes', MAX_ENEMIES):
        normalized = _enemy(row)
        escapes[_known_enemy(plan, normalized)] = normalized
    if set(kills) & set(escapes):
        raise DomainError('conflicting_joint_defense_enemy_result')
    if expired:
        # Retain acknowledged kills on a server timeout; every other issued
        # enemy, including not-yet-spawned enemies, remains billed to its owner.
        for key, row in plan['kills'].items():
            if key in kills and kills[key] != row or key in escapes:
                raise DomainError('conflicting_joint_defense_enemy_result')
            kills[key] = copy.deepcopy(row)
        escapes.update({key: copy.deepcopy(row) for key, row in issued.items() if key not in kills})
    if set(kills) | set(escapes) != set(issued):
        raise DomainError('incomplete_joint_defense_report')
    for field, final in (('kills', kills), ('escapes', escapes)):
        if any(final.get(key) != value for key, value in plan[field].items()):
            raise DomainError('conflicting_joint_defense_enemy_result')
    canonical = {'round': plan['round'], 'generation': plan['generation'],
                 'killInfos': [kills[key] for key in sorted(kills)],
                 'escapedEnemies': [escapes[key] for key in sorted(escapes)]}
    digest = _digest(canonical)
    previous = plan['completedReport']
    if previous is not None:
        if previous['digest'] != digest:
            raise DomainError('conflicting_joint_defense_report')
        return _result(plan, True)
    plan.update(kills=kills, escapes=escapes, status='completed',
                completedReport={'digest': digest, 'report': canonical})
    return _result(plan)


def layer_growth_allowed(phase):
    """All effect dispatchers must suppress growth for the complete help phase."""
    return phase not in ('help', 'help_battle', 'joint_defense', 'HELP_BATTLE', 5)


def source_remaining_by_owner(plan):
    """Issued enemies minus kills AND exits, grouped by their original owner."""
    issued = _issued(plan)
    source = plan['sourceCounts']
    if (type(source) is not dict or any(key not in ('0', '1', '2', '3') or
            type(value) is not int or not 0 <= value <= MAX_ENEMIES
            for key, value in source.items())):
        raise DomainError('invalid_joint_defense_source_counter')
    expected = dict.fromkeys(source, 0)
    if len(issued) != len(plan['enemies']):
        raise DomainError('invalid_joint_defense_source_counter')
    for enemy in issued.values():
        owner = str(_enemy(enemy)['ownerPlayerIndex'])
        if owner not in expected:
            raise DomainError('invalid_joint_defense_source_counter')
        expected[owner] += 1
    if source != expected:
        raise DomainError('invalid_joint_defense_source_counter')
    result = dict(source)
    terminal = set()
    for field in ('kills', 'escapes'):
        for key, record in plan[field].items():
            enemy = _enemy(record['killedEnemyInfo'] if field == 'kills' else record)
            if key != _enemy_key(enemy) or issued.get(key) != enemy or key in terminal:
                raise DomainError('invalid_joint_defense_source_counter')
            terminal.add(key)
            result[str(enemy['ownerPlayerIndex'])] -= 1
    return result
