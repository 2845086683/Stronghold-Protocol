# SPDX-License-Identifier: GPL-3.0-or-later
"""Durable custom-room coordinator over the existing CN economic engine.

This is the private bridge, not the official socket codec. Unity remains the
battle executor. No simulated battle, invented kill, or cross-seat inventory
write is used to get through a barrier. See docs/multiplayer-independent.md.
"""
import copy
import hashlib
import hmac
import json
import secrets
import time
import uuid

from .activity_profile import ACTIVITY, ActivityProfile
from .engine import DomainError, integer
from .native_combat import NativeCombat
from .preset_snapshot import snapshot_profile
from .setup_rules import stream
from .storage import dump

MODES = ('mode_multi_funny', 'mode_multi_normal', 'mode_multi_hard', 'mode_multi_abyss')
PREP_OPS = frozenset(('shop.buy', 'shop.refresh', 'shop.freeze', 'shop.upgrade',
    'board.move', 'board.destroy', 'board.equip', 'item.use', 'choice.pick'))


def digest(value):
    return hashlib.sha256(dump(value).encode()).hexdigest()


def text_id(value, name='requestId'):
    if not isinstance(value, str) or not 1 <= len(value) <= 128:
        raise DomainError('invalid_' + name)
    return value


class MultiplayerRooms:
    def __init__(self, store, protocol, activity=None, clock=None):
        self.store = store; self.e = store.engine; self.c = self.e.c; self.protocol = protocol
        self.activity = activity or ActivityProfile(store); self.clock = clock or time.time
        self.combat = NativeCombat(self.e, self.clock)
        with store.lock, store.db:
            store.db.executescript('''
                CREATE TABLE IF NOT EXISTS multiplayer_participants(
                    id TEXT PRIMARY KEY, token_hash TEXT NOT NULL, profile TEXT NOT NULL);
                CREATE TABLE IF NOT EXISTS multiplayer_rooms(
                    id TEXT PRIMARY KEY, code TEXT UNIQUE NOT NULL, state TEXT NOT NULL);
                CREATE TABLE IF NOT EXISTS multiplayer_receipts(
                    actor TEXT NOT NULL, scope TEXT NOT NULL, request TEXT NOT NULL,
                    fingerprint TEXT NOT NULL, reply TEXT NOT NULL,
                    PRIMARY KEY(actor,scope,request));
                CREATE TABLE IF NOT EXISTS multiplayer_room_faults(
                    room TEXT PRIMARY KEY, error TEXT NOT NULL);
            ''')
            from .multiplayer_trophies import initialize as initialize_trophies
            initialize_trophies(store.db)
            from .multiplayer_clock import initialize
            initialize(self)

    def register(self, body):
        if not isinstance(body, dict) or set(body) - {'nickname'}:
            raise DomainError('invalid_participant_registration')
        name = body.get('nickname', '本地博士')
        if not isinstance(name, str) or not 1 <= len(name) <= 32 or any(ord(x) < 32 for x in name):
            raise DomainError('invalid_nickname')
        participant = uuid.uuid4().hex; token = secrets.token_urlsafe(32)
        profile = {'nickname': name, 'squad': self.activity.defaults()['playerActivity']['chessSquad']}
        with self.store.lock, self.store.db:
            self.store.db.execute('INSERT INTO multiplayer_participants VALUES(?,?,?)',
                                 (participant, digest(token), dump(profile)))
        return {'ok': True, 'participantId': participant, 'token': token, 'officialFallback': False}

    def authenticate(self, authorization):
        if not isinstance(authorization, str) or not authorization.startswith('Bearer '):
            raise DomainError('multiplayer_authentication_required')
        token = authorization[7:]
        if not 32 <= len(token) <= 128: raise DomainError('multiplayer_authentication_required')
        with self.store.lock:
            row = self.store.db.execute('SELECT id,token_hash FROM multiplayer_participants WHERE token_hash=?',
                                        (digest(token),)).fetchone()
        if row is None or not hmac.compare_digest(row[1], digest(token)):
            raise DomainError('multiplayer_authentication_required')
        return row[0]

    def _profile(self, actor):
        row = self.store.db.execute('SELECT profile FROM multiplayer_participants WHERE id=?', (actor,)).fetchone()
        if row is None: raise DomainError('unknown_multiplayer_participant')
        return json.loads(row[0])

    def profile(self, actor):
        with self.store.lock:
            return {'ok': True, 'participantId': actor, 'profile': self._profile(actor), 'officialFallback': False}

    def bootstrap(self, actor):
        """CN-only UI data owned by one participant, including on a LAN host.

        Legacy activity_profiles/native-session state is intentionally not read.
        A remote client needs no operator credential or shared solo profile.
        """
        with self.store.lock:
            private = self._profile(actor)
            profile = self.activity.defaults()
            profile['playerActivity']['chessSquad'] = copy.deepcopy(private['squad'])
            profile['playerActivity']['trophyNum'] = private.get('trophyNum', 0)
            model = {'troop': copy.deepcopy(self.activity._roster), 'autoChessPerm': copy.deepcopy(profile['autoChessPerm']),
                # Native emote UI reads ownership from this participant projection.
                # Keep it derived from the activity catalog, including existing saves.
                'emoticon': {'unlockTheme': list(self.c.common['EnabledEmoticonThemeIdList'])},
                'activity': {'actAutoChessActivityList': {ACTIVITY: copy.deepcopy(profile['playerActivity'])}},
                'status': {'nickName': private['nickname'], 'nickNumber': '0000', 'level': 120},
                'inventory': {}, 'skin': {'characterSkins': {}, 'skinTs': {}, 'skinSp': {}}}
            from .multiplayer_reconnect import presence
            battle_info, native_resume = presence(self, actor)
            return {'ok': True, 'officialFallback': False, 'participantId': actor, 'catalogHash': self.c.hash,
                'playerActivity': profile['playerActivity'], 'playerModel': model, 'perm': profile['autoChessPerm'],
                'charMap': {value['charId']: copy.deepcopy(value) for value in self.activity._roster['chars'].values()},
                'syncInfo': {'changed': [], 'battleInfo': battle_info}, 'nativeResume': native_resume, 'profile': profile,
                'nativeAccepted': False, 'playerDataSource': 'participant private CN catalog; no official account'}

    def configure(self, actor, body):
        def operation():
            profile = self._profile(actor)
            proxy = {'playerActivity': {'chessSquad': copy.deepcopy(profile['squad'])}}
            payload = body.get('payload', {})
            if not isinstance(payload, dict): raise DomainError('invalid_payload')
            if body.get('op') == 'replaceSquad':
                from .multiplayer_profile import replace_squad
                proxy['playerActivity']['chessSquad'] = replace_squad(self.activity, payload)
            else:
                self.activity.apply_configuration(proxy, body.get('op'), payload)
            profile['squad'] = proxy['playerActivity']['chessSquad']
            self.store.db.execute('UPDATE multiplayer_participants SET profile=? WHERE id=?', (dump(profile), actor))
            return {'ok': True, 'participantId': actor, 'profile': profile, 'officialFallback': False}
        return self._receipt(actor, 'profile', body, operation)

    def _room(self, room_id):
        row = self.store.db.execute('SELECT state FROM multiplayer_rooms WHERE id=?', (room_id,)).fetchone()
        if row is None: raise DomainError('unknown_multiplayer_room')
        room = json.loads(row[0])
        if room['catalogHash'] != self.c.hash or room['version'] != 1:
            raise DomainError('multiplayer_save_version_mismatch')
        self._include_help_observers(room)
        return room

    @staticmethod
    def _include_help_observers(room):
        plan = room.get('help')
        if room['phase'] != 'help' or not plan: return
        # An eliminated retained member can load/watch the shared field too.
        # Reporting that field grants no defender, inventory or input rights;
        # _alive excludes this seat from the Help completion barrier. Include
        # it on old saves as a read-compatible projection, persisted only by
        # the next ordinary authenticated transaction.
        observers = {row['seat'] for row in room['seats'] if row.get('state') and
            not row.get('quit') and not row.get('left') and not row.get('teamLeft') and
            row['state']['player']['hp'] <= 0}
        plan['spectatorSeats'] = sorted(set(plan['spectatorSeats']) | observers)

    def _save(self, room):
        from .multiplayer_trophies import commit
        commit(self, room)
        self.store.db.execute('UPDATE multiplayer_rooms SET state=? WHERE id=?', (dump(room), room['roomId']))
        from .multiplayer_clock import schedule
        schedule(self, room)

    def _receipt(self, actor, scope, body, operation, replay_projection=None):
        if not isinstance(body, dict): raise DomainError('invalid_payload')
        request = text_id(body.get('requestId')); fingerprint = digest(body)
        with self.store.lock:
            self.store.db.execute('BEGIN IMMEDIATE')
            try:
                old = self.store.db.execute('SELECT fingerprint,reply FROM multiplayer_receipts WHERE actor=? AND scope=? AND request=?',
                                            (actor, scope, request)).fetchone()
                if old:
                    if old[0] != fingerprint: raise DomainError('request_id_conflict')
                    from .receipt_codec import decode_reply
                    reply = decode_reply(old[1])
                    if replay_projection is not None: reply = replay_projection(reply)
                    self.store.db.commit(); return reply
                self._profile(actor)
                reply = operation()
                from .receipt_codec import encode_reply
                self.store.db.execute('INSERT INTO multiplayer_receipts VALUES(?,?,?,?,?)',
                                      (actor, scope, request, fingerprint, encode_reply(reply)))
                self.store.db.commit(); return reply
            except Exception:
                self.store.db.rollback(); raise

    def _seat(self, room, actor):
        seat = next((v for v in room['seats'] if v['participantId'] == actor and not v.get('left')), None)
        if seat is None: raise DomainError('not_a_room_member')
        return seat

    def _event(self, room, kind, payload, recipients=None):
        room['eventSeq'] += 1
        room['events'].append({'seq': room['eventSeq'], 'eventId': room['roomId'] + ':' + str(room['eventSeq']),
            'generation': room['generation'], 'kind': kind, 'payload': copy.deepcopy(payload),
            'recipients': recipients})

    def _phase(self, room, phase, seconds=None):
        room['generation'] += 1; room['phase'] = phase
        from .multiplayer_lobby import touch
        if phase == 'building': touch(self, room)
        else: room['idleDeadlineTs'] = None
        room['deadlineTs'] = int(self.clock()) + seconds if seconds is not None else None
        if phase == 'special': room['specialFinTs'] = room['deadlineTs']
        room['settleAfterTs'] = room['deadlineTs'] + 5 if phase in ('battle', 'help') and seconds is not None else None
        self._event(room, 'phase', {'phase': phase, 'round': room['round'], 'deadlineTs': room['deadlineTs']})

    def create(self, actor, body):
        def operation():
            mode = body.get('modeId', 'mode_multi_funny')
            if mode not in MODES: raise DomainError('multiplayer_custom_mode_required')
            if any(self._seat_present(json.loads(row[0]), actor) for row in self.store.db.execute('SELECT state FROM multiplayer_rooms')):
                raise DomainError('participant_already_in_room')
            self._capture_profile(actor, body)
            from .multiplayer_identity import capture
            capture(self, actor, body)
            room_id = uuid.uuid4().hex
            while True:
                code = str(secrets.randbelow(900000) + 100000)
                if self.store.db.execute('SELECT 1 FROM multiplayer_rooms WHERE code=?', (code,)).fetchone() is None: break
            room = {'version': 1, 'roomId': room_id, 'code': code, 'catalogHash': self.c.hash,
                'modeId': mode, 'phase': 'building', 'generation': 1, 'round': 0,
                'owner': actor, 'matchSeed': secrets.randbits(32) or 1, 'deadlineTs': None,
                'idleDeadlineTs': int(self.clock()) + 900,
                'seats': [], 'eventSeq': 0, 'events': [], 'reports': {}, 'roundHistory': [], 'help': None}
            self._add_seat(room, actor)
            self.store.db.execute('INSERT INTO multiplayer_rooms VALUES(?,?,?)', (room_id, code, dump(room)))
            from .multiplayer_clock import schedule
            schedule(self, room)
            return self._view(room, actor)
        return self._receipt(actor, 'create-room', body, operation)

    @staticmethod
    def _seat_present(room, actor):
        return any(v['participantId'] == actor and not v.get('left') and not v.get('teamLeft') for v in room['seats'])

    def _add_seat(self, room, actor):
        used = {v['seat'] for v in room['seats'] if not v.get('left')}
        if len(used) >= 4: raise DomainError('room_full')
        index = next(v for v in range(4) if v not in used)
        room['seats'] = [v for v in room['seats'] if v['seat'] != index]
        profile = self._profile(actor)
        room['seats'].append({'seat': index, 'participantId': actor, 'nickname': profile['nickname'],
            'trophyNum': profile.get('trophyNum', 0),
            'displayProfile': copy.deepcopy(profile.get('displayProfile', {})),
            'connected': True, 'lastSeenTs': int(self.clock()), 'joinedTs': int(self.clock()), 'confirmed': False, 'loaded': False,
            'ready': actor == room['owner'], 'watchSeat': index, 'state': None, 'left': False})
        room['seats'].sort(key=lambda v: v['seat'])
        self._event(room, 'member-joined', {'seat': index})

    def join(self, actor, code, body):
        def operation():
            if not isinstance(code, str) or len(code) != 6 or not code.isascii() or not code.isdecimal():
                raise DomainError('invalid_room_code')
            found = self.store.db.execute('SELECT id FROM multiplayer_rooms WHERE code=?', (code,)).fetchone()
            if found is None: raise DomainError('unknown_multiplayer_room')
            room = self._room(found[0])
            if room.get('closed'): raise DomainError('multiplayer_room_closed')
            if self._seat_present(room, actor): return self._view(room, actor)
            if room['phase'] != 'building': raise DomainError('room_already_started')
            if any(self._seat_present(json.loads(row[0]), actor) for row in self.store.db.execute('SELECT state FROM multiplayer_rooms')):
                raise DomainError('participant_already_in_room')
            self._capture_profile(actor, body)
            from .multiplayer_identity import capture
            from .multiplayer_lobby import touch
            capture(self, actor, body)
            self._add_seat(room, actor); touch(self, room); self._save(room)
            return self._view(room, actor)
        return self._receipt(actor, 'join:' + str(code), body, operation)

    def _capture_profile(self, actor, body):
        if 'profileSquad' not in body: return
        from .multiplayer_profile import replace_squad
        profile = self._profile(actor)
        profile['squad'] = replace_squad(self.activity, {'chessSquad': body['profileSquad']})
        self.store.db.execute('UPDATE multiplayer_participants SET profile=? WHERE id=?', (dump(profile), actor))

    def get(self, actor, room_id, after_event=0, after_step=-1, step_epoch=0, *, native_view=False):
        integer(after_event, 'afterEvent')
        with self.store.lock: return self._view(self._room(room_id), actor, after_event, after_step, step_epoch, native_view=native_view)

    def native_start(self, actor, body):
        """ACK the original MULTI HTTP handshake after private room startup.

        BattleStartController consumes CommonStartBattleResponse before loading
        Unity. This read grants no loading-ready/view-ready/battle-start proof.
        """
        if not isinstance(body, dict) or set(body) != {'activityId', 'sceneId'} or body['activityId'] != ACTIVITY:
            raise DomainError('invalid_native_multiplayer_start')
        scene = body['sceneId']
        if not isinstance(scene, str) or len(scene) != 32 or any(c not in '0123456789abcdef' for c in scene):
            raise DomainError('invalid_native_multiplayer_scene')
        with self.store.lock:
            room = self._room(scene); seat = self._seat(room, actor)
            if room.get('closed') or seat.get('quit') or seat.get('teamLeft'):
                raise DomainError('multiplayer_seat_quit')
            self._require(room, 'loading', 'preparation', 'special', 'battle_countdown', 'battle', 'help', 'boss')
            state = seat.get('state')
            if not state or not isinstance(state.get('runId'), str) or not state['runId']:
                raise DomainError('native_multiplayer_scene_not_ready')
            return {'result': 0, 'battleId': state['runId'], 'pushMessage': [],
                    'playerDataDelta': {'modified': {}, 'deleted': {}}}

    def native_finish(self, actor, body):
        """Acknowledge native MULTI scene teardown after the committed result.

        CommonFinishBattleRequest carries opaque client battle data. It is not
        a second battle outcome: the room's accepted reports own HP/results.
        Retries read the same terminal state and never advance/reward the room.
        """
        if (not isinstance(body, dict) or set(body) - {'activityId', 'sceneId', 'data', 'battleData'} or
                body.get('activityId') != ACTIVITY):
            raise DomainError('invalid_native_multiplayer_finish')
        scene = body.get('sceneId')
        if not isinstance(scene, str) or len(scene) != 32 or any(c not in '0123456789abcdef' for c in scene):
            raise DomainError('invalid_native_multiplayer_scene')
        if ('data' in body and body['data'] is not None and not isinstance(body['data'], str) or
                'battleData' in body and body['battleData'] is not None and not isinstance(body['battleData'], dict)):
            raise DomainError('invalid_native_multiplayer_finish')
        with self.store.lock:
            room = self._room(scene); seat = self._seat(room, actor)
            from .multiplayer_settlement import settlement_committed
            if not settlement_committed(room, seat):
                raise DomainError('native_multiplayer_result_not_committed')
            return {'result': 0, 'apFailReturn': 0, 'itemReturn': [], 'rewards': [],
                    'unusualRewards': [], 'overrideRewards': [], 'additionalRewards': [],
                    'diamondMaterialRewards': [], 'furnitureRewards': [], 'pushMessage': [],
                    'playerDataDelta': {'modified': {}, 'deleted': {}}}

    def _view(self, room, actor, after_event=0, after_step=-1, step_epoch=0, *, native_view=False):
        if room.get('closed'):
            from .multiplayer_lobby import closed_view
            return closed_view(room, actor)
        seat = self._seat(room, actor)
        watched = seat['watchSeat'] if room['phase'] in ('preparation', 'special') else seat['seat']
        target = next(v for v in room['seats'] if v['seat'] == watched)
        if target['state'] is None and target.get('quit'): target = seat
        if target.get('state') and seat.get('state') and target['state']['stageId'] != seat['state']['stageId']: target = seat
        # A read never refreshes deadlines, RNGs, effects, or a single-player profile.
        from .multiplayer_lobby import ready as lobby_ready
        roster = [{'seat': v['seat'], 'participantId': v['participantId'], 'nickname': v['nickname'],
            'connected': v['connected'], 'ready': lobby_ready(room, v), 'loaded': v['loaded'], 'left': v['left'],
            'hp': v['state']['player']['hp'] if v['state'] else None,
            'stageId': v['state']['stageId'] if v['state'] else None} for v in room['seats']]
        public_board = None
        if target['state'] and room['phase'] in ('preparation', 'special', 'help', 'battle', 'boss', 'result'):
            player = target['state']['player']
            public_board = {key: copy.deepcopy(player[key]) for key in ('charChess', 'equipChess', 'trapChess', 'positions', 'bonds')}
            public_board.update(seat=target['seat'], stageId=target['state']['stageId'], nativeSquad=copy.deepcopy(target['state'].get('nativeSquad', [])),
                                specialEffects=copy.deepcopy(target['state']['specialEffects']))
        result = {'ok': True, 'activityId': ACTIVITY, 'roomId': room['roomId'], 'roomCode': room['code'],
            'matchSequence': room.get('matchSequence', 1),
            'selfScenePresence': {'seat': seat['seat'], 'matchSequence': room.get('matchSequence', 1),
                'sceneLeft': bool(seat.get('sceneLeft')), 'quit': bool(seat.get('quit')),
                'returned': bool(seat.get('returned'))},
            'serverTime': int(self.clock()),
            'modeId': room['modeId'], 'phase': room['phase'], 'generation': room['generation'], 'round': room['round'],
            'ownerSeat': next(v['seat'] for v in room['seats'] if v['participantId'] == room['owner']),
            'seat': seat['seat'], 'roster': roster, 'deadlineTs': room['deadlineTs'], 'eventSeq': room['eventSeq'],
            'roomIdleUntilTs': room.get('idleDeadlineTs'),
            'settleAfterTs': room.get('settleAfterTs'), 'fault': copy.deepcopy(room.get('fault')),
            'events': [copy.deepcopy(v) for v in room['events'] if v['seq'] > after_event and
                       (v['recipients'] is None or seat['seat'] in v['recipients'])],
            # The native HTTP contract already omits audit history. Exclude it
            # before copying/receipt compression, not after serializing it.
            'selfState': copy.deepcopy({k: v for k, v in seat['state'].items() if k != 'history'}
                if native_view and seat['state'] else seat['state']), 'observedBoard': public_board,
            'selectionQueue': copy.deepcopy(room.get('selectionQueue', [])), 'queueIndex': room.get('queueIndex', 0),
            'jointDefense': copy.deepcopy(room['help']), 'officialFallback': False, 'officialWireCompatible': False,
            'bossTimeline': copy.deepcopy({k: v for k, v in room['bossTimeline'].items() if k != 'reports'}
                if native_view and room.get('bossTimeline') else room.get('bossTimeline')), 'result': copy.deepcopy(room.get('result')),
            'partyReturn': {'policy': 'all-remaining-party-members-return-or-leave-v1',
                'returnedSeats': [row['seat'] for row in room['seats'] if row.get('returned')],
                'pendingSeats': [row['seat'] for row in room['seats'] if room['phase'] == 'result' and not row.get('returned') and not row.get('teamLeft') and not row.get('left')]},
            'nativeAccepted': False, 'transport': 'private HTTP room bridge',
            'reconnectCapability': {'durableRoomSnapshot': True, 'preparationResume': True,
                'runningUnityBattleContinuation': False, 'lostUnityMidBattleReconstruction': True,
                'absenceGraceRounds': 2, 'missingTerminalReportPolicy': 'private-two-round-reconstruction-v1'},
            'pending': ['native custom-room HTTP callbacks and persistent socket dispatcher',
                        'native cross-map help battlefield loading and terminal Step param contract']}
        if room['phase'] == 'help' and result['jointDefense']:
            # A first terminal partition freezes the internal outcome ledger;
            # it does not end the shared scene before all participants finish.
            # Existing native carry/counter consumers require the unchanged
            # battle lifecycle and immutable field/segment identity throughout
            # this barrier. Do not export internal ledger closure as scene end.
            result['jointDefense']['status'] = 'battle'
            result['jointDefense']['completedReport'] = None
        from .multiplayer_projection import project_room
        result['dtoProjection'] = project_room(self.protocol, room, seat['seat'])
        from .multiplayer_settlement import TYPE, project_settlement
        result['nativeSettleGameInfoType'] = TYPE
        result['nativeSettleGameInfo'] = project_settlement(self.protocol, room, seat)
        if (seat.get('quit') or seat.get('sceneLeft')) and not seat.get('teamLeft') and not seat.get('returned'):
            # Official QuitUp removes this seat from the scene with Kick(7),
            # independently of its surviving teammates. This private envelope
            # is recipient bound; it does not invent a team SETTLE transition.
            result['nativeSceneExit'] = {'reason': 7, 'kind': 'voluntary' if seat.get('quit') else 'completed',
                'roomId': room['roomId'], 'matchSequence': room.get('matchSequence', 1),
                'seat': seat['seat']}
        from .multiplayer_native_boss import view as boss_view
        result['bossNative'] = boss_view(room, seat['seat'], after_step, step_epoch)
        from .multiplayer_reconstruction import waiting
        if waiting(room, seat):
            result['selfReconstruction'] = copy.deepcopy(seat['reconstruction'])
            result['bossNative'] = None
        if room['help']:
            from .joint_defense import remaining_by_owner, source_remaining_by_owner
            result['jointDefenseRemainingByOwner'] = remaining_by_owner(room['help'])
            result['jointDefenseSourceRemainingByOwner'] = source_remaining_by_owner(room['help'])
        # A withdrawn seat keeps its result ledger, but no longer has Continue
        # presence. ACK that departure on reads too, including after settlement.
        if seat.get('teamLeft'): result['left'] = True
        if seat.get('reconnectSettlementPending') and not seat.get('returned') and not seat.get('teamLeft'):
            # A recipient-only terminal presentation. The surviving team's
            # phase, HP, clock and persisted states remain authoritative.
            terminal_room = {**room, 'phase': 'result', 'deadlineTs': None}
            result.update(phase='result', deadlineTs=None, nativeSettlementPending=True,
                dtoProjection=project_room(self.protocol, terminal_room, seat['seat']),
                bossNative=None, nativeField=None, jointDefense=None)
            result.pop('selfReconstruction', None)
        return result

    def execute(self, actor, room_id, body, *, view_options=None):
        # Presentation cursors never modify a command or its request fingerprint.
        # Its first reply is still an immutable receipt on retries.
        options = dict(view_options or {})
        def render(room):
            from .multiplayer_transport import command_view_options
            return self._view(room, actor, **command_view_options(room, options))
        def operation():
            room = self._room(room_id)
            if room.get('closed'):
                from .multiplayer_lobby import closed_view
                return closed_view(room, actor)
            seat = self._seat(room, actor)
            op = body.get('op'); payload = body.get('payload', {})
            if not isinstance(payload, dict): raise DomainError('invalid_payload')
            # _receipt already compared the untouched HTTP request fingerprint.
            # Translate only typed, saved bijections; never round a native ID or
            # grant another seat ownership through a compatibility alias.
            from .multiplayer_instance_ids import translate_payload
            payload = translate_payload(room, payload)
            from .multiplayer_completion import request as terminal_request, replayed, commit as commit_terminal, special_replayed
            terminal = terminal_request(room, seat, op, payload, digest, self.c.constants['protocolIds'])
            if op not in ('reconnect', 'heartbeat', 'disconnect'):
                generation = integer(body.get('generation'), 'generation', 1)
                # A previously committed native finish remains the same report
                # across a phase/round transition. Match, seat, phase, original
                # payload round and its full fingerprint must all agree.
                if replayed(room, terminal):
                    return render(room)
                if special_replayed(room, seat, op, payload, generation, self.c.constants['protocolIds']):
                    return render(room)
                # Party exit/return ACKs may outlive the battle UI that last
                # presented a generation. Their authenticated room/seat and
                # scene-left/result guards remain authoritative; an unrelated
                # teammate advancing the round cannot forbid that departure.
                if generation != room['generation'] and op not in ('leave', 'room.return'):
                    raise DomainError('stale_multiplayer_generation')
            before = self._broadcast_cursors(room)
            before_event = room['eventSeq']
            committed_result = copy.deepcopy(room.get('result'))
            self._apply(room, seat, op, payload)
            commit_terminal(room, terminal)
            if op not in ('heartbeat', 'reconnect', 'disconnect'):
                from .multiplayer_lobby import touch
                touch(self, room)
            if op not in ('disconnect', 'leave') and not seat.get('quit') and not seat.get('transportSuspension'):
                seat.update(connected=True, lastSeenTs=int(self.clock()))
            self._collect_broadcasts(room, before)
            self._save(room)
            if seat['left'] or (seat.get('teamLeft') and seat not in room['seats']):
                reply = {'ok': True, 'roomId': room_id, 'left': True, 'officialFallback': False}
                if committed_result is not None: reply['result'] = committed_result
                return reply
            reply = render(room)
            updates = [row for row in room['events'] if row['seq'] > before_event and
                       row['kind'] == 'special-preparation-update']
            if updates:
                reply['nativeSpPreparationUpdates'] = [{'eventSeq': row['seq'],
                    'round': row['payload']['round'], 'updateInfo': copy.deepcopy(row['payload']['updateInfo'])}
                    for row in updates]
            # Preserve the final/result evidence for existing callers, while
            # acknowledging that this client's transport must release the old
            # room and let the native Leave callback tear its service down.
            if op == 'leave' and seat.get('quit'): reply['left'] = True
            return reply
        def native_command():
            # A native business rejection is a valid request/response exchange.
            # Preserve its immutable receipt and return the latest board with
            # RetCode.FAIL; RRP.REQ_WRONG would produce the game's unknown error.
            self.store.db.execute('SAVEPOINT native_command')
            try:
                result = operation()
            except DomainError as error:
                self.store.db.execute('ROLLBACK TO native_command')
                self.store.db.execute('RELEASE native_command')
                from .multiplayer_rejections import projected_failure
                view = projected_failure(self, actor, room_id, body, error)
                if view is None:
                    raise
                return view
            self.store.db.execute('RELEASE native_command')
            return result
        def replay_projection(reply):
            room = self._room(room_id)
            if not isinstance(reply, dict) or reply.get('roomId') != room_id:
                return reply
            reply_match = reply.get('matchSequence')
            if reply_match is not None and reply_match != room.get('matchSequence', 1):
                retired = next((item for item in room.get('matchHistory', [])
                    if item['matchSequence'] == reply_match and item.get('multiplayerInstanceMigration')), None)
                if retired is not None:
                    # Same immutable request, but the matching native scene is
                    # retired. Never revive its DTO or alias it into a new run.
                    return {'ok': True, 'roomId': room_id, 'matchSequence': reply_match,
                            'left': True, 'officialFallback': False}
            migration = room.get('multiplayerInstanceMigration')
            if (not migration or
                    reply.get('matchSequence') != room.get('matchSequence', 1) or
                    migration.get('matchSequence') != room.get('matchSequence', 1)):
                return reply
            # The immutable receipt still proves this exact actor/request. Its
            # old DTO contains unsafe IDs and retired signed preparation proofs;
            # re-project the current same-match view without rewriting SQL.
            if room.get('closed'):
                from .multiplayer_lobby import closed_view
                current = closed_view(room, actor)
            else:
                retained = next((item for item in room['seats'] if item['participantId'] == actor), None)
                if retained is None or retained.get('left') or retained.get('teamLeft'):
                    return {'ok': True, 'roomId': room_id, 'matchSequence': reply_match,
                            'left': True, 'officialFallback': False}
                current = render(room)
            for name in ('commandRejected',):
                if name in reply: current[name] = copy.deepcopy(reply[name])
            if reply.get('commandRejected') and current.get('dtoProjection', {}).get('preparation'):
                current['dtoProjection']['preparation']['payload']['code'] = self.c.enum(
                    'Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessScenePreparationStatusResp+RetCode', 'FAIL')
            if reply.get('left'): current['left'] = True
            return current
        return self._receipt(actor, room_id, body, native_command, replay_projection)

    def _collect_broadcasts(self, room, before):
        for seat in room['seats']:
            old = before.get(seat['seat'])
            if old and seat['state']:
                for event in self.e.broadcasts.delta(old, seat['state'], recipient_uid_index=seat['seat']):
                    event['uIdx'] = seat['seat']
                    target = next((row.get('targetUidIndex') for row in
                        seat['state']['nativeBroadcastState']['events']
                        if row['message']['eventId'] == event['eventId']), None)
                    self._event(room, 'broadcast', event, [target] if target is not None else None)

    @staticmethod
    def _broadcast_cursors(room):
        # BattlefieldBroadcasts.delta needs only identity and sequence. Copying
        # every inventory, enemy and historical turn on a heartbeat is wasted
        # work; this immutable cursor still detects every new radio event.
        return {row['seat']: {'runId': row['state']['runId'],
            'nativeBroadcastState': {'seq': (row['state'].get('nativeBroadcastState') or {}).get('seq', 0)}}
            if row.get('state') else None for row in room['seats']}

    def _alive(self, room):
        pregame = room['phase'] in ('building', 'confirm', 'strategy', 'protocol_start', 'loading')
        return [v for v in room['seats'] if not v['left'] and not v.get('quit') and (pregame or v['state'] is None or v['state']['player']['hp'] > 0)]

    def _battle_members(self, room):
        seats = set(room.get('battleSeats', [v['seat'] for v in self._alive(room)]))
        return [v for v in room['seats'] if v['seat'] in seats]

    def _require(self, room, *phases):
        if room['phase'] not in phases: raise DomainError('wrong_multiplayer_phase', room['phase'])

    def _apply(self, room, seat, op, payload, *, _retain_party=False):
        from .multiplayer_reconstruction import waiting
        if waiting(room, seat) and op in ('help.kill', 'help.escape', 'help.finish'):
            if 'round' in payload and payload['round'] != room['round']: raise DomainError('stale_round')
            return
        quit_protocol = op == 'protocol' and isinstance(payload.get('type'), str) and payload['type'].rsplit('+', 1)[-1] == 'AutoChessBattleSceneQuitUp'
        private_reconnect = op == 'reconnect' and not seat.get('teamLeft')
        if seat.get('quit') and not quit_protocol and not private_reconnect and op not in ('heartbeat', 'watch', 'disconnect', 'scene.quit', 'room.return', 'leave'):
            raise DomainError('multiplayer_seat_quit')
        for name in ('uidIndex', 'playerUidIndex'):
            if name in payload and (type(payload[name]) is not int or payload[name] != seat['seat']):
                raise DomainError('wrong_multiplayer_player')
        from .multiplayer_party import apply_party
        if apply_party(self, room, seat, op, payload): return
        if op == 'protocol':
            name = payload.get('type'); data = payload.get('payload', {})
            if type(name) is int:
                name = next((k for k,v in self.c.constants['protocolIds'].items() if v == name), '')
            if not isinstance(name, str) or not isinstance(data, dict): raise DomainError('invalid_protocol_type')
            name = name.rsplit('+',1)[-1]
            from .multiplayer_reconstruction import waiting
            if waiting(room, seat) and name in (
                    'AutoChessBattleSceneSelfBattleFinishUp', 'AutoChessBattleSceneSelfBattleInfoUp',
                    'AutoChessBattleSceneHelpBattleFinishUp', 'AutoChessBattleSceneHelpBattleKillEnemyUp',
                    'AutoChessBattleSceneHelpBattleEnemyEscapeUp', 'AutoChessBattleSceneActionUp'):
                # Late native callbacks from the interrupted engine cannot
                # replace the forfeited round or kick the returning client.
                if 'round' in data and data['round'] != room['round']: raise DomainError('stale_round')
                return
            if name == 'AutoChessBattleSceneSpPreparationPickUp' and room['phase'] != 'special':
                raise DomainError('wrong_multiplayer_phase', room['phase'])
            from .multiplayer_native_up import apply_observation
            if apply_observation(self, room, seat, name, data): return
            from .multiplayer_reconnect import apply_native_rejoin
            if apply_native_rejoin(self, room, seat, name, data): return
            from .multiplayer_scene_broadcast import apply as apply_scene_broadcast
            if apply_scene_broadcast(self, room, seat, name, data): return
            from .multiplayer_chat import apply as apply_scene_chat
            if apply_scene_chat(self, room, seat, name, data): return
            if name == 'AutoChessBattleSceneActionUp':
                if room['phase'] != 'boss' or not room.get('bossTimeline'): raise DomainError('wrong_multiplayer_phase')
                if room['nativeBoss'].get('settleSuccess') is False:
                    raise DomainError('multiplayer_boss_already_finished')
                from .multiplayer_native_boss import apply_actions
                progress = (room['bossTimeline']['bossHp'], dict(room['bossTimeline']['contributions']))
                details = apply_actions(room, seat['seat'], data, self.c)
                for event in details['broadcasts']: self._event(room, 'broadcast', event)
                for member in self._battle_members(room): member['state']['battle']['bossHp'] = room['bossTimeline']['bossHp']
                if progress != (room['bossTimeline']['bossHp'], room['bossTimeline']['contributions']):
                    self._event(room, 'boss-progress', {'bossHp': room['bossTimeline']['bossHp'],
                        'contributions': room['bossTimeline']['contributions'], 'epoch': room['bossTimeline']['epoch']})
                if details['bossDefeated']:
                    room['nativeBoss'].setdefault('settleAtMs', int(self.clock() * 1000) + 2000)
                return
            special = {'AutoChessBattleSceneLoadingReadyUp':'loading.ready',
                'AutoChessBattleScenePreparationReadyUp':'ready', 'AutoChessBattleSceneSelfBattleFinishUp':'battle.finish',
                'AutoChessBattleSceneHelpBattleFinishUp':'help.finish',
                'AutoChessBattleSceneHelpBattleKillEnemyUp':'help.kill',
                'AutoChessBattleSceneHelpBattleEnemyEscapeUp':'help.escape', 'AutoChessBattleSceneQuitUp':'scene.quit'}
            if name in special: return self._apply(room, seat, special[name], data)
            operation, args = self.protocol.decode(name, data)
            return self._apply(room, seat, operation, args)
        if op in ('heartbeat', 'reconnect'):
            if 'unityLost' in payload and type(payload['unityLost']) is not bool:
                raise DomainError('invalid_unity_lost')
            seat['lastSeenTs'] = int(self.clock())
            if op == 'reconnect' or seat.get('transportSuspension'):
                from .multiplayer_reconstruction import reconnect
                reconnect(self, room, seat)
            elif not seat.get('transportSuspension'):
                seat['connected'] = True
            if op == 'reconnect':
                from .multiplayer_identity import capture
                # Static battle cards belong to the match snapshot, like its
                # squad. A new cached profile applies when the party returns.
                if capture(self, seat['participantId'], payload, seat if room['phase'] == 'building' else None):
                    self._event(room, 'display-profile', {'seat': seat['seat']})
            if op == 'reconnect' and payload.get('unityLost', False):
                from .multiplayer_reconnect import recover_unity
                recover_unity(self, room, seat)
            if op == 'reconnect': self._event(room, 'reconnected', {'seat': seat['seat']})
            return
        if op == 'disconnect':
            from .multiplayer_reconstruction import suspend
            suspend(self, room, seat, 'transport-disconnected'); return
        if op == 'room.dissolve':
            from .multiplayer_lobby import in_lobby, close
            if payload: raise DomainError('invalid_dissolve_payload')
            if not in_lobby(room) or room['phase'] == 'result' and not seat.get('returned'):
                raise DomainError('room_not_in_lobby')
            if seat['participantId'] != room['owner']: raise DomainError('room_owner_required')
            close(self, room, 'owner-dissolved'); return
        if op == 'leave':
            if '_retainParty' in payload: raise DomainError('invalid_party_quit_payload')
            if room['phase'] == 'building':
                seat['left'] = True
                others = [v for v in room['seats'] if not v['left']]
                if others and room['owner'] == seat['participantId']:
                    room['owner'] = others[0]['participantId']; others[0]['ready'] = True
                if not others: self._phase(room, 'result')
            else:
                # An explicit quit loses the seat, but its issued enemy ledger
                # must still be accounted for by the current round barrier.
                seat['connected'] = False; seat['quit'] = True
                from .multiplayer_party import finish_departed_field
                finish_departed_field(self, room, seat)
                if room['phase'] == 'result':
                    # Leaving an already-committed result releases this client;
                    # it cannot turn the final survivor's victory into a loss.
                    pass
                elif not self._alive(room):
                    self._end_match(room, False, max(0, room['round'] - 1))
                elif room['phase'] == 'confirm' and all(v['confirmed'] for v in self._alive(room)):
                    self._begin_strategies(room)
                elif room['phase'] in ('strategy', 'special') and room['selectionQueue'][room['queueIndex']] == seat['seat']:
                    if room['phase'] == 'strategy': self._next_strategy(room)
                    else: self._next_special(room)
                elif room['phase'] == 'preparation' and all(v['ready'] for v in self._alive(room)):
                    self._phase(room, 'battle_countdown', 2)
                elif room['phase'] == 'loading' and all(v['loaded'] for v in self._alive(room)):
                    self._begin_round(room, first=True)
                elif room['phase'] == 'help' and room.get('helpReports'):
                    self._resolve_help(room)
            if not _retain_party:
                from .multiplayer_party import team_left
                team_left(self, room, seat)
            self._event(room, 'member-left' if not _retain_party else 'battle-quit', {'seat': seat['seat']}); return
        if op == 'watch':
            self._require(room, 'preparation', 'special')
            target = integer(payload.get('seat', seat['seat']), 'seat', 0, 3)
            observed = next((v for v in room['seats'] if v['seat'] == target and not v['left']), None)
            if observed is None: raise DomainError('unknown_observation_target')
            if observed.get('state') and seat.get('state') and observed['state']['stageId'] != seat['state']['stageId']:
                raise DomainError('cross_base_stage_observation_unavailable')
            seat['watchSeat'] = target
            self._event(room, 'view', {'seat': target}, [seat['seat']]); return
        if op == 'start':
            self._require(room, 'building')
            if seat['participantId'] != room['owner']: raise DomainError('room_owner_required')
            peers = [v for v in self._alive(room) if v['participantId'] != room['owner']]
            if any(not v.get('ready') for v in peers): raise DomainError('multiplayer_players_not_ready')
            if any(not v.get('connected') or int(self.clock()) - v['lastSeenTs'] > 30 for v in peers):
                raise DomainError('multiplayer_players_disconnected')
            seat['ready'] = True
            if room.get('matchSequence', 1) > 1: room['matchSeed'] = secrets.randbits(32) or 1
            room['nativeStartedAtTs'] = int(self.clock())
            # The membership is now fixed; one, two or three humans need no bots.
            # Native INFO_SHOW displays the common enemy/banned-bond draw before
            # its confirmation barrier. Persist that draw now, once, so the
            # subsequent strategy barrier cannot change the previewed setup.
            self._initialize_players(room)
            self._phase(room, 'confirm', 25); return
        if op == 'room.ready':
            self._require(room, 'building')
            value = payload.get('ready')
            if type(value) is not bool: raise DomainError('invalid_room_ready')
            if seat['participantId'] == room['owner']:
                if not value: raise DomainError('room_owner_cannot_cancel_ready')
                # CN _EventOnRoomStartGame (0x602b40f) sends ReadyInRoomUp(true),
                # just like a guest's Ready button. The authenticated actor,
                # not a client-supplied role, determines its start semantics.
                return self._apply(room, seat, 'start', {})
            seat['ready'] = value
            self._event(room, 'room-ready', {'seat': seat['seat'], 'ready': value}); return
        if op == 'room.mode':
            self._require(room, 'building')
            if seat['participantId'] != room['owner']: raise DomainError('room_owner_required')
            mode = payload.get('modeId')
            if mode not in MODES: raise DomainError('multiplayer_custom_mode_required')
            room['modeId'] = mode
            for member in self._alive(room): member['ready'] = member['participantId'] == room['owner']
            self._event(room, 'room-mode', {'modeId': mode}); return
        if op == 'confirm':
            self._require(room, 'confirm'); seat['confirmed'] = True
            if all(v['confirmed'] for v in self._alive(room)): self._begin_strategies(room)
            return
        if op == 'band.choose':
            self._require(room, 'strategy')
            if room['selectionQueue'][room['queueIndex']] != seat['seat']: raise DomainError('not_selection_turn')
            self.e.apply(seat['state'], 'band.choose', payload)
            self._next_strategy(room); return
        if op == 'loading.ready':
            self._require(room, 'loading')
            if room.get('deadlineTs') is not None and int(self.clock()) >= room['deadlineTs']:
                raise DomainError('multiplayer_loading_expired')
            seat['loaded'] = True
            if all(v['loaded'] for v in self._alive(room)): self._begin_round(room, first=True)
            return
        if op == 'choice.pick' and room['phase'] == 'special':
            if room['selectionQueue'][room['queueIndex']] != seat['seat']: raise DomainError('not_selection_turn')
            slot = integer(payload.get('slotId'), 'slotId', 0, len(room['specialDraft']['slots']) - 1)
            if slot in room['specialTaken']: raise DomainError('special_slot_already_taken')
            previous_deadline = room['deadlineTs']
            pick_generation = room['generation']
            self.e.apply(seat['state'], op, payload)
            from .multiplayer_policy import apply_team_tactic
            apply_team_tactic(self.e, [v['state'] for v in self._alive(room) if v['seat'] != seat['seat']],
                key=room['specialDraft']['slots'][slot]['unitId'], event_id=room['specialEventId'], picker_seat=seat['seat'])
            room['specialTaken'].append(slot); room['specialChoices'][str(seat['seat'])] = slot
            room.setdefault('specialPickReceipts', {})[str(seat['seat'])] = {
                'round': room['round'], 'slotId': slot, 'generation': pick_generation}
            self._next_special(room)
            # Official SpPreparationPickDn carries the final player's choice
            # before PREPARE even though no SP scene snapshot survives then.
            # The final update retains the previous SP deadline, not the new
            # preparation deadline or a fabricated zero timestamp.
            update = {'playerChoice': {'uidIndex': seat['seat'], 'choice': slot},
                'finTs': room['deadlineTs'] if room['phase'] == 'special' else previous_deadline,
                'currQueueIndex': room['queueIndex']}
            self._event(room, 'special-preparation-update', {'round': room['round'], 'updateInfo': update})
            return
        if op == 'ready':
            self._require(room, 'preparation')
            if seat not in self._alive(room) or seat.get('quit'): raise DomainError('player_not_active')
            value = integer(payload.get('readyType'), 'readyType', 0, 1)
            seat['ready'] = bool(value); self._event(room, 'ready', {'seat': seat['seat'], 'ready': bool(value)})
            if all(v['ready'] for v in self._alive(room)): self._phase(room, 'battle_countdown', 2)
            return
        if op in PREP_OPS:
            self._require(room, 'preparation')
            if seat not in self._alive(room) or seat.get('quit'): raise DomainError('player_not_active')
            if seat['ready']: raise DomainError('cancel_ready_before_editing')
            self.e.apply(seat['state'], op, payload)
            from .multiplayer_item_gifts import queue as queue_item_gifts
            queue_item_gifts(self, room, seat)
            # The event only identifies the changed board. Other players never
            # receive this player's store or hand through a notification.
            self._event(room, 'player-updated', {'seat': seat['seat']}); return
        if op == 'bond.add' and room['phase'] == 'help': raise DomainError('joint_defense_layers_forbidden')
        if op in ('battle.kill', 'battle.escape', 'battle.info', 'bond.add'):
            self._require(room, 'battle')
            from .multiplayer_reconstruction import waiting
            if waiting(room, seat): return
            if seat not in self._alive(room): raise DomainError('player_not_active')
            if str(seat['seat']) in room['reports']: raise DomainError('self_battle_already_finished')
            if op == 'battle.escape':
                token = payload.get('isToken', False)
                if type(token) is not bool: raise DomainError('invalid_enemy_token')
                if token:
                    from .multiplayer_enemy_tokens import decode
                    record = decode(self.c, payload.get('enemyInstId'), seat['seat'])
                    acknowledged = seat['state']['battle'].setdefault('nativeEscapedTokenIds', [])
                    if record['enemyInstId'] not in acknowledged:
                        acknowledged.append(record['enemyInstId']); acknowledged.sort()
                    return
            self.e.apply(seat['state'], op, payload); return
        if op == 'battle.finish':
            self._require(room, 'battle')
            from .multiplayer_reconstruction import waiting
            if waiting(room, seat):
                if payload.get('round') != room['round']: raise DomainError('stale_round')
                return
            if seat not in self._alive(room): raise DomainError('player_not_active')
            self._self_finish(room, seat, payload); return
        if op == 'help.finish':
            self._require(room, 'help'); self._help_finish(room, seat, payload); return
        if op in ('help.kill', 'help.escape'):
            self._require(room, 'help')
            from .joint_defense import authority_seat, record_kill, record_escape, remaining_by_owner
            if seat['seat'] not in room['help']['spectatorSeats']:
                raise DomainError('joint_defense_authority_required')
            # A spectator's independently rendered field is not a second
            # outcome ledger. In particular its timeout must not race a real
            # defender kill and turn a perfect shared battle into HP loss.
            if seat['seat'] != authority_seat(room['help']): return
            ident = integer(payload.get('enemyInstId'), 'enemyInstId')
            token = payload.get('isToken', False)
            if type(token) is not bool: raise DomainError('invalid_enemy_token')
            enemies = [v for v in room['help']['enemies'] if v['enemyInstId'] == ident and v['isToken'] == token]
            if len(enemies) != 1: raise DomainError('ambiguous_joint_defense_enemy')
            if op == 'help.kill':
                record_kill(room['help'], {**payload, 'killedEnemyInfo': enemies[0]}, reporter_seat=seat['seat'])
            else: record_escape(room['help'], enemies[0], reporter_seat=seat['seat'])
            self._event(room, 'joint-defense-counts', remaining_by_owner(room['help'])); return
        if op == 'boss.actions':
            self._require(room, 'boss')
            if room.get('nativeBoss', {}).get('settleSuccess') is False:
                raise DomainError('multiplayer_boss_already_finished')
            if seat not in self._alive(room): raise DomainError('player_not_active')
            from .multiplayer_boss import apply_actions
            from .multiplayer_instance_ids import historical_fingerprints
            owned = seat['state']['battle']['nativeOwnedChessIds']
            details = apply_actions(room['bossTimeline'], payload, seat=seat['seat'], owned_ids=owned, catalog=self.c,
                compatible_fingerprints=historical_fingerprints(room, payload['actions']))
            for event in details['broadcasts']: self._event(room, 'broadcast', event)
            for member in self._battle_members(room): member['state']['battle']['bossHp'] = room['bossTimeline']['bossHp']
            self._event(room, 'boss-progress', {k: details[k] for k in ('bossHp', 'contributions', 'epoch')})
            if details['bossDefeated']: self._settle_boss(room, True)
            return
        raise DomainError('multiplayer_operation_pending', str(op))

    def _initialize_players(self, room):
        from . import multiplayer_policy
        for seat in self._alive(room):
            if seat['state'] is not None: continue
            profile = self._profile(seat['participantId']); diy, squad = snapshot_profile(self.c, profile['squad'])
            state = self.e.create_multiplayer_player(uuid.uuid4().hex, {'modeId': room['modeId'],
                'seed': room['matchSeed'], 'chessPool': sorted(profile['squad']), 'diyChessPool': diy})
            # Enables the existing native board/TOKEN ownership validator;
            # this persistence marker does not claim Unity or wire acceptance.
            state.update(activityId=ACTIVITY, nativeSquad=squad, multiplayerCoordinatorVersion=1,
                         nativeSessionVersion=1)
            # Native blackboards store these identities as System.Single.
            # Reserve disjoint ranges inside its exact integer domain, including
            # enemies/effects and every future economic allocation.
            from .multiplayer_instance_ids import initialize
            initialize(state, seat['seat'])
            # Setup RNG is common; purchase/reward streams are seat-specific.
            state['rngState'] = stream(room['matchSeed'], 'room:seat:' + str(seat['seat']))['rngState']
            for field in ('bandRewardRngState', 'specialRewardRngState', 'economyRewardRngState'):
                state[field] = stream(room['matchSeed'], 'room:seat:' + str(seat['seat']) + ':' + field)['rngState']
            multiplayer_policy.initialize(self.e, state)
            seat['state'] = state

    def _begin_strategies(self, room):
        self._initialize_players(room)
        room['selectionQueue'] = [v['seat'] for v in self._alive(room)]; room['queueIndex'] = 0
        self._phase(room, 'strategy', 50)

    def _next_strategy(self, room):
        room['queueIndex'] += 1
        while room['queueIndex'] < len(room['selectionQueue']) and not any(v['seat'] == room['selectionQueue'][room['queueIndex']] for v in self._alive(room)):
            room['queueIndex'] += 1
        if room['queueIndex'] >= len(room['selectionQueue']): self._phase(room, 'protocol_start', 3)
        else: self._phase(room, 'strategy', 50)

    def _begin_round(self, room, first=False):
        from .multiplayer_reconstruction import round_boundary
        round_boundary(self, room, 1 if first else room['round'] + 1)
        alive = self._alive(room)
        if not alive:
            self._end_match(room, False, max(0, room['round'] - 1)); return
        for seat in alive:
            if first: self.e.apply(seat['state'], 'loading.ready', {})
            else: self.e._round_start(seat['state'])
            seat['ready'] = False
        room['round'] = alive[0]['state']['round']; room['reports'] = {}; room['helpReports'] = {}; room['help'] = None; room['bossTimeline'] = None; room['nativeBoss'] = None
        from .multiplayer_item_gifts import deliver as deliver_item_gifts
        deliver_item_gifts(self, room)
        special = [v['seat'] for v in alive if v['state']['phase'] == 'special']
        if special:
            from .multiplayer_policy import shared_draft
            states = [v['state'] for v in alive if v['seat'] in special]
            room['specialDraft'] = shared_draft(self.e, states)
            room['specialRound'] = room['round']
            room['specialEventId'] = states[0]['player']['selfChoice']['id']
            room['specialChoices'] = {}; room['specialTaken'] = []; room['specialPickReceipts'] = {}
            room['selectionQueue'] = special; room['queueIndex'] = 0
            self._phase(room, 'special', 30)
        else: self._preparation(room)

    def _next_special(self, room):
        room['queueIndex'] += 1
        while room['queueIndex'] < len(room['selectionQueue']) and not any(v['seat'] == room['selectionQueue'][room['queueIndex']] for v in self._alive(room)):
            room['queueIndex'] += 1
        if room['queueIndex'] == len(room['selectionQueue']): self._preparation(room)
        else: self._phase(room, 'special', 16)

    def _preparation(self, room):
        seconds = integer(self.c.turns(room['modeId'])[room['round']]['NormalPhaseTime'], 'preparationSeconds', 1, 3600)
        self._phase(room, 'preparation', seconds)

    def _start_battle(self, room):
        from .multiplayer_policy import timers
        room['battleSeats'] = [v['seat'] for v in self._alive(room)]
        for seat in self._alive(room):
            state = seat['state']; self.e.apply(state, 'preparation.ready', {})
            state['battle']['nativeOwnedChessIds'] = [v['instId'] for field in ('charChess','trapChess') for v in state['player'][field]]
            from .multiplayer_reconstruction import capture_round
            capture_round(seat)
        boss = bool(self.c.turns(room['modeId'])[room['round']]['IsBossTurn'])
        timer = timers(self.c, room['modeId'], room['round'])
        timing = {'version': 1, 'displayDurationSeconds': timer['battleDisplaySeconds'],
                  'simulationDurationSeconds': timer['battleSimulationSeconds'], 'clock': 'native-fixed-play-time'}
        room['battleTiming'] = timing
        self._phase(room, 'boss' if boss else 'battle', timing['displayDurationSeconds'])
        for seat in self._alive(room):
            seat['state']['battle'].update(startedAtTs=int(self.clock()), deadlineTs=room['deadlineTs'], battleTiming=timing)
        if boss:
            from .multiplayer_boss import create
            first = self._alive(room)[0]['state']
            room['bossTimeline'] = create(self.c, self._alive(room), room['round'], room['generation'], first['bossId'])
            room['bossOvertimeCharged'] = 0
            from .multiplayer_native_boss import create as create_native
            create_native(room, self.clock(), self.c)
            self._event(room, 'boss-native-clock', {'epoch': room['nativeBoss']['epoch'], 'durationMs': 100,
                'policy': room['nativeBoss']['policy']})
        from .multiplayer_reconstruction import suspend
        for seat in list(self._battle_members(room)):
            if seat.get('transportSuspension') or not seat.get('connected'):
                suspend(self, room, seat, 'absent-at-battle-start')

    def _settle_boss(self, room, success):
        from .multiplayer_boss import hidden_eligible
        timeline = room['bossTimeline']
        baseline = min(r for r, turn in self.c.turns(room['modeId']).items() if turn['IsBossTurn'])
        if success and room['round'] == baseline: room['baselineWon'] = True
        for seat in self._battle_members(room):
            state = seat['state']; battle = state['battle']
            self.e.apply(state, 'multiplayer.finish', {'round': room['round'], 'escapedInstIds': battle['escaped'],
                'killedInstIds': battle['killed'], '_roomHpCost': 0, 'defeat': not success})
            state['completedReports'][str(room['round'])].update(bossId=timeline['bossId'], bossDefeated=success,
                sharedBossPolicy=timeline['bossHpPolicy'], sharedBossEpoch=timeline['epoch'])
        room['roundHistory'].append({'round': room['round'], 'bossTimeline': copy.deepcopy(timeline), 'success': success})
        if success and room['round'] == baseline and hidden_eligible(self.c, room['modeId'], room['seats']):
            self._begin_round(room); return
        won = bool(success or room.get('baselineWon'))
        self._end_match(room, won, room['round'] if success else baseline if won else room['round'] - 1)

    def _end_match(self, room, success, rounds_passed):
        room.setdefault('nativeEndedAtTs', int(self.clock()))
        for seat in room['seats']:
            if seat['state'] is None: continue
            state = seat['state']; state['phase'] = 'result'
            state['result'] = {'success': success, 'roundsPassed': rounds_passed,
                               'hp': state['player']['hp'], 'teamResult': True}
        room['result'] = {'success': success, 'roundsPassed': rounds_passed}
        self._phase(room, 'result')
        from .multiplayer_trophies import commit
        commit(self, room)
        from .multiplayer_party import archive_result
        archive_result(room)

    def _boss_overtime(self, room, now):
        due = max(0, now - room['deadlineTs'])
        outstanding = due - room.get('bossOvertimeCharged', 0)
        # The private team-HP policy consumes one point per completed real
        # second, rotating seats; this split is not asserted official behavior.
        for index in range(max(0, outstanding)):
            active = self._alive(room)
            if not active: break
            target = active[room.get('bossOvertimeCharged', 0) % len(active)]
            target['state']['player']['hp'] -= 1
            room['bossOvertimeCharged'] = room.get('bossOvertimeCharged', 0) + 1
        if not self._alive(room):
            # Publish the actual zero-HP native step before changing scene
            # phase, just as a defeated boss gets its terminal step first.
            native = room['nativeBoss']
            exhausted_at = room['deadlineTs'] + room.get('bossOvertimeCharged', 0)
            native.setdefault('settleAtMs', exhausted_at * 1000 + 2000)
            native['settleSuccess'] = False
        elif outstanding > 0:
            self._event(room, 'boss-team-hp', {'teamHp': sum(v['state']['player']['hp'] for v in self._alive(room)),
                'policy': 'private-one-hp-per-real-second-rotating-seat-v1'})

    def _self_report(self, room, seat, payload):
        state = seat['state']; battle = state['battle']
        if integer(payload.get('round'), 'round', 1) != room['round'] or state['round'] != room['round']: raise DomainError('stale_round')
        if type(payload.get('fromRejoin', False)) is not bool: raise DomainError('invalid_from_rejoin')
        issued = {v['instId'] for v in battle['units']}; owned = set(battle['nativeOwnedChessIds'])
        escaped = payload.get('escapedInstIds', []); kills = payload.get('killedInsts', [])
        tokens = payload.get('escapedTokenInstIds', [])
        if not isinstance(escaped, list) or not isinstance(kills, list) or not isinstance(tokens, list): raise DomainError('invalid_report')
        escaped = sorted({integer(v, 'enemyInstId') for v in escaped})
        tokens = sorted({integer(v, 'enemyTokenInstId') for v in tokens})
        records = {}
        for row in kills:
            if not isinstance(row, dict): raise DomainError('invalid_kill_records')
            ident = integer(row.get('enemyInstId'), 'enemyInstId'); attacker = integer(row.get('attackerInstId', -1), 'attackerInstId', -1)
            damage = integer(row.get('damageSrc', 0), 'damageSrc')
            if attacker != -1 and attacker not in owned: raise DomainError('unknown_native_attacker')
            if damage not in self.c.constants['enums']['Torappu.AutoChessBattleDamageSrcType'].values(): raise DomainError('invalid_damage_source')
            value = {'enemyInstId': ident, 'attackerInstId': attacker, 'damageSrc': damage}
            if ident in records and records[ident] != value: raise DomainError('conflicting_kill_record')
            records[ident] = value
        killed = set(records); leaks = set(escaped)
        if killed & leaks or not (killed | leaks) <= issued: raise DomainError('invalid_enemy_ids')
        if room.get('deadlineTs') is not None and int(self.clock()) >= room['deadlineTs']:
            # A native zero-boundary finish includes only observed outcomes.
            # Surviving/unborn issued units are escapes, never invented kills.
            leaks |= issued - killed
            escaped = sorted(leaks)
        if killed | leaks != issued: raise DomainError('incomplete_native_battle_report')
        if not set(battle['killed']) <= killed or not set(battle['escaped']) <= leaks: raise DomainError('conflicting_enemy_result')
        from .multiplayer_enemy_tokens import terminal
        terminal(self.c, tokens, seat['seat'], len(escaped))
        if not set(battle.get('nativeEscapedTokenIds', [])) <= set(tokens):
            raise DomainError('conflicting_enemy_result')
        chars = self.combat._characters(payload.get('charBattleStatusList', []),
                                        {v['instId'] for v in state['player']['charChess']})
        # Native CharBattleStatus intentionally excludes TOKENs. The private
        # sidecar is keyed by the owned preparation position as well as instId:
        # a summoned card may share its owner's id and may be deployed twice.
        from .joint_defense import _statuses
        from .protocol import decode_position
        from .native_tokens import current_tokens
        issued_tokens = {(row['instId'], pos['x'], pos['y'])
                         for row in current_tokens(self.c, state) if row['position'] > 0
                         for pos in [decode_position(row['position'])]}
        token_statuses = _statuses({'tokenBattleStatusList': payload.get('tokenBattleStatusList', [])},
                                   'tokenBattleStatusList', token=True)
        if any(len(key) != 3 or key not in issued_tokens for key in token_statuses):
            raise DomainError('unknown_native_token_carry_position')
        token_statuses = [{'instId': key[0], 'position': {'x': key[1], 'y': key[2]}, 'tech': value['tech']}
                          for key, value in sorted(token_statuses.items())]
        return {'round': room['round'], 'escapedInstIds': escaped, 'escapedTokenInstIds': tokens,
                'killedInsts': [records[v] for v in sorted(records)], 'charBattleStatusList': chars,
                'tokenBattleStatusList': token_statuses}

    def _self_finish(self, room, seat, payload, *, native_terminal=True):
        report = self._self_report(room, seat, payload); key = str(seat['seat'])
        if key in room['reports']:
            # Provenance is server-owned sidecar data, not part of the native
            # report's idempotent payload. Older saves have no such sidecar.
            previous = {name: value for name, value in room['reports'][key].items()
                        if name != 'nativeTerminalStatusesComplete'}
            if previous != report: raise DomainError('conflicting_round_report')
            return
        report['nativeTerminalStatusesComplete'] = (native_terminal is True and
            type(payload.get('charBattleStatusList')) is list and not payload.get('fromRejoin', False))
        room['reports'][key] = report
        seat.pop('reconstructionBaseline', None)
        self._event(room, 'self-finished', {'seat': seat['seat'], 'perfect': not report['escapedInstIds'] and not report['escapedTokenInstIds']})
        if all(str(v['seat']) in room['reports'] for v in self._battle_members(room)): self._resolve_self(room)

    def _resolve_self(self, room):
        from .joint_defense import build_plan, count_deployed_operators
        players = []
        for seat in self._battle_members(room):
            state = seat['state']; report = room['reports'][str(seat['seat'])]
            remaining = [{'ownerPlayerIndex': seat['seat'], 'enemyId': v['enemyId'], 'enemyInstId': v['instId'], 'isToken': False}
                         for v in state['battle']['units'] if v['instId'] in report['escapedInstIds']]
            # Indexed native summon IDs already carry their template and
            # origin. The original Help generator reverses that packed ID;
            # preserve its empty enemyId rather than omitting the enemy.
            from .multiplayer_enemy_tokens import terminal
            remaining.extend(terminal(self.c, report['escapedTokenInstIds'], seat['seat'], len(report['escapedInstIds'])))
            from .protocol import decode_position
            from .native_tokens import current_tokens
            tokens = [{**row, 'position': decode_position(row['position'])} for row in current_tokens(self.c, state) if row['position'] > 0]
            positions = [*state['player']['positions'], *tokens]
            players.append({'seat': seat['seat'], 'outcome': 'perfect' if not seat.get('quit') and not remaining and not report['escapedTokenInstIds'] else 'failed',
                'stageId': state['stageId'], 'battlefield': {'stageId': state['stageId'], 'specialEffects': state['specialEffects']},
                'positions': positions, 'deployedCount': count_deployed_operators(positions),
                'charBattleStatusList': report['charBattleStatusList'],
                'tokenBattleStatusList': report.get('tokenBattleStatusList', []),
                'nativeTerminalStatusesComplete': report.get('nativeTerminalStatusesComplete', False),
                'activeBonds': state['player']['bonds'], 'remainingEnemies': remaining})
        plan = build_plan(players, round_id=room['round'], generation=room['generation'] + 1,
                          started_at=int(self.clock()), duration_seconds=room['battleTiming']['displayDurationSeconds'])
        if plan is None: self._settle(room)
        else:
            from .joint_defense import authority_seat
            plan['authoritySeat'] = authority_seat(plan)
            room['help'] = plan; room['helpReports'] = {}
            self._phase(room, 'help', room['battleTiming']['displayDurationSeconds'])
            self._include_help_observers(room)
            self._event(room, 'joint-defense', plan)

    def _help_finish(self, room, seat, payload):
        from .joint_defense import authority_seat, finish
        if seat['seat'] not in room['help']['spectatorSeats']: raise DomainError('joint_defense_authority_required')
        if integer(payload.get('round'), 'round', 1) != room['round']: raise DomainError('stale_round')
        if seat['seat'] != authority_seat(room['help']): return
        finish(room['help'], payload, reporter_seat=seat['seat'],
               expired=room.get('deadlineTs') is not None and int(self.clock()) >= room['deadlineTs'])
        room.setdefault('helpReports', {})[str(seat['seat'])] = room['help']['completedReport']['digest']
        self._event(room, 'help-finished', {'seat': seat['seat']})
        self._resolve_help(room)

    def _resolve_help(self, room):
        from .joint_defense import authority_seat
        plan = room['help']
        # The original engine's authoritative complete partition is sufficient.
        # Waiting for display-only observers leaves a finished field running
        # until timeout, and permits an empty observer journal to win the race.
        if str(authority_seat(plan)) in room.get('helpReports', {}) and plan.get('completedReport'):
            self._settle(room)

    def _settle(self, room):
        for seat in self._battle_members(room):
            state = seat['state']; report = room['reports'][str(seat['seat'])]
            loss = len(report['escapedInstIds']) + len(report['escapedTokenInstIds'])
            if room['help'] is not None:
                from .joint_defense import remaining_by_owner
                loss = remaining_by_owner(room['help']).get(str(seat['seat']), 0)
                # Older saved Help plans omitted indexed tokens entirely.
                # Retain their original source loss without replaying an old
                # world or charging any token represented by the new ledger
                # twice. This read-compatible fallback never invents a kill.
                represented_tokens = {row['enemyInstId'] for row in room['help']['enemies']
                    if row['ownerPlayerIndex'] == seat['seat'] and row['isToken']}
                loss += len(set(report['escapedTokenInstIds']) - represented_tokens)
            args = {'round': room['round'], 'escapedInstIds': report['escapedInstIds'],
                'escapedTokenInstIds': report['escapedTokenInstIds'], 'killedInstIds': [v['enemyInstId'] for v in report['killedInsts']],
                '_roomHpCost': min(loss, self.c.season['ConstData']['CostPlayerHpLimit'])}
            self.e.apply(state, 'multiplayer.finish', args)
            if seat.get('quit'): state['player']['hp'] = 0
        from .multiplayer_help_rewards import settle as settle_help_rewards
        settle_help_rewards(self.e, room)
        room['roundHistory'].append({'round': room['round'], 'reports': copy.deepcopy(room['reports']), 'jointDefense': copy.deepcopy(room['help'])})
        if not self._alive(room):
            self._end_match(room, False, room['round'] - 1); return
        self._begin_round(room)

    def tick(self):
        """Server clock, also run without connected clients. Atomic per room."""
        now = int(self.clock())
        with self.store.lock:
            from .multiplayer_clock import due, schedule
            for row in due(self):
                self.store.db.execute('BEGIN IMMEDIATE')
                try:
                    room = self._room(row[0])
                    if room.get('closed'):
                        schedule(self, room); self.store.db.commit(); continue
                    from .multiplayer_lobby import tick as lobby_tick
                    changed = lobby_tick(self, room)
                    if room.get('closed'):
                        self._save(room); self.store.db.commit(); continue
                    before = self._broadcast_cursors(room)
                    for seat in room['seats']:
                        from .multiplayer_reconstruction import PRESENCE_TIMEOUT_SECONDS
                        if seat['connected'] and now - seat['lastSeenTs'] > PRESENCE_TIMEOUT_SECONDS:
                            from .multiplayer_reconstruction import suspend
                            suspend(self, room, seat, 'presence-timeout'); changed = True
                    expiry = room.get('settleAfterTs') or room['deadlineTs']
                    if room['phase'] == 'boss' and room['deadlineTs'] is not None:
                        from .multiplayer_native_boss import advance
                        if room.get('nativeBoss', {}).get('settleAtMs', 2**63) <= int(self.clock() * 1000):
                            self._settle_boss(room, room['nativeBoss'].get('settleSuccess', True))
                        elif room['bossTimeline']['bossHp'] > 0:
                            self._boss_overtime(room, now)
                        if room['phase'] == 'boss':
                            native = room['nativeBoss']
                            step_now = min(self.clock(), native['settleAtMs'] / 1000) if native.get('settleSuccess') is False else self.clock()
                            changed = advance(room, step_now) or changed
                            if native.get('settleSuccess') is False and native['settleAtMs'] <= int(self.clock() * 1000):
                                self._settle_boss(room, False)
                        changed = True
                    elif expiry is not None and now >= expiry:
                        self._expire(room); changed = True
                    if changed:
                        self._collect_broadcasts(room, before); self._save(room)
                    else:
                        schedule(self, room)
                    self.store.db.commit()
                except DomainError as ex:
                    self.store.db.rollback()
                    # One invalid/old room cannot stop the independent listener
                    # and disconnect other matches. Roll back partial defaults.
                    with self.store.db:
                        self.store.db.execute('INSERT OR REPLACE INTO multiplayer_room_faults VALUES(?,?)', (row[0], ex.code))
                        try:room = self._room(row[0])
                        except DomainError:continue
                        room['deadlineTs'] = None
                        room['settleAfterTs'] = None
                        room['fault'] = {'error': ex.code, 'detail': ex.detail}
                        self._event(room, 'room-fault', room['fault']); self._save(room)

    def _expire(self, room):
        phase = room['phase']
        if phase == 'confirm': self._begin_strategies(room)
        elif phase == 'strategy':
            seat = next(v for v in room['seats'] if v['seat'] == room['selectionQueue'][room['queueIndex']])
            self.e.apply(seat['state'], 'band.choose', {'bandId': sorted(self.c.season['BandDataListDict'])[0]})
            self._next_strategy(room)
        elif phase == 'loading':
            # A clock expiry is not proof of a loaded Unity world. Preserve
            # each actual LoadingReady ACK and withdraw only unavailable
            # loaders, using the same owner/member cleanup as an explicit
            # departure. Loaded survivors may then enter the first round.
            # This is an explicit private timeout policy, not a recorded
            # official timeout branch.
            for seat in list(self._alive(room)):
                if room['phase'] != 'loading': break
                if not seat['loaded']:
                    self._event(room, 'loading-timeout', {'seat': seat['seat']})
                    self._apply(room, seat, 'leave', {})
            if room['phase'] == 'loading' and self._alive(room):
                self._begin_round(room, first=True)
        elif phase == 'protocol_start': self._phase(room, 'loading', 60)
        elif phase == 'battle_countdown': self._start_battle(room)
        elif phase == 'special':
            seat = next(v for v in room['seats'] if v['seat'] == room['selectionQueue'][room['queueIndex']])
            slot = next(v['slotId'] for v in room['specialDraft']['slots'] if v['slotId'] not in room['specialTaken'])
            self._apply(room, seat, 'choice.pick', {'slotId': slot})
        elif phase == 'preparation':
            # Pending native choice dialogs have deterministic default slots;
            # the existing handlers still enforce stock/capacity/effect rules.
            for seat in self._alive(room):
                state = seat['state']
                for _ in range(128):
                    if not (state.get('traitRewardOffers') or state.get('itemChoice')): break
                    self.e.apply(state, 'choice.pick', {'slotId': 0})
            self._phase(room, 'battle_countdown', 2)
        elif phase == 'battle':
            for seat in self._battle_members(room):
                if str(seat['seat']) in room['reports']: continue
                battle = seat['state']['battle']; killed = set(battle['killed'])
                self._self_finish(room, seat, {'round': room['round'],
                    'escapedInstIds': [v['instId'] for v in battle['units'] if v['instId'] not in killed],
                    'escapedTokenInstIds': copy.deepcopy(battle.get('nativeEscapedTokenIds', [])),
                    'killedInsts': [{'enemyInstId': v, 'attackerInstId': -1, 'damageSrc': 0} for v in sorted(killed)],
                    'charBattleStatusList': []}, native_terminal=False)
        elif phase == 'help':
            from .joint_defense import finish
            finish(room['help'], {'round': room['round'], 'escapedEnemies': [], 'killInfos': []},
                   reporter_seat=room['help']['rightSeat'], expired=True)
            self._settle(room)
