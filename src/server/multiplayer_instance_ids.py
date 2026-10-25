"""Float-exact native instance identities and explicit saved-room migration.

The original CN blackboard stores autochess_inst_id as System.Single. Its
integer domain, not merely the DTO Int32 declaration, therefore bounds IDs.
Migration is pure and opt-in: no read/GET path or SQL receipt is modified here.
"""
import copy
import hashlib
import json

from .engine import DomainError

VERSION = 2
STRIDE = 1_000_000
LEGACY_STRIDE = 100_000_000
EXACT_MAX = 1 << 24
MAX_NODES = 2_000_000

# Domain instance references, recovered from the actual engine/protocol modules.
# Numeric values under other names (HP, damage, timestamps, seeds, positions,
# identifiers, prices and effect stacks) are deliberately never substituted.
SCALARS = frozenset(('instId', 'effectInstId', 'domainInstId', 'attackerInstId',
    'charInstId', 'charChessInstId', 'equipChessInstId', 'chessInstId',
    'itemInstId', 'sourceInstId', 'unloadInstId', 'enemyInstId'))
LISTS = frozenset(('equipInstIds', 'escapedInstIds', 'killedInstIds',
    'discardedOverflowInstIds', 'nativeOwnedCharIds', 'nativeOwnedChessIds',
    'nativeIssuedEnemyIds', 'killed', 'escaped'))
ID_KEYED = frozenset(('itemCounters', 'itemGainCounts', 'battleChessStatus'))
TOKEN_LISTS = frozenset(('escapedTokenInstIds', 'nativeEscapedTokenIds', 'escapedTokens'))


def _fail(reason):
    raise DomainError('invalid_multiplayer_instance_namespace', reason)


def _integer(value, reason):
    if type(value) is not int:
        _fail(reason)
    return value


def initialize(state, seat):
    """Assign an empty new player its disjoint, Single-exact allocation range."""
    _integer(seat, 'seat')
    if not 0 <= seat < 4 or state.get('multiplayerInstanceNamespace') is not None:
        _fail('new namespace owner')
    start = _integer(state.get('nextInstId'), 'initial allocator')
    if not 0 <= start < STRIDE:
        _fail('initial allocator outside range')
    # The normal engine initializes the counter before its first issued object.
    if any(state.get('player', {}).get(key) for key in ('charChess', 'trapChess', 'equipChess')):
        _fail('namespace initialization after allocation')
    state['nextInstId'] = seat * STRIDE + start
    state['multiplayerInstanceNamespace'] = {'version': VERSION, 'seat': seat, 'stride': STRIDE}


def allocate(state):
    """Engine._id calls this only for a versioned multiplayer state."""
    ns = state.get('multiplayerInstanceNamespace') or {}
    seat = _integer(ns.get('seat'), 'allocator seat')
    if ns != {'version': VERSION, 'seat': seat, 'stride': STRIDE} or not 0 <= seat < 4:
        _fail('allocator version')
    current = _integer(state.get('nextInstId'), 'allocator value')
    candidate = current + 1
    if not seat * STRIDE <= current < candidate < (seat + 1) * STRIDE or candidate > EXACT_MAX:
        _fail('allocator exhausted')
    state['nextInstId'] = candidate
    return candidate


def _digest(value):
    return hashlib.sha256(json.dumps(value, ensure_ascii=False, sort_keys=True,
        separators=(',', ':')).encode()).hexdigest()


def _states(room):
    result = {}
    for row in room.get('seats', []):
        seat = _integer(row.get('seat'), 'room seat')
        if not 0 <= seat < 4 or seat in result:
            _fail('duplicate room seat')
        if row.get('state') is not None:
            result[seat] = row['state']
    return result


