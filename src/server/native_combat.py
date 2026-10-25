"""Single-player native combat policy; no Unity simulation or boss step clock.

Enemy templates and instance identity are server owned. Unity reports the full
partition of issued enemies. PREPARE is deliberately untimed; BATTLE uses the CN
versioned display/simulation duration as a persisted Unix-seconds UI deadline.
"""
import copy
import hashlib
import time

from .engine import DomainError,integer
from .storage import dump
from . import battle_timing

VERSION=2
INTRO_MODE='mode_single_funny'
INTRO_STAGE='act1autochess_m01'
SINGLE_MODES=('mode_single_funny','mode_single_normal','mode_single_hard','mode_single_abyss')
# Pinned CN BossPlayerGroup.BOSS_PLAYER_GROUP_1. LOCAL._GatherBossBattleEnemy
# assigns this group to both the local player and enemy units.
# driven by Unity's LOCAL MockBossBattleManager rather than multiplayer StepDn.
BOSS_GROUP=1


def battle_attempt(state,round_id):
    """Absent preserves legacy identity. Only explicit recovery adds epochs."""
    attempts=state.get('nativeBattleAttempts',{})
    if (type(attempts)is not dict or len(attempts)>15 or
            any(not isinstance(key,str) or not 1<=len(key)<=2 or not key.isascii() or not key.isdecimal() or
                str(int(key))!=key or not 1<=int(key)<=15 or type(value)is not int or
                not 1<=value<=2147483647 for key,value in attempts.items())):
        raise DomainError('invalid_native_battle_attempt')
    return attempts.get(str(round_id),0)


def validate_finish_attempt(state,payload):
    """May run before memo replay using raw saved state, without presence GET."""
    if not isinstance(payload,dict):raise DomainError('invalid_payload')
    round_id=integer(payload.get('round'),'round',1)
    required=battle_attempt(state,round_id);actual=payload.get('battleAttempt',0)
    if type(actual)is not int or not 0<=actual<=2147483647:
        raise DomainError('invalid_native_battle_attempt')
    if actual!=required or ('battleAttempt' in payload and actual==0):
        raise DomainError('native_battle_attempt_mismatch')
    return required


def round_boss_id(catalog,state,round_id=None):
    """Resolve the boss from this round, including the R15 preparation preview.

    The domain switches bossId when Ready builds the battle. Before that point
    the next hidden round must already preview its selected hiddenBossId.
    """
    variants=catalog.rounds(state['modeId']).get(max(1,state['round']) if round_id is None else round_id,[])
    available={row.get('BossId') for row in variants if row.get('BossId')}
    for key in (state.get('bossId'),state.get('hiddenBossId')):
        if key in available:return key
    return state['bossId']


def configured_boss_hp(catalog,state):
    definition=catalog.season['BossInfoDict'][round_boss_id(catalog,state)]
    key={1:'BloodPoint',2:'BloodPointNormal',3:'BloodPointHard',4:'BloodPointAbyss'}.get(catalog.mode(state['modeId'])['ModeDifficulty'],'BloodPoint')
    return definition[key]


def local_boss_keys(catalog):
    """CN AutoChessBattleUtil.IsBossEnemy resolves the common boss EnemyIds."""
    return {value['EnemyId'] for value in catalog.common['BossInfoDict'].values()}


def checkpoint(state,catalog):
    mode=state.get('modeId');stage=catalog.season['StageDatasDict'].get(state.get('stageId'))
    if (mode not in SINGLE_MODES or catalog.mode(mode)['ModeType']!=1 or not stage or
            stage['Weight']<=0 or mode not in stage['Mode'] or state['round'] not in catalog.rounds(mode)):
        raise DomainError('native_single_battle_checkpoint')


