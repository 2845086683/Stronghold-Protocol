"""Preserve native-derived preparation tokens separately from owned inventory.

Unity emits TOKEN rows sharing their character owner's instId. The bridge uses
the pinned native classifier/configuration predicate before sending them. The
server checks ownership/layout and echoes these declared position DTOs. Each
positive-position character occupies one native deployment slot; TOKEN volume
comes from its selected CN prefab's serialized occupancy. Hand rows occupy none.
Tokens remain outside purchasable inventory and combat status.
"""
import copy

from .engine import DomainError, integer
from .hand_layout import HAND_COUNT
from .native_token_properties import ORIGIN_VOLUME, PREFAB_OCCUPIED
from .native_token_prepare_policy import suppress_hand_token


def token_properties(catalog, state, owner_id):
    """Resolve native owner setting -> talent/skill token -> serialized prefab.

    _ConvertInternal (CN 0x6013053) takes each nonempty selected talent tokenKey
    in order, then the selected MainSkill.overrideTokenKey wins. Preparation
    dummies do not run the battle Rune/module 'free summon' effect, so module
    IDs do not override this original prefab occupancy. No operator exceptions
    or caller-supplied volume are used.
    """
    owner = next((obj for obj in state['player']['charChess'] if obj['instId'] == owner_id), None)
    if owner is None:
        raise DomainError('invalid_native_token_owner')
    chess_id = catalog.data['identifiers'][str(owner['chessIdentifier'])]
    normal = catalog.season['ChessNormalIdLookupDict'].get(chess_id, chess_id)
    shop = catalog.season['CharShopChessDatas'][normal]
    selected = next((row for row in state.get('nativeSquad', []) if row['chessId'] == normal), None)
    if selected is None:
        selected = state.get('diyChessPool', {}).get(normal, {})
    char_id = selected.get('charId') or shop.get('CharId') or shop.get('BackupCharId')
    definition = catalog.data['characterDefinitions'].get(char_id)
    if not definition:
        raise DomainError('unknown_native_token_character', str(char_id))
    # _GetChessSquadConfig(0x6013946) builds AdvancedCharacterInst from the
    # shop tier's level status and the slot's selected skill/potential, then
    # GetTokenChessData consumes that exact squad.charData.
    status = next(row for row in catalog.season['ShopCharChessInfoData'][str(shop['ChessLevel'])]
                  if row['IsGolden'] == bool(catalog.chess[chess_id].get('IsGolden')))
    phase, level = status['EvolvePhase'], status['CharLevel']
    potential = selected.get('potentialRank', shop.get('BackupCharPotRank', 0))
    token_key = None
    for talent in definition.get('Talents') or []:
        candidates = [row for row in talent.get('Candidates') or []
                      if (phase, level) >= ((row.get('UnlockCondition') or {}).get('Phase', 0),
                                             (row.get('UnlockCondition') or {}).get('Level', 0))
                      and potential >= row.get('RequiredPotentialRank', 0)]
        # DoGetTalent sorts by unlock phase, level, potential descending and
        # returns the first qualified row (CN comparator 0x600791c), rather
        # than relying on the serialized candidates' original ordering.
        if candidates:
            chosen = max(candidates, key=lambda row: (
                (row.get('UnlockCondition') or {}).get('Phase', 0),
                (row.get('UnlockCondition') or {}).get('Level', 0), row.get('RequiredPotentialRank', 0)))
            if chosen.get('TokenKey'):
                token_key = chosen['TokenKey']
    skill_index = selected.get('skillIndex', shop.get('DefaultSkillIndex', 0))
    skills = definition.get('Skills') or []
    # Native saves overrideTokenKey before its later skill unlock validation.
    if type(skill_index) is int and 0 <= skill_index < len(skills) and skills[skill_index].get('OverrideTokenKey'):
        token_key = skills[skill_index]['OverrideTokenKey']
    token = catalog.data['characterDefinitions'].get(token_key)
    if token is None or not token_key.startswith('token_'):
        raise DomainError('unknown_native_preparation_token', str(char_id))
    phases = token.get('Phases') or []
    if not phases:
        raise DomainError('unknown_native_token_prefab', str(token_key))
    prefab = phases[min(phase, len(phases) - 1)]['CharacterPrefabKey']
    if prefab not in PREFAB_OCCUPIED:
        raise DomainError('unknown_native_token_prefab', str(prefab))
    occupied = PREFAB_OCCUPIED[prefab]
    return {'tokenKey': token_key, 'prefabKey': prefab, 'occupiedRemainingCharacterCnt': occupied,
            'originRemainingChrCntVolume': ORIGIN_VOLUME, 'deploymentVolume': ORIGIN_VOLUME if occupied else 0}


