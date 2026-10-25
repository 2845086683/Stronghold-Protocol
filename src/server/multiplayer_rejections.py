# SPDX-License-Identifier: GPL-3.0-or-later
"""Business failures are native responses, not lost private connections.

Only authenticated, declared preparation operations may use this path. A stale
request is never rebased or applied in a later phase. The immutable request
receipt stores the failure and the current recipient projection together.
"""
import copy
import re

from .protocol import UP
from .engine import DomainError, integer

RACES = frozenset((
    'stale_multiplayer_generation', 'preparation_view_stale_ack',
    'preparation_view_no_pending_plan', 'preparation_view_not_in_rest',
    'preparation_view_loading_expired', 'preparation_return_locked',
    'preparation_return_expired', 'wrong_multiplayer_phase',
))
BUSINESS = frozenset(('empty_shop_slot', 'insufficient_funds', 'stock_exhausted', 'max_shop_level',
                      'hand_full', 'trait_reward_selection_required', 'no_trait_reward_offer',
                      'equipment_target_not_allowed'))
VIEW_COMMANDS = frozenset(('view.ready', 'view.failed', 'view.refresh', 'ready', 'watch'))
ACTOR_FIELDS = frozenset(('uidIndex', 'playerUidIndex'))
# Pinned CN native_multiplayer_dispatch_dtos.inc field names. These validate
# syntax only; the existing transaction remains the sole economy/ownership
# executor. Optional native fields retain existing decoder/engine defaults.
NATIVE_FIELDS = {
    'AutoChessBattleSceneShopRefreshUp': ({}, {}),
    'AutoChessBattleSceneShopUpgradeUp': ({}, {}),
    'AutoChessBattleSceneShopFrozeUp': ({'isFrozen': 'bool'}, {}),
    'AutoChessBattleSceneShopBuyChessUp': ({'slotId': 'uint'}, {'isSpecial': 'bool'}),
    'AutoChessBattleSceneChangePositionUp': ({'unitPositionInfo': 'list', 'deployCount': 'uint'}, {}),
    'AutoChessBattleSceneDestroyChessUp': ({'chessInstId': 'id'}, {'chessType': 'kind'}),
    'AutoChessBattleSceneUseEquipUp': ({'charChessInstId': 'id', 'equipChessInstId': 'id'},
                                     {'isChangeEquip': 'bool', 'unloadInstId': 'optional-id'}),
    'AutoChessBattleSceneUseMagicUp': ({'instId': 'id', 'unitPositionInfo': 'list', 'deployCount': 'uint'}, {}),
    'AutoChessBattleSceneSelfChoicePickUp': ({'slotId': 'uint'}, {}),
    'AutoChessBattleSceneSpPreparationPickUp': ({'slotId': 'uint'}, {}),
}


def _actor_indices(value, seat):
    for key in ACTOR_FIELDS:
        if key in value and (type(value[key]) is not int or value[key] != seat):
            raise DomainError('wrong_multiplayer_player')


def _shape(value, required, optional=None, *, actor_fields=True):
    optional = optional or {}
    allowed_actor = ACTOR_FIELDS if actor_fields else frozenset()
    if not isinstance(value, dict) or not set(required) <= set(value) or set(value) - set(required) - set(optional) - allowed_actor:
        raise DomainError('invalid_native_command_shape')
    for key, kind in {**required, **optional}.items():
        if key not in value:
            continue
        item = value[key]
        if kind == 'bool':
            if type(item) is not bool: raise DomainError('invalid_native_command_shape')
        elif kind == 'list':
            if not isinstance(item, list): raise DomainError('invalid_native_command_shape')
        elif kind == 'sha':
            if not isinstance(item, str) or not re.fullmatch('[0-9a-f]{64}', item):
                raise DomainError('invalid_native_command_shape')
        elif kind == 'stage':
            if not isinstance(item, str) or not re.fullmatch('[a-zA-Z0-9_]{1,128}', item):
                raise DomainError('invalid_native_command_shape')
        else:
            bounds = {'uint': (0, 2147483647), 'id': (1, 2147483647), 'optional-id': (-1, 2147483647),
                      'kind': (0, 3), 'ready': (0, 1), 'seat': (0, 3), 'attempt': (1, 2147483647)}
            integer(item, key, *bounds[kind])


