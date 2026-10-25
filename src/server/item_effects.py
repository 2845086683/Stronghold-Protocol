# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Adapted from server/match/builtinMeta.js and server/sim/content/items/meta.js
# at cf7a9400270d5aa79b2e601e405dab782e25f120. See the reference attribution,
# licence and source hashes in ../third_party/stronghold-protocol-reference/.
"""Preparation and round-boundary item templates over authoritative state.

Equipment runes, conditional attributes and battle targeting stay in Unity.
Only effects whose native contract requires a server state change run here;
the dispatcher uses effect keys and Blackboard values, never item identities.
"""
import copy
from .catalog import draw


NATIVE_EFFECTS = frozenset(('env_gbuff_new', 'env_gbuff_new_with_verify',
    'char_attribute_mul', 'char_dynamic_ability_new', 'enemy_attribute_mul',
    'enemy_attribute_add', 'char_respawntime_mul'))
CONSUMABLE_EFFECTS = frozenset(('use_equip_upgrade_char',
    'use_equip_reward_char_chess', 'use_equip_reward_random_char_chess_in_shop',
    'use_equip_reward_char_chess_with_same_bond', 'use_equip_reward_char_chess_bond_layer',
    'use_equip_gain_coin_when_next_round_start', 'equip_destory_gain_random_coin',
    'use_equip_reward_special_goods_char_chess', 'equip_destory_deployment_cnt_change',
    'use_equip_recruit_new_char_and_give_char_to_player_most_bond', 'gain_coin_when_round_start'))
WORN_EFFECTS = frozenset(('equip_round_start_upgrade_char',
    'char_chess_transformation_equip', 'sell_char_count_gain_equip_owner_bond',
    'equip_with_another_gain_coin_when_gain_char'))
BOUNTY_EFFECTS = frozenset(('add_enemy_kill_gain_coin', 'add_enemy_selfbattle_win_gain_coin'))