def _rewrite(value, convert, *, path=(), action_layout='native', audit=None):
    """Rewrite declared instance slots, including typed native ActionUp slots."""
    nodes = [0]
    def visit(item, trail):
        nodes[0] += 1
        if nodes[0] > MAX_NODES:
            _fail('migration graph bound')
        if isinstance(item, list):
            return [visit(row, trail + ('[]',)) for row in item]
        if not isinstance(item, dict):
            return copy.deepcopy(item)
        result = {}
        native_enemy_alias = 'nativeBoss' in trail and trail[-2:-1] == ('enemies',)
        scheduled = len(trail) >= 2 and trail[-2] == 'scheduled' and item.get('kind') == 'promotion'
        token_enemy = bool(item.get('isToken'))
        for key, old in item.items():
            target_key = key
            if trail and trail[-1] in ID_KEYED:
                if not key.isdecimal():
                    _fail('non-numeric instance dictionary key')
                target_key = str(convert(int(key), trail + ('<key>',)))
            elif trail and trail[-1] == 'traitCounters':
                bits = key.split(':', 2)
                if len(bits) != 3 or not bits[0].isdecimal() or not bits[1].isdecimal():
                    _fail('trait instance counter key')
                bits[1] = str(convert(int(bits[1]), trail + ('<key>',)))
                target_key = ':'.join(bits)
            elif trail and trail[-1] in ('nativeTokenPositions', 'tokenBattleStatusList') and ':' in key:
                bits = key.split(':')
                if not bits[0].isdecimal():
                    _fail('token position key')
                bits[0] = str(convert(int(bits[0]), trail + ('<key>',)))
                target_key = ':'.join(bits)
            elif trail and trail[-1] in ('kills', 'escapes'):
                # joint_defense._enemy_key: owner:isToken:enemyInstId.
                bits = key.split(':')
                if len(bits) != 3 or not bits[0].isdecimal() or bits[1] not in ('0', '1'):
                    _fail('joint defense enemy identity key')
                try:
                    ident = int(bits[2])
                except ValueError:
                    _fail('joint defense enemy identity key')
                if bits[1] == '0':
                    bits[2] = str(convert(ident, trail + ('<key>',)))
                target_key = ':'.join(bits)
            if target_key in result:
                _fail('instance dictionary collision')
            where = trail + (key,)
            if not trail and key == 'matchHistory':
                # Earlier matches are immutable audit records, not live object
                # ownership. Their runs/allocators need not exist in this match.
                result[target_key] = copy.deepcopy(old)
            elif key in SCALARS and old is not None:
                # Native boss enemies use their own <=65535 wire alias and
                # packed summon IDs; their domainInstId remains authoritative.
                skip = (native_enemy_alias and key == 'instId') or (token_enemy and key == 'enemyInstId')
                result[target_key] = old if skip else convert(old, where)
            elif key in LISTS:
                if type(old) is not list:
                    _fail('instance list shape')
                result[target_key] = [convert(row, where + ('[]',)) for row in old]
            elif key in TOKEN_LISTS or key == 'instIdList':
                result[target_key] = copy.deepcopy(old)  # Native packed TOKEN/boss aliases.
            elif scheduled and key in ('target', 'item'):
                result[target_key] = convert(old, where)
            elif key == 'paramList' and 'operate' in item and 'playerUidIndex' in item:
                params = copy.deepcopy(old)
                operation = _integer(item['operate'], 'native action operation')
                if type(params) is not list or any(type(x) is not int for x in params):
                    _fail('native action params')
                # Recovered COST_BOSS_HP: [damage,attacker,source,credit].
                # Native ENEMY_KILLED: [alias,escapeCnt,attacker,source,credit].
                slot = 1 if operation == 3 else 2 if operation == 1 and action_layout == 'native' else None
                if slot is not None:
                    expected = 4 if operation == 3 else 5
                    if len(params) != expected:
                        _fail('native instance action shape')
                    params[slot] = convert(params[slot], where + (str(slot),))
                elif operation not in range(1, 11):
                    _fail('unknown native action layout')
                result[target_key] = params
            elif key == 'multiplayerInstanceMigration':
                result[target_key] = copy.deepcopy(old)
            else:
                result[target_key] = visit(old, where)
            if audit is not None and result[target_key] != old:
                audit['rewrittenFields'] += 1
        return result
    return visit(value, path)


