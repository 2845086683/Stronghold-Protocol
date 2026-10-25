# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Adapted from upstream effectsMeta.js, sim/content/garrisons/meta.js,
# match/bondsMeta.js and sim/content/bonds/{core,addon/meta}.js at commit
# cf7a9400270d5aa79b2e601e405dab782e25f120.  Python adaptation uses the existing
# private domain state/transaction and CN typed Blackboard parameters.
# See ../third_party/stronghold-protocol-reference/THIRD-PARTY-NOTICES.md.
"""Data-driven SERVER_* traits; Unity owns IN_BATTLE traits and battle runes.

Dispatch walks snapshots through a bounded FIFO. Nested grants, copy traits and
re-triggers share the budget; an error aborts the enclosing Store transaction.
There are no operator IDs, garrison IDs or translated descriptions with numeric
rules in the handlers. Descriptions only supply activation/dual-event flags not
represented as separate fields by the original activity data.
"""
import collections
import copy
import threading
from .catalog import draw, weighted
from .effect_pools import POOLS
from .preset_snapshot import base_bonds

EVENTS = frozenset(('SERVER_GAIN', 'SERVER_PREP_START', 'SERVER_PREP_FIN',
                    'SERVER_CHESS_SOLD', 'SERVER_PRICE', 'SERVER_REFRESH_SHOP'))
MAX_DEPTH = 6
MAX_TASKS = 256


def number(value, default=0):
    if isinstance(value, (int, float)) and not isinstance(value, bool):
        return int(value)
    try:
        return int(str(value))
    except (ValueError, TypeError):
        return default


def ids(value):
    return [part.strip() for part in str(value or '').split(',') if part.strip()]


