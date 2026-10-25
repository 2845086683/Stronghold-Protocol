"""Read-only native settlement DTO for one authenticated terminal room/run."""
import copy
from .engine import DomainError

ACTIVITY='act2autochess'


def response(rooms, actor, room_id, match, run_id, body):
    # Exact CN AutoChessSettleGameRequest fields. This read authorizes no
    # profile or reward mutation; the accepted room reports own the result.
    if (not isinstance(body,dict) or set(body)!={'activityId','quitBattle'} or
            body['activityId']!=ACTIVITY or body['quitBattle'] is not True):
        raise DomainError('invalid_native_multiplayer_settlement')
    if (not isinstance(match,str) or not match.isascii() or not match.isdecimal() or
            match.startswith('0') or len(match)>10):
        raise DomainError('invalid_native_multiplayer_settlement_owner')
    for value in (room_id,run_id):
        if not isinstance(value,str) or len(value)!=32 or any(c not in '0123456789abcdef' for c in value):
            raise DomainError('invalid_native_multiplayer_settlement_owner')
    with rooms.store.lock:
        room=rooms._room(room_id)
        seat=rooms._seat(room,actor)
        state=seat.get('state')
        if (room.get('closed') or seat.get('teamLeft') or seat.get('returned') or
                room.get('matchSequence',1)!=int(match) or not state or state.get('runId')!=run_id):
            raise DomainError('native_multiplayer_settlement_owner_changed')
        from .multiplayer_settlement import settlement_committed
        if not settlement_committed(room, seat):
            raise DomainError('native_multiplayer_result_not_committed')
        view=rooms._view(room,actor)
        if view.get('nativeSettleGameInfoType')!='Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo':
            raise DomainError('native_multiplayer_settlement_projection_unavailable')
        return {'result':0,'gameSettleData':copy.deepcopy(view['nativeSettleGameInfo']),
                'playerDataDelta':{'modified':{},'deleted':{}}}