def plan_migration(room):
    """Return a migrated copy and reversible audit; caller owns backup/commit.

    Completed idempotency hashes and SQL receipts stay immutable. The caller
    must re-project memo replies and compare legacy action hashes before
    canonical ID translation when the original raw action is still available.
    """
    states = _states(room)
    if not states or all((state.get('multiplayerInstanceNamespace') or {}).get('version') == VERSION for state in states.values()):
        return copy.deepcopy(room), {'changed': False, 'sourceSha256': _digest(room)}
    metadata = {}
    for seat, state in states.items():
        ns = state.get('multiplayerInstanceNamespace')
        if ns != {'version': 1, 'seat': seat, 'stride': LEGACY_STRIDE}:
            _fail('legacy room must have complete consistent seat namespaces')
        end = _integer(state.get('nextInstId'), 'legacy allocator') - seat * LEGACY_STRIDE
        if not 0 <= end < STRIDE:
            _fail('legacy allocator does not fit exact range')
        metadata[seat] = end
    aliases = {}
    def compact(value, path):
        _integer(value, 'instance reference type')
        if value in (-1, 0):
            return value
        if value < 0:
            _fail('unknown negative instance sentinel')
        seat, local = divmod(value, LEGACY_STRIDE)
        if seat not in metadata or not 0 < local <= metadata[seat]:
            _fail('unknown instance reference at ' + '.'.join(path))
        target = seat * STRIDE + local
        aliases[str(value)] = target
        return target
    audit = {'changed': True, 'rewrittenFields': 0, 'sourceSha256': _digest(room)}
    migrated = _rewrite(room, compact, audit=audit)
    inverse = {value: int(key) for key, value in aliases.items()}
    if len(inverse) != len(aliases):
        _fail('non-bijective instance mapping')
    def restore(value, path):
        if value in (-1, 0):
            return value
        if value not in inverse:
            _fail('inverse reference missing')
        return inverse[value]
    if _rewrite(migrated, restore) != room:
        _fail('migration changed non-instance state')
    # Proofs contain exact IDs, target DTO digests and completed world receipts;
    # they must be explicitly rebuilt before any deployment permission returns.
    invalidated = migrated.pop('preparationViews', None)
    field = (migrated.get('help') or {}).get('fieldLoad')
    if field:
        # Its immutable payload hashes, load attempts and readiness proofs are
        # coordinator-owned; guessing new signatures would authorize a stale
        # native board. The explicit deployment coordinator must retire/reissue
        # this plan before requesting migration, preserving elapsed budgets.
        _fail('signed Help field requires explicit coordinator retirement')
    migrated['preparationViewSequence'] = room.get('preparationViewSequence', 0) + 1
    for seat, state in _states(migrated).items():
        state['nextInstId'] = seat * STRIDE + metadata[seat]
        state['multiplayerInstanceNamespace'] = {'version': VERSION, 'seat': seat, 'stride': STRIDE}
    migration = {'version': VERSION, 'roomId': room.get('roomId'),
        'matchSequence': room.get('matchSequence', 1), 'sourceSha256': audit['sourceSha256'],
        'legacyAliases': aliases, 'invalidatedPreparationViewsSha256': _digest(invalidated),
        'legacyStride': LEGACY_STRIDE, 'stride': STRIDE}
    migrated['multiplayerInstanceMigration'] = migration
    audit.update(aliases=len(aliases), allNativeIdsSingleExact=all(0 < value <= EXACT_MAX for value in inverse),
        semanticInverseVerified=True, preparationProofsInvalidated=invalidated is not None,
        sqlReceiptsRewritten=False, resultSha256=_digest(migrated), migration=copy.deepcopy(migration))
    return migrated, audit


def translate_payload(room, payload, *, action_layout='native'):
    """Translate only recorded old aliases; never approximate rounded IDs.

    Call after computing/checking the untouched incoming request fingerprint.
    Domain handlers still validate actual ownership/phase and object existence.
    """
    migration = room.get('multiplayerInstanceMigration')
    if not migration:
        return copy.deepcopy(payload)
    if migration.get('version') != VERSION or migration.get('roomId') != room.get('roomId') or migration.get('matchSequence') != room.get('matchSequence', 1):
        _fail('legacy alias owner')
    aliases = migration.get('legacyAliases', {})
    known = set(aliases.values())
    def collect(value, path):
        if type(value) is int and value > 0:
            known.add(value)
        return value
    for state in _states(room).values():
        for field in ('player', 'battle', 'specialEffects', 'nativeTokenPositions'):
            _rewrite(state.get(field), collect)
    def convert(value, path):
        _integer(value, 'incoming instance reference')
        if value in (-1, 0):
            return value
        if str(value) in aliases:
            return aliases[str(value)]
        if value not in known:
            _fail('unknown legacy/canonical instance reference at ' + '.'.join(path))
        return value
    return _rewrite(payload, convert, action_layout=action_layout)


def historical_fingerprints(room, payload, *, action_layout='native'):
    """Exact current/legacy alternatives for in-room action-sequence dedup.

    Existing persisted hashes stay unchanged. Both alternatives derive from
    one typed payload and one same-room/match bijection; arbitrary rounded IDs
    or a different action are never accepted as an equivalent historical batch.
    SQL request fingerprints must still be checked against original raw input.
    """
    canonical = translate_payload(room, payload, action_layout=action_layout)
    result = {_digest(canonical)}
    migration = room.get('multiplayerInstanceMigration')
    if not migration:
        return result
    inverse = {value: int(key) for key, value in migration['legacyAliases'].items()}
    if len(inverse) != len(migration['legacyAliases']):
        _fail('historical action alias collision')
    def restore(value, path):
        return inverse.get(value, value)
    result.add(_digest(_rewrite(canonical, restore, action_layout=action_layout)))
    return result