def _view_shape(operation, data):
    if operation == 'ready':
        _shape(data, {'readyType': 'ready'})
    elif operation == 'watch':
        _shape(data, {}, {'seat': 'seat'}, actor_fields=False)
    elif operation == 'view.ready':
        _shape(data, {'planHash': 'sha', 'attempt': 'attempt', 'stageId': 'stage',
                      'targetSeat': 'seat', 'terrainSha256': 'sha', 'targetRevision': 'uint'}, actor_fields=False)
    elif operation == 'view.failed':
        from .multiplayer_preparation_view import FAILURES
        _shape(data, {'planHash': 'sha', 'attempt': 'attempt', 'reason': 'stage'}, actor_fields=False)
        if data.get('reason') not in FAILURES: raise DomainError('invalid_preparation_view_failure')
    elif operation == 'view.refresh':
        _shape(data, {}, actor_fields=False)


def projected_failure(rooms, actor, room_id, body, error):
    operation = body.get('op')
    if not isinstance(operation, str):
        return None
    native = False
    room = rooms._room(room_id)
    member = rooms._seat(room, actor)
    if any(member.get(key) for key in ('quit', 'sceneLeft', 'returned', 'left')):
        return None
    _actor_indices(body, member['seat'])
    data = body.get('payload')
    if not isinstance(data, dict): return None
    _actor_indices(data, member['seat'])
    if operation == 'protocol':
        request = data
        if not isinstance(request, dict) or not isinstance(request.get('payload'), dict):
            return None
        if set(request) - {'type', 'payload', 'protocolId'} - ACTOR_FIELDS: return None
        kind = request.get('type')
        ids = {key.rsplit('+', 1)[-1]: value for key, value in rooms.c.constants['protocolIds'].items()}
        if type(kind) is int:
            name = next((key for key, value in ids.items() if value == kind), '')
        elif isinstance(kind, str):
            name = kind.rsplit('+', 1)[-1]
        else:
            return None
        if name not in ids or ('protocolId' in request and (type(request['protocolId']) is not int or request['protocolId'] != ids[name])):
            return None
        data = request['payload']
        _actor_indices(data, member['seat'])
        operation = UP.get(name)
        if name == 'AutoChessBattleScenePreparationReadyUp':
            operation = 'ready'
            _view_shape(operation, data)
        elif name == 'AutoChessBattleSceneObserveUp':
            operation = 'watch'
            _shape(data, {'obIndex': 'seat'}, actor_fields=False)
        elif name == 'AutoChessBattleSceneCancelObserveUp':
            operation = 'watch'
            _shape(data, {}, actor_fields=False)
        elif name in NATIVE_FIELDS:
            _shape(data, *NATIVE_FIELDS[name])
            # Stateless decoder validates encoded board positions, all Int32
            # declarations and duplicate IDs without applying any world write.
            decoded, _ = rooms.protocol.decode(name, data)
            if decoded != operation: return None
        else:
            return None
        native = name in NATIVE_FIELDS or operation == 'ready'
        if not native and operation != 'watch':
            return None
    elif operation in VIEW_COMMANDS:
        _view_shape(operation, data)
        if operation == 'view.ready' and 'level_' + data['stageId'] not in rooms.c.data['levelTemplates']:
            raise DomainError('invalid_native_command_shape')
    else:
        # Raw debug economy commands have no separately reusable input schema.
        # Only product native DTOs and explicit view/ready/watch commands enter
        # this semantic response path; arbitrary domain failures still fail.
        return None
    if (native or operation == 'ready') and (not member.get('state') or member['state']['player']['hp'] <= 0):
        return None
    if error.code not in RACES and not (native and error.code in BUSINESS):
        return None
    view = rooms._view(room, actor)
    if view.get('left') or not view.get('selfState') or not view.get('dtoProjection'):
        return None
    response = copy.deepcopy(view['dtoProjection']['preparation'])
    response['payload']['code'] = rooms.c.enum(
        'Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessScenePreparationStatusResp+RetCode', 'FAIL')
    view['dtoProjection']['preparation'] = response
    view['commandRejected'] = {'requestId': body['requestId'], 'error': error.code}
    return view
