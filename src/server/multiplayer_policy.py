# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Draft weights/cards and reinforcement schedules adapted from
# Stronghold-Protocol data/{config,choices}.json at
# c24e17eec3af1e6dedfa9935a6d15245cf2f4586. See the existing third_party notices.
"""Versioned room policy, using CN effects and the proven solo effect handlers.

The official unpublished random weights remain explicit private policy. CN
IsSpPrepare, native effect definitions, and native DTOs stay authoritative.
Room ownership, turn queues, deadlines and shared choices belong to the room
coordinator; this module never makes one player's Ready advance the room.
"""
import copy
import hashlib
import json

from .catalog import draw, weighted
from .setup_rules import stream
from .special_choice_policy import POLICY as DRAFT_POLICY
from .special_choices import FAMILIES, GATE_KEYS, SpecialChoices

VERSION = 1
MODES = ('mode_multi_funny', 'mode_multi_normal', 'mode_multi_hard', 'mode_multi_abyss')
SOURCE_COMMIT = 'c24e17eec3af1e6dedfa9935a6d15245cf2f4586'
# Exact cards.tactic[].team flags from this pinned choices.json. `ally` kind
# and the allybuff prefix also include PERSONAL cards and are not team gates.
TEAM_TACTICS = frozenset(['allybuff_select_1', *('allybuff_select_2_' + str(i) for i in range(1, 9)),
    *('allybuff_select_' + str(i) for i in (3, 4, 11, 13, 14, 18, 19))])
ATTACK_STACKS = {
    'mode_multi_funny': (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1),
    'mode_multi_normal': (0, 1, 1, 2, 2, 2, 2, 3, 4, 4, 5, 6, 7, 7, 7),
    'mode_multi_hard': (1, 2, 2, 3, 3, 3, 3, 3, 4, 5, 6, 6, 7, 8, 8),
    'mode_multi_abyss': (1, 2, 2, 3, 3, 4, 5, 5, 5, 5, 6, 6, 7, 8, 8),
}
ABYSS_HP_STACKS = (1, 2, 2, 3, 4, 4, 7, 8, 8, 8, 9, 10, 10, 10, 10)
ABYSS_HP_SUPPLY_ROUNDS = (6, 13, 14, 15)
NORMAL_DISPLAY_SECONDS = (45, 45, 55, 55, 55, 70, 85, 85, 95, 95, 115, 115, 115)
_SPECIAL_ROUNDS = {'mode_multi_funny': (3, 9), 'mode_multi_normal': (3, 6, 9),
                   'mode_multi_hard': (3, 9, 11), 'mode_multi_abyss': (3, 9, 11)}


def _check(state, round_id=None):
    from .engine import DomainError, integer
    mode = state.get('modeId')
    if mode not in MODES:
        raise DomainError('multiplayer_mode_required')
    if round_id is None:
        number = integer(state.get('round', 0), 'round', 0, len(ATTACK_STACKS[mode]))
        number = max(1, number)
    else:
        number = round_id
    integer(number, 'round', 1, len(ATTACK_STACKS[mode]))
    return mode, number


def strengths(state, round_id=None):
    """Keep attack and HP exponents separate; boss server HP is independent."""
    mode, number = _check(state, round_id)
    attack = ATTACK_STACKS[mode][number - 1]
    hp = ABYSS_HP_STACKS[number - 1] if mode == 'mode_multi_abyss' else attack
    base = .8 if mode in MODES[:2] else 1.0
    supplement = 1.08 if mode == 'mode_multi_abyss' and number in ABYSS_HP_SUPPLY_ROUNDS else 1.0
    ids = (['aceffect_enemy_4'] if base != 1 else []) + ['aceffect_enemy_3'] * attack
    if mode == 'mode_multi_abyss' and number >= 3:
        ids.append('aceffect_enemy_1')
    # These native HP-only packs retain the original exclusion of hit-count
    # enemies. Never synthesize the correction with the coupled ATK/HP pack.
    ids += ['aceffect_enemy_2_2'] * (hp - attack)
    if supplement != 1:
        ids.append('aceffect_enemy_2')
    correction = 1.2 ** (hp - attack) * supplement
    return {'version': VERSION, 'sourcePolicy': 'cn-multi-native-effects-reference-schedule-v1',
            'sourceCommit': SOURCE_COMMIT, 'attackExponent': attack, 'hpExponent': hp,
            'attackMultiplier': base * 1.1 ** attack,
            'maxHpMultiplier': base * 1.2 ** hp * supplement,
            'moveSpeedMultiplier': 1.15 if mode == 'mode_multi_abyss' and number >= 3 else 1.0,
            'hpSupplementMultiplier': supplement, 'nativeEffectIds': ids,
            'nativeAdditionalHpMultiplier': 1.0, 'nativeHpOnlyPackMultiplier': correction,
            'nativeProjectionComplete': True,
            'bossServerHpUnaffected': True, 'nativeEffectSemanticsRetained': True}


