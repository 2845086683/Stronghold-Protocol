# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Round schedules adapted from Stronghold-Protocol data/config.json at
# cf7a9400270d5aa79b2e601e405dab782e25f120 (unchanged at 488db49f11daf862b67ce26fa4cf89aa6cba2466).
# See ../third_party/stronghold-protocol-reference/ for licence and notices.
"""Private difficulty policy, executed with existing CN native Rune effects.

These are not new EffectInfo IDs and are never appended to the UI special-
effect DTO. Each native Rune rebuild requests fresh packed data for every ID,
including repeated reinforcement stacks, without modifying shared databases.
The reference schedule is explicit private-server policy, not an assertion
that the official server selected these exact rounds.
"""
VERSION = 1
BASE_EFFECTS = {'mode_single_funny': 'aceffect_enemy_5',
                'mode_single_normal': 'aceffect_enemy_5',
                'mode_single_hard': 'aceffect_enemy_4',
                'mode_single_abyss': None}
REINFORCEMENTS = {
    'mode_single_funny': (0,)*9,
    'mode_single_normal': (0,0,0,1,1,1,1,2,2,2,3,4,5,5,5),
    'mode_single_hard': (0,0,0,0,1,1,1,1,2,2,3,3,4,4,4),
    'mode_single_abyss': (1,2,2,2,2,3,3,3,3,4,5,5,6,7,7),
}
ALLOWED_EFFECTS = frozenset(('aceffect_enemy_1','aceffect_enemy_3','aceffect_enemy_4','aceffect_enemy_5'))


def effect_ids(state, round_id=None):
    if state.get('modeStrengthVersion') != VERSION:
        return []
    mode = state['modeId']
    number = max(1, state['round']) if round_id is None else round_id
    schedule = REINFORCEMENTS.get(mode)
    if schedule is None:
        return []
    if type(number) is not int or not 1 <= number <= len(schedule):
        raise ValueError('Invalid mode-strength round')
    base = BASE_EFFECTS[mode]
    result = [base] if base else []
    result += ['aceffect_enemy_3'] * schedule[number-1]
    if mode == 'mode_single_abyss' and number >= 3:
        result.append('aceffect_enemy_1')
    return result


def project(state):
    """Persist the preparation forecast so native Ready can preflight packs."""
    if state.get('modeStrengthVersion') == VERSION:
        state['nativeModeEffectIds'] = effect_ids(state)


def preserve_legacy_shape(state):
    """Keep pre-policy saves/results free of newly introduced mode metadata.

    The engine also serves non-native domain callers. The native adapter owns
    its compatibility envelope and removes only these two optional fields for
    a run that never opted into the versioned strength policy.
    """
    if state.get('modeStrengthVersion') == VERSION:
        return
    state.pop('nativeModeEffectIds', None)
    containers = [state.get('battle'), *state.get('completedReports', {}).values()]
    for container in containers:
        if isinstance(container, dict):
            container.pop('nativeModeEffectIds', None)
            container.pop('nativeModeStrength', None)


def describe(catalog, state):
    attack = hp = speed = 1.0
    for effect in effect_ids(state):
        for row in catalog.effects(effect):
            bb = row['Blackboard'] or {}
            if row['Key'] == 'enemy_attribute_mul':
                attack *= bb.get('atk', 1)
                hp *= bb.get('max_hp', 1)
            elif row['Key'] == 'env_gbuff_new' and bb.get('key') == 'enemy_move_speed_mul':
                speed *= 1 + bb.get('move_speed', 0)
    return {'version': VERSION if state.get('modeStrengthVersion')==VERSION else 0,
            'sourcePolicy': 'cn-native-effects-reference-round-schedule-v1' if state.get('modeStrengthVersion')==VERSION else 'legacy-unscaled',
            'attackMultiplier': attack, 'maxHpMultiplier': hp, 'moveSpeedMultiplier': speed,
            'nativeEffectSemanticsRetained': True}
