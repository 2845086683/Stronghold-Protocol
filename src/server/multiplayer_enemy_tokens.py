"""Indexed native escaped-summon identities, shared with the original Help path.

CN OnSelfBattleEnemyReachExit (60138f5/ba67f0) packs template identifier,
total escape ordinal, and playerViewIndex into an Int32. The native Help
generator reverses the identifier itself; the server retains the empty wire
enemyId. Unindexed auxiliary summons are not issued battle enemies.
"""
from .engine import DomainError, integer


def decode(catalog, value, seat, *, total_escaped=None):
    packed = integer(value, 'enemyTokenInstId', 1, 2147483647)
    origin = packed & 255
    ordinal = (packed >> 8) & 255
    identifier = packed >> 16
    identifiers = {row['ExtraEnemyIdentifier'] for row in catalog.common['RandomEnemyAttributeDict'].values()
                   if row['ExtraEnemyIdentifier'] > 0}
    if origin != seat or ordinal < 1 or identifier not in identifiers:
        raise DomainError('invalid_multiplayer_escaped_token_identity')
    # The original arithmetic does not mask its ordinal before shifting.
    # Restrict the non-overflow representation and bind it to this terminal
    # report's complete escape count; do not reinterpret a carry as a new type.
    if total_escaped is not None and (total_escaped > 255 or ordinal > total_escaped):
        raise DomainError('invalid_multiplayer_escaped_token_ordinal')
    return {'ownerPlayerIndex': seat, 'enemyId': '', 'enemyInstId': packed, 'isToken': True}


def terminal(catalog, values, seat, normal_escapes):
    result = [decode(catalog, value, seat, total_escaped=normal_escapes + len(values)) for value in values]
    ordinals = [(row['enemyInstId'] >> 8) & 255 for row in result]
    if len(ordinals) != len(set(ordinals)):
        raise DomainError('conflicting_multiplayer_escaped_token_ordinal')
    return result
