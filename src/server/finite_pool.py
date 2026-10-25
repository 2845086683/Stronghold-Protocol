# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Stock rules/data adapted from Stronghold-Protocol server/match/pool.js,
# gamedata.js, data/config.json and tools/build-data.mjs at
# cf7a9400270d5aa79b2e601e405dab782e25f120. See third_party attribution.
"""Finite run-local stocks derived from ownership, never from DTO displays.

Normal/elite characters occupy one/three copies; normal/upgraded items one/two.
Removing ownership (consume, destroy, sell, overflow) therefore returns copies
without a second mutable ledger which could miss an effect's removal path.
Displayed goods/offers reserve no stock, matching the pinned reference. New
runs persist their capacities; historical runs without poolVersion stay legacy.
"""
from .catalog import weighted
from .setup_rules import chess_eligible

CHAR_COPIES = {1: 12, 2: 14, 3: 18, 4: 16, 5: 8, 6: 5}
CHAR_OVERRIDES = {'chess_char_1_07_a': 10, 'chess_char_1_08_a': 10,
    'chess_char_1_09_a': 10, 'chess_char_1_10_a': 10, 'chess_char_1_14_a': 10,
    'chess_char_2_11_a': 8, 'chess_char_2_02_a': 12, 'chess_char_2_09_a': 12,
    'chess_char_2_12_a': 12, 'chess_char_2_13_a': 12, 'chess_char_2_14_a': 12}
ITEM_COPIES = {1: 4, 2: 6, 3: 7, 4: 8, 5: 7, 6: 3}
ITEM_OVERRIDES = {'chess_item_2_06_e_a': 5, 'chess_item_4_02_e_a': 6,
    'chess_item_4_05_e_a': 6, 'chess_item_4_10_e_a': 6, 'chess_item_4_01_e_a': 7,
    'chess_item_4_04_e_a': 7, 'chess_item_4_08_e_a': 7, 'chess_item_5_07_e_a': 2}
# Source marks these five families as effect-only despite CN HideInShop=false.
# This is item-origin data, not a character-specific handler.
EFFECT_ONLY = frozenset(('chess_item_2_03_e_a', 'chess_item_3_09_e_a',
    'chess_item_3_10_e_a', 'chess_item_4_09_e_a', 'chess_item_5_08_e_a'))


class FinitePool:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c

    @staticmethod
    def enabled(state):
        return state.get('poolVersion') == 1

    def base(self, key):
        return self.c.season['ChessNormalIdLookupDict'].get(key, key)

    def need(self, key):
        if not self.c.chess[key].get('IsGolden'):
            return 1
        return 3 if key in self.c.season['CharChessDataDict'] else 2

    def acquisition_need(self,state,key,merge=True):
        need=self.need(key);row=self.c.chess[key]
        upgrade=row.get('UpgradeChessId');count=row.get('UpgradeNum')
        if merge and upgrade and count:
            extra=max(0,self.need(upgrade)-count*need)
            if extra:
                owned=sum(obj['chessIdentifier']==row['Identifier'] for obj in state['player']['charChess'])
                need+=((owned+1)//count)*extra
        return need

    def shop_item(self, key):
        row = self.c.season['TrapShopChessDatas'].get(key)
        return bool(row and not row['HideInShop'] and row['ItemType'] == 1
                    and not self.c.chess[key].get('IsGolden') and self.base(key) not in EFFECT_ONLY)

    def initialize(self, state):
        capacities = {}
        for key in state['chessPool']:
            row = self.c.season['CharShopChessDatas'][key]
            capacities[key] = CHAR_OVERRIDES.get(key, CHAR_COPIES[row['ChessLevel']])
        for key, row in self.c.season['TrapShopChessDatas'].items():
            if self.shop_item(key):
                capacities[key] = ITEM_OVERRIDES.get(key, ITEM_COPIES[row['ItemLevel']])
        state['poolVersion'] = 1
        state['finitePool'] = {'policyVersion': 1, 'capacities': capacities,
            'displayReservesStock': False, 'ownershipDerived': True}

    def held(self, state, key):
        base = self.base(key)
        return sum(self.need(self.c.data['identifiers'][str(obj['chessIdentifier'])])
            for field in ('charChess', 'equipChess', 'trapChess')
            for obj in state['player'][field]
            if self.base(self.c.data['identifiers'][str(obj['chessIdentifier'])]) == base)

    def remaining(self, state, key):
        """Public remaining count; None denotes an effect-only unlimited family."""
        if not self.enabled(state):
            return None
        cap = state['finitePool']['capacities'].get(self.base(key))
        return None if cap is None else max(0, cap - self.held(state, key))

    def available(self, state, key, shop=False):
        if key not in self.c.chess or not chess_eligible(self.c, state, key):
            return False
        if not self.enabled(state):
            return True
        if key in self.c.season['CharChessDataDict']:
            if self.base(key) not in state['chessPool']:
                return False
            if shop and (key not in self.c.season['CharShopChessDatas'] or self.c.chess[key].get('IsGolden')):
                return False
        elif shop and not self.shop_item(key):
            return False
        left = self.remaining(state, key)
        return left is None or left >= self.acquisition_need(state,key)

    def candidates(self, state, keys, shop=False):
        return [key for key in dict.fromkeys(keys) if self.available(state, key, shop)]

    def draw(self, state, keys, shop=False):
        values = self.candidates(state, keys, shop)
        if not values:
            return None
        # Character rolls are copy-weighted. Item stock gates eligibility but
        # does not imply a measured probability model for individual items.
        pairs = [(key, self.remaining(state, key) or 1) if key in self.c.season['CharChessDataDict']
                 else (key, 1) for key in values]
        return weighted(state, pairs)

    def require(self, state, key, *, fixed=False,merge=True):
        """Only explicitly fixed grants may occupy a debt.

        Fixed trait gifts may create a nonselectable/banned character without
        making it eligible to roll. Random grants filter candidates first.
        Ordinary purchases and reward picks are strict even when displayed.
        This deliberately follows the user's finite-stock requirement rather
        than the reference's already-displayed-character overdraw exception.
        """
        if not self.enabled(state):
            return
        from .engine import DomainError
        if fixed:
            return
        if not chess_eligible(self.c, state, key):
            raise DomainError('banned_chess', key)
        left=self.remaining(state,key)
        if (key in self.c.season['CharChessDataDict'] and self.base(key) not in state['chessPool'] or
            left is not None and left<self.acquisition_need(state,key,merge)):
            raise DomainError('stock_exhausted', key)

    def audit(self, state):
        if not self.enabled(state):
            return {}
        return {key: {'capacity': cap, 'held': self.held(state, key),
                      'rawLeft': cap - self.held(state, key),
                      'remaining': max(0, cap - self.held(state, key))}
                for key, cap in state['finitePool']['capacities'].items()}

    def prune_displayed(self,state):
        """Remove exhausted visible cards without drawing replacements.

        Ownership may change through a fixed gift or another displayed copy.
        Keep slot IDs/frozen flags of surviving goods, and drop empty reward
        queues so the native UI is never trapped on an unclaimable offer.
        """
        if not self.enabled(state):return
        store=state['player']['store']
        store['goods']=[good for good in store['goods'] if self.available(state,
            self.c.data['identifiers'][str(good['chessIdentifier'])],shop=True)]
        offers=[]
        for offer in state.get('traitRewardOffers') or []:
            offer['options']=[key for key in offer['options'] if self.available(state,key)]
            if offer['options']:offers.append(offer)
        if 'traitRewardOffers' in state:state['traitRewardOffers']=offers