def _owners(catalog, state):
    player = state['player']
    slots = {row['instId']: ((row['position']['y'] << 16) + row['position']['x'])
             for row in player['positions']}
    slots.update({row['instId']: row['position'] for row in player.get('handPositions', [])})
    return {obj['instId']: (obj['chessIdentifier'], slots[obj['instId']])
            for obj in player['charChess'] if obj['instId'] in slots}


def preparation_token_displayable(catalog, properties):
    """Respect the original HIDDEN entry before the missing-entry supplement.

    CN IsChessIdHasRelatedTokenInPrepare (0x601351a, 0xb280ea) rejects
    TokenDisplayType==1. Battle token resolution is independent of this UI gate.
    """
    original = catalog.common['ShopStateTokenDict'].get(properties['tokenKey'])
    if original is not None:
        return original['TokenDisplayType'] != 1
    return not suppress_hand_token(catalog, properties['tokenKey'], properties['prefabKey'])


def current_tokens(catalog, state):
    """Only native rows whose owner still has the recorded layout identity."""
    owners = _owners(catalog, state)
    result = []
    for row in state.get('nativeTokenPositions', []):
        owner = owners.get(row['instId'])
        if owner is None or owner[1] != row['ownerPosition']:
            continue
        if row['position'] > 0 and owner[1] <= 0:
            continue
        properties = token_properties(catalog, state, row['instId'])
        if not preparation_token_displayable(catalog, properties):
            continue
        result.append(row)
    return result


def hand_tokens(catalog, state, incoming=None):
    """Current physical hand TOKEN rows, separate from owned inventory.

    A complete native movement snapshot replaces the previous TOKEN layout.
    Other acquisition/equip/sale paths use only still-current saved owners.
    Ownership and native prefab checks remain at accept_tokens after movement.
    """
    rows = current_tokens(catalog, state) if incoming is None else incoming
    if not isinstance(rows, list) or len(rows) > 128:
        raise DomainError('invalid_native_token_positions')
    characters = {obj['instId'] for obj in state['player']['charChess']}
    reserved = set()
    result = []
    for row in rows:
        if not isinstance(row, dict):
            raise DomainError('invalid_native_token_positions')
        owner = integer(row.get('instId'), 'tokenOwner', 1)
        if owner not in characters or integer(row.get('chessType'), 'tokenType', 3, 3) != 3:
            raise DomainError('invalid_native_token_owner')
        position = integer(row.get('position'), 'tokenPosition', -2147483648)
        if position > 0:
            continue
        integer(position, 'tokenHandPosition', 1 - HAND_COUNT, 0)
        properties = token_properties(catalog, state, owner)
        if not preparation_token_displayable(catalog, properties):
            raise DomainError('preparation_token_not_displayable')
        if position in reserved:
            raise DomainError('native_token_position_collision')
        reserved.add(position)
        result.append(row)
    return result


def reserved_hand_positions(catalog, state, incoming=None):
    return {row['position'] for row in hand_tokens(catalog, state, incoming)}


