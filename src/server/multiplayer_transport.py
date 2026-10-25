"""Bounded transport aggregation; original command receipts remain authoritative."""
import gzip
import re
from .engine import DomainError

BATTLE_UP = frozenset('AutoChessBattleScene' + name + 'Up' for name in (
    'SelfBattleAddBond', 'SelfBattleEnemyEscape', 'SelfBattleKillEnemy',
    'SelfBattleInfo', 'SelfBattleFinish', 'HelpBattleKillEnemy',
    'HelpBattleEnemyEscape', 'HelpBattleFinish', 'Action'))


def view_options(headers):
    """Optional native presentation hints, separate from immutable commands."""
    options = {'native_view': headers.get('X-Stronghold-Native-View') == '1'}
    for header, key, minimum in (('X-Stronghold-After-Event', 'after_event', 0),
            ('X-Stronghold-After-Step', 'after_step', -1),
            ('X-Stronghold-Step-Epoch', 'step_epoch', 0)):
        raw = headers.get(header)
        if raw is None: continue
        if not re.fullmatch(r'-?[0-9]{1,18}', raw) or int(raw) < minimum:
            raise DomainError('invalid_multiplayer_view_cursor')
        options[key] = int(raw)
    return options


def command_view_options(room, options):
    # A stale scene cursor must not roll back an otherwise valid command.
    # A future cursor restarts bounded replay instead of skipping unseen data.
    result = dict(options)
    if result.get('after_event', 0) > room['eventSeq']: result['after_event'] = 0
    native = room.get('nativeBoss') or {}
    if result.get('after_step', -1) > native.get('lastSeq', -1): result['after_step'] = -1
    return result


def execute_batch(rooms, actor, room_id, body, *, view_options=None):
    if not isinstance(body, dict) or set(body) != {'commands'}:
        raise DomainError('invalid_multiplayer_batch')
    commands = body['commands']
    if not isinstance(commands, list) or not 2 <= len(commands) <= 16:
        raise DomainError('invalid_multiplayer_batch')
    ids = set()
    for command in commands:
        if not isinstance(command, dict) or set(command) != {'requestId', 'op', 'generation', 'payload'}:
            raise DomainError('invalid_multiplayer_batch')
        request = command['requestId']
        payload = command['payload']
        if (not isinstance(request, str) or not re.fullmatch('[a-f0-9]{32}', request) or request in ids or
                command['op'] != 'protocol' or type(command['generation']) is not int or command['generation'] < 1 or
                not isinstance(payload, dict) or set(payload) != {'type', 'payload'} or
                not isinstance(payload['type'], str) or payload['type'].rsplit('+', 1)[-1] not in BATTLE_UP or
                not isinstance(payload['payload'], dict)):
            raise DomainError('invalid_multiplayer_batch')
        ids.add(request)
    outcomes = []
    # Do not wrap these in another SQL transaction. Each immutable command has
    # its own existing receipt/rollback. A lost aggregate ACK replays that prefix
    # byte-for-byte; it never re-executes committed battle effects.
    with rooms.store.lock:
        rooms._seat(rooms._room(room_id), actor)
        for command in commands:
            try:
                reply = rooms.execute(actor, room_id, command, view_options=view_options)
            except DomainError as error:
                outcomes.append({'requestId': command['requestId'], 'accepted': False,
                                 'error': error.code})
                break  # Untouched suffix remains queued at the client.
            outcomes.append({'requestId': command['requestId'], 'accepted': True})
        room = rooms._room(room_id)
        reply = rooms._view(room, actor, **command_view_options(room, view_options or {}))
    reply['commandBatchResults'] = outcomes
    return reply


def encoded_reply(raw, accept_encoding):
    """Low CPU compression negotiated only with clients that can decode it."""
    quality = {}
    for item in accept_encoding.lower().split(','):
        parts = [part.strip() for part in item.split(';')]
        q = 1.0
        for parameter in parts[1:]:
            if parameter.startswith('q='):
                try: q = float(parameter[2:])
                except ValueError: q = 0
        quality[parts[0]] = q
    if len(raw) >= 4096 and 0 < quality.get('gzip', quality.get('*', 0)) <= 1:
        compressed = gzip.compress(raw, compresslevel=1, mtime=0)
        if len(compressed) < len(raw): return compressed, True
    return raw, False
