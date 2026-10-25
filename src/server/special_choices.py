# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Effect lifecycle adapted from Stronghold-Protocol server/match/choices.js,
# server/sim/content/choices.js and server/match/builtinMeta.js at
# cf7a9400270d5aa79b2e601e405dab782e25f120. See third_party reference notices.
# Round pools/group drafts additionally adapted from server/match/choices.js
# at c24e17eec3af1e6dedfa9935a6d15245cf2f4586; native mechanics stay CN.
"""CN/TC three-family SP drafts, with explicitly private selection policy.

IsSpPrepare is authoritative. Legacy setup keeps the seeded no-repeat family
cycle. Official unpublished pools/weights use the documented source-pinned
private policy; native battle values remain CN runes.
Setup v2 uses round-specific source-pinned event/card pools. Terrain only
references aliases present in this map; the original native executor controls
predefined visibility. This module never invents buildable tiles or devices.
"""
import hashlib
import json
from collections import Counter
from .catalog import draw, weighted
from .native_enemy_keys import CN_VERIFIED_ENEMY_KEYS
from .preset_snapshot import base_bonds
from .setup_rules import stream, shuffle
from .special_choice_policy import POLICY as DRAFT_POLICY

VERSION = 2
FAMILIES = {5: 0, 9: 2, 10: 3}
PREP_KEYS = frozenset(('global_special_choice_gain_coin', 'global_special_choice_gain_equip',
    'global_special_choice_refresh_free', 'global_special_choice_bond_addlayer',
    'single_special_choice_gloden_equip_chess', 'single_special_choice_gloden_char_chess',
    'single_special_choice_gain_bond_chess'))
GATE_KEYS = frozenset(('global_special_choice_all_activated',
    'global_special_choice_prep_finish_bench_at_least', 'global_special_choice_prep_finish_bench_at_most',
    'global_special_choice_prep_finish_same_row_at_least'))
NATIVE_KEYS = frozenset(('env_gbuff_new', 'env_gbuff_new_with_verify', 'char_respawntime_mul',
    'enemy_attribute_mul', 'enemy_attribute_add', 'auto_chess_change_map'))
GLOBAL_BUFFS = frozenset(('act1autochess_debuff_3', 'act1autochess_debuff_9',
    'halfIdle_magic_resist_penetration', 'halfIdle_def_penetration'))
BOUNTY_KEYS = ('add_enemy_kill_gain_coin', 'add_enemy_selfbattle_win_gain_coin', 'next_battle_add_enemy_win_gain_coin')


def special_rounds(catalog, mode):
    """Target 下半 version overlay; preserve the extracted CN/TC catalogue.

    PRTS 卫戍协议：盟约_下半 gives 绝境 after R3 (start R4).
    Pinned CN/TC act2 instead marks R3. The user's chosen target takes priority;
    all other flags remain their extracted values.
    """
    result = [number for number, rows in catalog.rounds(mode).items() if any(row['IsSpPrepare'] for row in rows)]
    return [4 if number == 3 else number for number in result] if mode == 'mode_single_hard' else result


