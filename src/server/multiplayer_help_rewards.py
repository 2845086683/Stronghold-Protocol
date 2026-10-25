# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
"""Attribute Help kill bounties without a second combat/reward simulator.

The original Help kill DTO supplies killedByPlayer and attackerInstId. The
joint-defense ledger validates both against the selected perfect defenders.
Enemy origin still controls HP loss; it must not choose the bounty payee.
"""
import copy

from .engine import DomainError
from .joint_defense import _enemy_key, _issued, _kill


def settle(engine, room):
    """Queue each confirmed kill once in the room's settlement transaction.

    Called after the normal per-seat result handlers, while each original
    battle snapshot still exists and before the next rest-period income.
    Existing single-player item rewards and bounty duration remain owned by
    ItemEffects.battle_result; only Help's additional last-hit rewards pass
    through this coordinator.
    """
    plan = room.get('help')
    if plan is None:
        return []
    if plan['status'] != 'completed':
        raise DomainError('joint_defense_bounty_requires_completed_report')
    if 'bountyRewards' in plan:
        return copy.deepcopy(plan['bountyRewards'])
    seats = {row['seat']: row['state'] for row in room['seats']}
    issued_help = _issued(plan)
    source_units = {seat: {row['instId']: row for row in state['battle']['units']}
                    for seat, state in seats.items() if state and state.get('battle')}
    groups = {}
    for key, record in plan['kills'].items():
        value = _kill(plan, record)
        enemy = value['killedEnemyInfo']
        if key != _enemy_key(enemy) or issued_help.get(key) != enemy:
            raise DomainError('invalid_joint_defense_bounty_source')
        # Tokens/splits cannot claim the original issued main body's bounty,
        # even if their separate native namespace reuses its numeric ID.
        if enemy['isToken']:
            continue
        origin, killer = enemy['ownerPlayerIndex'], value['killedByPlayer']
        source = seats[origin]
        report = source['completedReports'][str(room['round'])]
        issued = source_units.get(origin, {}).get(enemy['enemyInstId'])
        if (issued is None or issued['enemyId'] != enemy['enemyId'] or
                enemy['enemyInstId'] not in report['escaped']):
            raise DomainError('invalid_joint_defense_bounty_source')
        groups.setdefault((origin, killer), set()).add(enemy['enemyInstId'])
    awards = []
    for (origin, killer), identities in sorted(groups.items()):
        for reward in engine.items.kill_bounty_rewards(seats[origin]['battle'], identities):
            if not reward['coin']:
                continue
            awards.append({**reward, 'ownerPlayerIndex': origin, 'killedByPlayer': killer})
        amount = engine.bands.kill_reward_amount(seats[origin]['battle'], identities)
        if amount:
            awards.append({'rewardKind': 'bandKillCoin', 'coin': amount,
                'killedInstIds': sorted(row['instId'] for row in seats[origin]['battle']['units']
                    if row.get('bandKillCoin') and row['instId'] in identities),
                'ownerPlayerIndex': origin, 'killedByPlayer': killer})
    # Build and validate the complete transfer set before mutating any player.
    for award in awards:
        target = seats[award['killedByPlayer']]
        if award.get('rewardKind') == 'bandKillCoin':
            # Preserve the strategy's existing immediate settlement timing,
            # including its interaction with next-rest carry-over interest.
            target['player']['store']['coin'] += award['coin']
        else:
            target['scheduled'].append({'round': room['round'] + 1,
                                        'kind': 'coin', 'count': award['coin']})
        target['completedReports'][str(room['round'])].setdefault(
            'jointDefenseBountyRewards', []).append(copy.deepcopy(award))
    plan['bountyRewards'] = copy.deepcopy(awards)
    return awards
