"""Durable act2 private sessions for a declaration-based native-object bridge.

The server owns economy/progression. Unity remains combat authority. These HTTP
envelopes are a local adapter contract; they are not the official socket codec.
"""
import copy
import hashlib
import json
import re
import secrets
import uuid

from .activity_profile import ACTIVITY,ActivityProfile
from .engine import DomainError,integer
from .protocol import NS
from .storage import dump
from .native_combat import VERSION,NativeCombat,SINGLE_MODES,repair_identity_counter,validate_finish_attempt
from .hand_layout import reconcile
from .native_tokens import current_tokens,project_tokens
from .preset_snapshot import snapshot_profile
from .session_presence import RUN_ID


class NativeSession:
    def __init__(self,store,protocol,activity=None,clock=None):
        self.store=store;self.protocol=protocol;self.activity=activity or ActivityProfile(store,clock=clock);self.c=store.engine.c
        if clock is not None:self.activity.presence.clock=clock
        self.combat=NativeCombat(store.engine,clock)
        with store.lock,store.db:
            store.db.executescript('CREATE TABLE IF NOT EXISTS native_session_starts(id TEXT PRIMARY KEY,fingerprint TEXT NOT NULL,reply TEXT NOT NULL);'
                'CREATE TABLE IF NOT EXISTS native_session_commands(run TEXT NOT NULL,id TEXT NOT NULL,fingerprint TEXT NOT NULL,reply TEXT NOT NULL,PRIMARY KEY(run,id));')

    def _request(self,body):
        if not isinstance(body,dict):raise DomainError('invalid_payload')
        ids=[body[k] for k in ('actId','activityId') if k in body]
        if not ids or any(k!=ACTIVITY for k in ids):raise DomainError('wrong_private_activity')
        request=body.get('requestId')
        if not isinstance(request,str) or not 1<=len(request)<=128:raise DomainError('invalid_request_id')
        return request,hashlib.sha256(dump(body).encode()).hexdigest()

    def _replay(self,table,where,args,fingerprint):
        row=self.store.db.execute('SELECT fingerprint,reply FROM '+table+' WHERE '+where,args).fetchone()
        if row:
            if row[0]!=fingerprint:raise DomainError('request_id_conflict')
            return self.protocol.replay_reply(json.loads(row[1]))

    def _normalize_snapshot(self,state):
        if state.get('activityId')!=ACTIVITY or not state.get('nativeSessionVersion'):raise DomainError('outside_private_native_session')
        self.activity.presence.initialize(state)
        # Older private saves omitted this supplementary native rune. Derive
        # its threshold from the same CN max-cultivation table as new profiles;
        # an existing explicit selection is retained. GET only normalizes a copy.
        for slot in state.get('nativeSquad',[]):
            if slot.get('cultivateEffectId') is None:slot['cultivateEffectId']=self.c.cultivate_effect(slot.get('charId'))
        reconcile(self.c,state)
        if 'nativeTokenPositions' in state:
            # The bridge validates the private state as well as official DTOs.
            # Keep both layouts identical, retaining metadata only for current,
            # displayable tokens whose cells survive the normal projection.
            player=state['player']
            occupied=[{'position':(row['position']['y']<<16)+row['position']['x']} for row in player['positions']]
            occupied.extend(player.get('handPositions',[]))
            visible={(row['instId'],row['position'],row['direction']) for row in project_tokens(self.c,state,occupied)}
            state['nativeTokenPositions']=[row for row in current_tokens(self.c,state)
                if (row['instId'],row['position'],row['direction']) in visible]
        return state

    def _state(self,run):
        state=self.activity.presence.state(run)
        if 'nativeDraftDiscardedAtTs' in state:raise DomainError('native_draft_discarded')
        return self._normalize_snapshot(state)

    def _check_start_receipt(self,reply):
        # Create receipts are immutable transport history. Explicitly leaving
        # an unconfirmed briefing revokes that draft, including its old start
        # ACK; returning the cached preview would resurrect a cancelled launch.
        row=self.store.db.execute('SELECT state FROM runs WHERE id=?',(reply.get('runId'),)).fetchone()
        if row is not None and 'nativeDraftDiscardedAtTs' in json.loads(row[0]):
            raise DomainError('native_draft_discarded')
        return reply

    def _replay_unstarted(self,state,payload):
        if not isinstance(payload,dict) or set(payload)!={'expectedRevision'}:raise DomainError('invalid_payload')
        if integer(payload['expectedRevision'],'expectedRevision',1)!=state['revision']:
            raise DomainError('native_battle_replay_revision_changed')
        self.store.engine._phase(state,'battle','boss')
        # Full equality is intentional: even a single accepted battle-start
        # layer, zero-damage action, reward or info callback prevents replay.
        # An exact HTTP retry remains safe and is handled before this method.
        target=dump(state)
        rows=self.store.db.execute('SELECT id,reply FROM native_session_commands WHERE run=? ORDER BY rowid DESC',(state['runId'],))
        for request,encoded in rows:
            reply=json.loads(encoded);saved=reply.get('state');details=reply.get('details') or {}
            if (not reply.get('ok') or not isinstance(saved,dict) or saved.get('revision')!=state['revision'] or
                    saved.get('round')!=state['round'] or saved.get('phase') not in ('battle','boss') or
                    details.get('nativeCombatVersion')!=VERSION or 'issuedEnemyCount' not in details or
                    details.get('duplicateReady')):
                continue
            if dump(self._normalize_snapshot(saved))==target:
                return self.combat.replay_unstarted(state,request)
        raise DomainError('native_battle_replay_progressed')

    def envelope(self,state,before=None,op=None,ok=True,request_id=None,details=None):
        projection=self.protocol.project(state)
        broadcasts=self.store.engine.broadcasts.delta(before,state) if ok else []
        turns=self.c.turns(state['modeId'])
        normal_rounds=[key for key,value in turns.items() if not value['IsBossTurn']]
        boss_rounds=[key for key,value in turns.items() if value['IsBossTurn']]
        return {'ok':ok,'requestId':request_id,'activityId':ACTIVITY,'runId':state['runId'],
            'revision':state['revision'],'state':state,'messages':self.protocol.batch(before,state,op,ok),
            'nativeBroadcasts':broadcasts,'nativeBroadcastReceipt':({
                'runId':state['runId'],'fromRevision':before['revision'],'toRevision':state['revision'],
                'fromRound':before['round'],'toRound':state['round']} if broadcasts else None),
            'prebattle':self.protocol.prebattle(state,projection['allStateSync']['payload']['allStateSyncData']),
            'details':details or {},'nativeReady':False,'nativeAccepted':False,'officialWireCompatible':False,
            'pending':projection['pending'],'officialFallback':False,
            'nativeSettleGameInfoType':'Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo',
            'nativeSettleGameInfo':self.protocol.settlement(state),
            'nativeCombatCapability':{'version':VERSION,'normalBattleRound':1,'normalBattleRounds':normal_rounds,
                'bossBattleRound':min(boss_rounds),'bossBattleRounds':boss_rounds,'supportedModes':list(SINGLE_MODES),
                'midBattleResume':False,'unstartedBattleReplay':True,
                'unstartedReplayRequiresExactReadyCheckpoint':True,
                'bossOrHelpBattle':False,'bossBattle':True,'helpBattle':False,
                'bossExecution':'native LOCAL MockBossBattleManager; private terminal report','stepClock':False},
            'transport':'loopback HTTP JSON to private native-object adapter'}

    def current(self):
        with self.store.lock:
            profile=self.activity.get();run=profile.get('activeRun') or profile.get('draftRun')
            if run is None:return {'ok':True,'activityId':ACTIVITY,'activeRun':None,'officialFallback':False}
            return self.envelope(self._state(run))

    def get(self,run):
        with self.store.lock:return self.envelope(self._state(run))

    def latest_result(self):
        """Project a durable completed session without refreshing any save.

        ActivityProfile.get and the ordinary session getter enforce persistent
        expiry and may write. Result history deliberately reads raw rows instead.
        An active pointer takes priority over showing an older completed game.
        """
        empty={'ok':True,'activityId':ACTIVITY,'latestResult':None,'officialFallback':False}
        with self.store.lock:
            profile=self.store.db.execute('SELECT data FROM activity_profiles WHERE id=?',(ACTIVITY,)).fetchone()
            if profile and any(json.loads(profile[0]).get(key) is not None for key in ('activeRun','draftRun')):return empty
            latest=None;latest_order=None
            for rowid,run,encoded in self.store.db.execute('SELECT rowid,id,state FROM runs'):
                try:state=json.loads(encoded)
                except (ValueError,TypeError):continue
                if (not isinstance(state,dict) or not isinstance(run,str) or not RUN_ID.fullmatch(run) or
                        state.get('runId')!=run or state.get('activityId')!=ACTIVITY or
                        state.get('catalogHash')!=self.c.hash or state.get('policyVersion')!=1 or
                        type(state.get('nativeSessionVersion')) is not int or state['nativeSessionVersion']!=1 or
                        state.get('modeId') not in SINGLE_MODES or state.get('phase')!='result'):
                    continue
                ended=state.get('nativeEndedAtTs');result=state.get('result')
                if (type(ended)is not int or ended<0 or not isinstance(result,dict) or
                        type(result.get('success')) is not bool or type(result.get('roundsPassed')) is not int):
                    continue
                order=(ended,rowid)
                if latest_order is None or order>latest_order:latest=state;latest_order=order
            if latest is None:return empty
            shown=json.loads(profile[0]).get('nativeResultsPresented',{}) if profile else {}
            attempts=latest.get('nativeBattleAttempts',{});attempt=attempts.get(str(latest['round']),0) if isinstance(attempts,dict) else None
            identity={'runId':latest['runId'],'revision':latest['revision'],'battleAttempt':attempt}
            # An acknowledged native UI construction consumes only this exact
            # result. Never fall back to an older, previously unseen game.
            if isinstance(shown,dict) and shown.get(latest['runId'])==identity:return empty
            return self.envelope(latest)

    def result_presented(self,run,body):
        """Persist that the native result DTO was actually constructed.

        This UI receipt cannot finish a battle, authorize scene unload, award
        anything, or change a run. Explicit result GET remains available.
        """
        if not isinstance(run,str) or not RUN_ID.fullmatch(run):raise DomainError('invalid_native_scene_id')
        if not isinstance(body,dict) or set(body)!={'activityId','revision','battleAttempt'}:
            raise DomainError('invalid_result_presented')
        if body['activityId']!=ACTIVITY:raise DomainError('wrong_private_activity')
        revision=integer(body['revision'],'revision',1)
        attempt=integer(body['battleAttempt'],'battleAttempt',0)
        with self.store.lock,self.store.db:
            row=self.store.db.execute('SELECT p.catalog_hash,p.data,r.state FROM activity_profiles p '
                'JOIN runs r ON r.id=? WHERE p.id=?',(run,ACTIVITY)).fetchone()
            if row is None:raise DomainError('result_presented_session_required')
            profile=json.loads(row[1]);state=json.loads(row[2])
            if (not isinstance(profile,dict) or not isinstance(state,dict) or row[0]!=self.c.hash or
                    profile.get('activityId')!=ACTIVITY or profile.get('activeRun') not in (None,run) or
                    profile.get('draftRun') is not None):raise DomainError('wrong_native_scene')
            if (state.get('runId')!=run or state.get('activityId')!=ACTIVITY or state.get('catalogHash')!=self.c.hash or
                    type(state.get('policyVersion')) is not int or state['policyVersion']!=1 or
                    type(state.get('nativeSessionVersion')) is not int or state['nativeSessionVersion']!=1 or
                    state.get('modeId') not in SINGLE_MODES or state.get('phase')!='result' or
                    type(state.get('round')) is not int or state['round']<1 or
                    ('nativeBandConfirmed' in state and state['nativeBandConfirmed'] is not True) or
                    not isinstance(state.get('result'),dict) or type(state['result'].get('success')) is not bool or
                    type(state['result'].get('roundsPassed')) is not int):raise DomainError('invalid_result_presented_session')
            if type(state.get('revision')) is not int or state['revision']!=revision:
                raise DomainError('result_presented_revision_changed')
            attempts=state.get('nativeBattleAttempts',{})
            current=attempts.get(str(state['round']),0) if isinstance(attempts,dict) else None
            if type(current) is not int or current<0 or current!=attempt:raise DomainError('native_battle_attempt_mismatch')
            identity={'runId':run,'revision':revision,'battleAttempt':attempt}
            shown=profile.get('nativeResultsPresented',{})
            if not isinstance(shown,dict):raise DomainError('invalid_result_presented_index')
            if shown.get(run)!=identity:
                shown[run]=identity;profile['nativeResultsPresented']=shown
                self.store.db.execute('UPDATE activity_profiles SET data=? WHERE id=?',(dump(profile),ACTIVITY))
        return {'ok':True,'activityId':ACTIVITY,**identity,'presented':True,'officialFallback':False}

    def _finish_exit_input(self,run,request_id,revision,body,kind):
        """Validate the inherited finish-service body, never derive a result."""
        if not isinstance(run,str) or not RUN_ID.fullmatch(run):raise DomainError('invalid_native_scene_id')
        if not isinstance(request_id,str) or not re.fullmatch(r'[A-Za-z0-9._:-]{1,128}',request_id):
            raise DomainError('invalid_request_id')
        integer(revision,'revision',1)
        if (not isinstance(body,dict) or set(body)-{'activityId','data','battleData'} or
                not {'activityId','data'}<=set(body)):
            raise DomainError('invalid_'+kind+'_request')
        if body['activityId']!=ACTIVITY:raise DomainError('wrong_private_activity')
        if not isinstance(body['data'],str) or len(body['data'])>1048576:raise DomainError('invalid_'+kind+'_data')
        battle_data=body.get('battleData')
        if battle_data is not None:
            if not isinstance(battle_data,dict) or set(battle_data)-{'stats','isCheat','completeTime'}:
                raise DomainError('invalid_'+kind+'_battle_data')
            stats=battle_data.get('stats')
            if stats is not None and (not isinstance(stats,dict) or len(stats)>256 or
                    any(not isinstance(key,str) or len(key)>256 or not isinstance(value,str) or len(value)>4096
                        for key,value in stats.items())):
                raise DomainError('invalid_'+kind+'_battle_data')
            cheat=battle_data.get('isCheat')
            if cheat is not None and (not isinstance(cheat,str) or len(cheat)>4096):
                raise DomainError('invalid_'+kind+'_battle_data')
            if 'completeTime' in battle_data:integer(battle_data['completeTime'],'completeTime',0,9223372036854775807)

    @staticmethod
    def _finish_exit_reply():
        return {'result':0,'apFailReturn':0,'itemReturn':[],'rewards':[],'unusualRewards':[],
            'overrideRewards':[],'additionalRewards':[],'diamondMaterialRewards':[],'furnitureRewards':[],
            'pushMessage':[],'playerDataDelta':{'modified':{},'deleted':{}}}

    def saved_exit_ack(self,run,request_id,revision,body):
        """Read-only native finish-service ACK for an already saved solo exit.

        The inherited training service is only the scene-unload transport. Its
        opaque battle payload is never a score, tutorial completion or reward.
        Authority comes exclusively from the exact successful Suspend receipt
        and an unchanged, still-active private save. Do not call profile getters
        or presence expiry here: even their normal reads may persist changes.
        """
        self._finish_exit_input(run,request_id,revision,body,'saved_exit')
        with self.store.lock:
            # One SELECT gives all three records the same SQLite snapshot,
            # including against an external writer, without opening any write
            # transaction or invoking a getter that may normalize a record.
            row=self.store.db.execute('SELECT p.catalog_hash,p.data,r.state,c.reply FROM activity_profiles p '
                'JOIN runs r ON r.id=? JOIN native_session_commands c ON c.run=r.id AND c.id=? WHERE p.id=?',
                (run,request_id,ACTIVITY)).fetchone()
            if row is None:raise DomainError('saved_exit_proof_required')
            profile=json.loads(row[1]);state=json.loads(row[2]);receipt=json.loads(row[3])
            if not all(isinstance(value,dict) for value in (profile,state,receipt)):
                raise DomainError('invalid_saved_exit_session')
            if row[0]!=self.c.hash or profile.get('activityId')!=ACTIVITY or profile.get('activeRun')!=run:
                raise DomainError('wrong_native_scene')
            if (state.get('runId')!=run or state.get('activityId')!=ACTIVITY or state.get('catalogHash')!=self.c.hash or
                    type(state.get('policyVersion')) is not int or state['policyVersion']!=1 or
                    type(state.get('nativeSessionVersion')) is not int or state['nativeSessionVersion']!=1 or
                    state.get('modeId') not in SINGLE_MODES or state.get('phase') not in ('loading','preparation','special') or
                    state.get('result') is not None or type(state.get('round')) is not int or state['round']<0 or
                    ('nativeBandConfirmed' in state and state['nativeBandConfirmed'] is not True) or
                    ('nativeBandConfirmed' not in state and state['round']==0)):
                raise DomainError('invalid_saved_exit_session')
            if type(state.get('revision')) is not int or state['revision']!=revision:
                raise DomainError('saved_exit_revision_changed')
            details=receipt.get('details')
            if (receipt.get('ok') is not True or receipt.get('activityId')!=ACTIVITY or receipt.get('runId')!=run or
                    receipt.get('requestId')!=request_id or type(receipt.get('revision')) is not int or
                    receipt['revision']!=revision or not isinstance(details,dict) or details.get('suspended') is not True or
                    not isinstance(receipt.get('state'),dict) or dump(receipt['state'])!=dump(state)):
                raise DomainError('saved_exit_receipt_mismatch')
        return self._finish_exit_reply()

    def terminal_exit_ack(self,run,request_id,revision,body):
        """Acknowledge native unload only after the exact real combat finish.

        Terminal presence may already have cleared activeRun. A single SELECT
        binds the retained profile, current run and immutable successful combat
        ACK to one snapshot. No getter, expiry refresh, result transition or
        reward processing is allowed on this cleanup-only path.
        """
        self._finish_exit_input(run,request_id,revision,body,'terminal_exit')
        with self.store.lock:
            row=self.store.db.execute('SELECT p.catalog_hash,p.data,r.state,c.reply FROM activity_profiles p '
                'JOIN runs r ON r.id=? JOIN native_session_commands c ON c.run=r.id AND c.id=? WHERE p.id=?',
                (run,request_id,ACTIVITY)).fetchone()
            if row is None:raise DomainError('terminal_exit_proof_required')
            profile=json.loads(row[1]);state=json.loads(row[2]);receipt=json.loads(row[3])
            if not all(isinstance(value,dict) for value in (profile,state,receipt)):
                raise DomainError('invalid_terminal_exit_session')
            if (row[0]!=self.c.hash or profile.get('activityId')!=ACTIVITY or
                    profile.get('activeRun') not in (None,run) or profile.get('draftRun') is not None):
                raise DomainError('wrong_native_scene')
            result=state.get('result');round_id=state.get('round');player=state.get('player')
            if (state.get('runId')!=run or state.get('activityId')!=ACTIVITY or state.get('catalogHash')!=self.c.hash or
                    type(state.get('policyVersion')) is not int or state['policyVersion']!=1 or
                    type(state.get('nativeSessionVersion')) is not int or state['nativeSessionVersion']!=1 or
                    state.get('modeId') not in SINGLE_MODES or state.get('phase')!='result' or
                    type(round_id) is not int or round_id not in self.c.turns(state['modeId']) or
                    ('nativeBandConfirmed' in state and state['nativeBandConfirmed'] is not True) or
                    not isinstance(result,dict) or type(result.get('success')) is not bool or
                    type(result.get('roundsPassed')) is not int or not 0<=result['roundsPassed']<=round_id or
                    result.get('abandoned') is True or state.get('nativeSessionEndReason') is not None or
                    not isinstance(player,dict) or type(player.get('hp')) is not int or player['hp']<0 or
                    type(result.get('hp')) is not int or result['hp']!=player['hp'] or
                    type(state.get('nativeEndedAtTs')) is not int or state['nativeEndedAtTs']<0):
                raise DomainError('invalid_terminal_exit_session')
            if type(state.get('revision')) is not int or state['revision']!=revision:
                raise DomainError('terminal_exit_revision_changed')
            details=receipt.get('details')
            if (receipt.get('ok') is not True or receipt.get('activityId')!=ACTIVITY or receipt.get('runId')!=run or
                    receipt.get('requestId')!=request_id or type(receipt.get('revision')) is not int or
                    receipt['revision']!=revision or not isinstance(details,dict) or details.get('suspended') is not None or
                    type(details.get('nativeCombatVersion')) is not int or details['nativeCombatVersion']!=VERSION or
                    type(details.get('completedRound')) is not int or details['completedRound']!=round_id or
                    details.get('duplicateRound',False) is not False or
                    not isinstance(receipt.get('state'),dict) or dump(receipt['state'])!=dump(state)):
                raise DomainError('terminal_exit_receipt_mismatch')
            reports=state.get('completedReports');history=state.get('history')
            completed=reports.get(str(round_id)) if isinstance(reports,dict) else None
            last=history[-1] if isinstance(history,list) and history else None
            if (not isinstance(completed,dict) or not isinstance(last,dict) or last.get('op')!='battle.finish' or
                    last.get('phase')!='result' or type(last.get('round')) is not int or last['round']!=round_id or
                    not isinstance(last.get('payload'),dict) or type(last['payload'].get('round')) is not int or
                    last['payload']['round']!=round_id):
                raise DomainError('terminal_exit_combat_proof_required')
            boss=self.c.turns(state['modeId'])[round_id]['IsBossTurn']
            field='nativeBossReport' if boss else 'nativeReport'
            report=completed.get(field);digest=completed.get(field+'Digest')
            if (not isinstance(report,dict) or type(report.get('round')) is not int or report['round']!=round_id or
                    not isinstance(digest,str) or not re.fullmatch(r'[0-9a-f]{64}',digest) or
                    hashlib.sha256(dump(report).encode()).hexdigest()!=digest or
                    (boss and (type(details.get('bossDefeated')) is not bool or
                        details['bossDefeated'] is not completed.get('bossDefeated') or
                        details['bossDefeated'] is not (report.get('result')=='victory'))) or
                    (not boss and details.get('bossDefeated') is not None)):
                raise DomainError('terminal_exit_combat_proof_required')
            attempt=validate_finish_attempt(state,report)
            if completed.get('battleAttempt',0)!=attempt or type(completed.get('battleAttempt',0)) is not int:
                raise DomainError('native_battle_attempt_mismatch')
        return self._finish_exit_reply()

    def create(self,body):
        request_id,fingerprint=self._request(body)
        with self.store.lock:
            previous=self._replay('native_session_starts','id=?',(request_id,),fingerprint)
            if previous is not None:return self._check_start_receipt(previous)
            profile=self.activity.get()
            deferred=body.get('deferBandSelection',False)
            if type(deferred)is not bool:raise DomainError('invalid_native_band_confirmation')
            mode=body.get('modeId','mode_single_funny')
            if mode not in SINGLE_MODES:raise DomainError('native_single_mode_required')
            if any(k in body for k in ('chessPool','diyChessPool','squad','stageId')):raise DomainError('private_profile_snapshot_required')
            band=body.get('bandId')
            if band not in self.c.season['BandDataListDict']:raise DomainError('unknown_band')
            draft=profile.get('draftRun')
            if draft:
                preview=self._state(draft)
                if not self.activity.presence.draft(preview):raise DomainError('invalid_native_band_draft')
                if deferred and preview['modeId']==mode:
                    # A lost create acknowledgement must not redraw the map,
                    # bans, roster snapshot or seed on the next launch attempt.
                    reply=self.envelope(preview,request_id=request_id,details={'configurationSnapshot':True,'reusedDraft':True})
                    with self.store.db:self.store.db.execute('INSERT INTO native_session_starts VALUES(?,?,?)',(request_id,fingerprint,dump(reply)))
                    return reply
                if body.get('replaceActiveRunId')!=draft:raise DomainError('private_session_already_active',draft)
            retired=None
            if profile['activeRun']:
                current=self._state(profile['activeRun'])
                if current['phase']!='result':
                    # Selecting another independent difficulty starts a new run.
                    # Require the observed old run explicitly, retain its complete
                    # abandoned history, and commit both records atomically below.
                    legacy=any(current.get(key,0)==0 for key in ('poolVersion','setupVersion','modeStrengthVersion'))
                    restart_legacy=body.get('restartLegacyRun') is True and legacy
                    if body.get('replaceActiveRunId')!=current['runId'] or mode==current['modeId'] and not restart_legacy:
                        raise DomainError('private_session_already_active',current['runId'])
                    retired=copy.deepcopy(current)
                    self.store.engine.apply(retired,'quit',{})
                    retired['nativeEndedAtTs']=int(self.combat.clock())
            # The server reads the saved private roster; callers cannot inject a
            # second chess pool or silently discard selected skills/modules.
            if any(k in body for k in ('chessPool','diyChessPool','squad','stageId')):raise DomainError('private_profile_snapshot_required')
            band=body.get('bandId')
            if band not in self.c.season['BandDataListDict']:raise DomainError('unknown_band')
            configured=copy.deepcopy(profile['playerActivity']['chessSquad'])
            if not configured:raise DomainError('empty_private_chess_pool')
            diy,native_squad=snapshot_profile(self.c,configured)
            seed=body['seed'] if 'seed' in body else secrets.randbits(32) or 1
            args={'modeId':mode,'seed':seed,'chessPool':sorted(configured),'diyChessPool':diy}
            run=uuid.uuid4().hex;state=self.store.engine.create(run,args)
            state.update(activityId=ACTIVITY,nativeSessionVersion=1,nativeActionReports={},lastNativeActionSeq=-1,lastNativeActionRound=0,
                nativeSquad=native_squad)
            self.store.engine.setup.refresh_bans(state)
            if deferred:
                # The native briefing requires a valid preview band and HP.
                # This is presentation only: no domain selection/effect/history,
                # active presence or reconnect deadline exists until confirmation.
                state.update(phase='loading',bandId=band,nativeBandConfirmed=False)
                state['player']['hp']=self.c.season['BandDataListDict'][band]['TotalHp']
                reconcile(self.c,state)
                profile.update(activeRun=None,draftRun=run)
            else:
                state.update(nativeBandConfirmed=True,nativeStartedAtTs=int(self.combat.clock()))
                self.activity.presence.initialize(state)
                self.store.engine.apply(state,'band.choose',{'bandId':band})
                profile['activeRun']=run;profile['draftRun']=None
            reply=self.envelope(state,request_id=request_id,details={'configurationSnapshot':True,'bandSelectionDeferred':deferred})
            with self.store.db:
                if retired is not None:self.store.db.execute('UPDATE runs SET state=? WHERE id=?',(dump(retired),retired['runId']))
                self.store.db.execute('INSERT INTO runs VALUES(?,?)',(run,dump(state)))
                self.store.db.execute('UPDATE activity_profiles SET data=? WHERE id=?',(dump(profile),ACTIVITY))
                self.store.db.execute('INSERT INTO native_session_starts VALUES(?,?,?)',(request_id,fingerprint,dump(reply)))
            return reply

    def _actions(self,state,payload):
        """Only the CN proven boss damage record; step timing remains pending."""
        if not isinstance(payload,dict):raise DomainError('invalid_payload')
        self.store.engine._phase(state,'battle','boss')
        seq=integer(payload.get('seq'),'seq');actions=payload.get('actions')
        if not isinstance(actions,list) or len(actions)>4096:raise DomainError('invalid_native_actions')
        digest=hashlib.sha256(dump(actions).encode()).hexdigest();reports=state['nativeActionReports'];key=str(state['round'])+':'+str(seq)
        if key in reports:
            if reports[key]!=digest:raise DomainError('native_action_seq_conflict')
            return {'duplicateNativeSeq':True,'stepTimingImplemented':False}
        if state.get('lastNativeActionRound')==state['round'] and seq<=state['lastNativeActionSeq']:raise DomainError('stale_native_action_seq')
        for action in actions:
            if not isinstance(action,dict):raise DomainError('invalid_native_actions')
            if action.get('playerUidIndex')!=0:raise DomainError('wrong_native_player')
            if action.get('operate')!=self.c.enum('Torappu.AutoChessBattleStepActionOperate','COST_BOSS_HP'):
                raise DomainError('native_step_operate_adapter_pending',str(action.get('operate')))
            params=action.get('paramList')
            if not isinstance(params,list) or len(params)!=4 or any(type(v)is not int for v in params):raise DomainError('invalid_boss_damage_params')
            integer(params[0],'damage',1,self.c.number('Torappu.Battle.AutoChessBattleConst','MAX_BATTLE_DAMAGE')-1)
            integer(params[1],'attackerInstId',-2147483648)
            if params[2] not in self.c.constants['enums']['Torappu.AutoChessBattleDamageSrcType'].values():raise DomainError('invalid_damage_source')
            integer(params[3],'creditedPlayer',-1,0)
            self.store.engine.apply(state,'boss.damage',{'damage':params[0]})
        state['lastNativeActionRound']=state['round'];state['lastNativeActionSeq']=seq;reports[key]=digest
        return {'acknowledgedNativeSeq':seq,'stepTimingImplemented':False}

    def _select_band(self,state,payload):
        """Local launch selection only; reuse domain HP/version/history rules."""
        self.store.engine._phase(state,'loading')
        if state['round']!=0:raise DomainError('native_band_selection_closed')
        if not isinstance(payload,dict) or set(payload)!={'bandId'}:raise DomainError('invalid_payload')
        band=payload['bandId']
        if not isinstance(band,str) or band not in self.c.season['BandDataListDict']:raise DomainError('unknown_band')
        if state.get('nativeBandConfirmed') is True:
            if band!=state['bandId']:raise DomainError('native_band_selection_closed')
            return {'selectedBandId':band,'duplicateBandSelection':True,'initialStateReplaced':False}
        profile=json.loads(self.store.db.execute('SELECT data FROM activity_profiles WHERE id=?',(ACTIVITY,)).fetchone()[0])
        if profile.get('draftRun')!=state['runId'] or profile.get('activeRun') not in (None,state['runId']):
            raise DomainError('wrong_native_scene')
        # Engine.band.choose owns the HP rule and command history. The loading
        # state is an isolated transaction copy; this intermediate phase is
        # never published, and band.choose performs no random draws or setup.
        state['phase']='band'
        details=self.store.engine.apply(state,'band.choose',{'bandId':band})
        state.update(nativeBandConfirmed=True,nativeStartedAtTs=int(self.combat.clock()))
        self.activity.presence.initialize(state)
        profile.update(activeRun=state['runId'],draftRun=None)
        self.activity.presence._save_profile(profile)
        return {**details,'selectedBandId':band,'initialStateReplaced':True}

    def _suspend(self,state,payload):
        """Leave a solo scene without abandoning it or inventing battle results.

        Preparation and special decisions are already durable. An unfinished
        native battle can only be staged again when its complete state exactly
        matches the successful Ready receipt. Accepted battle progress cannot
        be reconstructed by the current adapter and therefore fails closed.
        """
        if not isinstance(payload,dict) or set(payload)!={'expectedRevision'}:raise DomainError('invalid_payload')
        if integer(payload['expectedRevision'],'expectedRevision')!=state['revision']:
            raise DomainError('native_suspend_revision_changed')
        if self.activity.presence.draft(state):raise DomainError('native_band_selection_required')
        profile=json.loads(self.store.db.execute('SELECT data FROM activity_profiles WHERE id=?',(ACTIVITY,)).fetchone()[0])
        if profile.get('activeRun')!=state['runId']:raise DomainError('wrong_native_scene')
        phase=state['phase'];details={}
        if phase=='paused':
            previous=state.get('previousPhase')
            if previous!='preparation':raise DomainError('native_suspend_battle_progress_pending')
            state['phase']=previous;state.pop('previousPhase')
            self.combat._replay_transition(state,'session.suspend',{'sourcePhase':'paused','resumePhase':previous})
        elif phase in ('battle','boss'):
            details=self._replay_unstarted(state,payload)
        else:self.store.engine._phase(state,'loading','preparation','special')
        return {**details,'suspended':True,'resumePhase':state['phase'],
            'nativeMidBattleResume':False,'requiresManualPreparationReady':bool(state.get('nativeReplayPending'))}

    def _discard_draft(self,state,payload):
        """Cancel a briefing preview without starting or abandoning a match.

        Keep the immutable creation/command receipts and a small audit marker,
        but detach the draft from the profile. No engine operation, strategy
        effect, expiry, score or reward belongs to an unconfirmed preview.
        """
        if not isinstance(payload,dict) or set(payload)!={'expectedRevision'}:raise DomainError('invalid_payload')
        if integer(payload['expectedRevision'],'expectedRevision',0)!=state['revision']:
            raise DomainError('native_draft_revision_changed')
        if not self.activity.presence.draft(state):raise DomainError('native_draft_confirmation_closed')
        profile=json.loads(self.store.db.execute('SELECT data FROM activity_profiles WHERE id=?',(ACTIVITY,)).fetchone()[0])
        if profile.get('draftRun')!=state['runId'] or profile.get('activeRun') is not None:
            raise DomainError('wrong_native_scene')
        state.update(nativeDraftDiscardedAtTs=int(self.combat.clock()),nativeDraftDiscardedFromRevision=state['revision'])
        state['revision']+=1;state['seq']+=1
        state['history'].append({'seq':state['seq'],'op':'session.draft.discard','payload':{},'round':0,'phase':'loading'})
        profile['draftRun']=None;self.activity.presence._save_profile(profile)
        return {'draftDiscarded':True,'matchStarted':False,'requiresNewLaunch':True}

    def execute(self,run,body):
        request_id,fingerprint=self._request(body)
        with self.store.lock:
            self.store.db.execute('BEGIN IMMEDIATE')
            try:
                # A deliberately restored battle uses a new, persisted attempt
                # only for that round. Reject an old successful HTTP receipt
                # before _replay can return its former terminal snapshot. Read
                # the raw row here: presence/normalization may write or expire.
                name=body.get('type');payload=body.get('payload')
                raw=self.store.db.execute('SELECT state FROM runs WHERE id=?',(run,)).fetchone()
                if raw is not None and 'nativeDraftDiscardedAtTs' in json.loads(raw[0]):
                    # Only the exact successful cancellation may be replayed.
                    # Old commands/start receipts cannot reopen its briefing,
                    # and a late cancellation must not detach a newer draft.
                    if name!='StrongholdPrivateDiscardDraft':raise DomainError('native_draft_discarded')
                    previous=self._replay('native_session_commands','run=? AND id=?',(run,request_id),fingerprint)
                    if previous is None:raise DomainError('native_draft_discarded')
                    self.store.db.commit();return previous
                finish_types=('AutoChessBattleSceneSelfBattleFinishUp',NS+'AutoChessBattleSceneSelfBattleFinishUp',
                    self.c.constants['protocolIds'][NS+'AutoChessBattleSceneSelfBattleFinishUp'],'StrongholdPrivateBossBattleFinish')
                if name in finish_types and isinstance(payload,dict):
                    raw=self.store.db.execute('SELECT state FROM runs WHERE id=?',(run,)).fetchone()
                    if raw is not None:
                        observed=json.loads(raw[0])
                        if 'nativeBattleAttempts' in observed or 'battleAttempt' in payload:
                            attempt=validate_finish_attempt(observed,payload)
                            if attempt>0 and request_id.startswith(('native-self-finish-','native-boss-finish-')):
                                kind='boss' if name=='StrongholdPrivateBossBattleFinish' else 'self'
                                expected='native-'+kind+'-finish-'+run+'-'+str(payload['round'])+'-r'+str(attempt)
                                if request_id!=expected:raise DomainError('native_battle_attempt_mismatch')
                previous=self._replay('native_session_commands','run=? AND id=?',(run,request_id),fingerprint)
                if previous is not None:self.store.db.commit();return previous
                before=self._state(run);state=copy.deepcopy(before);op=None;is_suspend=False;is_discard=False
                # Domain validation and native DTO projection share one command
                # boundary. Helpers may update profile pointers as well as the
                # isolated state copy; a rejected DTO must roll both back while
                # retaining the outer transaction for its durable error receipt.
                self.store.db.execute('SAVEPOINT native_session_operation')
                try:
                    name=body.get('type');payload=body.get('payload',{})
                    is_suspend=name=='StrongholdPrivateSuspendSession'
                    is_discard=name=='StrongholdPrivateDiscardDraft'
                    if self.activity.presence.draft(state) and name not in ('StrongholdPrivateSelectBand','StrongholdPrivateDiscardDraft'):
                        raise DomainError('native_band_selection_required')
                    if state.get('nativeReplayPending'):
                        loading='AutoChessBattleSceneLoadingReadyUp'
                        if name in (loading,NS+loading,self.c.constants['protocolIds'][NS+loading]):
                            op='battle.replay.pending';raise DomainError('native_battle_replay_manual_ready_required')
                        allowed=('AutoChessBattleScenePreparationReadyUp','AutoChessBattleSceneQuitUp')
                        if not is_suspend and name not in (*allowed,*(NS+value for value in allowed),
                                *(self.c.constants['protocolIds'][NS+value] for value in allowed)):
                            op='battle.replay.pending';raise DomainError('native_battle_replay_pending')
                    bond_name='AutoChessBattleSceneSelfBattleAddBondUp'
                    if name in (bond_name,NS+bond_name,self.c.constants['protocolIds'][NS+bond_name]):op='bond.add'
                    # A completed final report can be semantically retried with
                    # a new HTTP ID after the state has advanced to round2.
                    full=self.c.constants['protocolIds'][NS+'AutoChessBattleSceneSelfBattleFinishUp']
                    finish=name in ('AutoChessBattleSceneSelfBattleFinishUp',NS+'AutoChessBattleSceneSelfBattleFinishUp',full,'StrongholdPrivateBossBattleFinish')
                    completed=finish and isinstance(payload,dict) and str(payload.get('round')) in state['completedReports']
                    if 'round' in body and integer(body['round'],'round')!=state['round'] and not completed:raise DomainError('stale_round')
                    if name=='StrongholdPrivateReplayUnstartedBattle':
                        op='battle.replay.unstarted';details=self._replay_unstarted(state,payload)
                    elif name=='StrongholdPrivateSelectBand':
                        op='band.choose';details=self._select_band(state,payload)
                    elif is_suspend:
                        op='session.suspend';details=self._suspend(state,payload)
                    elif is_discard:
                        op='session.draft.discard';details=self._discard_draft(state,payload)
                    elif name=='StrongholdPrivateBossBattleFinish':
                        op='boss.finish';details=self.combat.boss_finish(state,payload)
                    elif name in ('AutoChessBattleSceneActionUp',NS+'AutoChessBattleSceneActionUp',self.c.constants['protocolIds'][NS+'AutoChessBattleSceneActionUp']):
                        op='native.actions';details=self._actions(state,payload)
                    else:
                        op,args=self.protocol.decode(name,payload)
                        if op=='bond.add' and 'round' not in body:raise DomainError('native_bond_round_required')
                        repair_identity_counter(state)
                        if op in ('board.move','item.use'):
                            # Hand positions are omitted from domain deployment,
                            # but every client-supplied instance must be owned.
                            for unit in payload['unitPositionInfo']:self.store.engine._instance(state,unit['instId'])
                        if op=='preparation.ready':
                            details=(self.combat.start_replayed(state) if state.get('nativeReplayPending')
                                     else self.combat.ready(state,args))
                        elif op=='battle.finish':details=self.combat.finish(state,args)
                        elif op=='pause' and state['phase'] in ('battle','boss'):raise DomainError('native_battle_pause_pending')
                        else:details=self.store.engine.apply(state,op,args)
                    if state['phase']=='result' and 'nativeEndedAtTs' not in state:state['nativeEndedAtTs']=int(self.combat.clock())
                    # Band identity belongs to staticPlayers, outside the
                    # preparation-only delta; replace the initial full DTO.
                    reply=self.envelope(state,None if name=='StrongholdPrivateSelectBand' else before,op,request_id=request_id,details=details)
                    if is_suspend or is_discard:reply['messages']=[]
                    self.store.db.execute('UPDATE runs SET state=? WHERE id=?',(dump(state),run))
                    self.store.db.execute('RELEASE native_session_operation')
                except DomainError as ex:
                    self.store.db.execute('ROLLBACK TO native_session_operation')
                    self.store.db.execute('RELEASE native_session_operation')
                    reply=self.envelope(before,before,op,False,request_id)
                    reply.update(error=ex.code,detail=ex.detail)
                    if is_suspend or is_discard:reply['messages']=[]
                self.store.db.execute('INSERT INTO native_session_commands VALUES(?,?,?,?)',(run,request_id,fingerprint,dump(reply)))
                self.store.db.commit();return reply
            except Exception:
                self.store.db.rollback();raise
