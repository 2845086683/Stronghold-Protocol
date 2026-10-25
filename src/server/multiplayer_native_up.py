"""Small room-only adapters for recovered CN observation declarations.

Unknown requests still fail closed. The ordinary economy/kill/finish adapters
remain in Protocol.decode; those are reused by solo and multiplayer alike.
"""
from .engine import DomainError, integer


def apply_observation(rooms, room, seat, name, data):
    if name not in ('AutoChessBattleSceneObserveUp', 'AutoChessBattleSceneCancelObserveUp',
                    'AutoChessBattleSceneDeadAutoObUp'):
        return False
    if name == 'AutoChessBattleSceneObserveUp':
        if set(data) != {'obIndex'}:
            raise DomainError('invalid_observation_payload')
        target = integer(data['obIndex'], 'obIndex', 0, 3)
    else:
        if data:
            raise DomainError('invalid_observation_payload')
        target = seat['seat']
        if name == 'AutoChessBattleSceneDeadAutoObUp':
            if seat['state'] is None or seat['state']['player']['hp'] > 0:
                raise DomainError('automatic_observation_requires_dead_player')
            alive = rooms._alive(room)
            if alive:
                target = alive[0]['seat']
    rooms._apply(room, seat, 'watch', {'seat': target})
    return True
