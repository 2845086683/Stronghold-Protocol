"""Successful domain events projected to the existing CN battlefield radio.

The ledger is part of the command transaction, not a message queue polled by
GET. A committed command's immutable ACK may be retried; its stable event IDs
let the native adapter display it once even when the first ACK was lost.
Reading old saves must never initialize this ledger or replay their history.
"""
import copy

VERSION = 1


class BattlefieldBroadcasts:
    def __init__(self, engine):
        self.e = engine
        self.c = engine.c
        self.definitions = {row['Id']: row for row in self.c.data['common']['BroadcastList']}
        self.shop_levels = {int(row['ParamList'][0]): row['Id']
                            for row in self.definitions.values()
                            if row['Type'] == 2 and row.get('ParamList')}

    @staticmethod
    def _fail(code):
        from .engine import DomainError
        raise DomainError(code)

    @staticmethod
    def initialize(state):
        state['nativeBroadcastState'] = {'version': VERSION, 'seq': 0,
                                         'seen': {}, 'events': []}

    def _uid(self, value):
        if type(value) is not int or not 0 <= value <= 3:
            self._fail('invalid_broadcast_player')
        return value

    def _emit(self, state, key, broadcast_id, uid, strings=None, integers=None,
              target=None):
        if broadcast_id not in self.definitions:
            self._fail('unknown_native_broadcast')
        uid = self._uid(uid)
        if target is not None:
            target = self._uid(target)
        ledger = state.get('nativeBroadcastState')
        if ledger is None:
            # Only an actual new event upgrades an existing live save.
            self.initialize(state)
            ledger = state['nativeBroadcastState']
        if ledger.get('version') != VERSION:
            self._fail('native_broadcast_version_mismatch')
        if key in ledger['seen']:
            previous = next(row for row in ledger['events'] if row['seq'] == ledger['seen'][key])
            original = previous['message']
            if (original['broadcastId'] != broadcast_id or original['uIdx'] != uid or
                    original['strParams'] != list(strings or []) or
                    original['intParams'] != list(integers or []) or previous['targetUidIndex'] != target):
                self._fail('conflicting_broadcast_event')
            return None
        seq = ledger['seq'] + 1
        message = {'eventId': state['runId'] + ':' + str(seq),
                   'runId': state['runId'], 'round': state['round'],
                   'uIdx': uid, 'broadcastId': broadcast_id,
                   'strParams': list(strings or []), 'intParams': list(integers or [])}
        ledger['events'].append({'seq': seq, 'key': key, 'targetUidIndex': target,
                                 'message': message})
        ledger['seq'] = seq
        ledger['seen'][key] = seq
        return copy.deepcopy(message)

    def event(self, state, event, **payload):
        """Called by successful ownership/economy handlers, never client input."""
        uid = self._uid(state['player']['uidIndex'])
        if event == 'SHOP_LEVEL':
            level = payload.get('level')
            if type(level) is not int or level not in self.shop_levels:
                self._fail('invalid_broadcast_shop_level')
            return self._emit(state, 'shop:' + str(uid) + ':' + str(level),
                              self.shop_levels[level], uid)
        if event == 'CHAR_ELITE':
            subject = payload.get('subject')
            if not isinstance(subject, dict):
                self._fail('invalid_broadcast_elite')
            ident = subject.get('chessIdentifier')
            inst = subject.get('instId')
            if type(ident) is not int or type(inst) is not int or inst < 1:
                self._fail('invalid_broadcast_elite')
            key = self.c.data['identifiers'].get(str(ident))
            definition = self.c.chess.get(key)
            if (key not in self.c.season['CharChessDataDict'] or
                    not definition or not definition.get('IsGolden')):
                return None
            # One original elite acquisition can recurse into other gains;
            # its historical subject survives all resulting in-place merges.
            return self._emit(state, 'elite:' + str(uid) + ':' + str(inst) + ':' + str(ident),
                              'comment_golden_char', uid, integers=[ident])
        if event == 'CHAR_GIFT':
            return self._gift(state, payload)
        self._fail('unknown_broadcast_event')

    def _gift(self, state, payload):
        """Future completed transfer only: a private, recipient-directed event.

        Native ParseMsg uses intParams[0] for the sender's name and uses uIdx
        plus chessId for operator resolution. Choosing the recipient's squad
        is the explicit private transfer policy, not an inferred wire codec.
        There is intentionally no solo HTTP endpoint that manufactures gifts.
        """
        sender = self._uid(payload.get('senderUidIndex'))
        recipient = self._uid(payload.get('recipientUidIndex'))
        transfer = payload.get('transferId')
        chess = payload.get('chessId')
        participants = state.get('players')
        indices = ({row.get('uidIndex') for row in participants if isinstance(row, dict)}
                   if isinstance(participants, list) else {state['player']['uidIndex']})
        if sender == recipient or not {sender, recipient} <= indices:
            self._fail('broadcast_gift_requires_participants')
        if (not isinstance(transfer, str) or not 1 <= len(transfer) <= 128 or
                not isinstance(chess, str) or chess not in self.c.chess):
            self._fail('invalid_broadcast_gift')
        return self._emit(state, 'gift:' + transfer, 'comment_char_gift', recipient,
                          strings=[chess], integers=[sender], target=recipient)

    @staticmethod
    def delta(before, after, *, recipient_uid_index=None):
        """Pure projection; reconnect/current/result reads deliver no events."""
        if before is None or before.get('runId') != after.get('runId'):
            return []
        previous = before.get('nativeBroadcastState') or {}
        ledger = after.get('nativeBroadcastState') or {}
        cutoff = previous.get('seq', 0)
        # Coordinated player engines retain their solo-local uidIndex=0.
        # The room must supply its authenticated seat for directed messages.
        receiver = after['player']['uidIndex'] if recipient_uid_index is None else recipient_uid_index
        return [copy.deepcopy(row['message']) for row in ledger.get('events', [])
                if row['seq'] > cutoff and row.get('targetUidIndex') in (None, receiver)]
