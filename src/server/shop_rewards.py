# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Promotion reward rules adapted from pinned Stronghold-Protocol
# server/match/player/acquire.js and data/config.json at
# cf7a9400270d5aa79b2e601e405dab782e25f120. See ../third_party/
# stronghold-protocol-reference/ for licence and source provenance.
"""Durable promotion rewards using the native COMB shop already projected.

Slot counts remain the mode-specific CN values, verified against the reference
configuration. Reward draws use the private deterministic eligible-pool policy;
official shared-copy weighting is not inferred from this adapter.
"""
from .catalog import draw
from .engine import DomainError


REWARD_POLICY = {'version': 1, 'count': 3, 'tierOffset': 1, 'maxTier': 6,
                 'price': 0, 'expiresAtRoundEnd': True}


class ShopRewards:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c

    def merged(self, state, base_id, owner):
        """One three/two-copy character merge earns one distinct pick-one offer.

        Direct promotion and equipment merging do not call this path. A merge
        earned after prep has ended stays queued until the next prep; only a
        subsequent explicit Ready can expire it.
        """
        tier = min(state['player']['store']['level'] + REWARD_POLICY['tierOffset'],
                   REWARD_POLICY['maxTier'])
        pool = sorted(key for key in set(state['chessPool'])
                      if key in self.c.season['CharShopChessDatas'] and self.e.pool.available(state,key,shop=True))
        options = []
        for level in range(tier, 0, -1):
            eligible = [key for key in pool
                        if self.c.season['CharShopChessDatas'][key]['ChessLevel'] == level]
            while eligible and len(options) < REWARD_POLICY['count']:
                key = self.e.pool.draw(state,eligible,shop=True) if self.e.pool.enabled(state) else draw(state,eligible)
                eligible.remove(key)
                options.append(key)
            if len(options) == REWARD_POLICY['count']:
                break
        if not options:
            return None
        offer = {'source': 'merge', 'tier': tier, 'options': options,
                 'price': REWARD_POLICY['price'], 'expiresAtRoundEnd': True,
                 'mergedFrom': base_id, 'mergedInstId': owner['instId'],
                 'round': state['round'], 'policyVersion': REWARD_POLICY['version']}
        state.setdefault('traitRewardOffers', []).append(offer)
        return offer

    def preparation_ready(self, state):
        """Expire optional merge rewards; retain existing mandatory choices."""
        offers = state.get('traitRewardOffers') or []
        if any(not offer.get('expiresAtRoundEnd', False) for offer in offers):
            raise DomainError('trait_reward_selection_required')
        if offers:
            offers.clear()

    def require_regular_shop(self, state):
        """Optional merge offers do not lock the underlying ordinary shop.

        The reference preparation gate permits these actions while its reward
        queue exists. A refresh/upgrade changes only the ordinary store; it
        never redraws an already issued reward. Legacy mandatory choices retain
        their existing lock until a native choice is acknowledged.
        """
        if any(not offer.get('expiresAtRoundEnd', False)
               for offer in state.get('traitRewardOffers') or []):
            raise DomainError('trait_reward_selection_required')
