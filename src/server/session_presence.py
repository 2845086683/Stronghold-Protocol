"""Persistent private-session presence projected into the native activity DTO.

The two marker strings are local adapter identifiers, never network addresses or
credentials. Expiry is fixed at strategy confirmation (legacy saves derive it
from their persisted start), independent of activity/session read frequency.
"""
import re
import time
from contextlib import contextmanager

from .engine import DomainError
from .storage import dump

ADDRESS='stronghold-private-local'
TOKEN='stronghold-private-session'
RUN_ID=re.compile(r'[0-9a-f]{32}\Z')


class SessionPresence:
    def __init__(self,store,activity_id,clock=None):
        self.store=store;self.activity_id=activity_id;self.clock=clock or time.time
        duration=store.engine.c.data['common']['ConstData']['SingleReconnectTime']
        if type(duration)is not int or duration<=0:raise DomainError('invalid_native_reconnect_duration')
        self.duration=duration

    @contextmanager
    def _transaction(self):
        # NativeSession.execute already owns BEGIN IMMEDIATE. Never commit or
        # roll back its enclosing command/receipt transaction from this helper.
        own=not self.store.db.in_transaction
        if own:self.store.db.execute('BEGIN IMMEDIATE')
        try:
            yield
            if own:self.store.db.commit()
        except Exception:
            if own:self.store.db.rollback()
            raise

    def _validate(self,state,run=None):
        ident=state.get('runId')
        if not isinstance(ident,str) or not RUN_ID.fullmatch(ident) or run is not None and ident!=run:
            raise DomainError('invalid_native_scene_id')
        version=state.get('nativeSessionVersion')
        if state.get('activityId')!=self.activity_id or type(version)is not int or version<1:
            raise DomainError('outside_private_native_session')
        mode=state.get('modeId');config=self.store.engine.c.season['ModeDataDict'].get(mode) if isinstance(mode,str) else None
        if not config or config['ModeType']!=1:raise DomainError('native_single_mode_required')

    def initialize(self,state):
        """Set once before the caller persists a new run; also migrates copies."""
        self._validate(state)
        if self.draft(state):return False
        started=state.get('nativeStartedAtTs')
        if type(started)is not int or started<0:raise DomainError('native_session_timestamp_missing')
        expires=state.get('nativeExpiresAtTs')
        if expires is None:
            state['nativeExpiresAtTs']=started+self.duration
            return True
        if type(expires)is not int or expires<=started:raise DomainError('invalid_native_session_expiry')
        return False

    def draft(self,state):
        """A launch preview is durable, but is not a reconnectable match.

        The pre-0.1.46 bridge selected its preview strategy before the player
        saw the decision screen. A round-zero loading save has no gameplay to
        preserve; return it to the explicit decision boundary on first read.
        Advanced and terminal legacy saves retain their existing semantics.
        """
        if 'nativeBandConfirmed' not in state:
            return state.get('round')==0 and state.get('phase')=='loading'
        if type(state['nativeBandConfirmed']) is not bool:raise DomainError('invalid_native_band_confirmation')
        if state['nativeBandConfirmed']:return False
        if state.get('round')!=0 or state.get('phase')!='loading':raise DomainError('invalid_native_band_draft')
        return True

    def normalize_draft(self,state):
        if not self.draft(state):return False
        changed=state.get('nativeBandConfirmed') is not False
        state['nativeBandConfirmed']=False
        for key in ('nativeStartedAtTs','nativeExpiresAtTs'):
            if key in state:state.pop(key);changed=True
        return changed

    def _retire(self,state,reason,now):
        if state['phase']=='result':return False
        self.store.engine.apply(state,'quit',{})
        if reason=='quit':state['result'].update(success=False,abandoned=True)
        state.setdefault('nativeEndedAtTs',now)
        state['nativeSessionEndReason']=reason
        if reason=='expired':state['nativeExpiredAtTs']=now
        return True

    def _refresh(self,state,now):
        self._validate(state)
        if state['phase']=='result':return False
        if self.draft(state):return self.normalize_draft(state)
        changed=self.initialize(state)
        if now>=state['nativeExpiresAtTs']:changed=self._retire(state,'expired',now) or changed
        return changed

    def _save_profile(self,profile):
        self.store.db.execute('UPDATE activity_profiles SET data=? WHERE id=?',(dump(profile),self.activity_id))

    def _clear_terminal_pointer(self,state,profile=None):
        if state['phase']!='result':return
        if profile is None:
            import json
            row=self.store.db.execute('SELECT data FROM activity_profiles WHERE id=?',(self.activity_id,)).fetchone()
            profile=json.loads(row[0]) if row else None
        if profile is not None and profile.get('activeRun')==state['runId']:
            profile['activeRun']=None;self._save_profile(profile)

    def synchronize(self,profile):
        """Normalize the profile's canonical active run and clear terminal links."""
        run=profile.get('activeRun')
        if run is None:return None
        with self.store.lock,self._transaction():
            state=self.store.get(run);self._validate(state,run)
            if self._refresh(state,int(self.clock())):
                self.store.db.execute('UPDATE runs SET state=? WHERE id=?',(dump(state),run))
            if self.draft(state):
                profile['activeRun']=None;profile['draftRun']=run;self._save_profile(profile)
                return None
            self._clear_terminal_pointer(state,profile)
            return state if state['phase']!='result' else None

    def state(self,run):
        """Load by explicit run ID with the same persistent expiry enforcement."""
        with self.store.lock,self._transaction():
            state=self.store.get(run);self._validate(state,run)
            if self._refresh(state,int(self.clock())):
                self.store.db.execute('UPDATE runs SET state=? WHERE id=?',(dump(state),run))
            self._clear_terminal_pointer(state)
            return state

    def battle_info(self,profile):
        state=self.synchronize(profile)
        if state is None:return None
        current_round=state.get('round')
        if type(current_round)is not int or not 0<=current_round<=2147483647:raise DomainError('invalid_native_round')
        return {'sceneId':state['runId'],'address':ADDRESS,'token':TOKEN,'modeId':state['modeId'],
                # Native singleSettleTs = endTime + SingleReconnectTime - now.
                'endTime':state['nativeExpiresAtTs']-self.duration,'curRound':current_round}

    def quit(self,profile,scene_id):
        """Canonical abandonment, idempotent by original scene; no battle report."""
        if not isinstance(scene_id,str) or not RUN_ID.fullmatch(scene_id):raise DomainError('invalid_native_scene_id')
        with self.store.lock,self._transaction():
            state=self.store.get(scene_id);self._validate(state,scene_id)
            # A delayed retry for an already terminal old run must never quit
            # a newly active run. An unlinked nonterminal run cannot be targeted.
            if state['phase']!='result' and profile.get('activeRun')!=scene_id:raise DomainError('wrong_native_scene')
            now=int(self.clock());changed=self._refresh(state,now)
            if state['phase']!='result':changed=self._retire(state,'quit',now) or changed
            if changed:self.store.db.execute('UPDATE runs SET state=? WHERE id=?',(dump(state),scene_id))
            self._clear_terminal_pointer(state,profile)
        return {'result':0,'battleInfo':None}