class SpecialChoices:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c

    @staticmethod
    def fail(code, detail=''):
        from .engine import DomainError
        raise DomainError(code, detail)

    def _roll(self, state, pairs):
        """Reward RNG is persisted separately from ordinary shop/trait draws."""
        if not pairs:
            return None
        rng = {'rngState': state.get('specialRewardRngState', stream(state['stageSeed'], 'special:rewards:v1')['rngState'])}
        result = weighted(rng, pairs)
        state['specialRewardRngState'] = rng['rngState']
        return result

    def _items(self, state, tier=None):
        return [key for key, row in self.c.season['TrapShopChessDatas'].items()
                if self.e.pool.shop_item(key) and (tier is None or row['ItemLevel'] == tier)
                and self.e.pool.available(state, key)]

    def _chars(self, state, bond):
        values = [key for key in state['chessPool'] if bond in base_bonds(self.c, state, key)
                  and self.e.pool.available(state, key)]
        low = [key for key in values if self.c.season['CharShopChessDatas'][key]['ChessLevel'] <= state['player']['store']['level']]
        return low or values

    def _live_bond(self, state, bond):
        return (bond in self.c.season['BondInfoDict'] and bond not in state.get('bannedBondIds', [])
                and bond not in state.get('inactiveBondIds', [])
                and any(bond in base_bonds(self.c, state, key) and self.e.setup.eligible(state, key)
                        for key in state['chessPool']))

    def tactic_available(self, state, key):
        rows = self.c.effects(key)
        if not rows or {row['Key'] for row in rows} - (PREP_KEYS | GATE_KEYS | NATIVE_KEYS):
            return False
        for row in rows:
            kind = row['Key']; bb = row['Blackboard'] or {}
            if kind == 'auto_chess_change_map' and not self._terrain_available(state, key):
                return False
            if kind in ('env_gbuff_new', 'env_gbuff_new_with_verify') and bb.get('key') not in GLOBAL_BUFFS:
                return False
            if kind == 'global_special_choice_bond_addlayer' and not any(self._live_bond(state, bond.strip()) for bond in str(bb.get('bond_list', '')).split(',')):
                return False
            if kind == 'single_special_choice_gain_bond_chess' and not (self._live_bond(state, bb.get('bond')) and self._chars(state, bb.get('bond'))):
                return False
            if kind == 'global_special_choice_gain_equip' and (bb.get('pool') != 'pool_equip_shop_1' or not self._items(state, 1)):
                return False
        return True

    def _terrain_available(self, state, key):
        """Require an actual visibility change on the current authored stage."""
        card = DRAFT_POLICY['tacticCards'].get(key, {})
        if card.get('kind') != 'terrain' or card.get('stageId') != state.get('stageId'):
            return False
        from .board_terrain import level_id
        predefined = self.c.level(level_id(self.c, state)).get('Predefines') or {}
        shown = {obj['Alias']: not obj.get('Hidden', False)
                 for field in ('CharacterInsts', 'TokenInsts')
                 for obj in predefined.get(field) or [] if obj.get('Alias')}
        rows = self.c.effects(key)
        if not (bool(rows) and all(row['Key'] == 'auto_chess_change_map' and row.get('Blackboard')
            and set(row['Blackboard']) <= shown.keys()
            and all(type(value) in (int, float) and value in (0, 1) for value in row['Blackboard'].values())
            for row in rows)):
            return False

        def apply(effects, visibility):
            # CN _RefreshPredefineShown: 1 shows, 0 hides; mode-only
            # aliases are restricted even when explicitly enabled.
            multi = state['modeId'].startswith('mode_multi_')
            for effect in effects:
                if effect['Key'] != 'auto_chess_change_map':
                    continue
                for alias, value in (effect.get('Blackboard') or {}).items():
                    if alias in visibility and type(value) in (int, float) and value in (0, 1):
                        visibility[alias] = bool(value) and not (
                            alias.endswith('single_only') and multi or
                            alias.endswith('multi_only') and not multi)

        # Per-player domain states use local uidIndex 0; room projection
        # remaps it later. Disabled/other-player effects must not create
        # phantom fortifications. Preserve native effect application order.
        for ref in state.get('specialEffects', []):
            targets = ref.get('affectedUidIndex', [])
            if targets == 0 or isinstance(targets, list) and 0 in targets:
                apply(self.c.effects(ref['effectId']), shown)
        changed = dict(shown)
        apply(rows, changed)
        return changed != shown

    def draft_schedule(self, state):
        # Keep the user's accepted 下半 hard R4 overlay. Upstream marks R3;
        # changing draft contents must not silently change the chosen version.
        number = 3 if state['modeId'] == 'mode_single_hard' and state['round'] == 4 else state['round']
        return DRAFT_POLICY['schedule'].get(state['modeId'], {}).get(str(number))

    def draft_candidates(self, state, family, schedule=None):
        """Complete eligibility set, independent of random draft samples."""
        schedule = schedule or self.draft_schedule(state) or {}
        if family in ('supply', 'shop'):
            lo, hi = schedule.get('supplyTiers', (1, 6)) if family == 'supply' else (1, 6)
            return sorted(key for key in self._items(state)
                if lo <= self.c.season['TrapShopChessDatas'][key]['ItemLevel'] <= hi)
        if family == 'bounty':
            kind = schedule.get('bountyDraft', 'initial' if state['round'] < 8 else 'boss' if state['round'] < 11 else 'hunter')
            return sorted(key for key, row in DRAFT_POLICY['bountyCards'].items()
                if row.get('draft') and row.get('draftPool') == kind and self._bounty(key))
        if family == 'tactic':
            values = [key for key in DRAFT_POLICY['tacticCards'] if self.tactic_available(state, key)]
            spec = DRAFT_POLICY['tacticDraft']
            if state['round'] in spec.get('rounds', []):
                values = [key for key in values if DRAFT_POLICY['tacticCards'][key]['kind'] in spec['kinds']]
            return sorted(values)
        return []

    @staticmethod
    def _distinct(rng, pairs, count):
        pool = list(pairs); result = []
        while pool and len(result) < count:
            key = weighted(rng, pool); result.append(key)
            pool = [(value, weight) for value, weight in pool if value != key]
        return result

    def _bounty_cards(self, state, rng, schedule, count):
        kind = schedule['bountyDraft']; spec = DRAFT_POLICY['bountyDrafts'][kind]
        eligible = self.draft_candidates(state, 'bounty', schedule); allowed = set(eligible)
        groups = spec['groups']
        if spec['pick'] == 'seen':
            index = weighted(rng, [(i, max(1, len(group.get('seen', [])))) for i, group in enumerate(groups)])
        else:
            index = draw(rng, list(range(max(len(groups), spec.get('slots', 0)))))
        group = groups[index] if index < len(groups) else None
        rule = spec.get('rule', {}); cards = DRAFT_POLICY['bountyCards']; pairs = []
        if group:
            weights = group.get('weights', [])
            pairs = [(key, weights[i] if i < len(weights) else 1)
                     for i, key in enumerate(group['cards']) if key in allowed]
        selected = [key for key, _ in pairs]
        if kind == 'initial' and not group:
            tiers = shuffle(rng, rule['tiers'])
            preferred = set(rule.get('prefer', [])); pref_tiers = {cards[key]['tier'] for key in eligible if key in preferred}
            for tier in [t for t in tiers if t in pref_tiers] + [t for t in tiers if t not in pref_tiers]:
                candidates = [key for key in eligible if key not in selected and cards[key]['tier'] == tier
                    and cards[key]['series'] in rule['series']
                    and sum(cards[old]['series'] == cards[key]['series'] for old in selected) < rule.get('perSeries', 1)]
                prefer = [key for key in candidates if key in preferred]
                if candidates: selected.append(draw(rng, prefer or candidates))
            pairs = [(key, 1) for key in selected]
        elif kind == 'hunter':
            size = len(selected) + group.get('open', 0) if group else rule.get('size', 7)
            giants = [key for key in rule.get('giants', []) if key in allowed]
            if len(selected) < size and not set(selected) & set(giants) and giants:
                selected.append(draw(rng, [key for key in giants if key not in selected]))
            for key in shuffle(rng, [key for key in rule.get('cards', []) if key in allowed]):
                if len(selected) >= size: break
                series = cards[key]['series']
                if key in selected or (series in rule.get('onePerSeries', []) and
                    any(cards[old]['series'] == series for old in selected)): continue
                if series == 16 and sum(cards[old]['series'] == 16 and old not in giants for old in selected) >= rule.get('maxSeries16', 2): continue
                selected.append(key)
            known = dict(pairs); pairs = [(key, known.get(key, 1)) for key in selected]
        options = self._distinct(rng, pairs, spec.get('count', 6))
        # A source card unavailable in the current CN build must not strand UI;
        # fill only from this exact round's eligible family, never another tier.
        if len(options) < count:
            options += shuffle(rng, [key for key in eligible if key not in options])[:count-len(options)]
        return shuffle(rng, options)[:count], {'kind': kind, 'group': index, 'groupCards': selected}

    def _shop_cards(self, state, rng, count):
        spec = DRAFT_POLICY['shopDraft']; eligible = self.draft_candidates(state, 'shop')
        if state['round'] not in spec.get('rounds', []):
            return [draw(rng, eligible) for _ in range(count)] if eligible else []
        options = []; weights = spec.get('itemWeights', {})
        for slot in spec['slots']:
            kinds = [(key, weight) for key, weight in slot.items()
                     if key != 'coin' or spec['coin'] in eligible]
            if not kinds:
                if eligible: options.append(draw(rng, eligible))
                continue
            kind = weighted(rng, kinds)
            if kind == 'coin': values = [spec['coin']]
            else:
                values = []
                for tier in range(int(kind), 0, -1):
                    values = [key for key in eligible if self.c.season['TrapShopChessDatas'][key]['ItemLevel'] == tier]
                    if values: break
                values = values or eligible
            if values: options.append(weighted(rng, [(key, weights.get(key, 1)) for key in values]))
        return shuffle(rng, options)[:count]

    def _events(self, state, family, schedule):
        effect_type = {'supply':5, 'shop':5, 'bounty':9, 'tactic':10}[family]
        result = []
        for key in schedule.get('events', {}).get(family, []):
            row = self.c.season['EffectChoiceInfoDict'].get(key)
            if not row or not key.endswith('_s') or row['EffectType'] != effect_type or row['ChoiceType'] != FAMILIES[effect_type]: continue
            if key.startswith('terrain_'):
                # Event titles are the same native BUFF_GAIN UI. Keep source
                # event choice scoped to this map as well as the actual cards.
                suffix = key[len('terrain_'):].rsplit('_s', 1)[0]
                if not self._terrain_available(state, 'map_' + suffix): continue
            result.append(key)
        return sorted(result)

    def _round_start_v2(self, state):
        schedule = self.draft_schedule(state)
        if not schedule: self.fail('missing_special_choice_schedule', state['modeId'] + ':' + str(state['round']))
        pools = {name:self.draft_candidates(state, name, schedule) for name in ('supply','shop','bounty','tactic')}
        families = [(row['family'], row['weight']) for row in schedule['families']
                    if row['weight'] > 0 and pools[row['family']] and self._events(state, row['family'], schedule)]
        # Exhausted finite items cannot strand a run. Preserve a valid native
        # family/event fallback, while recording its exact eligibility set.
        if not families:
            families = [(name, 1) for name in ('supply','shop','tactic','bounty')
                        if pools[name] and self._events(state, name, schedule)]
        if not families: self.fail('special_choice_pool_exhausted')
        rng = stream(state['stageSeed'], 'special:draft:v2:' + str(state['round']))
        family = weighted(rng, families); count = min(3, max(1, int(schedule['cards'])))
        source = {}
        if family == 'bounty': options, source = self._bounty_cards(state, rng, schedule, count)
        elif family == 'shop': options = self._shop_cards(state, rng, count)
        elif family == 'supply': options = [draw(rng, pools[family]) for _ in range(count)]
        else:
            spec = DRAFT_POLICY['tacticDraft']; weights = spec['weights'] if state['round'] in spec['rounds'] else {}
            options = [weighted(rng, [(key, weights.get(key, 1)) for key in pools[family]]) for _ in range(count)]
        if not options: self.fail('special_choice_pool_exhausted', family)
        event = draw(rng, self._events(state, family, schedule))
        effect_type = {'supply':5, 'shop':5, 'bounty':9, 'tactic':10}[family]
        state['specialChoiceVersion'] = VERSION
        state['specialDraft'] = {'version':VERSION, 'round':state['round'], 'choiceType':FAMILIES[effect_type],
            'effectType':effect_type, 'family':family, 'stageId':state['stageId'],
            'sourceCommit':DRAFT_POLICY['sourceCommit'], 'source':source,
            'candidateIds':pools[family], 'eventCandidates':self._events(state, family, schedule),
            'candidateHash':hashlib.sha256(json.dumps({'cards':pools[family], 'events':self._events(state, family, schedule)},
                sort_keys=True,separators=(',', ':')).encode()).hexdigest(),
            'slots':[{'unitId':key,'slotId':index,'price':0} for index,key in enumerate(options)]}
        state['player']['selfChoice'] = {'id':event, 'options':options}; state['phase'] = 'special'

    def _bounty(self, key):
        rows = [row for row in self.c.effects(key) if row['Key'] in BOUNTY_KEYS]
        if not rows:
            return None
        row = min(rows, key=lambda item: BOUNTY_KEYS.index(item['Key']))
        bb = row['Blackboard'] or {}
        return row if bb.get('enemy_id') in CN_VERIFIED_ENEMY_KEYS else None

    def candidates(self, state, family):
        if family == 5:
            return sorted(self._items(state))
        return sorted(key for key, row in self.c.season['EffectInfoDataDict'].items()
            if row['EffectType'] == family and (self._bounty(key) if family == 9 else self.tactic_available(state, key)))

    def _used_families(self, state):
        records = list(state.get('specialChoiceHistory', []))
        last = state.get('lastSpecialChoice')
        if last and not any(row.get('round') == last.get('round') for row in records):
            records.append(last)
        used = []
        for row in sorted(records, key=lambda item: item.get('round', 0)):
            if row.get('round', 0) >= state['round']:
                continue
            key = row.get('effectId', row.get('unitId'))
            family = row.get('effectType') or self.c.season['EffectInfoDataDict'].get(key, {}).get('EffectType')
            if family is None and key in self.c.season['TrapShopChessDatas']:
                family = 5
            if family not in FAMILIES:
                continue
            if len(set(used)) == len(FAMILIES):
                used = []
            if family not in used:
                used.append(family)
        return used

    def round_start(self, state):
        if (state.get('setupVersion', 0) >= 2 and state['phase'] == 'special'
            and state.get('specialDraft', {}).get('version') == VERSION
            and state['specialDraft']['round'] == state['round']):
            return
        state.pop('specialDraft', None)
        for ref in state['specialEffects']:
            if any(row['Key'] in GATE_KEYS - {'global_special_choice_all_activated'} for row in self.c.effects(ref['effectId'])):
                ref['affectedUidIndex'] = []
        if state['round'] not in special_rounds(self.c, state['modeId']):
            return
        if state.get('setupVersion', 0) >= 2 and state['modeId'] in DRAFT_POLICY['schedule']:
            return self._round_start_v2(state)
        pools = {family: self.candidates(state, family) for family in FAMILIES}
        # A fully exhausted item pool must not strand the round on an empty UI.
        available = [family for family in FAMILIES if pools[family]]
        fresh = [family for family in available if family not in self._used_families(state)]
        rng = stream(state['stageSeed'], 'special:draft:v1:' + str(state['round']))
        family = draw(rng, fresh or available)
        events = sorted(key for key, row in self.c.season['EffectChoiceInfoDict'].items()
                        if row['ChoiceType'] == FAMILIES[family] and row['EffectType'] == family and not key.endswith('_tr'))
        event = draw(rng, events)
        pool = pools[family][:]; options = []
        while pool and len(options) < 3:
            key = draw(rng, pool); pool.remove(key); options.append(key)
        state['specialChoiceVersion'] = 1
        state['specialDraft'] = {'version': 1, 'round': state['round'], 'choiceType': FAMILIES[family],
            'effectType': family, 'slots': [{'unitId': key, 'slotId': index, 'price': 0} for index, key in enumerate(options)]}
        state['player']['selfChoice'] = {'id': event, 'options': options}
        state['phase'] = 'special'

    def _append(self, state, key, targets=None):
        row = {'effectInstId': self.e._id(state), 'effectId': key, 'affectedUidIndex': [0] if targets is None else targets}
        state['specialEffects'].append(row)
        return row

    def _gain_random(self, state, values, count):
        for _ in range(max(0, count)):
            eligible = self.e.pool.candidates(state, values)
            pairs = [(key, self.e.pool.remaining(state, key) or 1) for key in eligible]
            chosen = self._roll(state, pairs)
            if chosen is None:
                state.setdefault('specialChoiceSkips', []).append({'round': state['round'], 'reason': 'stock_exhausted'})
                break
            self.e._gain(state, chosen, stock='strict')

    def pick(self, state, args):
        from .engine import integer
        self.e._phase(state, 'special')
        choice = state['player'].get('selfChoice')
        if not choice or not choice.get('options'):
            self.fail('missing_special_choice')
        slot = integer(args.get('slotId'), 'slotId', 0, len(choice['options']) - 1)
        key = choice['options'][slot]; event = self.c.season['EffectChoiceInfoDict'].get(choice['id'], {})
        family = event.get('EffectType')
        if family not in FAMILIES or event.get('ChoiceType') != FAMILIES[family]:
            self.fail('invalid_special_choice_event', choice['id'])
        draft = state.get('specialDraft')
        if draft and (draft['round'] != state['round'] or draft['effectType'] != family or
                      [row['unitId'] for row in draft['slots']] != choice['options']):
            self.fail('special_draft_mismatch')
        if family == 5:
            if key not in self._items(state):
                self.fail('stock_exhausted', key)
            self.e._gain(state, key, stock='strict')
        else:
            if self.c.season['EffectInfoDataDict'].get(key, {}).get('EffectType') != family:
                self.fail('invalid_special_choice_type', key)
            if family == 9:
                effect = self._bounty(key)
                if effect is None:
                    self.fail('unsupported_special_bounty', key)
                bb = effect['Blackboard'] or {}; ref = self._append(state, key)
                state.setdefault('itemBounties', []).append({'effectInstId': ref['effectInstId'], 'effectId': key,
                    'fromRound': state['round'], 'remainingBattles': max(1, int(bb.get('round', 1))),
                    'enemyId': bb['enemy_id'], 'count': max(1, int(bb.get('count', 1))),
                    'coin': max(0, int(bb.get('coin', 0))),
                    'rewardKind': 'add_enemy_kill_gain_coin' if effect['Key'] == 'add_enemy_kill_gain_coin' else 'add_enemy_selfbattle_win_gain_coin'})
                self.e.items._effect_enemies(state)
            else:
                # Reject unsupported cards in old/manually supplied saves too.
                if not self.tactic_available(state, key):
                    self.fail('choice_handler_pending', key)
                rows = self.c.effects(key)
                conditional = any(row['Key'] in GATE_KEYS - {'global_special_choice_all_activated'} for row in rows)
                ref = self._append(state, key, [] if conditional else [0])
                for row in rows:
                    kind = row['Key']; bb = row['Blackboard'] or {}; count = max(0, int(bb.get('count', 1)))
                    if kind == 'global_special_choice_gain_coin':
                        state['player']['store']['coin'] += count
                    elif kind == 'global_special_choice_refresh_free':
                        state['player']['store']['freeRefreshCnt'] += count
                    elif kind == 'global_special_choice_bond_addlayer':
                        for bond in dict.fromkeys(str(bb.get('bond_list', '')).split(',')):
                            if self._live_bond(state, bond.strip()):
                                self.e._layers(state, self.c.season['BondInfoDict'][bond.strip()]['Identifier'], count)
                    elif kind == 'global_special_choice_gain_equip':
                        self._gain_random(state, self._items(state, 1), count)
                    elif kind == 'single_special_choice_gain_bond_chess':
                        self._gain_random(state, self._chars(state, bb.get('bond')), count)
                    elif kind in ('single_special_choice_gloden_equip_chess', 'single_special_choice_gloden_char_chess'):
                        state.setdefault('specialPurchaseCharges', []).append({'effectInstId': ref['effectInstId'], 'key': kind, 'remaining': count})
        result = {'round': state['round'], 'slotId': slot, 'effectId': key, 'choiceEventId': choice['id'],
                  'effectType': family, 'choiceType': FAMILIES[family]}
        state.setdefault('specialChoiceHistory', []).append(result.copy())
        state['lastSpecialChoice'] = result
        state['player']['selfChoice'] = None; state.pop('specialDraft', None); state['phase'] = 'preparation'
        return {'choiceId': key}

    def after_purchase(self, state, obj):
        """Promotion emits elite acquisition, without a three-copy merge offer."""
        for charge in list(state.get('specialPurchaseCharges', [])):
            definition = self.e._definition(obj)
            char = obj in state['player']['charChess']
            if definition.get('IsGolden') or not definition.get('UpgradeChessId') or charge['remaining'] <= 0:
                continue
            if char != (charge['key'] == 'single_special_choice_gloden_char_chess'):
                continue
            # Unlike a true three-copy merge, this explicit char promotion may
            # overdraw stock. An item needs room for its additional second copy.
            left = self.e.pool.remaining(state, definition['ChessId'])
            if not char and left is not None and left < 1:
                continue
            obj = self.e.acquisitions.promote(state, obj); charge['remaining'] -= 1
            if charge['remaining'] <= 0:
                state['specialPurchaseCharges'].remove(charge)
                state['specialEffects'] = [row for row in state['specialEffects'] if row['effectInstId'] != charge['effectInstId']]
        self.e._bonds(state)

    def preparation_ready(self, state):
        from .hand_layout import inventory
        bench = len(inventory(self.c, state))
        owned = {obj['instId'] for obj in state['player']['charChess']}
        rows = Counter(row['position']['y'] for row in state['player']['positions'] if row['instId'] in owned)
        for ref in state['specialEffects']:
            gates = [row for row in self.c.effects(ref['effectId']) if row['Key'] in GATE_KEYS]
            if not gates:
                continue
            active = True
            for row in gates:
                kind = row['Key']; count = max(0, int((row['Blackboard'] or {}).get('count', 0)))
                if kind.endswith('bench_at_least'): active &= bench >= count
                elif kind.endswith('bench_at_most'): active &= bench <= count
                elif kind.endswith('same_row_at_least'): active &= max(rows.values(), default=0) >= count
            ref['affectedUidIndex'] = [0] if active else []
