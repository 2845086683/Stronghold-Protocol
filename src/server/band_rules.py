# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Adapted from Stronghold-Protocol server/sim/content/bands/meta.js at
# cf7a9400270d5aa79b2e601e405dab782e25f120; parameters come from pinned CN.
# See ../third_party/stronghold-protocol-reference/ for licence and notices.
"""Strategy preparation hooks, dispatched by effect key rather than operator.

Unity owns env_gbuff/native battle execution, EconomyEffects owns its five
economic keys, and this module owns the remaining lifecycle grants. Counters
and the independent reward RNG live in the transactionally saved run state.
"""
from .catalog import draw
from .setup_rules import stream, chess_bonds

SUPPORTED = frozenset(('preparation_start_gain_chess_from_round',
    'preparation_start_gain_chess_every_n_round', 'prep_start_gain_chess_from_pool_in_round',
    'gain_bond_char_per_round', 'round_start_bond_check_gain_layer',
    'round_start_activate_char_chess_effect_in_board', 'round_start_gain_char_chess_in_shop_every_n_round',
    'preparation_start_add_special_goods_every_n_round', 'band_first_self_refresh_present_char',
    'band_shop_refresh_copy_max_lv_char', 'refresh_shop_count_gain_coin_bond_char_chess',
    'round_start_gain_coin_by_bond_char_chess_buy', 'first_sell_char_chess_exchange_char_chess_in_shop'))


