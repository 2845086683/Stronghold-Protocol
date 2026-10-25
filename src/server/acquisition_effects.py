# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Acquisition/merge lifecycle adapted from server/match/player/acquire.js and
# server/match/effectsMeta.js at cf7a9400270d5aa79b2e601e405dab782e25f120.
# Private rule correction: acquiring an ordinary copy and acquiring the merged
# elite are distinct SERVER_GAIN events, including their round gain counters.
# See ../third_party/stronghold-protocol-reference/ for licence and notices.
"""One ownership lifecycle for purchases, grants and merge products.

The native DTO needs the surviving instance even when a gain effect recursively
grants another copy and merges the original before its outer call returns.
Temporary redirects belong to this call stack, never to a persisted save.
Explicit promotions use promote(), which emits the new elite identity without
creating the three-copy merge's separate reward offer.
"""
import copy
import threading
from contextlib import contextmanager

MAX_EVENTS = 256
MAX_DEPTH = 32


class AcquisitionEffects:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c
        self.local = threading.local()

    @staticmethod
    def _fail(code):
        from .engine import DomainError
        raise DomainError(code)

    @contextmanager
    def _context(self, state):
        context = getattr(self.local, 'context', None)
        root = context is None
        if root:
            context = {'state': state, 'redirects': {}, 'events': 0, 'depth': 0}
            self.local.context = context
        elif context['state'] is not state:
            self._fail('cross_state_acquisition')
        if context['depth'] >= MAX_DEPTH:
            self._fail('acquisition_depth_exceeded')
        context['depth'] += 1
        try:
            yield context
        finally:
            context['depth'] -= 1
            if root:
                del self.local.context

    def _notify(self, state, obj, field, context):
        if context['events'] >= MAX_EVENTS:
            self._fail('acquisition_budget_exceeded')
        context['events'] += 1
        # The incoming ordinary identity must survive any in-place merge in
        # nested handlers. Garrison dispatch also snapshots this historical
        # event; observers receive the same ordinary/elite identity.
        subject = copy.deepcopy(obj)
        state['player']['roundAnalytics']['chessGained'].append(
            {'chessIdentifier': subject['chessIdentifier'], 'count': 1})
        self.e._bonds(state)
        if field == 'charChess':
            self.e.broadcasts.event(state, 'CHAR_ELITE', subject=subject)
            self.e.effects.dispatch(state, 'SERVER_GAIN', subject)
            self.e.items.event(state, 'SERVER_GAIN', subject)
        self.e.effects.settle_bonds(state)

    def _survivor(self, state, obj, field, context):
        visited = set()
        while obj['instId'] in context['redirects']:
            ident = obj['instId']
            if ident in visited:
                self._fail('acquisition_redirect_cycle')
            visited.add(ident)
            obj = context['redirects'][ident]
        owner = next((row for row in state['player'][field]
                      if row['instId'] == obj['instId']), None)
        if owner is None:
            self._fail('acquisition_owner_missing')
        return owner

    def gain(self, state, obj, field, definition, merge=True):
        with self._context(state) as context:
            self._notify(state, obj, field, context)
            # PREP_FIN equipment grants retain the existing deferred merge
            # rule, including rewards nested in gained traits or milestones.
            if field == 'equipChess' and state.get('prepEndedRound') == state['round']:
                merge = False
            if merge:
                self.e._merge(state, field, definition)
            self.e._bonds(state)
            self.e.effects.settle_bonds(state)
            self.e._reprice(state)
            from .hand_layout import reconcile
            reconcile(self.c, state)
            return self._survivor(state, obj, field, context)

    def merged(self, state, consumed, owner):
        """Called once after each merge's structural change, before recursion.

        Keep the original native instance/tile policy. Only character merge
        products create SERVER_GAIN; equipment merges still only replace
        already acquired inventory and detach their consumed equipment IDs.
        """
        with self._context(state) as context:
            for obj in consumed:
                if obj['instId'] != owner['instId']:
                    context['redirects'][obj['instId']] = owner
            key = self.e._definition(owner)['ChessId']
            if key in self.c.season['CharChessDataDict']:
                self._notify(state, owner, 'charChess', context)

    def promote(self, state, owner):
        """Promote one owned instance and return its final surviving owner.

        Existing elites are an idempotent no-op. The caller owns price/stock
        and item consumption; this shared lifecycle owns acquisition counters,
        ordinary trait dispatch, redirected ownership and native hand layout.
        Unlike _merge(), it does not award a COMB merge offer.
        """
        with self._context(state) as context:
            field = next((name for name in ('charChess', 'equipChess', 'trapChess')
                          if any(row['instId'] == owner['instId'] for row in state['player'][name])), None)
            if field is None:
                self._fail('acquisition_owner_missing')
            owner = next(row for row in state['player'][field] if row['instId'] == owner['instId'])
            definition = self.e._definition(owner)
            if definition.get('IsGolden') or not definition.get('UpgradeChessId'):
                return owner
            self.e._promote(owner)
            self._notify(state, owner, field, context)
            self.e._bonds(state)
            self.e.effects.settle_bonds(state)
            self.e._reprice(state)
            from .hand_layout import reconcile
            reconcile(self.c, state)
            return self._survivor(state, owner, field, context)