def project(state):
    if state.get('multiplayerPolicyVersion') != VERSION:
        return
    value = strengths(state)
    state['modeStrengthVersion'] = VERSION
    state['nativeModeEffectIds'] = value['nativeEffectIds'][:]
    state['multiplayerModeStrength'] = value
    # Complete means the explicit pack list covers this policy; native adapter
    # execution and Unity runtime acceptance have their own independent gates.
    state['multiplayerNativeProjectionComplete'] = value['nativeProjectionComplete']


def special_rounds(catalog, mode):
    _check({'modeId': mode, 'round': 1})
    return sorted(number for number, rows in catalog.rounds(mode).items()
                  if any(row['IsSpPrepare'] for row in rows))


def timers(catalog, mode, round_id):
    _check({'modeId': mode, 'round': round_id})
    from .engine import integer
    turn = catalog.turns(mode)[round_id]
    steps = {row['StepType']: row['Time'] for row in catalog.common['EnterStepList']}
    return {'infoCheckSeconds': steps[1], 'strategyPerSeatSeconds': steps[2],
            'protocolStartSeconds': steps[3], 'roundStartSeconds': 2,
            'specialFirstSeatSeconds': 30,
            'specialNextSeatSeconds': catalog.mode(mode)['SpecialPhaseTime'],
            'preparationSeconds': integer(turn['NormalPhaseTime'], 'preparationSeconds', 1, 86400),
            'battleDisplaySeconds': 120 if turn['IsBossTurn'] else NORMAL_DISPLAY_SECONDS[round_id - 1],
            'battleSimulationSeconds': 240 if turn['IsBossTurn'] else NORMAL_DISPLAY_SECONDS[round_id - 1] * 2,
            'helpHasFreshFullBudget': True, 'bossCountdownDoesNotForceFinish': True}


def draft_schedule(state):
    """Source-pinned six-card room draft; never substitute a solo `_s` event."""
    mode, number = _check(state)
    if number not in _SPECIAL_ROUNDS[mode]:
        return None
    if mode == 'mode_multi_funny':
        families = [('supply', 45), ('tactic', 35), ('shop', 20)]
    elif mode == 'mode_multi_normal':
        families = [('bounty', 50), ('supply', 25), ('shop', 10), ('tactic', 15)]
    else:
        families = [('bounty', 100)] if number != 11 else [('bounty', 14), ('shop', 4), ('tactic', 4)]
    bounty = 'initial' if number < 8 else 'boss' if number < 11 else 'hunter'
    prefix, count = {'initial': ('enemy_initial_', 10), 'boss': ('bossInitial_', 6),
                     'hunter': ('bounty_hunter_', 15)}[bounty]
    tactic = ['buff_select_' + str(i) for i in range(1, 5)]
    if mode in MODES[2:]:
        tactic += ['hardbuff_select_' + str(i) for i in range(1, 4)]
    tactic += ['terrain_' + suffix for suffix in ('m01_1', 'm02_1', 'm02_2', 'm03_1', 'm04_1', 'm06_1', 'm07_1')]
    return {'families': [{'family': name, 'weight': weight} for name, weight in families],
            'cards': 6, 'supplyTiers': list({3: (1, 4), 6: (2, 5), 9: (3, 6), 11: (4, 6)}[number]),
            'bountyDraft': bounty, 'assumed': not (mode in MODES[2:] and number in (3, 9)),
            'events': {'bounty': [prefix + str(i) for i in range(1, count + 1)],
                       'supply': ['equip_free_' + str(i) for i in range(1, 5)],
                       'shop': ['artifact_paid_' + str(i) for i in range(1, 6)], 'tactic': tactic}}