class ItemEffects:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c

    @staticmethod
    def fail(code, detail=''):
        from .engine import DomainError
        raise DomainError(code, detail)

    def _pool(self, state, *, bonds=None, tier=None, maximum=None):
        wanted = set(bonds or [])
        result = []
        for key in state['chessPool']:
            if not self.e.pool.available(state,key):
                continue
            row = self.c.season['CharShopChessDatas'][key]
            level = row['ChessLevel']
            if tier is not None and level != tier or maximum is not None and level > maximum:
                continue
            from .preset_snapshot import base_bonds
            if wanted and not wanted.intersection(base_bonds(self.c,state,key)):
                continue
            result.append(key)
        return sorted(result)

    def _same_bond(self, state, char, maximum):
        return self._pool(state, bonds=self.e.effects.bonds(state, char), maximum=maximum)

    def _grant(self, state, pool, count=1):
        if not pool:
            return
        for _ in range(max(0, count)):
            selected=self.e.pool.draw(state,pool)
            if selected is None:break
            self.e._gain(state,selected,stock='strict')

    def _offer(self, state, pool, count, item):
        pool = self.e.pool.candidates(state,pool)
        options = []
        for _ in range(min(max(0, count), len(pool))):
            key = draw(state, pool)
            options.append(key)
            pool.remove(key)
        if options:
            state.setdefault('traitRewardOffers', []).append({
                'source': 'item', 'itemInstId': item['instId'], 'options': options})

    def _destroy_carrier(self, state, char):
        # Destroy is separate from sell: there is no refund or SERVER_CHESS_SOLD.
        char['equipInstIds'] = []
        state['player']['charChess'].remove(char)
        state['player']['positions'] = [row for row in state['player']['positions']
                                         if row['instId'] != char['instId']]
        self.e._bonds(state)

    def magic(self, state, args):
        self.e._phase(state, 'preparation')
        if state.get('itemChoice'):
            self.fail('item_choice_selection_required')
        self.e.shop_rewards.require_regular_shop(state)
        item, _ = self.e._instance(state, args.get('instId'), ('trapChess',))
        effects = self.c.effects(self.e._definition(item).get('EffectId'))
        keys = {effect['Key'] for effect in effects}
        if 'unitPositionInfo' in args:
            positions = {key: copy.deepcopy(args[key]) for key in
                ('unitPositionInfo', 'handPositions', 'nativeTokenPositions', 'deployCount') if key in args}
            positions.setdefault('nativeTokenPositions', [])
            self.e.apply(state, 'board.move', positions)
        if keys == {'trap_copy_front_char'}:
            target = self.e.effects.front(state, item)
            if target is None:
                self.fail('magic_no_front_operator')
            chess = self.e._definition(target)['ChessId']
            equipment = [self.e._definition(obj)['ChessId'] for obj in state['player']['equipChess']
                if obj['instId'] in target['equipInstIds']]
            self._consume_magic(state, item)
            owner = self.e._gain(state, chess)
            for key in equipment:
                gear = self.e._gain(state, key)
                # Copy preserves worn equipment rather than triggering a new
                # consumable use. A third-copy merge may already make the
                # surviving carrier elite while its copied projection remains.
                owner['equipInstIds'].append(gear['instId'])
                if any(effect['Key'] == 'equip_round_start_upgrade_char'
                        for effect in self.c.effects(self.e._definition(gear).get('EffectId'))):
                    state['scheduled'].append({'round': state['round'] + 1, 'kind': 'promotion',
                        'target': owner['instId'], 'item': gear['instId']})
            self.e._bonds(state)
            return {'consumed': True, 'copiedChessId': chess}
        if 'trap_create_self_choice' not in keys or keys - {'trap_create_self_choice', 'trap_disney_special'}:
            self.fail('magic_choice_handler_pending', ','.join(sorted(keys)))
        event = next(effect for effect in effects if effect['Key'] == 'trap_create_self_choice')
        choice_id = (event['Blackboard'] or {}).get('choice_event')
        if choice_id not in self.c.season['EffectChoiceInfoDict']:
            self.fail('unknown_item_choice', str(choice_id))
        inactive = set(self.c.mode(state['modeId']).get('InactiveEnemyKey') or [])
        # Local deterministic policy: three distinct available low-price
        # bounty cards. Native CN supplies the cards/conditions, not pool weights.
        candidates = []
        for effect_id, rows in self.c.season['EffectBuffInfoDataDict'].items():
            info = self.c.season['EffectInfoDataDict'].get(effect_id, {})
            relevant = [row for row in rows if row['Key'] in BOUNTY_EFFECTS]
            if (info.get('EffectType') == 9 and 0 < info.get('EnemyPrice', 0) <= 2 and len(relevant) == 1
                    and (relevant[0]['Blackboard'] or {}).get('enemy_id') not in inactive):
                candidates.append(effect_id)
        candidates.sort()
        if not candidates:
            self.fail('empty_item_bounty_pool')
        options = []
        for _ in range(min(3, len(candidates))):
            selected = draw(state, candidates)
            candidates.remove(selected)
            options.append(selected)
        self._consume_magic(state, item)
        state['itemChoice'] = {'itemInstId': item['instId'], 'choiceId': choice_id}
        state['player']['selfChoice'] = {'id': choice_id, 'options': options}
        return {'consumed': True, 'choiceId': choice_id}

    @staticmethod
    def _consume_magic(state, item):
        state['player']['trapChess'].remove(item)
        state['player']['positions'] = [row for row in state['player']['positions'] if row['instId'] != item['instId']]

    def choice(self, state, args):
        from .engine import integer
        self.e._phase(state, 'preparation')
        pending = state.get('itemChoice')
        choice = state['player'].get('selfChoice')
        if not pending or not choice or choice['id'] != pending['choiceId']:
            self.fail('missing_item_choice')
        slot = integer(args.get('slotId'), 'slotId', 0, len(choice['options']) - 1)
        selected = choice['options'][slot]
        self._add_bounty(state, selected)
        state.pop('itemChoice')
        state['player']['selfChoice'] = None
        return {'choiceId': selected}

    def _add_bounty(self, state, effect_id):
        rows = [row for row in self.c.effects(effect_id) if row['Key'] in BOUNTY_EFFECTS]
        if len(rows) != 1:
            self.fail('invalid_item_bounty', effect_id)
        effect = rows[0]
        bb = effect['Blackboard'] or {}
        ident = self.e._id(state)
        state.setdefault('itemBounties', []).append({'effectInstId': ident, 'effectId': effect_id,
            'fromRound': state['round'], 'remainingBattles': max(1, int(bb.get('round', 1))),
            'enemyId': bb['enemy_id'], 'count': max(1, int(bb.get('count', 1))),
            'coin': max(0, int(bb.get('coin', 0))), 'rewardKind': effect['Key']})
        state['specialEffects'].append({'effectInstId': ident, 'effectId': effect_id, 'affectedUidIndex': [0]})
        self._effect_enemies(state)

    @staticmethod
    def _effect_enemies(state):
        state['player']['effectEnemies'] = [{key: row[key] for key in ('effectInstId', 'enemyId', 'count')}
            for row in state.get('itemBounties', []) if row['remainingBattles'] > 0 and row['fromRound'] <= state['round']]

    def prepare_battle(self, state):
        from .native_combat import bounty_rows
        battle = state['battle']
        active = [row for row in state.get('itemBounties', []) if row['remainingBattles'] > 0 and row['fromRound'] <= state['round']]
        battle['itemBounties'] = copy.deepcopy(active)
        for row in bounty_rows(self.c, state, battle['levelId']):
            for _ in range(row['count']):
                battle['units'].append({'instId': self.e._id(state), 'enemyId': row['enemyKey'],
                    'actionIndex': row['actionIndex'], 'effectInstId': row['effectInstId']})

    def destroy(self, state, item):
        # Active destruction of the messenger art is distinct from using it.
        # Solo has no next player to receive a replacement art.
        for effect in self.c.effects(self.e._definition(item).get('EffectId')):
            if effect['Key'] == 'trap_disney_special':
                state['player']['store']['coin'] += int((effect['Blackboard'] or {}).get('count', 1))

    def equip(self, state, args):
        self.e._phase(state, 'preparation')
        self.e.shop_rewards.require_regular_shop(state)
        player = state['player']
        char, _ = self.e._instance(state, args.get('charChessInstId'), ('charChess',))
        item, _ = self.e._instance(state, args.get('equipChessInstId'), ('equipChess',))
        if any(item['instId'] in obj['equipInstIds'] for obj in player['charChess']):
            self.fail('already_equipped')
        definition = self.e._definition(item)
        effects = self.c.effects(definition.get('EffectId'))
        keys = {effect['Key'] for effect in effects}
        unknown = keys - CONSUMABLE_EFFECTS - WORN_EFFECTS - NATIVE_EFFECTS
        if unknown:
            self.fail('effect_handler_pending', ','.join(sorted(unknown)))
        # A DIY chess ID is an owner-local slot, including its golden form.
        # The beacon transfers its former carrier, so accepting this target
        # would give another player an ambiguous personal configuration. Keep
        # the restriction on the effect type, before consuming or replacing
        # any equipment, rather than maintaining a list of selected operators.
        if 'use_equip_recruit_new_char_and_give_char_to_player_most_bond' in keys:
            carrier = self.e._definition(char)['ChessId']
            base = self.c.season['ChessNormalIdLookupDict'].get(carrier, carrier)
            if base in self.c.season['DiyChessDict']:
                self.fail('equipment_target_not_allowed')
        if keys.intersection(('use_equip_upgrade_char', 'equip_round_start_upgrade_char')):
            if self.e._definition(char).get('IsGolden') or not self.e._definition(char).get('UpgradeChessId'):
                self.fail('already_elite')
        replace = args.get('isChangeEquip', False)
        if type(replace) is not bool:
            self.fail('invalid_replacement')
        unload = args.get('unloadInstId')
        if replace and (type(unload) is not int or unload not in char['equipInstIds']):
            self.fail('invalid_replacement')
        consumed = bool(keys & CONSUMABLE_EFFECTS)
        if not consumed:
            if not replace and len(char['equipInstIds']) >= self.c.number('Torappu.Battle.AutoChessBattleConst', 'MAX_WEAR_EQUIP_CNT'):
                self.fail('equipment_slots_full')
            if replace:
                char['equipInstIds'].remove(unload)
                # Explicit replacement destroys the selected worn equipment.
                # It is not the automatic detachment performed on a sold or
                # merged operator; ownership-based stock returns immediately.
                player['equipChess'] = [obj for obj in player['equipChess'] if obj['instId'] != unload]
                state['scheduled'] = [row for row in state['scheduled'] if row.get('item') != unload]
            char['equipInstIds'].append(item['instId'])
            if 'equip_round_start_upgrade_char' in keys:
                state['scheduled'] = [row for row in state['scheduled'] if row.get('item') != item['instId']]
                state['scheduled'].append({'round': state['round'] + 1, 'kind': 'promotion',
                    'target': char['instId'], 'item': item['instId']})
        else:
            # A consumable never occupies a slot or unloads the holder's gear.
            # Snapshot bonds before a grant can merge/destroy the chosen carrier.
            bonds = self.e.effects.bonds(state, char)
            normal = self.c.season['ChessNormalIdLookupDict'][self.e._definition(char)['ChessId']]
            tier = self.e.effects.tier(char)
            player['equipChess'].remove(item)
            for effect in effects:
                key = effect['Key']
                bb = effect['Blackboard'] or {}
                count = int(bb.get('count', 1))
                if key == 'use_equip_upgrade_char':
                    char = self.e.acquisitions.promote(state, char)
                elif key == 'equip_destory_gain_random_coin':
                    player['store']['coin'] += draw(state, list(range(int(bb['min']), int(bb['max']) + 1)))
                elif key == 'use_equip_gain_coin_when_next_round_start':
                    state['scheduled'].append({'round': state['round'] + 1, 'kind': 'coin', 'count': count})
                elif key == 'gain_coin_when_round_start':
                    state.setdefault('itemRoundEffects', []).append({'kind': 'coin', 'count': count,
                        'fromRound': state['round'] + 1, 'sourceInstId': item['instId'], 'effectId': definition['EffectId']})
                elif key == 'use_equip_reward_char_chess_bond_layer':
                    self.e.effects.add(state, bonds, int(bb['layer']))
                elif key == 'equip_destory_deployment_cnt_change':
                    player['maxDeploymentCnt'] = min(12, max(player['maxDeploymentCnt'], count))
                elif key == 'use_equip_reward_random_char_chess_in_shop':
                    for _ in range(count):
                        candidates = [good for good in player['store']['goods']
                            if self.c.data['identifiers'][str(good['chessIdentifier'])] in self.c.season['CharChessDataDict']
                            and self.e.pool.available(state,self.c.data['identifiers'][str(good['chessIdentifier'])])]
                        if not candidates:
                            break
                        good = draw(state, candidates)
                        player['store']['goods'].remove(good)
                        self.e._gain(state, self.c.data['identifiers'][str(good['chessIdentifier'])],stock='strict')
                elif key == 'use_equip_reward_char_chess_with_same_bond':
                    self._grant(state, self._pool(state, bonds=bonds, maximum=player['store']['level']), count)
                elif key == 'use_equip_reward_char_chess':
                    owned = sum(self.e._definition(obj)['ChessId'] == normal for obj in player['charChess'])
                    if owned >= 2:
                        # A defined third-copy clone, not a random pool roll.
                        # The existing target may be banned or already exhaust
                        # its stock; fixed copies still occupy the shared stock.
                        self.e._gain(state,normal,stock='fixed')
                    else:
                        self._grant(state,self._pool(state,bonds=bonds,maximum=6))
                elif key == 'use_equip_reward_special_goods_char_chess':
                    self._offer(state, self._pool(state, bonds=bonds, maximum=player['store']['level']), int(bb.get('refresh_cnt', 3)), item)
                elif key == 'use_equip_recruit_new_char_and_give_char_to_player_most_bond':
                    if state.get('multiplayerCoordinatorVersion') == 1:
                        from .preset_snapshot import wire_squad
                        original = self.e._definition(char)['ChessId']
                        configured = next((row for row in wire_squad(self.c, state)
                                           if row['chessId'] == normal), None)
                        state.setdefault('pendingTeamItemGifts', []).append({
                            'sourceItemInstId': item['instId'], 'chessId': original,
                            'bonds': list(bonds), 'fromRound': state['round'],
                            'configuration': copy.deepcopy(configured)})
                    self._destroy_carrier(state, char)
                    pool = []
                    for level in range(tier, 0, -1):
                        pool.extend(self._pool(state, tier=level))
                        if len(pool) >= int(bb.get('refresh_cnt', 2)):
                            break
                    self._offer(state, pool, int(bb.get('refresh_cnt', 2)), item)
        self.e._bonds(state)
        return {'consumed': consumed, 'itemId': definition['ChessId']}

    def round_start(self, state):
        player = state['player']
        for effect in list(state['scheduled']):
            if effect['round'] > state['round']:
                continue
            if effect['kind'] == 'coin':
                player['store']['coin'] += effect['count']
            elif effect['kind'] == 'promotion':
                char = next((obj for obj in player['charChess'] if obj['instId'] == effect['target']), None)
                if char is not None and effect['item'] in char['equipInstIds']:
                    char = self.e.acquisitions.promote(state, char)
                    char['equipInstIds'].remove(effect['item'])
                    player['equipChess'] = [obj for obj in player['equipChess'] if obj['instId'] != effect['item']]
            state['scheduled'].remove(effect)
        for effect in state.get('itemRoundEffects', []):
            if effect['fromRound'] <= state['round'] and effect['kind'] == 'coin':
                player['store']['coin'] += effect['count']

    def _worn(self, state):
        items = {obj['instId']: obj for obj in state['player']['equipChess']}
        for char in list(state['player']['charChess']):
            for ident in list(char['equipInstIds']):
                item = items.get(ident)
                if item is not None:
                    for effect in self.c.effects(self.e._definition(item).get('EffectId')):
                        yield char, item, effect['Key'], effect['Blackboard'] or {}

    @staticmethod
    def kill_bounty_rewards(battle, killed):
        """Use the issued main-body identities for both self and Help kills.

        A summoned child sharing an enemy template never inherits a bounty.
        Perfect-self-battle cards are settled separately and cannot transfer
        their reward to the player who rescues one of their escaped enemies.
        """
        killed = set(killed)
        rewards = []
        by_effect = {}
        for unit in battle['units']:
            effect = unit.get('effectInstId')
            if effect is not None:
                by_effect.setdefault(effect, set()).add(unit['instId'])
        for bounty in battle.get('itemBounties', []):
            if bounty['rewardKind'] != 'add_enemy_kill_gain_coin':
                continue
            ids = sorted(by_effect.get(bounty['effectInstId'], set()) & killed)
            if ids:
                rewards.append({'effectInstId': bounty['effectInstId'],
                                'killedInstIds': ids, 'coin': bounty['coin'] * len(ids)})
        return rewards

    def battle_result(self, state):
        battle = state['battle']
        report = state['completedReports'][str(state['round'])]
        killed = set(report['killed'])
        rewards = []
        kill_rewards = {row['effectInstId']: row['coin']
                        for row in self.kill_bounty_rewards(battle, killed)}
        for bounty in battle.get('itemBounties', []):
            if bounty['rewardKind'] == 'add_enemy_kill_gain_coin':
                coin = kill_rewards.get(bounty['effectInstId'], 0)
            else:
                count = int(state['player']['hp'] > 0 and not report['escaped'] and not report.get('escapedTokens'))
                coin = bounty['coin'] * count
            if coin:
                state['scheduled'].append({'round': state['round'] + 1, 'kind': 'coin', 'count': coin})
            rewards.append({'effectInstId': bounty['effectInstId'], 'coin': coin})
            current = next((row for row in state.get('itemBounties', []) if row['effectInstId'] == bounty['effectInstId']), None)
            if current is not None:
                current['remainingBattles'] -= 1
                if current['remainingBattles'] <= 0:
                    state['itemBounties'].remove(current)
                    state['specialEffects'] = [row for row in state['specialEffects'] if row['effectInstId'] != current['effectInstId']]
        if rewards:
            report['itemBountyRewards'] = rewards
        self._effect_enemies(state)
        for char, item, key, _ in list(self._worn(state)):
            if key != 'char_chess_transformation_equip' or char not in state['player']['charChess']:
                continue
            pool = self._pool(state, tier=min(6, self.e.effects.tier(char) + 1))
            if pool:
                selected = draw(state, pool)
                self._destroy_carrier(state, char)
                self.e._gain(state, selected)

    def event(self, state, event, subject):
        for char, item, key, bb in list(self._worn(state)):
            if event == 'SERVER_CHESS_SOLD' and key == 'sell_char_count_gain_equip_owner_bond':
                counters = state.setdefault('itemCounters', {})
                ident = str(item['instId'])
                counters[ident] = counters.get(ident, 0) + 1
                if counters[ident] >= max(1, int(bb.get('count', 8))):
                    counters[ident] = 0
                    self._grant(state, self._same_bond(state, char, state['player']['store']['level']))
            elif event == 'SERVER_GAIN' and key == 'equip_with_another_gain_coin_when_gain_char':
                bond = bb.get('bond')
                bonds = self.e.effects.bonds(state, char)
                member = bond in bonds or ('maniShip' in bonds and self.e.effects.active(state, 'maniShip')
                    and self.e.effects.active(state, bond) and self.c.common['BondInfoDict'].get(bond, {}).get('BondType') == 2)
                others = set(str(bb.get('other_equip', '')).split(','))
                carried = any(self.e._definition(obj)['ChessId'] in others for obj in state['player']['equipChess']
                    if obj['instId'] != item['instId'] and obj['instId'] in char['equipInstIds'])
                counters = state.setdefault('roundEffectStats', {}).setdefault('itemGainCounts', {})
                ident = str(item['instId'])
                if member and carried and counters.get(ident, 0) < max(0, int(bb.get('max', 3))):
                    counters[ident] = counters.get(ident, 0) + 1
                    state['player']['store']['coin'] += int(bb.get('count', 2))
