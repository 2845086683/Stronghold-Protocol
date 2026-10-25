# SPDX-License-Identifier: GPL-3.0-or-later
"""Resolve the same preparation terrain that the native round selects.

Ordinary rest uses the persisted personal stage. Final-assault rest already
uses its selected boss level; checking the personal stage there rejects real
native floor cells and can accept cells absent from the final-assault layout.
The selected template is immutable, so this lookup never redraws a round.
"""
from types import SimpleNamespace


def level_id(catalog, state):
    number = state['round']
    if not catalog.turns(state['modeId']).get(number, {}).get('IsBossTurn'):
        return 'level_' + state['stageId']
    if state.get('setupVersion', 0) >= 2:
        from .setup_rules import SetupRules
        return SetupRules(SimpleNamespace(c=catalog)).variant(state, number)['LevelId']
    # Old native saves predate roundVariantPicks. Preserve the same deterministic
    # template resolution used by native_combat's preparation projection.
    from .native_combat import round_boss_id
    choices = catalog.rounds(state['modeId'])[number]
    matching = [row for row in choices if row.get('BossId') == round_boss_id(catalog, state, number)]
    return (matching or choices)[0]['LevelId']


def tile(catalog, state, row, col):
    return catalog.level_tile(level_id(catalog, state), row, col)