def accept_tokens(catalog, state, rows):
    if not isinstance(rows, list) or len(rows) > 128:
        raise DomainError('invalid_native_token_positions')
    if rows and not state.get('nativeSessionVersion'):
        raise DomainError('outside_private_native_session')
    owners = _owners(catalog, state)
    player = state['player']
    occupied = {(row['position']['y'] << 16) + row['position']['x'] for row in player['positions']}
    occupied.update(row['position'] for row in player.get('handPositions', []))
    accepted = []
    for row in rows:
        if not isinstance(row, dict):
            raise DomainError('invalid_native_token_positions')
        owner = integer(row.get('instId'), 'tokenOwner', 1)
        if owner not in owners or integer(row.get('chessType'), 'tokenType', 3, 3) != 3:
            raise DomainError('invalid_native_token_owner')
        position = integer(row.get('position'), 'tokenPosition', -2147483648)
        direction = integer(row.get('direction'), 'tokenDirection', 0, 3)
        if position in occupied:
            raise DomainError('native_token_position_collision')
        if position <= 0:
            integer(position, 'tokenHandPosition', 1 - HAND_COUNT, 0)
        else:
            from .board_terrain import tile as preparation_tile
            if not preparation_tile(catalog, state, position >> 16, position & 65535):
                raise DomainError('invalid_native_token_tile')
        occupied.add(position)
        identifier, owner_position = owners[owner]
        if position > 0 and owner_position <= 0:
            raise DomainError('native_token_owner_not_deployed')
        properties = token_properties(catalog, state, owner)
        if not preparation_token_displayable(catalog, properties):
            raise DomainError('preparation_token_not_displayable')
        accepted.append({'instId': owner, 'chessType': 3, 'position': position,
                         'direction': direction, 'ownerChessIdentifier': identifier,
                         'ownerPosition': owner_position, 'tokenKey': properties['tokenKey'],
                         'deploymentVolume': properties['deploymentVolume']})
    state['nativeTokenPositions'] = accepted


def project_tokens(catalog, state, positions):
    """Retain same-owner promotions; drop destroyed owners or moved layouts."""
    occupied = {row['position'] for row in positions}
    result = []
    for row in current_tokens(catalog, state):
        if row['position'] in occupied:
            continue
        occupied.add(row['position'])
        result.append({key: copy.deepcopy(row[key]) for key in ('instId', 'position', 'chessType', 'direction')})
    return result


def deployment_volume(catalog, state):
    """Mirror the native Character/Token dummy volume without adding ownership.

    Character's constructor initializes originRemainingChrCntVolume=1; the
    original TOKEN prefab supplies its occupied flag, which may be false.
    DummyManager sums all battlefield dummy volumes. Other
    item/device rows retain their existing zero-character-volume treatment.
    Reuse token projection so removed/moved owners cannot consume stale slots.
    """
    player = state['player']
    owned = {obj['instId'] for obj in player['charChess']}
    positions = [{'position': (row['position']['y'] << 16) + row['position']['x']}
                 for row in player['positions']]
    occupied = positions + player.get('handPositions', [])
    characters = sum(row['instId'] in owned and
                     (row['position']['y'] << 16) + row['position']['x'] > 0
                     for row in player['positions'])
    tokens = sum(token_properties(catalog, state, row['instId'])['deploymentVolume']
                 for row in project_tokens(catalog, state, occupied) if row['position'] > 0)
    return characters + tokens


def require_deployment_capacity(catalog, state, declared=None, previous=None):
    actual = deployment_volume(catalog, state)
    # Official drag requests can carry the counter from the restored source
    # dummy map. Older private clients sent the new layout volume. Accept only
    # these two exact values; neither declaration weakens the post-layout cap.
    if declared is not None and integer(declared, 'deployCount') not in (actual, previous):
        raise DomainError('deploy_count_mismatch')
    if actual > state['player']['maxDeploymentCnt']:
        raise DomainError('deployment_limit')
    return actual