class _RoomDraft(SpecialChoices):
    """Reuse eligibility, finite-stock, bounty and native map-alias checks."""
    def draft_schedule(self, state):
        return draft_schedule(state)

    def _events(self, state, family, schedule):
        effect_type = {'supply': 5, 'shop': 5, 'bounty': 9, 'tactic': 10}[family]
        result = []
        for key in schedule['events'].get(family, []):
            row = self.c.season['EffectChoiceInfoDict'].get(key)
            if not row or key.endswith(('_s', '_tr')) or row['EffectType'] != effect_type or row['ChoiceType'] != FAMILIES[effect_type]:
                continue
            if key.startswith('terrain_') and not self._terrain_available(state, 'map_' + key[len('terrain_'):]):
                continue
            result.append(key)
        return sorted(result)

    def generate(self, state):
        schedule = self.draft_schedule(state)
        if schedule is None:
            self.fail('missing_multiplayer_special_schedule', str(state['round']))
        pools = {name: self.draft_candidates(state, name, schedule) for name in ('supply', 'shop', 'bounty', 'tactic')}
        families = [(row['family'], row['weight']) for row in schedule['families']
                    if row['weight'] > 0 and pools[row['family']] and self._events(state, row['family'], schedule)]
        if not families:
            families = [(name, 1) for name in pools if pools[name] and self._events(state, name, schedule)]
        if not families:
            self.fail('special_choice_pool_exhausted')
        rng = stream(state['stageSeed'], 'multiplayer:special:draft:v1:' + str(state['round']))
        family = weighted(rng, families); count = schedule['cards']; source = {}
        if family == 'bounty':
            options, source = self._bounty_cards(state, rng, schedule, count)
        elif family == 'shop':
            options = self._shop_cards(state, rng, count)
        elif family == 'supply':
            options = [draw(rng, pools[family]) for _ in range(count)]
        else:
            spec = DRAFT_POLICY['tacticDraft']; weights = spec['weights'] if state['round'] in spec['rounds'] else {}
            options = [weighted(rng, [(key, weights.get(key, 1)) for key in pools[family]]) for _ in range(count)]
        if not options:
            self.fail('special_choice_pool_exhausted', family)
        event = draw(rng, self._events(state, family, schedule))
        effect_type = {'supply': 5, 'shop': 5, 'bounty': 9, 'tactic': 10}[family]
        state['specialChoiceVersion'] = 2
        state['specialDraft'] = {'version': 2, 'multiplayerPolicyVersion': VERSION,
            'round': state['round'], 'choiceType': FAMILIES[effect_type], 'effectType': effect_type,
            'family': family, 'stageId': state['stageId'], 'sourceCommit': SOURCE_COMMIT, 'source': source,
            'privateWeightsAssumed': schedule['assumed'], 'candidateIds': pools[family],
            'eventCandidates': self._events(state, family, schedule),
            'candidateHash': hashlib.sha256(json.dumps({'cards': pools[family], 'events': self._events(state, family, schedule)},
                sort_keys=True, separators=(',', ':')).encode()).hexdigest(),
            'slots': [{'unitId': key, 'slotId': index, 'price': 0} for index, key in enumerate(options)]}
        state['player']['selfChoice'] = {'id': event, 'options': options}
        state['phase'] = 'special'


