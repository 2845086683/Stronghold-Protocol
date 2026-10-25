# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Setup/float32 replacement rules adapted from Stronghold-Protocol
# server/match/{pool,waves}.js at cf7a9400270d5aa79b2e601e405dab782e25f120;
# enemy generation rechecked at c24e17eec3af1e6dedfa9935a6d15245cf2f4586.
# Setup v2 was checked against the same rules at upstream
# c24e17eec3af1e6dedfa9935a6d15245cf2f4586; see setup-randomness-0145-provenance.json.
# CN season/common data and CN enemy attributes are authoritative. See the
# third_party/stronghold-protocol-reference notices. Original game data retains
# its original rights; no Unity simulation or inferred enemy scaling is used.
"""Persisted per-run bans and native scheduler enemy composition.

Dedicated random streams prevent preview requests and non-shop setup draws
from moving the ordinary shop RNG. Older runs without setupVersion retain
their saved template policy; new runs use the versioned deterministic policy.
"""
import copy
import hashlib
import json
import struct
from .catalog import draw, weighted
from .native_enemy_keys import CN_VERIFIED_ENEMY_KEYS

VERSION = 2
BAN_COUNTS = {0: (0, 0), 1: (0, 1), 2: (3, 4), 3: (3, 4), 4: (3, 4)}


def stream(seed, name):
    value = int.from_bytes(hashlib.sha256((str(seed) + ':' + name).encode()).digest()[:4], 'little')
    return {'rngState': value or 1}


def shuffle(rng, values):
    result = list(values)
    for index in range(len(result)-1, 0, -1):
        other = draw(rng, list(range(index+1)))
        result[index], result[other] = result[other], result[index]
    return result


def chess_bonds(catalog, state, key):
    # One CN mapping is shared by bans, owned bond counting and native squad DTOs.
    from .preset_snapshot import base_bonds
    return base_bonds(catalog, state, key)


def chess_eligible(catalog, state, key):
    if key not in catalog.chess:
        return False
    if not state.get('setupVersion'):
        return True
    off = set(state.get('bannedBondIds', [])) | set(state.get('inactiveBondIds', []))
    bonds = chess_bonds(catalog, state, key)
    return not (bonds and all(bond in off for bond in bonds))


def f32(value):
    return struct.unpack('<f', struct.pack('<f', value))[0]