def template_rows(catalog,state):
    """Original first-fragment action index, including positions of local actions."""
    round_id=max(1,state['round']);variants=catalog.rounds(state['modeId']).get(round_id,[])
    if not variants:return []
    battle=state.get('battle')
    if battle and battle['round']==round_id:level_id=battle['levelId']
    elif state.get('setupVersion',0)>=2:
        from .setup_rules import SetupRules
        from types import SimpleNamespace
        level_id=SetupRules(SimpleNamespace(c=catalog)).variant(state,round_id)['LevelId']
    else:
        matching=[v for v in variants if v.get('BossId')==round_boss_id(catalog,state,round_id)]
        level_id=(matching or variants)[0]['LevelId']
    waves=catalog.level(level_id).get('Waves') or []
    if not waves or not waves[0].get('Fragments'):return []
    from .setup_rules import SetupRules
    from types import SimpleNamespace
    actions=SetupRules(SimpleNamespace(c=catalog)).actions(state,level_id)
    result=[{'enemyType':0,'enemyKey':action['Key'],'actionIndex':action['nativeActionIndex'],'count':action['Count'],'round':round_id}
        for action in actions
        if action['ActionType']==0 and action['Count']>0 and action.get('ManagedByScheduler',True) and
            not action.get('RandomSpawnGroupKey') and action['Key'] not in local_boss_keys(catalog)]
    result.extend({key:value for key,value in row.items() if key!='effectInstId'}
        for row in bounty_rows(catalog,state,level_id))
    return result


def bounty_rows(catalog,state,level_id):
    """Out-of-range indexes select CN's native _GetSpEnemyActionData fallback.

    Both GenerateSelfBattleEnemyData and GenerateBossBattleEnemyData call the
    same original action factory. No local action rewrite or guessed route is
    needed: the native level selects its ground/flying special action template.
    """
    waves=catalog.level(level_id).get('Waves') or []
    if not waves or not waves[0].get('Fragments'):return []
    first=len(waves[0]['Fragments'][0].get('Actions') or [])
    battle=state.get('battle')
    active=(battle.get('itemBounties',[]) if battle and battle['round']==state['round'] else
        [row for row in state.get('itemBounties',[]) if row['remainingBattles']>0 and row['fromRound']<=state['round']])
    return [{'enemyType':0,'enemyKey':row['enemyId'],'actionIndex':first+index,'count':row['count'],
        'round':state['round'],'effectInstId':row['effectInstId']} for index,row in enumerate(active)]


def repair_identity_counter(state):
    """Existing saves retain IDs; only advance a stale allocator on next command."""
    owned=[]
    for field in ('charChess','equipChess','trapChess'):
        owned.extend(integer(v['instId'],'savedInstanceId') for v in state['player'][field])
    if len(owned)!=len(set(owned)):raise DomainError('native_duplicate_saved_instance')
    used=list(owned)
    used.extend(integer(v['effectInstId'],'savedEffectId') for v in state.get('specialEffects',[]))
    if state.get('battle'):used.extend(integer(v['instId'],'savedEnemyId') for v in state['battle']['units'])
    for report in state['completedReports'].values():used.extend(report.get('killed',[])+report.get('escaped',[]))
    state['nextInstId']=max([integer(state['nextInstId'],'nextInstId'),*used])


