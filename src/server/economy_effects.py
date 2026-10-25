# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Adapted from server/sim/content/bands/meta.js and match/player/economy.js
# at cf7a9400270d5aa79b2e601e405dab782e25f120. See the retained reference
# licence/source hashes in ../third_party/stronghold-protocol-reference/.
"""Band economy hooks; CN keys/Blackboards, persistent transactional counters.

Shop draws keep the existing stream. Strategy grants/offers have an independent
persisted xorshift stream, so an upgrade does not secretly reroll the base shop.
Unpublished reward pool weights remain the documented private-server policy.
"""
from .catalog import draw
from .preset_snapshot import base_bonds


SUPPORTED = frozenset(('first_buy_in_round_char_price_change',
    'band_coin_cost_gain_random_char_by_shop_level',
    'band_cost_coin_reach_cnt_gain_chess_from_pool', 'up_shop_add_special_goods',
    'up_shop_next_refresh_must_present_bond_char'))


class EconomyEffects:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c

    def _rows(self, state):
        for index, effect in enumerate(self.e._band_effects(state)):
            if effect['Key'] in SUPPORTED:
                yield effect['Key'] + ':' + str(index), effect['Key'], effect['Blackboard'] or {}

    @staticmethod
    def _counter(state, ident):
        return state.setdefault('bandEconomyCounters', {}).setdefault(ident, {})

    def _has_bond(self, state, key, bond):
        return key in self.c.season['CharShopChessDatas'] and bond in base_bonds(self.c, state, key)

    def _chars(self, state, bond=None):
        return sorted(key for key in state['chessPool']
            if self.c.season['CharShopChessDatas'][key]['ChessLevel'] <= state['player']['store']['level']
            and self.e.pool.available(state, key)
            and (not bond or self._has_bond(state, key, bond)))

    def _reward_context(self, state):
        # Seed this independent stream only when a real strategy reward draws.
        # Reading prices, normal upgrades and other bands never create it.
        return {**state, 'rngState': state.get('economyRewardRngState', (state['stageSeed'] ^ 0xEC012345) or 1)}

    def _draw_reward(self, state, values):
        local = self._reward_context(state)
        chosen = draw(local, values)
        state['economyRewardRngState'] = local['rngState']
        return chosen

    def _roll_pool(self, state, pool):
        from .engine import DomainError
        local = self._reward_context(state)
        try:
            chosen = self.e.effects.roll_pool(local, pool)
        except DomainError as error:
            if error.code == 'empty_trait_pool':
                return None
            raise
        state['economyRewardRngState'] = local['rngState']
        return chosen

    def price(self, state, chess_id, value):
        for ident, key, bb in self._rows(state):
            record = state.get('bandEconomyCounters', {}).get(ident, {})
            if (key == 'first_buy_in_round_char_price_change' and self._has_bond(state, chess_id, bb.get('bond'))
                    and record.get('firstBuyRound') != state['round']):
                value = min(value, max(0, int(bb.get('price', 1))))
        return max(0, value)

    def event(self, state, event, **args):
        for ident, key, bb in self._rows(state):
            handler = getattr(self, '_effect_' + key)
            handler(state, ident, bb, event, args)
        self._sync_refresh_flag(state)

    def _effect_first_buy_in_round_char_price_change(self, state, ident, bb, event, args):
        if event == 'BUY' and self._has_bond(state, args['chessId'], bb.get('bond')):
            self._counter(state, ident)['firstBuyRound'] = state['round']

    def _effect_band_coin_cost_gain_random_char_by_shop_level(self, state, ident, bb, event, args):
        if event != 'SPEND' or args.get('amount', 0) <= 0:
            return
        step = int(bb.get('coin_cnt', 0))
        if step <= 0:
            return
        record = self._counter(state, ident)
        record['remainder'] = record.get('remainder', 0) + args['amount']
        while record['remainder'] >= step:
            pool = self._chars(state)
            if not pool:
                record['pendingPoolUnavailable'] = True
                break
            record['remainder'] -= step
            record['rewards'] = record.get('rewards', 0) + 1
            for _ in range(max(1, int(bb.get('count', 1)))):
                self.e._gain(state, self._draw_reward(state, pool))

    def _effect_band_cost_coin_reach_cnt_gain_chess_from_pool(self, state, ident, bb, event, args):
        if event != 'SPEND' or args.get('amount', 0) <= 0:
            return
        record = self._counter(state, ident)
        if record.get('done'):
            return
        record['spent'] = record.get('spent', 0) + args['amount']
        if record['spent'] < max(1, int(bb.get('coin_cnt', 1))):
            return
        chosen = self._roll_pool(state, bb.get('pool'))
        if chosen is None:
            record['pendingPoolUnavailable'] = True
            return
        # Mark before SERVER_GAIN can re-enter any nested effect dispatch.
        record['done'] = True
        record.pop('pendingPoolUnavailable', None)
        self.e._gain(state, chosen)

    def _effect_up_shop_add_special_goods(self, state, ident, bb, event, args):
        if event != 'LEVEL_UP':
            return
        from .engine import DomainError
        if int(bb.get('choice', 1)) != 1:
            raise DomainError('strategy_multi_choice_pending')
        count = min(16, max(1, int(bb.get('count', 3))))
        options = []
        for _ in range(count * 6):
            chosen = self._roll_pool(state, bb.get('pool'))
            if chosen is None:
                break
            if chosen not in options:
                options.append(chosen)
            if len(options) >= count:
                break
        record = self._counter(state, ident)
        if not options:
            record['pendingPoolUnavailable'] = True
            return
        record['offers'] = record.get('offers', 0) + 1
        state.setdefault('traitRewardOffers', []).append({'source': 'band-economy',
            'effectKey': 'up_shop_add_special_goods', 'pool': bb.get('pool'),
            'options': options, 'price': 0, 'nativeAdapter': 'declaration-special-shop-COMB'})

    def _effect_up_shop_next_refresh_must_present_bond_char(self, state, ident, bb, event, args):
        if event == 'LEVEL_UP':
            levels = [int(value.strip()) for value in str(bb.get('lvlist', '')).split(',') if value.strip()]
            if args['level'] in levels:
                record = self._counter(state, ident)
                record['pendingRefreshes'] = record.get('pendingRefreshes', 0) + 1
        elif event == 'MANUAL_REFRESH':
            record = self._counter(state, ident)
            if record.get('pendingRefreshes', 0) <= 0:
                return
            record['pendingRefreshes'] -= 1
            pool = self._chars(state, bb.get('bond'))
            if not pool:
                record['poolUnavailableOnRefresh'] = True
                return
            for good in state['player']['store']['goods']:
                current = self.c.data['identifiers'][str(good['chessIdentifier'])]
                if current not in self.c.season['CharShopChessDatas'] or self._has_bond(state, current, bb.get('bond')):
                    continue
                chosen = draw(state, pool)
                good['chessIdentifier'] = self.c.chess[chosen]['Identifier']
                good['price'] = self.e._price(state, chosen)

    def _sync_refresh_flag(self, state):
        # This is the native hint for a pending strategy refresh, never a free
        # refresh grant. Existing freeRefreshCnt remains independent.
        state['player']['store']['specialRefresh'] = any(
            key == 'up_shop_next_refresh_must_present_bond_char' and
            state.get('bandEconomyCounters', {}).get(ident, {}).get('pendingRefreshes', 0) > 0
            for ident, key, _ in self._rows(state))