class SetupRules:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c
        self._power = {}
        self._legacy_power = {}
        self._override = {row['Id']: row for row in self.c.level('level_autochess_enemy_data').get('EnemyDbRefs') or []}

    def eligible(self, state, key):
        return chess_eligible(self.c, state, key)

    def refresh_bans(self, state):
        state['bannedChessIds'] = sorted(key for key in state['chessPool'] if not self.eligible(state, key))

    def initialize(self, state):
        # Setup is a creation event. A repeated request or a loaded v1/v2 run
        # must retain its original draw, even when the catalog's candidates or
        # the shop RNG have since changed. Profile changes use refresh_bans.
        if state.get('setupVersion'):
            return
        mode = self.c.mode(state['modeId'])
        inactive = sorted(mode.get('InactiveBondIdList') or [])
        active = set(mode['ActiveBondIdList'] if mode.get('ActiveBondIdList') is not None
                     else self.c.season['BondInfoDict'])
        candidates = sorted(key for key, row in self.c.season['BondInfoDict'].items()
                            if row['Weight'] > 0 and key in active and key not in inactive)
        core = [key for key in candidates if self.c.common['BondInfoDict'][key]['IsPower']]
        addon = [key for key in candidates if key not in core]
        ncore, naddon = BAN_COUNTS[mode['ModeDifficulty']]
        if len(core) < ncore or len(addon) < naddon:
            raise ValueError('Insufficient eligible CN bonds for mode: ' + state['modeId'])
        bans = (shuffle(stream(state['stageSeed'], 'setup:bans:core:v2'), core)[:ncore]
                + shuffle(stream(state['stageSeed'], 'setup:bans:addon:v2'), addon)[:naddon])
        topology = self._topology_candidates(state['modeId'])
        state.update(setupVersion=VERSION, modeStrengthVersion=1, bannedBondIds=sorted(bans),
                     inactiveBondIds=inactive, bannedChessIds=[],
                     stageId=weighted(stream(state['stageSeed'], 'setup:stage:v2'), topology['stages']),
                     bossId=weighted(stream(state['stageSeed'], 'setup:boss:v2'), topology['bosses']),
                     hiddenBossId=(weighted(stream(state['stageSeed'], 'setup:hidden-boss:v2'), topology['hiddenBosses'])
                                   if topology['hiddenBosses'] else None))
        candidate_data = {**topology, 'coreBonds': core, 'addonBonds': addon,
                          'banCounts': [ncore, naddon]}
        state['setupRandomness'] = {
            'version': VERSION, 'algorithm': 'sha256-separated-xorshift32-v2',
            'source': 'CN-mode-topology-and-positive-weights',
            'candidateHash': hashlib.sha256(json.dumps(candidate_data, sort_keys=True,
                separators=(',', ':')).encode()).hexdigest(),
            'candidates': candidate_data,
        }
        # Persist the exact native template chosen for every round. Neither a
        # preparation preview nor Ready may draw again or advance the shop RNG.
        state['roundVariantPicks'] = {}
        for round_id, rows in sorted(self.c.rounds(state['modeId']).items()):
            boss_round = self.c.turns(state['modeId'])[round_id]['IsBossTurn']
            wanted = (state['hiddenBossId'] if round_id in topology['hiddenRounds']
                      else state['bossId']) if boss_round else None
            choices = sorted((row for row in rows if row.get('BossId') == wanted),
                             key=lambda row: row['LevelId'])
            if not choices:
                raise ValueError('Missing selected CN round template: ' + str(round_id))
            row = draw(stream(state['stageSeed'], 'setup:template:v2:' + str(round_id)), choices)
            state['roundVariantPicks'][str(round_id)] = {'LevelId': row['LevelId'], 'BossId': row.get('BossId')}
        from .mode_strength import project
        project(state)
        self.refresh_bans(state)
        self.initialize_enemies(state)

    def _topology_candidates(self, mode_id):
        """Only mode-supported, positive-weight CN maps and leader templates.

        The current CN data yields 1/8/7/7 solo maps, seven ordinary leaders
        and three hidden leaders. Training has its own fixed leader; standard
        solo has no hidden round. Hidden and ordinary picks are independent,
        as in the cited reference; there is no invented exclusive boss pair.
        """
        stages = [[key, row['Weight']] for key, row in sorted(self.c.season['StageDatasDict'].items())
                  if row['Weight'] > 0 and mode_id in row['Mode']]
        if not stages:
            raise ValueError('No positive-weight CN stage for mode: ' + mode_id)
        boss_rounds = []
        hidden_rounds = []
        sets = {False: [], True: []}
        for round_id, turn in sorted(self.c.turns(mode_id).items()):
            if not turn['IsBossTurn']:
                continue
            rows = self.c.rounds(mode_id).get(round_id, [])
            for hidden in (False, True):
                allowed = {row['BossId'] for row in rows if row.get('BossId') in self.c.season['BossInfoDict']
                           and self.c.season['BossInfoDict'][row['BossId']]['IsHidingBoss'] == hidden}
                if allowed:
                    sets[hidden].append(allowed)
                    (hidden_rounds if hidden else boss_rounds).append(round_id)
        candidates = {}
        for hidden, label in ((False, 'bosses'), (True, 'hiddenBosses')):
            available = set.intersection(*sets[hidden]) if sets[hidden] else set()
            candidates[label] = [[key, self.c.season['BossInfoDict'][key]['Weight']]
                                 for key in sorted(available)
                                 if self.c.season['BossInfoDict'][key]['Weight'] > 0]
        if not candidates['bosses'] or hidden_rounds and not candidates['hiddenBosses']:
            raise ValueError('No compatible positive-weight CN leader for mode: ' + mode_id)
        return {'stages': stages, **candidates, 'bossRounds': boss_rounds, 'hiddenRounds': hidden_rounds}

    def variant(self, state, round_id=None):
        """Read a v2 saved template; never silently reroll an invalid save."""
        round_id = max(1, state['round']) if round_id is None else round_id
        saved = state.get('roundVariantPicks', {}).get(str(round_id))
        if not saved:
            raise ValueError('Missing saved CN round template: ' + str(round_id))
        for row in self.c.rounds(state['modeId']).get(round_id, []):
            if row['LevelId'] == saved.get('LevelId') and row.get('BossId') == saved.get('BossId'):
                return row
        raise ValueError('Saved CN round template is not valid in mode: ' + str(round_id))

    def initialize_enemies(self, state):
        """One immutable composition for every native round, on private streams.

        CN supplies the six candidate types, three picks, each type's slot
        count, positive per-entry weights and half-season pools. Independent
        seed streams are explicit server policy; the native client still owns
        action timing, paths, enemy mechanics and the actual battle.
        """
        constants = self.c.common['ConstData']
        types = self.c.common['EnemyTypeDatas']
        candidates = sorted(key for key, row in types.items() if row['InvolveRandom'])
        number = constants['SpecialEnemyNum']
        maximum = constants['MaxLevelCnt']
        if type(number) is not int or not 0 <= number <= len(candidates) or type(maximum) is not int or maximum < 1:
            raise ValueError('Invalid CN random enemy type counts')
        fills = [key for key, row in types.items()
                 if row['TypeIdentifier'] == constants['EnemyTypeIdentifierToFillRandom']]
        if len(fills) != 1:
            raise ValueError('Missing unique CN random enemy filler type')
        fill = fills[0]
        chosen = shuffle(stream(state['stageSeed'], 'setup:enemy-types:v2'), candidates)[:number]
        slots = []
        for key in chosen:
            count = self.c.season['SpecialEnemyRandomTypeDict'][key]['Count']
            if type(count) is not int or count < 0:
                raise ValueError('Invalid CN random enemy type slot count: ' + key)
            slots.extend([key] * count)
        if len(slots) > maximum:
            raise ValueError('CN random enemy type slots exceed round schedule')
        slots.extend([fill] * (maximum - len(slots)))
        # Every configured candidate must be available locally. Silently
        # dropping missing prefabs would bias weights and mask broken data.
        entries = {}
        for entry_id, row in sorted(self.c.season['SpecialEnemyInfoDict'].items()):
            if row['RandomWeight'] <= 0:
                continue
            keys = [row['SpecialEnemyKey'], *(row.get('AttachedNormalEnemyKeys') or []),
                    *(row.get('AttachedEliteEnemyKeys') or [])]
            for key in keys:
                if key == 'enemy_9012_acloon':
                    # 炎佑 is an allied bond summon, although it has an enemy
                    # database record and a verified prefab. Never an opponent.
                    continue
                if (key not in CN_VERIFIED_ENEMY_KEYS or
                        key not in self.c.common['RandomEnemyAttributeDict'] or
                        key not in self.c.data['characterTables']['enemy_database']['Enemies']):
                    raise ValueError('Unverified configured CN random enemy: ' + key)
            entries[entry_id] = row
        state['enemyTypes'] = sorted(chosen, key=lambda key: types[key]['SortId'])
        state['enemyTypeSchedule'] = shuffle(stream(state['stageSeed'], 'setup:enemy-schedule:v2'), slots)
        state['roundEnemyPicks'] = {str(index+1): self._pick(state,
            stream(state['stageSeed'], 'setup:enemy-pick:v2:' + str(index+1)), key, index+1)
            for index, key in enumerate(state['enemyTypeSchedule'])}
        state.setdefault('setupRandomness', {})['enemies'] = {
            'version': 2, 'source': 'CN-common-EnemyTypeDatas-and-season-SpecialEnemyInfoDict',
            'randomTypeCandidates': candidates, 'chosenTypeCount': number,
            'roundCount': maximum, 'firstHalfLastRound': maximum//2, 'fillType': fill,
            'candidateEntryCount': len(entries),
            'candidateHash': hashlib.sha256(json.dumps(entries, sort_keys=True,
                separators=(',', ':')).encode()).hexdigest(),
            'alliedEnemyRecordExclusions': ['enemy_9012_acloon'],
            'modeExclusionScope': 'special-key-only; attached-normal-and-elite-retained',
        }

    def _pick(self, state, rng, enemy_type, round_id):
        forbidden = set(self.c.mode(state['modeId']).get('InactiveEnemyKey') or [])
        rows = [(key, row) for key, row in sorted(self.c.season['SpecialEnemyInfoDict'].items())
                if row['Type'] == enemy_type and row['IsInFirstHalf'] == (round_id <= self.c.common['ConstData']['MaxLevelCnt']//2)
                and row['SpecialEnemyKey'] not in forbidden and row['SpecialEnemyKey'] != 'enemy_9012_acloon'
                and row['SpecialEnemyKey'] in CN_VERIFIED_ENEMY_KEYS and row['RandomWeight'] > 0]
        if not rows and enemy_type != 'SPECIAL':
            return self._pick(state, rng, 'SPECIAL', round_id)
        if not rows:
            raise ValueError('No verified CN random enemy entry: ' + enemy_type)
        selected = weighted(rng, [(key, row['RandomWeight']) for key, row in rows])
        row = dict(rows)[selected]
        def attached(field):
            values = [key for key in row.get(field) or [] if key in CN_VERIFIED_ENEMY_KEYS and key != 'enemy_9012_acloon']
            return draw(rng, values) if values else None
        return {'type': row['Type'], 'special': row['SpecialEnemyKey'],
                'normal': attached('AttachedNormalEnemyKeys'), 'elite': attached('AttachedEliteEnemyKeys')}

    def is_fly(self, key):
        return bool(self.c.common['RandomEnemyAttributeDict'].get(key, {}).get('IsFlyEnemy'))

    def power(self, key, *, legacy=False):
        # Native _GetEnemyAttrPower uses the selected template's resolved
        # enemy refs, then the database level. None of the configured random
        # candidates are overridden in those templates. The separate common
        # enemy-data level's spawn overrides must not leak into counts. Keep
        # the old v1 policy only for existing saved compositions.
        cache = self._legacy_power if legacy else self._power
        if key in cache:
            return cache[key]
        attributes = {}
        level = self.c.common['RandomEnemyAttributeDict'].get(key, {}).get('Level', 0)
        for row in sorted(self.c.data['characterTables']['enemy_database']['Enemies'].get(key, []), key=lambda row: row['Level']):
            if row['Level'] <= level:
                attributes.update({name: value['MValue'] for name, value in (row['EnemyData'].get('Attributes') or {}).items() if value and value.get('MDefined')})
        if legacy:
            attributes.update({name: value['MValue'] for name, value in ((self._override.get(key, {}).get('OverwrittenData') or {}).get('Attributes') or {}).items() if value and value.get('MDefined')})
        constants = self.c.common['ConstData']
        power = f32(0)
        for name, factor in [('Atk', 'EnemyAtkFactor'), ('MaxHp', 'EnemyMaxHpFactor'), ('Def_', 'EnemyDefFactor'), ('MagicResistance', 'EnemyMagicResistanceFactor')]:
            power = f32(power + f32(attributes.get(name, 0) * constants[factor]))
        cache[key] = power
        return power

    def replaced_count(self, old, count, new, *, legacy=False):
        factors = self.c.common['RandomEnemyAttributeDict']
        old_factor = factors.get(old, {}).get('EnemyBattleEffectivenessFactor', 1) or 1
        new_factor = factors.get(new, {}).get('EnemyBattleEffectivenessFactor', 1) or 1
        origin = f32(f32(count * self.power(old, legacy=legacy)) / old_factor)
        unit = f32(self.power(new, legacy=legacy) / new_factor)
        constants = self.c.common['ConstData']
        value = round(f32(origin / unit)) if unit > 0 else constants['MinReplacedEnemyCount']
        return max(constants['MinReplacedEnemyCount'], min(constants['MaxReplacedEnemyCount'], value))

    def actions(self, state, level_id):
        """Keep original indexes/local actions, replace only scheduler placeholders."""
        result = []
        waves = self.c.level(level_id).get('Waves') or []
        if len(waves) > 1 or waves and len(waves[0].get('Fragments') or []) > 1:
            from .engine import DomainError
            raise DomainError('native_multiple_fragment_battle_pending')
        pick = state.get('roundEnemyPicks', {}).get(str(max(1, state['round']))) if state.get('setupVersion') else None
        constants = self.c.common['ConstData']
        placeholders = {constants['TemplateEnemy'+name+suffix]: (name.lower(), bool(suffix))
                        for name in ('Normal', 'Elite', 'Special') for suffix in ('', 'Fly')}
        for wave in waves:
            for fragment in wave.get('Fragments') or []:
                for index, action in enumerate(fragment.get('Actions') or []):
                    if action['ActionType'] != 0 or action['Count'] <= 0 or action['Key'] == 'enemy_9012_acloon':
                        continue
                    row = copy.deepcopy(action)
                    row['nativeActionIndex'] = index
                    slot = placeholders.get(row['Key'])
                    if pick and slot and row.get('ManagedByScheduler', True) and not row.get('RandomSpawnGroupKey'):
                        key = pick.get(slot[0])
                        if not key or self.is_fly(key) != slot[1]:
                            continue
                        row['Key'] = key
                        row['Count'] = self.replaced_count(action['Key'], action['Count'], key, legacy=state.get('setupVersion', 0) == 1)
                        if state.get('setupVersion', 0) >= 2:
                            row['Interval'] = action['Count'] * action.get('Interval', 0) / row['Count']
                    result.append(row)
        return self._band_enemies(state, result)

    def _band_enemies(self, state, actions):
        """Global enemy-change effect, with a stable per-round private draw.

        Original native action indexes/routes remain authoritative. Only
        scheduler ground enemies may be replaced; boss/local auxiliaries and
        separately appended item bounties are outside this candidate ledger.
        """
        band = self.c.season['BandDataListDict'].get(state.get('bandId'))
        if not band:
            return actions
        for effect_index, effect in enumerate(self.c.effects(band['EffectId'])):
            if effect['Key'] != 'round_start_all_player_change_enemy_2':
                continue
            bb = effect['Blackboard'] or {}
            if state['round'] < int(bb.get('round', 1)):
                continue
            keys = [key.strip() for key in str(bb.get('enemylist', '')).split(',') if key.strip() in CN_VERIFIED_ENEMY_KEYS and key.strip() != 'enemy_9012_acloon']
            if not keys:
                continue
            rng = stream(state['stageSeed'], 'bands:enemy-change:v1:'+str(state['round'])+':'+str(effect_index))
            lo = max(0, int(bb.get('min', 0)))
            hi = max(lo, int(bb.get('max', lo)))
            wanted = draw(rng, list(range(lo, hi+1)))
            bosses = {row['EnemyId'] for row in self.c.common['BossInfoDict'].values()}
            flat = [(index, unit) for index, row in enumerate(actions) for unit in range(row['Count'])]
            size = len(flat)
            candidates = [(index, unit) for ordinal, (index, unit) in enumerate(flat)
                if actions[index]['Key'] not in bosses and not self.is_fly(actions[index]['Key'])
                and actions[index].get('ManagedByScheduler', True) and not actions[index].get('RandomSpawnGroupKey')
                and not actions[index].get('IsUnharmfulAndAlwaysCountAsKilled')
                and float(bb.get('minweight', 0)) <= (ordinal/(size-1) if size>1 else 0) <= float(bb.get('maxweight', 1))]
            chosen = set(shuffle(rng, candidates)[:wanted])
            if not chosen:
                continue
            rewritten = []
            for index, row in enumerate(actions):
                selected = [unit for unit in range(row['Count']) if (index, unit) in chosen]
                if not selected:
                    rewritten.append(row)
                    continue
                if row['Count'] > len(selected):
                    rewritten.append({**row, 'Count': row['Count']-len(selected)})
                for _ in selected:
                    rewritten.append({**row, 'Key': draw(rng, keys), 'Count': 1,
                                      'bandKillCoin': max(0, int(bb.get('count', 1)))})
            actions = rewritten
        return actions
