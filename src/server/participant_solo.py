"""Participant-owned online access to the existing single-player executors.

Only the authenticated actor chooses the database namespace. Configuration stays
canonical in multiplayer_participants; each single run freezes that same squad.
No changes to ActivityProfile, NativeSession or their combat/economy code.
"""
import copy
from contextlib import contextmanager
import os
from pathlib import Path
import re
import tempfile
import stat
from urllib.parse import parse_qs, urlsplit

from .activity_profile import ACTIVITY, PREFIX, ActivityProfile
from .engine import DomainError
from .native_session import NativeSession
from .storage import Store, dump


class ParticipantSolo:
    def __init__(self, store, protocol, rooms):
        self.store, self.protocol, self.rooms = store, protocol, rooms
        filename = store.db.execute('PRAGMA database_list').fetchone()[2]
        self.temporary = tempfile.TemporaryDirectory(prefix='stronghold-participant-solo-') if not filename else None
        self.root = Path(self.temporary.name) if self.temporary else Path(filename).parent / 'participant-solo'

    def close(self):
        if self.temporary:
            self.temporary.cleanup()

    @staticmethod
    def redirected(path):
        try:
            info = path.lstat()
        except FileNotFoundError:
            return False
        return stat.S_ISLNK(info.st_mode) or bool(getattr(info, 'st_file_attributes', 0) & 0x400)

    @contextmanager
    def session(self, actor, *, sync_profile=True):
        if not isinstance(actor, str) or not re.fullmatch(r'[0-9a-f]{32}', actor):
            raise DomainError('invalid_participant_identity')
        with self.store.lock:
            profile = self.rooms._profile(actor)
            for folder in (self.root, self.root / actor):
                if self.redirected(folder) or folder.exists() and not folder.is_dir():
                    raise DomainError('participant_storage_redirected')
                folder.mkdir(mode=0o700, parents=True, exist_ok=True)
                if os.name != 'nt':
                    folder.chmod(0o700)
            path = self.root / actor / 'stronghold.sqlite3'
            if self.redirected(path) or path.exists() and not path.is_file():
                raise DomainError('participant_storage_redirected')
            private = Store(path, self.store.engine)
            try:
                if os.name != 'nt':
                    path.chmod(0o600)
                activity = ActivityProfile(private)
                saved = activity.get() if sync_profile else None
                # Pull the canonical roster without altering a running game's
                # frozen nativeSquad/chessPool snapshot or progression.
                if saved is not None and (saved['playerActivity']['chessSquad'] != profile['squad'] or
                        saved['playerActivity'].get('trophyNum', 0) != profile.get('trophyNum', 0)):
                    saved['playerActivity']['chessSquad'] = copy.deepcopy(profile['squad'])
                    # Alliance awards belong to the shared cloud profile.
                    # A participant's solo save must not overwrite them when
                    # composing the main-page bootstrap after a restart.
                    saved['playerActivity']['trophyNum'] = profile.get('trophyNum', 0)
                    with private.db:
                        private.db.execute('UPDATE activity_profiles SET data=? WHERE id=?', (dump(saved), ACTIVITY))
                yield activity, NativeSession(private, self.protocol, activity)
            finally:
                private.close()

    def bootstrap(self, actor, reply):
        with self.session(actor) as (activity, native):
            own = activity.get()
            reply['playerActivity'] = copy.deepcopy(own['playerActivity'])
            reply['playerModel']['activity']['actAutoChessActivityList'][ACTIVITY] = copy.deepcopy(own['playerActivity'])
            reply['profile'] = copy.deepcopy(own)
            if reply['syncInfo']['battleInfo'] is None:
                reply['syncInfo']['battleInfo'] = activity.presence.battle_info(own)
            reply['participantSoloVersion'] = 1
            return reply

    def request(self, actor, verb, raw_path, body=None):
        """Return the old domain reply intact inside an authenticated envelope."""
        parsed = urlsplit(raw_path)
        path = parsed.path.split('/')
        if parsed.fragment or not path or any(part in ('.', '..', '') for part in path):
            raise DomainError('unknown_participant_solo_route')
        native_exit = verb == 'POST' and len(path) == 4 and path[0] == 'native-session' and path[2] in ('saved-exit', 'terminal-exit')
        with self.session(actor, sync_profile=not native_exit) as (activity, native):
            if path[0] == 'activity' and len(path) == 2 and verb == 'POST' and not parsed.query:
                if path[1] not in ('syncInfo', 'quitSingleGame', 'getFriendCharAssistList'):
                    raise DomainError('unknown_participant_solo_route')
                return {'result': 0, **activity.invoke(PREFIX + path[1], body)}
            if path[0] != 'native-session':
                raise DomainError('unknown_participant_solo_route')
            if len(path) == 1 and not parsed.query:
                return native.current() if verb == 'GET' else native.create(body)
            if len(path) == 2 and verb == 'GET' and not parsed.query:
                return native.latest_result() if path[1] == 'latest-result' else native.get(path[1])
            if len(path) == 3 and verb == 'POST' and not parsed.query:
                if path[2] == 'protocol':
                    return native.execute(path[1], body)
                if path[2] == 'presented':
                    return native.result_presented(path[1], body)
            if len(path) == 4 and verb == 'POST' and path[2] in ('saved-exit', 'terminal-exit'):
                query = parse_qs(parsed.query, keep_blank_values=True, strict_parsing=True)
                if set(query) != {'revision'} or len(query['revision']) != 1:
                    raise DomainError('invalid_native_exit_revision')
                revision = query['revision'][0]
                if not re.fullmatch(r'[1-9][0-9]{0,9}', revision):
                    raise DomainError('invalid_native_exit_revision')
                finish = native.saved_exit_ack if path[2] == 'saved-exit' else native.terminal_exit_ack
                return finish(path[1], path[3], int(revision), body)
            raise DomainError('unknown_participant_solo_route')