class BandRules:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c

    def _rows(self, state):
        for index, row in enumerate(self.e._band_effects(state)):
            if row['Key'] in SUPPORTED:
                yield str(index)+':'+row['Key'], row['Key'], row['Blackboard'] or {}

    @staticmethod
    def _counter(state, ident):
        return state.setdefault('bandLifecycleCounters', {}).setdefault(ident, {})

    def _draw(self, state, values):
        rng = {'rngState': state.get('bandRewardRngState', stream(state['stageSeed'], 'bands:rewards:v1')['rngState'])}
        selected = draw(rng, values)
        state['bandRewardRngState'] = rng['rngState']
        return selected

    def _pool(self, state, name):
        from .engine import DomainError
        local = {**state, 'rngState': state.get('bandRewardRngState', stream(state['stageSeed'], 'bands:rewards:v1')['rngState'])}
        try:
            selected = self.e.effects.roll_pool(local, name)
        except DomainError as error:
            if error.code == 'empty_trait_pool':
                return None
            raise
        state['bandRewardRngState'] = local['rngState']
        return selected

    def _chars(self, state, bond=None, any_tier=False):
        result = sorted(key for key in state['chessPool'] if self.e.pool.available(state, key)
                        and (not bond or bond in chess_bonds(self.c, state, key))
                        and self.c.season['CharShopChessDatas'][key]['ChessLevel'] <= state['player']['store']['level'])
        if not result and any_tier:
            result = sorted(key for key in state['chessPool'] if self.e.pool.available(state, key)
                            and (not bond or bond in chess_bonds(self.c, state, key)))
        return result

    def _goods(self, state):
        return [good for good in state['player']['store']['goods']
                if self.c.data['identifiers'][str(good['chessIdentifier'])] in self.c.season['CharShopChessDatas']
                and self.e.pool.available(state,self.c.data['identifiers'][str(good['chessIdentifier'])])]

    def _set_good(self, state, good, key, frozen=False):
        if not self.e.pool.available(state,key,shop=True):return
        good.update(chessIdentifier=self.c.chess[key]['Identifier'], price=self.e._price(state, key), isFrozen=frozen)

    def event(self, state, event, **args):
        for ident, key, bb in self._rows(state):
            record = self._counter(state, ident)
            if event == 'ROUND_START':
                if record.get('lastRound') == state['round']:
                    continue
                # Mark before grants can recursively fire SERVER_GAIN.
                record['lastRound'] = state['round']
            getattr(self, '_effect_'+key)(state, record, bb, event, args)

    def _effect_preparation_start_gain_chess_from_round(self, state, record, bb, event, args):
        if event == 'ROUND_START' and state['round'] == int(bb.get('round', -1)):
            for _ in range(max(1, int(bb.get('count', 1)))):
                self.e._gain(state, bb.get('chess') or bb.get('chess_id'))

    def _effect_preparation_start_gain_chess_every_n_round(self, state, record, bb, event, args):
        every = int(bb.get('round', 0))
        if event == 'ROUND_START' and every > 0 and state['round'] % every == 0:
            for _ in range(max(1, int(bb.get('count', 1)))):
                self.e._gain(state, bb.get('chess') or bb.get('chess_id'))

    def _effect_prep_start_gain_chess_from_pool_in_round(self, state, record, bb, event, args):
        if event == 'ROUND_START' and state['round'] == int(bb.get('round', -1)):
            for _ in range(max(1, int(bb.get('count', 1)))):
                selected = self._pool(state, bb.get('pool'))
                if selected:
                    self.e._gain(state, selected)

    def _effect_gain_bond_char_per_round(self, state, record, bb, event, args):
        first, every = int(bb.get('round', 1)), max(1, int(bb.get('preround', 1)))
        if event != 'ROUND_START' or state['round'] < first or (state['round']-first) % every:
            return
        for _ in range(max(1, int(bb.get('count', 1)))):
            values = self._chars(state, bb.get('bond'), True)
            if values:
                self.e._gain(state, self._draw(state, values))

    def _effect_round_start_bond_check_gain_layer(self, state, record, bb, event, args):
        if event != 'ROUND_START' or state['round'] != int(bb.get('round', -1)):
            return
        active = [row for row in state['player']['bonds'] if row['isActive']]
        count = int(bb.get('count1' if len(active) == int(bb.get('factioncount', 1)) else 'count2', 0))
        for row in active:
            self.e._layers(state, row['bondIdentifier'], count)

    def _effect_round_start_activate_char_chess_effect_in_board(self, state, record, bb, event, args):
        if event != 'ROUND_START':
            return
        ev = bb.get('event_type', 'SERVER_GAIN')
        selected = []
        for obj in self.e.effects.board(state):
            records = self.e.effects._records(obj)
            if any(row['EventType'] == ev for _, row in records):
                selected.append(obj)
        selected.sort(key=lambda obj: (-self.e.effects.position(state, obj)['position']['x'],
                                       self.e.effects.position(state, obj)['position']['y']))
        for obj in selected[:max(1, int(bb.get('count', 1)))]:
            self.e.effects.dispatch(state, ev, obj, trigger=True)

    def _effect_round_start_gain_char_chess_in_shop_every_n_round(self, state, record, bb, event, args):
        every = int(bb.get('round', 0))
        if event != 'ROUND_START' or every <= 0 or state['round'] % every:
            return
        for _ in range(max(1, int(bb.get('count', 1)))):
            goods = self._goods(state)
            if not goods:
                break
            good = self._draw(state, goods)
            state['player']['store']['goods'].remove(good)
            self.e._gain(state, self.c.data['identifiers'][str(good['chessIdentifier'])])

    def _effect_preparation_start_add_special_goods_every_n_round(self, state, record, bb, event, args):
        every = int(bb.get('round', 0))
        if event != 'ROUND_START' or every <= 0 or state['round'] % every:
            return
        count = max(1, int(bb.get('refresh_cnt', 2)))
        options = []
        for _ in range(count * 6):
            key = self._pool(state, bb.get('pool'))
            if not key:
                break
            if key not in options:
                options.append(key)
            if len(options) == count:
                break
        for _ in range(max(1, int(bb.get('choice_cnt', 1)))):
            if options:
                state.setdefault('traitRewardOffers', []).append({'source': 'band-lifecycle', 'options': options[:],
                    'price': 0, 'effectKey': 'preparation_start_add_special_goods_every_n_round',
                    'round': state['round'], 'nativeAdapter': 'declaration-special-shop-COMB'})

    def _effect_band_first_self_refresh_present_char(self, state, record, bb, event, args):
        if event != 'MANUAL_REFRESH' or state.get('roundEffectStats', {}).get('manualRefreshes', 0) > 2:
            return
        goods = self._goods(state)
        have = sum(bb.get('bond') in chess_bonds(self.c, state, self.c.data['identifiers'][str(good['chessIdentifier'])]) for good in goods)
        pool = self._chars(state, bb.get('bond'))
        others = [good for good in goods if bb.get('bond') not in chess_bonds(self.c, state, self.c.data['identifiers'][str(good['chessIdentifier'])])]
        while have < max(1, int(bb.get('count', 1))) and others and pool:
            good = self._draw(state, others)
            others.remove(good)
            self._set_good(state, good, self._draw(state, pool))
            have += 1

    def _effect_band_shop_refresh_copy_max_lv_char(self, state, record, bb, event, args):
        goods = self._goods(state)
        if event != 'MANUAL_REFRESH' or len(goods) < 2:
            return
        def tier(good):
            return self.c.season['CharShopChessDatas'][self.c.data['identifiers'][str(good['chessIdentifier'])]]['ChessLevel']
        best = max(map(tier, goods))
        source = self._draw(state, [good for good in goods if tier(good) == best])
        target = self._draw(state, [good for good in goods if good is not source])
        self._set_good(state, target, self.c.data['identifiers'][str(source['chessIdentifier'])], True)

    def _effect_refresh_shop_count_gain_coin_bond_char_chess(self, state, record, bb, event, args):
        if event != 'MANUAL_REFRESH':
            return
        record['refreshes'] = record.get('refreshes', 0) + 1
        every = int(bb.get('refresh_count', 0))
        if every <= 0 or record['refreshes'] % every:
            return
        if record.get('rewardRound') != state['round']:
            record.update(rewardRound=state['round'], rewards=0)
        values = self._chars(state, bb.get('bond'))
        if record['rewards'] < int(bb.get('max_count', 2)) and values:
            record['rewards'] += 1
            self.e._gain(state, self._draw(state, values))

    def _effect_round_start_gain_coin_by_bond_char_chess_buy(self, state, record, bb, event, args):
        if event == 'ROUND_START':
            state['player']['store']['coin'] += record.pop('pendingFunds', 0)
            record['bought'] = 0
        elif event == 'BUY' and bb.get('bond') in chess_bonds(self.c, state, args['chessId']):
            amount = min(int(bb.get('count', 1)), max(0, int(bb.get('max_count', 3))-record.get('bought', 0)))
            record['bought'] = record.get('bought', 0) + amount
            record['pendingFunds'] = record.get('pendingFunds', 0) + amount

    def _effect_first_sell_char_chess_exchange_char_chess_in_shop(self, state, record, bb, event, args):
        pass  # sale() must run after removal, before crediting the sale price.

    def sale(self, state, obj, value):
        definition = self.e._definition(obj)
        if definition.get('IsGolden') or definition['ChessId'] not in self.c.season['CharChessDataDict']:
            return value
        for ident, key, bb in self._rows(state):
            if key != 'first_sell_char_chess_exchange_char_chess_in_shop':
                continue
            record = self._counter(state, ident)
            if record.get('saleRound') != state['round']:
                record.update(saleRound=state['round'], sales=0)
            goods = self._goods(state)
            if record['sales'] >= max(1, int(bb.get('count', 1))) or not goods:
                continue
            record['sales'] += 1
            good = self._draw(state, goods)
            chosen = self.c.data['identifiers'][str(good['chessIdentifier'])]
            self._set_good(state, good, definition['ChessId'])
            self.e._gain(state, chosen)
            value = 0
        return value

    @staticmethod
    def kill_reward_amount(battle, killed):
        """Shared issued-identity calculation for self and rescued Help kills."""
        resolved = set(killed)
        return sum(unit.get('bandKillCoin', 0) for unit in battle['units'] if unit['instId'] in resolved)

    def battle_result(self, state, killed):
        # Called once after the validated report is persisted in completedReports.
        # Native side enemies left alive/unborn in a boss victory earn no coins.
        amount = self.kill_reward_amount(state['battle'], killed)
        if amount:
            state['player']['store']['coin'] += amount
            state['completedReports'][str(state['round'])]['bandKillCoin'] = amount