def shared_draft(engine, states):
    """One six-slot draft eligible on every seat, including differing maps.

    Inventory/finite-stock and terrain aliases are intersected before drawing.
    No room mutation happens until generation succeeds. The coordinator owns
    taken-slot exclusion and persists this returned draft for the whole queue.
    """
    from .engine import DomainError
    if not isinstance(states, list) or not states:
        raise DomainError('multiplayer_special_players_required')
    mode, number = _check(states[0])
    if any(_check(state) != (mode, number) for state in states):
        raise DomainError('multiplayer_special_round_mismatch')

    class Shared(_RoomDraft):
        def draft_candidates(self, state, family, schedule=None):
            pools = [set(super(Shared, self).draft_candidates(peer, family, schedule)) for peer in states]
            return sorted(set.intersection(*pools))

        def _events(self, state, family, schedule):
            pools = [set(super(Shared, self)._events(peer, family, schedule)) for peer in states]
            return sorted(set.intersection(*pools))

    selected = copy.deepcopy(states[0])
    Shared(engine).generate(selected)
    draft = selected['specialDraft']
    draft['sharedStageIds'] = sorted({state['stageId'] for state in states})
    for state in states:
        state['specialChoiceVersion'] = selected['specialChoiceVersion']
        state['specialDraft'] = copy.deepcopy(draft)
        state['player']['selfChoice'] = copy.deepcopy(selected['player']['selfChoice'])
        state['phase'] = 'special'
    return copy.deepcopy(draft)


def apply_team_tactic(engine, peers, *, key, event_id, picker_seat):
    """Use the proven card handler while retaining each recipient's own turn.

    The temporary selection envelope exists only inside the room transaction.
    Rewards/effects survive; the peer's draft, UI selection and prior personal
    history do not become a second consumed choice.
    """
    if key not in TEAM_TACTICS:
        return
    for state in peers:
        saved = {name: copy.deepcopy(state[name]) for name in ('phase', 'specialDraft', 'lastSpecialChoice', 'specialChoiceHistory') if name in state}
        choice = copy.deepcopy(state['player'].get('selfChoice'))
        event = engine.c.season['EffectChoiceInfoDict'][event_id]
        state.update(phase='special', specialDraft={'version': 2, 'round': state['round'],
            'effectType': event['EffectType'], 'choiceType': event['ChoiceType'],
            'slots': [{'unitId': key, 'slotId': 0, 'price': 0}]})
        state['player']['selfChoice'] = {'id': event_id, 'options': [key]}
        engine.apply(state, 'choice.pick', {'slotId': 0})
        for name in ('phase', 'specialDraft', 'lastSpecialChoice', 'specialChoiceHistory'):
            if name in saved: state[name] = saved[name]
            else: state.pop(name, None)
        state['player']['selfChoice'] = choice
        state['history'][-1]['op'] = 'multiplayer.team-choice'
        state.setdefault('multiplayerTeamEffectHistory', []).append({'round': state['round'], 'effectId': key, 'pickerSeat': picker_seat})


def initialize(engine, state):
    _check(state)
    if state.get('multiplayerPolicyVersion') == VERSION:
        return
    state['multiplayerPolicyVersion'] = VERSION
    state['multiplayerRules'] = {'version': VERSION, 'hiddenActivatedLayersExclusive': 1200,
        'hiddenTeamHpExclusive': 1, 'specialCards': 6, 'matchingEnabled': False,
        'nativeProjection': 'per-seat-CN-resources; common-mode-CN-packs-v1'}
    # The COMMON marker shares the version number, while multiplayerPolicyVersion
    # and the mode_multi ID keep its effect list outside the LOCAL adapter.
    project(state)


def round_start(engine, state):
    initialize(engine, state)
    project(state)
    old = state.get('specialDraft', {})
    if state['phase'] == 'special' and old.get('multiplayerPolicyVersion') == VERSION and old.get('round') == state['round']:
        return
    state.pop('specialDraft', None)
    for ref in state['specialEffects']:
        if any(row['Key'] in GATE_KEYS - {'global_special_choice_all_activated'} for row in engine.c.effects(ref['effectId'])):
            ref['affectedUidIndex'] = []
    if state['round'] in special_rounds(engine.c, state['modeId']):
        _RoomDraft(engine).generate(state)