class NativeCombat:
    def __init__(self,engine,clock=None):self.engine=engine;self.c=engine.c;self.clock=clock or time.time

    def _replay_transition(self,state,op,payload):
        # This is an adapter transition, not a preparation/gameplay event. In
        # particular it must not settle traits, grant items or draw any RNG.
        state['revision']+=1;state['seq']+=1
        state['history'].append({'seq':state['seq'],'op':op,'payload':copy.deepcopy(payload),
            'round':state['round'],'phase':state['phase']})

    def replay_unstarted(self,state,ready_request):
        """Stage an exact post-Ready checkpoint for a fresh native scene.

        NativeSession has compared the complete state with a durable successful
        Ready response. Accepted battle callbacks make that comparison fail;
        this is not recovery of elapsed combat or a rollback of its rewards.
        """
        checkpoint(state,self.c);self.engine._phase(state,'battle','boss')
        battle=state['battle']
        if battle.get('nativeCombatVersion')!=VERSION:raise DomainError('native_battle_replay_version')
        if battle.get('battleAttempt',0)!=battle_attempt(state,state['round']):raise DomainError('native_battle_attempt_mismatch')
        state['nativeReplayPending']={'version':1,'round':state['round'],'sourcePhase':state['phase'],
            'sourceRevision':state['revision'],'readyRequestId':ready_request,
            'battleSha256':hashlib.sha256(dump(battle).encode()).hexdigest()}
        state['phase']='preparation'
        self._replay_transition(state,'battle.replay.unstarted',{'readyRequestId':ready_request})
        return {'unstartedBattleReplay':True,'readyRequestId':ready_request,'requiresManualPreparationReady':True}

    def start_replayed(self,state):
        """Reuse the prepared battle after manual Ready; never rerun PREP_FIN."""
        checkpoint(state,self.c);self.engine._phase(state,'preparation')
        pending=state.get('nativeReplayPending');battle=state.get('battle')
        if (not isinstance(pending,dict) or pending.get('version')!=1 or pending.get('round')!=state['round'] or
                pending.get('sourcePhase') not in ('battle','boss') or not battle or
                battle.get('nativeCombatVersion')!=VERSION or battle['round']!=state['round'] or
                hashlib.sha256(dump(battle).encode()).hexdigest()!=pending.get('battleSha256')):
            raise DomainError('native_battle_replay_checkpoint_changed')
        now=integer(int(self.clock()),'serverTime',1,9223372036854775807)
        if battle.get('battleAttempt',0)!=battle_attempt(state,state['round']):raise DomainError('native_battle_attempt_mismatch')
        duration=battle_timing.force_end_duration(self.c,state['modeId'],state['round'],battle)
        state['phase']=pending['sourcePhase'];battle.update(startedAtTs=now,deadlineTs=now+duration)
        del state['nativeReplayPending']
        self._replay_transition(state,'battle.replay.start',{'readyRequestId':pending['readyRequestId']})
        return {'nativeCombatVersion':VERSION,'replayedUnstartedBattle':True,
            'battleDeadlineTs':battle['deadlineTs'],'issuedEnemyCount':len(battle['units'])}

    def ready(self,state,args):
        checkpoint(state,self.c)
        if state['phase'] in ('battle','boss') and state['battle'].get('nativeCombatVersion')==VERSION:
            return {'duplicateReady':True,'nativeCombatVersion':VERSION}
        self.engine._phase(state,'preparation')
        repair_identity_counter(state)
        # Supported CN single-player templates are exactly one fragment. Reject a changed
        # template instead of silently flattening a different action namespace.
        variants=self.c.rounds(state['modeId'])[state['round']]
        for variant in variants:
            waves=self.c.level(variant['LevelId'])['Waves']
            if len(waves)!=1 or len(waves[0]['Fragments'])!=1:raise DomainError('native_multiple_fragment_battle_pending')
        self.engine.apply(state,'preparation.ready',args)
        from .mode_strength import preserve_legacy_shape
        preserve_legacy_shape(state)
        # PREP_FIN can grant/merge inventory, and the final preparation boundary
        # discards overflowing characters. Capture the resulting owned set.
        owned=[v['instId'] for field in ('charChess','trapChess') for v in state['player'][field]]
        battle=state['battle'];bosses=local_boss_keys(self.c)
        attempt=battle_attempt(state,state['round'])
        if attempt:battle['battleAttempt']=attempt
        local_boss_actions=[action for action in battle['actions'] if action['Key'] in bosses]
        battle['nativeLocalBossKeys']=list(dict.fromkeys(action['Key'] for action in local_boss_actions))
        battle['nativeLocalBossCount']=sum(action['Count'] for action in local_boss_actions)
        # Native _InsertLocalActions retains the original boss action (including
        # boss runes/predefines) itself. Issuing it again in enemyGroup would
        # duplicate the real boss. Only scheduler replacements receive IDs.
        if state['phase']=='boss' and battle['nativeLocalBossCount']!=1:raise DomainError('native_single_local_boss_required')
        # _InsertLocalActions also preserves non-boss local/random actions and
        # unmanaged spawns. R15 uses these for its native boss auxiliaries. Their
        # original ActionData has no server-issued extraMeta and must not be
        # duplicated in enemyGroup. Keep an exact per-action registration bound
        # so the client can distinguish them from arbitrary unknown entities.
        local=[action for action in battle['actions'] if action['Key'] not in bosses and
            (not action.get('ManagedByScheduler',True) or action.get('RandomSpawnGroupKey'))]
        battle['nativeLocalEnemyActions']=[{'enemyId':action['Key'],'actionIndex':action['nativeActionIndex'],
            'count':action['Count']} for action in local]
        battle['nativeLocalEnemyKeys']=list(dict.fromkeys(action['Key'] for action in local))
        battle['nativeLocalEnemyCount']=sum(action['Count'] for action in local)
        local_indexes={action['nativeActionIndex'] for action in local}
        battle['actions']=[action for action in battle['actions'] if action['Key'] not in bosses and action['nativeActionIndex'] not in local_indexes]
        battle['units']=[unit for unit in battle['units'] if unit['enemyId'] not in bosses and unit['actionIndex'] not in local_indexes]
        rows=template_rows(self.c,state)
        expected=[(row['enemyKey'],row['actionIndex']) for row in rows for _ in range(row['count'])]
        actual=[(unit['enemyId'],unit['actionIndex']) for unit in battle['units']]
        if actual!=expected:raise DomainError('native_enemy_template_mismatch')
        now=integer(int(self.clock()),'serverTime',1,9223372036854775807)
        # New Ready commands opt in, including old saves at a fresh preparation
        # boundary. Never rewrite an already-started battle or accepted report.
        battle['battleTiming']=battle_timing.configured(self.c,state['modeId'],state['round'])
        duration=battle_timing.force_end_duration(self.c,state['modeId'],state['round'],battle)
        battle.update(nativeCombatVersion=VERSION,startedAtTs=now,deadlineTs=now+duration,nativeOwnedChessIds=owned,
            nativeOwnedCharIds=[v['instId'] for v in state['player']['charChess']],
            nativeBattleKind='boss-local' if state['phase']=='boss' else 'self',initialBossHp=battle['bossHp'])
        state['nativeCombatVersion']=VERSION
        return {'nativeCombatVersion':VERSION,'battleDeadlineTs':battle['deadlineTs'],'issuedEnemyCount':len(battle['units'])}

    def _characters(self,statuses,owned):
        if not isinstance(statuses,list) or len(statuses)>4096:raise DomainError('invalid_character_status')
        chars={}
        for status in statuses:
            if not isinstance(status,dict):raise DomainError('invalid_character_status')
            ident=integer(status.get('instId'),'charInstId')
            if ident not in owned:raise DomainError('unknown_native_character')
            normalized={'instId':ident,'hp':integer(status.get('hp'),'hpRatio',0,100),'tech':integer(status.get('tech'),'spRatio',0,100)}
            if ident in chars and chars[ident]!=normalized:raise DomainError('conflicting_character_status')
            chars[ident]=normalized
        return [chars[k] for k in sorted(chars)]

    def _report(self,state,payload,previous=None,*,boss=False):
        if not isinstance(payload,dict):raise DomainError('invalid_payload')
        if payload.get('fromRejoin',False) is not False:raise DomainError('native_battle_rejoin_pending')
        tokens=payload.get('escapedTokenInstIds',[])
        if not isinstance(tokens,list) or len(tokens)>4096:raise DomainError('invalid_token_report')
        # Native spawned tokens are outside the server-issued template ledger.
        # The LOCAL Unity result owns their identities; retain a separate list
        # and count unique escapes under the same round HP-loss cap.
        tokens=sorted({integer(v,'tokenEnemyInstId') for v in tokens})
        for field in ('uidIndex','playerUidIndex'):
            if field in payload and (type(payload[field]) is not int or payload[field]!=0):raise DomainError('wrong_native_player')
        round_id=integer(payload.get('round'),'round',1)
        checkpoint(state,self.c)
        if round_id not in self.c.rounds(state['modeId']) or self.c.turns(state['modeId'])[round_id]['IsBossTurn']!=boss:
            raise DomainError('native_self_battle_round_required')
        source=previous if previous is not None else state['battle']
        attempt=validate_finish_attempt(state,payload)
        if source.get('battleAttempt',0)!=attempt:raise DomainError('native_battle_attempt_mismatch')
        elapsed=None
        if 'elapsedSeconds' in payload:
            # Unity's logical battle clock follows pause/speed changes. A wall
            # clock deadline cannot measure these seconds in a LOCAL battle.
            elapsed=integer(payload['elapsedSeconds'],'elapsedSeconds',0,86400)
        issued=set(source.get('nativeIssuedEnemyIds',[]) if previous is not None else [v['instId'] for v in source['units']])
        owned=set(source.get('nativeOwnedChessIds',[]))
        if not issued:raise DomainError('native_issued_enemy_snapshot_missing')
        escaped=payload.get('escapedInstIds',[])
        if not isinstance(escaped,list):raise DomainError('invalid_report')
        escaped=sorted({integer(v,'enemyInstId') for v in escaped})
        records=payload.get('killedInsts')
        if records is None:
            # Compatibility with the earlier local domain adapter. This shape
            # remains private JSON, while native callers use the declared records.
            ids=payload.get('killedInstIds',[])
            if not isinstance(ids,list):raise DomainError('invalid_kill_records')
            records=[{'enemyInstId':v,'attackerInstId':-1,'damageSrc':0} for v in ids]
        if not isinstance(records,list):raise DomainError('invalid_kill_records')
        kills={}
        for record in records:
            if not isinstance(record,dict):raise DomainError('invalid_kill_records')
            ident=integer(record.get('enemyInstId'),'enemyInstId');attacker=integer(record.get('attackerInstId',-1),'attackerInstId',-1)
            damage=integer(record.get('damageSrc',0),'damageSrc')
            if damage not in self.c.constants['enums']['Torappu.AutoChessBattleDamageSrcType'].values():raise DomainError('invalid_damage_source')
            if attacker!=-1 and attacker not in owned:raise DomainError('unknown_native_attacker')
            normalized={'enemyInstId':ident,'attackerInstId':attacker,'damageSrc':damage}
            if ident in kills and kills[ident]!=normalized:raise DomainError('conflicting_kill_record')
            kills[ident]=normalized
        killed=set(kills);escaped_set=set(escaped)
        if not (killed|escaped_set)<=issued or killed&escaped_set:raise DomainError('invalid_enemy_ids')
        duration=battle_timing.simulation_duration(self.c,state['modeId'],round_id,source)
        if not boss and elapsed is not None and elapsed>=duration:
            # The native timeout stops spawning/combat immediately. Outstanding
            # issued enemies are escapes, never fabricated kills/rewards.
            escaped_set|=issued-killed
            escaped=sorted(escaped_set)
        if not boss and (killed|escaped_set)!=issued:raise DomainError('incomplete_native_battle_report')
        if previous is None and (not set(source['killed'])<=killed or not set(source['escaped'])<=escaped_set):raise DomainError('conflicting_enemy_result')
        chars=self._characters(payload.get('charBattleStatusList',[]),set(source.get('nativeOwnedCharIds',owned)))
        report={'round':round_id,'fromRejoin':False,'escapedInstIds':escaped,'escapedTokenInstIds':tokens,
            'killedInsts':[kills[k] for k in sorted(kills)],'charBattleStatusList':chars}
        if elapsed is not None:report['elapsedSeconds']=elapsed
        if attempt:report['battleAttempt']=attempt
        return report

    def finish(self,state,payload):
        if not isinstance(payload,dict):raise DomainError('invalid_payload')
        round_id=integer(payload.get('round'),'round',1)
        checkpoint(state,self.c)
        if round_id not in self.c.rounds(state['modeId']) or self.c.turns(state['modeId'])[round_id]['IsBossTurn']:
            raise DomainError('native_self_battle_round_required')
        previous=state['completedReports'].get(str(round_id))
        if previous and previous.get('nativeReportDigest'):
            report=self._report(state,payload,previous)
            if hashlib.sha256(dump(report).encode()).hexdigest()!=previous['nativeReportDigest']:raise DomainError('conflicting_round_report')
            return {'duplicateRound':True,'hpCost':previous['hpCost']}
        checkpoint(state,self.c);self.engine._phase(state,'battle')
        if state['battle'].get('nativeCombatVersion')!=VERSION:raise DomainError('native_battle_restart_pending')
        report=self._report(state,payload);battle=copy.deepcopy(state['battle'])
        result=self.engine.apply(state,'battle.finish',{'round':round_id,'escapedInstIds':report['escapedInstIds'],
            'escapedTokenInstIds':report['escapedTokenInstIds'],'killedInstIds':[v['enemyInstId'] for v in report['killedInsts']]})
        from .mode_strength import preserve_legacy_shape
        preserve_legacy_shape(state)
        saved=state['completedReports'][str(round_id)]
        saved.update(nativeReport=report,nativeReportDigest=hashlib.sha256(dump(report).encode()).hexdigest(),
            nativeIssuedEnemyIds=[v['instId'] for v in battle['units']],nativeOwnedChessIds=battle['nativeOwnedChessIds'],nativeOwnedCharIds=battle['nativeOwnedCharIds'],
            startedAtTs=battle['startedAtTs'],deadlineTs=battle['deadlineTs'],completedAtTs=int(self.clock()))
        if 'battleTiming' in battle:saved['battleTiming']=copy.deepcopy(battle['battleTiming'])
        if 'battleAttempt' in battle:saved['battleAttempt']=battle['battleAttempt']
        if 'elapsedSeconds' in report:
            saved.update(elapsedSeconds=report['elapsedSeconds'],
                timedOut=report['elapsedSeconds']>=battle_timing.simulation_duration(self.c,state['modeId'],round_id,battle))
        return {**result,'nativeCombatVersion':VERSION,'completedRound':round_id}

    def boss_finish(self,state,payload):
        """Accept the real LOCAL Unity result after its mock boss HP callback.

        LOCAL does not use the multiplayer step clock or ActionUp. This private
        report is deliberately not mislabeled as official SelfBattleFinishUp.
        """
        if not isinstance(payload,dict):raise DomainError('invalid_payload')
        round_id=integer(payload.get('round'),'round',1)
        checkpoint(state,self.c)
        if round_id not in self.c.turns(state['modeId']) or not self.c.turns(state['modeId'])[round_id]['IsBossTurn']:
            raise DomainError('native_boss_battle_round_required')
        previous=state['completedReports'].get(str(round_id))
        source=previous if previous is not None else state.get('battle')
        if not source:raise DomainError('native_boss_snapshot_missing')
        result=payload.get('result')
        if result not in ('victory','defeat','interrupt'):raise DomainError('invalid_native_game_result')
        # LOCAL reports the real native enemy HP after battle runes. The UI
        # BloodPoint configuration is a different scale and must not cap it.
        hp=integer(payload.get('bossHp'),'bossHp')
        # Native boss death can finish while issued side enemies remain alive
        # or unborn. Validate actual outcomes without inventing a full partition.
        side=self._report(state,payload,previous,boss=True)
        report={'round':round_id,'result':result,'bossHp':hp,'charBattleStatusList':side['charBattleStatusList'],
            'killedInsts':side['killedInsts'],'escapedInstIds':side['escapedInstIds'],'escapedTokenInstIds':side['escapedTokenInstIds']}
        if 'elapsedSeconds' in side:report['elapsedSeconds']=side['elapsedSeconds']
        if 'battleAttempt' in side:report['battleAttempt']=side['battleAttempt']
        if 'playerHp' in payload:report['playerHp']=integer(payload['playerHp'],'playerHp',0,2147483647)
        if 'bossBroadcastMask' in payload:
            report['bossBroadcastMask']=integer(payload['bossBroadcastMask'],'bossBroadcastMask',0,7)
        digest=hashlib.sha256(dump(report).encode()).hexdigest()
        if previous is not None:
            if previous.get('nativeBossReportDigest')!=digest:raise DomainError('conflicting_round_report')
            return {'duplicateRound':True,'hpCost':previous['hpCost']}
        checkpoint(state,self.c);self.engine._phase(state,'boss')
        if state['battle'].get('nativeCombatVersion')!=VERSION:raise DomainError('native_battle_restart_pending')
        if round_id!=state['round']:raise DomainError('stale_round')
        if result=='victory' and hp>1:raise DomainError('native_boss_still_alive')
        battle=copy.deepcopy(state['battle']);victory=result=='victory'
        # MockBossBattleManager suicides at <= 1 HP. Preserve the observed value
        # in the report and normalize the domain terminal boss HP to zero.
        overtime=battle_timing.boss_overtime_cost(self.c,state['modeId'],round_id,battle,report.get('elapsedSeconds',0))
        initial_player_hp=state['player']['hp']
        if 'playerHp' in report:integer(report['playerHp'],'playerHp',0,max(0,initial_player_hp-overtime))
        health={'bossPlayerHp':report['playerHp']} if 'playerHp' in report else {}
        state['battle']['bossHp']=0 if victory else hp
        outcome=self.engine.apply(state,'battle.finish',{'round':round_id,
            'killedInstIds':[row['enemyInstId'] for row in side['killedInsts']],
            'escapedInstIds':side['escapedInstIds'],'escapedTokenInstIds':side['escapedTokenInstIds'],
            'bossTimeoutHpCost':overtime,'defeat':not victory,**health})
        from .mode_strength import preserve_legacy_shape
        preserve_legacy_shape(state)
        state['completedReports'][str(round_id)].update(nativeBossReport=report,nativeBossReportDigest=digest,
            bossId=state['bossId'],bossDefeated=victory,initialBossHp=battle['initialBossHp'],
            nativeLocalBossKeys=battle['nativeLocalBossKeys'],nativeLocalBossCount=battle['nativeLocalBossCount'],
            nativeLocalEnemyActions=battle['nativeLocalEnemyActions'],nativeLocalEnemyKeys=battle['nativeLocalEnemyKeys'],nativeLocalEnemyCount=battle['nativeLocalEnemyCount'],
            nativeIssuedEnemyIds=[v['instId'] for v in battle['units']],nativeOwnedChessIds=battle['nativeOwnedChessIds'],nativeOwnedCharIds=battle['nativeOwnedCharIds'],
            startedAtTs=battle['startedAtTs'],deadlineTs=battle['deadlineTs'],completedAtTs=int(self.clock()))
        if 'battleTiming' in battle:state['completedReports'][str(round_id)]['battleTiming']=copy.deepcopy(battle['battleTiming'])
        if 'battleAttempt' in battle:state['completedReports'][str(round_id)]['battleAttempt']=battle['battleAttempt']
        if 'bossBroadcastMask' in report:
            state['completedReports'][str(round_id)]['bossBroadcastMask']=report['bossBroadcastMask']
        if 'elapsedSeconds' in report:
            state['completedReports'][str(round_id)].update(elapsedSeconds=report['elapsedSeconds'],bossTimeoutHpCost=overtime)
        if 'playerHp' in report:
            state['completedReports'][str(round_id)].update(nativeInitialPlayerHp=initial_player_hp,nativeRemainingPlayerHp=report['playerHp'])
        return {**outcome,'nativeCombatVersion':VERSION,'completedRound':round_id,'bossDefeated':victory}
