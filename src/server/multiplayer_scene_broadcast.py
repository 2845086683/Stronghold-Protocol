"""Validate the native shop-level radio echo against committed room facts.

CN CheckShopLevel (0x6013549) sends the configured Type 2 ID and an empty
string list through the bare BroadcastUp request (0x6013636). It does not
request an upgrade. Economy handlers already emit this fact; the shared
broadcast ledger makes the native echo, including a reconnect, idempotent.
"""
from .engine import DomainError, integer


def apply(rooms, room, seat, name, data):
    if name != 'AutoChessBattleSceneBroadcastUp':
        return False
    if set(data) != {'uIdx', 'broadcastId', 'strParams'}:
        raise DomainError('invalid_broadcast_payload')
    if integer(data['uIdx'], 'uIdx', 0, 3) != seat['seat']:
        raise DomainError('wrong_multiplayer_player')
    if room['phase'] not in ('preparation', 'special', 'battle_countdown', 'battle', 'help', 'boss'):
        raise DomainError('wrong_multiplayer_phase', room['phase'])
    if seat.get('left') or seat.get('teamLeft') or seat.get('sceneLeft') or seat.get('quit') or not seat.get('state'):
        raise DomainError('multiplayer_seat_quit')
    ident = data['broadcastId']
    if not isinstance(ident, str) or not 1 <= len(ident) <= 128:
        raise DomainError('invalid_broadcast_payload')
    if not isinstance(data['strParams'], list) or data['strParams']:
        raise DomainError('invalid_broadcast_payload')
    broadcasts = rooms.e.broadcasts
    level = next((level for level, key in broadcasts.shop_levels.items() if key == ident), None)
    if level is None:
        # Other categories need integers absent from this upstream DTO, or
        # represent effects whose authoritative handlers own their emission.
        raise DomainError('unsupported_native_broadcast')
    state = seat['state']
    if level > state['player']['store']['level']:
        raise DomainError('broadcast_shop_level_not_reached')
    # Derive the event from the reached server level, never client parameters.
    # On a legacy save without this ledger, backfill only that proven fact.
    broadcasts.event(state, 'SHOP_LEVEL', level=level)
    return True