class GarrisonEffects:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c
        self.local = threading.local()

    def fail(self, code, detail=''):
        from .engine import DomainError
        raise DomainError(code, detail)

    def definition(self, obj):
        return self.e._definition(obj)

    def bonds(self, state, obj):
        result = base_bonds(self.c, state, self.definition(obj)['ChessId'])
        equipped = [item for item in state['player']['equipChess'] if item['instId'] in obj.get('equipInstIds', [])]
        records = [self.definition(item) for item in equipped]
        if any(record.get('CanGiveBond') for record in records):
            for record in records:
                bond = record.get('GiveBondId')
                if not record.get('CanGiveBond') and bond in self.c.season['BondInfoDict'] and bond not in result:
                    result.append(bond)
        return result

    def position(self, state, obj):
        return next((row for row in state['player']['positions'] if row['instId'] == obj['instId']), None)

    def board(self, state):
        rows = [(self.position(state, obj), obj) for obj in state['player']['charChess']]
        return [obj for pos, obj in sorted((row for row in rows if row[0]),
                key=lambda row: (row[0]['position']['x'], -row[0]['position']['y'], row[1]['instId']))]

    def hand(self, state):
        from .hand_layout import enabled, reconcile, VALID_HAND_COUNT
        valid = None
        if enabled(state):
            valid = {row['instId'] for row in reconcile(self.c, state)
                     if row['position'] > -VALID_HAND_COUNT}
        return [obj for obj in state['player']['charChess']
                if self.position(state, obj) is None and (valid is None or obj['instId'] in valid)]

    def tier(self, obj):
        key = self.definition(obj)['ChessId']
        normal = self.c.season['ChessNormalIdLookupDict'].get(key, key)
        return self.c.season['CharShopChessDatas'][normal]['ChessLevel']

    def active(self, state, key):
        ident = self.c.season['BondInfoDict'][key]['Identifier']
        return next((row['isActive'] for row in state['player']['bonds'] if row['bondIdentifier'] == ident), False)

    def layer_count(self, state, key):
        ident = self.c.season['BondInfoDict'][key]['Identifier']
        return next((row['stackCnt'] for row in state['player']['bonds'] if row['bondIdentifier'] == ident), 0)

    def add(self, state, keys, count, require_active=False):
        count = number(count)
        if count <= 0:
            return
        for key in keys:
            definition = self.c.season['BondInfoDict'].get(key)
            if definition is None:
                self.fail('unknown_trait_bond', str(key))
            if not require_active or self.active(state, key):
                self.e._layers(state, definition['Identifier'], count)

    def top(self, state):
        keys = [key for key in self.c.season['BondInfoDict'] if self.active(state, key)]
        return max(keys, key=lambda key: self.layer_count(state, key)) if keys else None

    def front(self, state, obj, distance=1):
        pos = self.position(state, obj)
        if pos is None:
            return None
        # Torappu.Direction: UP=0, RIGHT=1, DOWN=2, LEFT=3. The native codec
        # retains that numeric value in domain positions; +y is the upper row.
        dx, dy = ((0, 1), (1, 0), (0, -1), (-1, 0))[pos.get('direction', 0)]
        x = pos['position']['x'] + dx * distance
        y = pos['position']['y'] + dy * distance
        target = next((row['instId'] for row in state['player']['positions']
                       if row['position'] == {'x': x, 'y': y}), None)
        return next((row for row in state['player']['charChess'] if row['instId'] == target), None)

    def _records(self, obj):
        return [(key, self.c.season['GarrisonDataDict'][key]) for key in (self.definition(obj).get('GarrisonIds') or [])]

    @staticmethod
    def _matches(record, event):
        if record['EventType'] == event:
            return True
        # E.g. the data's PREP_START trait explicitly also names PREP_FIN.
        return (event == 'SERVER_PREP_FIN' and record['EventType'] == 'SERVER_PREP_START'
                and '<休整期结束时>' in record.get('Description', ''))

    def dispatch(self, state, event, subject=None, *, templates=None, as_subject=None, trigger=False, snapshot=None):
        if event not in EVENTS:
            self.fail('unknown_trait_event', event)
        context = getattr(self.local, 'context', None)
        root = context is None
        if root:
            context = {'queue': collections.deque(), 'depth': -1, 'tasks': 0, 'state': state}
            self.local.context = context
        elif context['state'] is not state:
            self.fail('cross_state_trait_dispatch')
        depth = context['depth'] + 1
        if depth >= MAX_DEPTH:
            self.fail('trait_dispatch_depth_exceeded')
        if subject is not None:
            subjects = [subject]
        else:
            subjects = snapshot if snapshot is not None else [*self.board(state), *self.hand(state)]
        for source in subjects:
            context['queue'].append((depth, event, copy.deepcopy(source),
                                     copy.deepcopy(templates), copy.deepcopy(as_subject), trigger))
        if not root:
            return
        try:
            while context['queue']:
                if context['tasks'] >= MAX_TASKS:
                    self.fail('trait_dispatch_budget_exceeded')
                context['tasks'] += 1
                context['depth'], ev, source, records, actor, is_trigger = context['queue'].popleft()
                self._execute(state, ev, source, records, actor, is_trigger)
        finally:
            del self.local.context

    def _execute(self, state, event, source, templates, actor, trigger):
        if event not in ('SERVER_GAIN', 'SERVER_CHESS_SOLD', 'SERVER_PRICE'):
            # An in-place merge retains the native instance ID/tile, but the
            # resulting elite did not witness the event that caused its merge.
            # Recheck both identity and valid ownership against the dispatch
            # snapshot; temporary hand cells never run owned-piece traits.
            eligible = [*self.board(state), *self.hand(state)]
            if not any(row['instId'] == source['instId'] and row['chessIdentifier'] == source['chessIdentifier']
                       for row in eligible):
                return
            if actor is not None and not any(row['instId'] == actor['instId'] and row['chessIdentifier'] == actor['chessIdentifier']
                                             for row in eligible):
                return
        actor = actor or source
        records = templates if templates is not None else self._records(source)
        for gid, record in records:
            if not self._matches(record, event):
                continue
            bb = record.get('Blackboard') or {}
            condition = bb.get('conditionkey')
            if condition == 'character_target_inboard' and self.position(state, actor) is None:
                continue
            if condition == 'character_same_row':
                pos = self.position(state, actor)
                row_count = sum(self.position(state, obj)['position']['y'] == pos['position']['y'] for obj in self.board(state)) if pos else 0
                if row_count < number(bb.get('check_count')):
                    continue
            elif condition not in (None, 'character_target_inboard'):
                self.fail('trait_condition_pending', str(condition))
            handler = getattr(self, '_effect_' + record['EffectType'], None)
            if handler is None:
                self.fail('trait_handler_pending', record['EffectType'])
            repeats = 1
            if event == 'SERVER_GAIN':
                for key, definition in self.c.season['BondInfoDict'].items():
                    if not self.active(state, key):
                        continue
                    for buff in self.c.effects(definition['EffectId']):
                        values = buff.get('Blackboard') or {}
                        if (buff['Key'] == 'bond_layer_char_garrison_bonus' and values.get('event') == event
                                and self.layer_count(state, key) >= number(values.get('layer'))):
                            repeats = max(repeats, number(values.get('count'), 1))
            for _ in range(min(10, repeats)):
                handler(state, actor, gid, record, bb, trigger)

    @staticmethod
    def require_active(record):
        desc = record.get('Description') or ''
        return '无需激活' not in desc and ('已激活' in desc or '激活且' in desc)

    def _effect_SERVER_GAIN_FREE_REFRESH_COUNT(self, s, obj, gid, g, b, trigger):
        s['player']['store']['freeRefreshCnt'] += number(b.get('count'), 1)

    def _effect_SERVER_ONCE_GOLD(self, s, obj, gid, g, b, trigger):
        s['scheduled'].append({'round': s['round'] + 1, 'kind': 'coin', 'count': number(b.get('count'))})

    def _effect_SERVER_ONCE_GOLD_WITH_BOND_CONDITION(self, s, obj, gid, g, b, trigger):
        if self.position(s, obj) is None and not any(self.active(s, key) for key in ids(b.get('bond'))):
            return
        self._effect_SERVER_ONCE_GOLD(s, obj, gid, g, b, trigger)

    def _effect_SERVER_GAIN_EQUIP(self, s, obj, gid, g, b, trigger):
        key = b.get('chess')
        if key not in self.c.chess:
            self.fail('unknown_trait_reward', str(key))
        for _ in range(min(10, number(b.get('count'), 1))):
            self.e._gain(s, key)

    _effect_SERVER_GAIN_CHAR = _effect_SERVER_GAIN_EQUIP

    def _effect_SERVER_ADD_BOND(self, s, obj, gid, g, b, trigger):
        covered = set()
        for _, other in self._records(obj):
            if (other['EffectType'] == 'SERVER_ADD_MULTIPLE_BOND' and other['EventType'] == g['EventType']
                    and other.get('Description') == g.get('Description')):
                covered.update(ids((other.get('Blackboard') or {}).get('bond')))
        self.add(s, [key for key in ids(b.get('bond')) if key not in covered], b.get('count'), self.require_active(g))

    def _effect_SERVER_ADD_MULTIPLE_BOND(self, s, obj, gid, g, b, trigger):
        counts = ids(b.get('count'))
        for index, key in enumerate(ids(b.get('bond'))):
            self.add(s, [key], counts[min(index, len(counts) - 1)] if counts else 0, self.require_active(g))

    def _effect_SERVER_ADD_BOND_CHESS_ALL(self, s, obj, gid, g, b, trigger):
        self.add(s, self.bonds(s, obj), b.get('count'), self.require_active(g))

    def _effect_SERVER_ADD_BOND_ACTIVATED_MOST_LAYER(self, s, obj, gid, g, b, trigger):
        key = self.top(s)
        if key:
            self.add(s, [key], b.get('count'), True)

    def _effect_SERVER_ADD_ACT_BOND_DIFF_LV_MOST_LAYER(self, s, obj, gid, g, b, trigger):
        key = self.top(s)
        if key:
            count = len({self.tier(row) for row in self.board(s) if key in self.bonds(s, row)})
            self.add(s, [key], count * number(b.get('multi'), 1), True)

    def _effect_SERVER_ADD_BOND_ACTIVATED_RANDOM(self, s, obj, gid, g, b, trigger):
        keys = [key for key in self.c.season['BondInfoDict'] if self.active(s, key)]
        for _ in range(min(10, number(b.get('rand_count'), 1))):
            if keys:
                self.add(s, [draw(s, keys)], b.get('count'), True)

    def _effect_SERVER_ADD_BOND_METHOD(self, s, obj, gid, g, b, trigger):
        method = b.get('add_method')
        for key in ids(b.get('bond')):
            if method == 'shoplv':
                n = s['player']['store']['level']
            elif method == 'round_gain_char':
                n = sum(row['count'] for row in s['player']['roundAnalytics']['chessGained']
                        if self.c.data['identifiers'][str(row['chessIdentifier'])] in self.c.season['CharChessDataDict'])
            elif method == 'hand_count':
                n = len(self.hand(s))
            elif method == 'same_row':
                pos = self.position(s, obj)
                n = sum(self.position(s, row)['position']['y'] == pos['position']['y'] for row in self.board(s)) if pos else 0
            elif method == 'same_bond_diff_lv':
                n = len({self.tier(row) for row in self.board(s) if key in self.bonds(s, row)})
            else:
                self.fail('trait_add_method_pending', str(method))
            self.add(s, [key], n * number(b.get('multi'), 1), self.require_active(g))

    def _effect_SERVER_ADD_BOND_IN_HAND(self, s, obj, gid, g, b, trigger):
        for row in self.hand(s):
            self.add(s, self.bonds(s, row), b.get('count'), True)

    def _effect_SERVER_ADD_BOND_POSITION(self, s, obj, gid, g, b, trigger):
        if self.position(s, obj) is None:
            return
        targets = [obj, self.front(s, obj, -1 if b.get('dir') == 'behind' else 1)]
        for target in targets:
            if target:
                self.add(s, self.bonds(s, target), b.get('count'), True)

    def _effect_SERVER_ADD_BOND_FRONT_ALL_LAYER(self, s, obj, gid, g, b, trigger):
        for distance in range(1, 64):
            target = self.front(s, obj, distance)
            if target:
                self.add(s, self.bonds(s, target), b.get('count'), True)

    def _effect_SERVER_ADD_BOND_ROUND_COIN_COST(self, s, obj, gid, g, b, trigger):
        count = s['player']['roundAnalytics']['coinCost'] // max(1, number(b.get('count'), 1))
        self.add(s, ids(b.get('bond')), count * number(b.get('layer')), self.require_active(g))

    def _effect_SERVER_ADD_REFRESH_CNT_MULTIPLIER_BOND_LAYER(self, s, obj, gid, g, b, trigger):
        n = s.get('roundEffectStats', {}).get('manualRefreshes', 0) * number(b.get('multiplier'))
        if number(b.get('max_layer')) > 0:
            n = min(n, number(b['max_layer']))
        self.add(s, ids(b.get('bond')), n, self.require_active(g))

    def _effect_SERVER_GAIN_BOND_LAYER_BY_REFRESH_CNT(self, s, obj, gid, g, b, trigger):
        if trigger:
            return
        counters = s.setdefault('traitCounters', {})
        key = ':'.join((str(s['round']), str(obj['instId']), gid))
        counters[key] = counters.get(key, 0) + 1
        if counters[key] == number(b.get('refresh_cnt'), 1):
            self.add(s, ids(b.get('bond')), b.get('layer'), self.require_active(g))

    def _effect_SERVER_CHESS_PRICE(self, s, obj, gid, g, b, trigger):
        # Data stores the discount from the tier price, matching the reference
        # order: this trait runs before bond/strategy price modifiers.
        obj['price'] = max(0, obj['price'] - number(b.get('price')))

    def price(self, state, chess_id, base):
        subject = {'instId': -1, 'chessIdentifier': self.c.chess[chess_id]['Identifier'], 'price': base}
        self._execute(state, 'SERVER_PRICE', subject, None, None, False)
        value = subject['price']
        for key, definition in self.c.season['BondInfoDict'].items():
            for effect in self.c.effects(definition['EffectId']):
                if effect['Key'] != 'bond_multi_layer_char_goods_price_bond_discount':
                    continue
                level = state.get('bondDiscounts', {}).get(key, 0)
                bb = effect['Blackboard'] or {}
                if level >= 2 or level >= 1 and bb.get('bond', key) in base_bonds(self.c, state, chess_id):
                    value = max(0, value - number(bb.get('discount')))
        return value

    def settle_bonds(self, state):
        # A milestone equipment grant re-enters _gain -> settlement. Paying
        # the next already-due milestone recursively grows one Python stack
        # frame per reward (e.g. activating a pre-stacked Victoria bond).
        # Re-entry requests another pass instead; the active outer pass owns
        # payment and still persists each counter before granting its reward.
        context = getattr(self.local, 'settlement', None)
        if context is not None:
            if context['state'] is not state:
                self.fail('cross_state_bond_settlement')
            context['pending'] = True
            return
        context = {'state': state, 'pending': False}
        self.local.settlement = context
        try:
            for _ in range(MAX_TASKS):
                context['pending'] = False
                self._settle_bonds_pass(state)
                if not context['pending']:
                    return
            self.fail('bond_settlement_budget_exceeded')
        finally:
            del self.local.settlement

    def _settle_bonds_pass(self, state):
        counters = state.setdefault('bondEconomyCounters', {})
        for key, definition in self.c.season['BondInfoDict'].items():
            if not self.active(state, key):
                continue
            layers = self.layer_count(state, key)
            for index, effect in enumerate(self.c.effects(definition['EffectId'])):
                bb = effect['Blackboard'] or {}
                kind = effect['Key']
                identity = key + ':' + str(index)
                if kind in ('bond_layer_gain_coin', 'bond_layer_added_reward_equip'):
                    step = number(bb.get('layer'))
                    if step <= 0:
                        self.fail('invalid_bond_milestone', identity)
                    due = layers // step
                    while counters.get(identity, 0) < due:
                        # Update before a grant can re-enter settlement.
                        counters[identity] = counters.get(identity, 0) + 1
                        if kind == 'bond_layer_gain_coin':
                            gain = number(bb.get('count'))
                            if state['phase'] in ('battle', 'boss') or state.get('prepEndedRound') == state['round']:
                                state['scheduled'].append({'round': state['round'] + 1, 'kind': 'coin', 'count': gain})
                            else:
                                state['player']['store']['coin'] += gain
                        else:
                            for _ in range(min(10, number(bb.get('count'), 1))):
                                chosen = self.optional_pool(state, bb.get('pool'))
                                if chosen is not None:
                                    self.e._gain(state, chosen, merge=state.get('prepEndedRound') != state['round'])
                elif kind == 'bond_multi_layer_char_goods_price_bond_discount':
                    discount = 2 if layers >= number(bb.get('layer2'), 2147483647) else 1 if layers >= number(bb.get('layer1'), 2147483647) else 0
                    discounts = state.setdefault('bondDiscounts', {})
                    discounts[key] = max(discounts.get(key, 0), discount)

    def refresh_bonds(self, state):
        if state['player']['store']['freeRefreshCnt'] > 0:
            return
        for key, definition in self.c.season['BondInfoDict'].items():
            if not self.active(state, key):
                continue
            for effect in self.c.effects(definition['EffectId']):
                if effect['Key'] == 'bond_refresh_shop_next_free':
                    bb = effect['Blackboard'] or {}
                    probability = max(0, min(1, bb.get('baseprob', 0) + bb.get('prob', 0) * self.layer_count(state, key)))
                    if draw(state, range(1000000)) / 1000000 < probability:
                        state['player']['store']['freeRefreshCnt'] += 1

    def prep_end_bonds(self, state):
        state['prepEndedRound'] = state['round']
        for key, definition in self.c.season['BondInfoDict'].items():
            if not self.active(state, key):
                continue
            for effect in self.c.effects(definition['EffectId']):
                if effect['Key'] != 'bond_activated_add_layer':
                    continue
                bb = effect['Blackboard'] or {}
                variants = {(self.definition(obj)['ChessId']) for obj in self.board(state) if key in self.bonds(state, obj)}
                count = number(bb.get('more_layer')) if len(variants) >= number(bb.get('count'), 2147483647) else number(bb.get('layer'))
                self.add(state, [bond for bond in self.c.season['BondInfoDict'] if self.active(state, bond)], count, True)

    def roll_pool(self, s, pool_id, golden=False):
        pool = POOLS.get(pool_id)
        if pool is None:
            self.fail('trait_pool_policy_pending', str(pool_id))
        if pool.get('weighted'):
            pairs = list(pool['weighted'])
            if golden and pool.get('goldenWeights'):
                pairs = [(pair[0], pool['goldenWeights'][index]) for index, pair in enumerate(pairs)]
            pairs = [(key, weight) for key, weight in pairs
                     if key in self.c.chess and self.e.pool.available(s, key)]
            if not pairs:
                self.fail('empty_trait_pool', str(pool_id))
            return weighted(s, pairs)
        values = list(pool.get('items', []))
        if pool.get('rule') == 'shopEligible':
            if pool['kind'] == 'chess':
                values = list(s['chessPool'])
                values = [key for key in values if self.c.season['CharShopChessDatas'][key]['ChessLevel'] >= pool.get('minTier', 1)
                          and (not pool.get('bond') or pool['bond'] in base_bonds(self.c, s, key))]
            else:
                values = [key for key, row in self.c.season['TrapShopChessDatas'].items()
                          if not row['HideInShop'] and self.c.chess[key]['ItemType'] == 1
                          and self.e.pool.shop_item(key)]
            def tier(key):
                return (self.c.season['CharShopChessDatas'][key]['ChessLevel'] if pool['kind'] == 'chess'
                        else self.c.season['TrapShopChessDatas'][key]['ItemLevel'])
            if pool.get('maxTier') == 'shopLevel':
                values = [key for key in values if tier(key) <= s['player']['store']['level']]
            if 'tiers' in pool:
                values = [key for key in values if tier(key) in pool['tiers']]
            if 'tier' in pool:
                values = [key for key in values if tier(key) == pool['tier']]
        values = [key for key in values if key in self.c.chess
                  and self.e.pool.available(s, (self.c.chess[key].get('UpgradeChessId') or key) if pool.get('golden') else key)]
        if not values:
            self.fail('empty_trait_pool', str(pool_id))
        key = draw(s, values)
        return (self.c.chess[key].get('UpgradeChessId') or key) if pool.get('golden') else key

    def optional_pool(self, state, pool_id, golden=False):
        """An exhausted finite random award is skipped, without rolling debt."""
        from .engine import DomainError
        try:
            return self.roll_pool(state, pool_id, golden)
        except DomainError as error:
            if error.code != 'empty_trait_pool':
                raise
            skips = state.setdefault('traitPoolSkips', {})
            key = str(pool_id)
            skips[key] = skips.get(key, 0) + 1
            return None

    def _effect_SERVER_POOL_EQUIP(self, s, obj, gid, g, b, trigger):
        for _ in range(min(10, number(b.get('count'), 1))):
            chosen = self.optional_pool(s, b.get('pool'), gid.endswith('_b'))
            if chosen is not None:
                self.e._gain(s, chosen)

    _effect_SERVER_POOL_CHAR = _effect_SERVER_POOL_EQUIP

    def _effect_SERVER_GAIN_RANDOM_EQUIP_CHESS_IN_POOL(self, s, obj, gid, g, b, trigger):
        rounds = [number(row) for row in ids(b.get('round_list'))]
        if not rounds or s['round'] in rounds:
            self._effect_SERVER_POOL_EQUIP(s, obj, gid, g, b, trigger)

    def _effect_SERVER_MOST_BOND(self, s, obj, gid, g, b, trigger):
        best = max((row['charCnt'] for row in s['player']['bonds']), default=0)
        keys = [key for key, definition in self.c.season['BondInfoDict'].items()
                if next(row['charCnt'] for row in s['player']['bonds'] if row['bondIdentifier'] == definition['Identifier']) == best and best > 0]
        while keys:
            key = draw(s, keys)
            candidates = [chess for chess in s['chessPool'] if key in base_bonds(self.c, s, chess)
                          and self.e.pool.available(s, chess)
                          and self.c.chess[chess]['ChessId'] != self.definition(obj)['ChessId']]
            if candidates:
                self.e._gain(s, draw(s, candidates))
                return
            keys.remove(key)

    def _effect_SERVER_TRIGGER_ANOTHER(self, s, obj, gid, g, b, trigger):
        event = b.get('event', 'SERVER_GAIN')
        if b.get('scope') == 'front':
            target = self.front(s, obj)
        else:
            candidates = [row for row in self.board(s) if row['instId'] != obj['instId']
                          and any(self._matches(record, event) for _, record in self._records(row))]
            candidates.sort(key=lambda row: (-self.position(s, row)['position']['y'], -self.position(s, row)['position']['x']))
            target = candidates[0] if candidates else None
        if target:
            self.dispatch(s, event, target, trigger=True)

    def _effect_SERVER_TRIGGER_FRONT_COUNT(self, s, obj, gid, g, b, trigger):
        for distance in range(1, min(8, number(b.get('count'), 1)) + 1):
            target = self.front(s, obj, distance)
            if target:
                self.dispatch(s, b.get('event', 'SERVER_GAIN'), target, trigger=True)

    def _copy_front(self, s, obj, event, key):
        target = self.front(s, obj)
        for _ in range(8):
            if target is None:
                return
            records = [(gid, g) for gid, g in self._records(target) if self._matches(g, event)]
            if not records:
                return
            if not all(g['EffectType'] == key for _, g in records):
                self.dispatch(s, event, target, templates=records, as_subject=obj, trigger=True)
                return
            target = self.front(s, target)
        self.fail('trait_copy_chain_exceeded')

    def _effect_SERVER_FRONT_SAME_EFFECT_PREP_START(self, s, obj, gid, g, b, trigger):
        self._copy_front(s, obj, 'SERVER_PREP_START', g['EffectType'])

    def _effect_SERVER_FRONT_SAME_EFFECT_PREP_FIN(self, s, obj, gid, g, b, trigger):
        self._copy_front(s, obj, 'SERVER_PREP_FIN', g['EffectType'])

    def _effect_SERVER_SELL_CHESS_GAIN_SPECIAL_GOODS(self, s, obj, gid, g, b, trigger):
        # Domain selection is durable and never silently replaced by a random
        # reward. Its native special-shop adapter is tracked separately.
        pool = b.get('pool' + str(s['player']['store']['level']), b.get('max_pool'))
        options = []
        for _ in range(24):
            key = self.optional_pool(s, pool)
            if key is None:
                break
            if key not in options:
                options.append(key)
            if len(options) == 3:
                break
        if not options:
            return
        s.setdefault('traitRewardOffers', []).append({'source': gid, 'pool': pool,
            'options': options, 'price': 0, 'nativeAdapter': 'declaration-special-shop-COMB'})

    def coverage(self):
        referenced = {gid for row in self.c.season['CharChessDataDict'].values() for gid in (row.get('GarrisonIds') or [])}
        rows = []
        for key, value in self.c.season['GarrisonDataDict'].items():
            native = value['EventType'] == 'IN_BATTLE'
            supported = hasattr(self, '_effect_' + value['EffectType'])
            rows.append({'garrisonId': key, 'referenced': key in referenced, 'event': value['EventType'],
                         'effectType': value['EffectType'], 'owner': 'Unity' if native else 'private-server',
                         'domainImplemented': supported if not native else False,
                         'nativeUiAccepted': False})
        return {'version': 1, 'rows': rows, 'events': sorted(EVENTS), 'nativeBattleReimplemented': False,
                'poolPolicy': 'Reference unpublished pools/weights; deterministic private xorshift32-v1',
                'nativeAcceptance': False}
