"""Declared official DTO names/IDs, projected over a local JSON debug bridge.

This is NOT an official binary transport codec. Final action paramList decoding
and HTTP user-data envelopes are held separate until their client contract is known.
"""
import copy
from .engine import DomainError,integer
from .native_combat import BOSS_GROUP,round_boss_id,template_rows
from .hand_layout import HAND_COUNT,reconcile
from .preset_snapshot import wire_squad

NS='Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+'
UP={
 'AutoChessBattleSceneLoadingReadyUp':'loading.ready',
 'AutoChessBattleSceneShopRefreshUp':'shop.refresh',
 'AutoChessBattleSceneShopBuyChessUp':'shop.buy',
 'AutoChessBattleSceneShopFrozeUp':'shop.freeze',
 'AutoChessBattleSceneShopUpgradeUp':'shop.upgrade',
 'AutoChessBattleSceneUseEquipUp':'board.equip',
 'AutoChessBattleSceneUseMagicUp':'item.use',
 'AutoChessBattleSceneDestroyChessUp':'board.destroy',
 'AutoChessBattleSceneChangePositionUp':'board.move',
 'AutoChessBattleSceneSelfBattleAddBondUp':'bond.add',
 'AutoChessBattleSceneSelfBattleEnemyEscapeUp':'battle.escape',
 'AutoChessBattleSceneSelfBattleInfoUp':'battle.info',
 'AutoChessBattleSceneSelfBattleFinishUp':'battle.finish',
 'AutoChessBattleSceneSpPreparationPickUp':'choice.pick',
 'AutoChessBattleSceneSelfChoicePickUp':'choice.pick',
 'AutoChessBattleScenePauseUp':'pause','AutoChessBattleSceneResumeUp':'resume',
 'AutoChessBattleSceneQuitUp':'quit'}
PHASE={'band':'LOADING','loading':'LOADING','special':'SP_PREPARE','preparation':'PREPARE',
       'battle':'BATTLE','boss':'BOSS_BATTLE','paused':'PAUSE','result':'SETTLE'}

def encode_position(position):
    """CN GridPosition.Encode: (row << 16) + col. Domain uses x=col/y=row.

    Non-positive values are the native hand-list namespace, not coordinates.
    Evidence: pinned CN GridPosition tokens 0x6006d8e/0x6006d8f and
    AutoChessBattleUtil.GetTileByPosition, recorded in the server contract.
    """
    if not isinstance(position,dict):raise DomainError('invalid_positions')
    x=integer(position.get('x'),'x',0,65535);y=integer(position.get('y'),'y',0,32767)
    value=(y<<16)+x
    if value<=0:raise DomainError('native_hand_position_is_not_battlefield')
    return value

def decode_position(value):
    integer(value,'position',1,2147483647)
    return {'x':value&65535,'y':value>>16}

class Protocol:
    def __init__(self,catalog):self.c=catalog
    def special_effects(self,rows):
        """CN AutoChessBattleSpecialEffect uses List<Int32>, despite its name.

        Old private decisions saved a scalar. Normalize only the DTO copy so
        resuming or replaying an accepted choice never grants its reward again.
        Item effects and new decisions already store the declared list shape.
        """
        result=copy.deepcopy(rows)
        for row in result:
            targets=row['affectedUidIndex']
            if type(targets) is int:targets=[targets]
            if not isinstance(targets,list):raise DomainError('invalid_special_effect_targets')
            row['affectedUidIndex']=[integer(target,'affectedUidIndex',-2147483648,2147483647) for target in targets]
        return result
    def replay_reply(self,reply):
        """Repair only native special-effect DTOs in an immutable cached ACK.

        Preserve its original state, revision, command result and ACK sequence;
        never re-execute the domain event or substitute a newer run snapshot.
        """
        result=copy.deepcopy(reply)
        for message in result.get('messages',[]):
            name=message['type'].removeprefix(NS)
            payload=message['payload']
            if name=='AutoChessBattleSceneChangeStatusDn':scene=payload['sceneStatus']
            elif name=='AutoChessBattleSceneAllStateSyncDn':scene=payload['allStateSyncData']['sceneDetail']
            else:continue
            scene['specialEffects']=self.special_effects(scene['specialEffects'])
        return result
    def prebattle(self,s,sync):
        """One persisted run feeds both native prebattle views and combat DTOs.

        StageInfoGroupViewModel.Input is an existing CN/TC declaration. A null
        trainingBondList selects its normal full-bond path; no training config
        is copied into the private independent simulation.
        """
        banned=copy.deepcopy(sync['bannedBonds'])
        enemy_types=copy.deepcopy(s.get('enemyTypes',[]))
        banned_chess=set(s.get('bannedChessIds',[]))
        # Native StageInfo groups test the elite key before showing its normal
        # portrait. Preserve every equivalent display key for banned pool cards.
        banned_display=sorted(banned_chess|{key for key,normal in self.c.season['ChessNormalIdLookupDict'].items() if normal in banned_chess})
        return {'version':1,'activityId':s['activityId'],'runId':s['runId'],'revision':s['revision'],
            'modeId':s['modeId'],'stageId':s['stageId'],'stageSeed':s['stageSeed'],'bandId':s['bandId'],
            'setupVersion':s.get('setupVersion',0),'modeStrengthVersion':s.get('modeStrengthVersion',0),
            'poolVersion':s.get('poolVersion',0),'bannedBondIds':copy.deepcopy(s.get('bannedBondIds',[])),
            'chessPool':copy.deepcopy(s['chessPool']),'diyChessPool':copy.deepcopy(s['diyChessPool']),
            'nativeSquad':copy.deepcopy(s.get('nativeSquad',[])),
            'inactiveBondIds':copy.deepcopy(s.get('inactiveBondIds',[])),
            'bannedBonds':banned,'enemyTypes':enemy_types,'bannedChessIds':copy.deepcopy(s.get('bannedChessIds',[])),
            'bannedChessDisplayIds':banned_display,
            'stageInfoInput':{'actId':s['activityId'],'bossId':sync['bossId'],'modeId':s['modeId'],
                'enemyTypes':copy.deepcopy(enemy_types),'bannedBondList':copy.deepcopy(banned),
                'trainingBondList':None}}
    def message(self,name,payload):
        full=NS+name
        return {'type':full,'protocolId':self.c.constants['protocolIds'][full],'payload':payload,
                'compatibility':'declaration projection; wire and native-object acceptance pending'}
    def settlement(self,s):
        """Declared native result-page data for this private run.

        The native LOCAL return path supplies its own SDK identity to the sole
        team entry on a copied DTO. The server never reads or modifies an
        official account, and reward/medal accounting remains local policy.
        """
        if s['phase']!='result' or not s.get('result'):return None
        p=s['player'];result=s['result'];deployed={row['instId'] for row in p['positions']}
        equipment={obj['instId']:self.c.data['identifiers'][str(obj['chessIdentifier'])] for obj in p['equipChess']}
        squad=[{'chessId':self.c.data['identifiers'][str(obj['chessIdentifier'])],
            'equipId':[equipment[ident] for ident in obj['equipInstIds'] if ident in equipment]}
            for obj in p['charChess'] if obj['instId'] in deployed]
        bonds={value['Identifier']:key for key,value in self.c.season['BondInfoDict'].items()}
        records=list(s['completedReports'].values())
        starts=[value['startedAtTs'] for value in records if 'startedAtTs' in value]
        ends=[value['completedAtTs'] for value in records if 'completedAtTs' in value]
        card={'title':'','nickname':'本地博士','nicknameNumber':'0000','level':120,'trophyNum':0,
            'secretary':'char_002_amiya','secretarySkinId':'char_002_amiya#1','secretarySkinSp':False,
            'avatarType':'DEFAULT','avatarId':'0','nameCardSkinId':'','nameCardSkinTmpl':0}
        return {'modeId':s['modeId'],'bandId':s['bandId'],
            'startTs':s.get('nativeStartedAtTs',min(starts) if starts else 0),
            'endTs':s.get('nativeEndedAtTs',max(ends) if ends else 0),
            'gameFinished':not result.get('abandoned',False),'isViolation':False,
            'bossRecord':[{'bossId':value['bossId'],'isHiddenBoss':value['bossId']==s.get('hiddenBossId')}
                for value in records if value.get('bossDefeated')],
            'onStageChars':squad,'onStageBond':[{'bondId':bonds[value['bondIdentifier']],'layer':value['stackCnt']}
                for value in p['bonds'] if value['isActive']],
            'teamInfo':[{'uid':'local-'+s['runId'],'channel':0,'passRound':result['roundsPassed'],
                'gameCode':self.c.enum('Torappu.UI.AutoChess.AutoChessSettleGamePlayerStatus',
                    'PASS' if result['success'] else 'INTERRUPT' if result.get('abandoned') else 'DEAD'),
                'card':card,'uidIndex':0}],
            'recordInfos':{'trophyRecord':0,'normalMilestone':0,'dailyMilestone':0,'dailyProcessAdd':0}}
    def project(self,s):
        p=copy.deepcopy(s['player'])
        # This metadata is private persistence, not an extra official DTO field.
        # Project original positive coordinates and native non-positive hand
        # slots into the one positions list accepted by ChessMapInfo.
        hand_state={**s,'player':p,'nativeTokenPositions':copy.deepcopy(s.get('nativeTokenPositions',[]))}
        hand=reconcile(self.c,hand_state)
        # Validate saved TOKEN owner positions against this reconciled copy,
        # before its ordinary coordinates become encoded native integers.
        token_state={**hand_state,'player':copy.deepcopy(p)}
        p.pop('handPositions',None)
        p['store']['isSpecialGoods']=self.c.enum('Torappu.AutoChessBattleShopGoodsType','NORMAL')
        # Native special-recruit shop uses the same card/slot DTO as ordinary
        # shops. Keep base goods/frozen flags only in authoritative domain state.
        offers=s.get('traitRewardOffers') or []
        if offers:
            options=offers[0]['options']
            p['store']['isSpecialGoods']=self.c.enum('Torappu.AutoChessBattleShopGoodsType','COMB')
            p['store']['specialRefresh']=False
            p['store']['goods']=[{'chessIdentifier':self.c.chess[key]['Identifier'],'slotId':index,'price':0,'isFrozen':False} for index,key in enumerate(options)]
        pending=[]
        for unit in p['positions']:
            unit['position']=encode_position(unit['position'])
            obj=next(v for field in ('charChess','trapChess') for v in p[field] if v['instId']==unit['instId'])
            key=self.c.data['identifiers'][str(obj['chessIdentifier'])]
            unit['chessType']=0 if key in self.c.season['CharChessDataDict'] else self.c.chess[key]['ItemType']
        p['positions'].extend(copy.deepcopy(hand))
        from .native_tokens import project_tokens
        p['positions'].extend(project_tokens(self.c,token_state,p['positions']))
        if p['selfChoice'] and s['phase']!='special':
            p['selfChoice']['options']=[{'unitId':key,'slotId':i,'price':0} for i,key in enumerate(p['selfChoice']['options'])]
        elif s['phase']=='special':p['selfChoice']=None
        runtime_state={'band':'LOADING_NOT_READY','loading':'LOADING_NOT_READY','special':'PREPARATION_NOT_READY',
            'preparation':'PREPARATION_NOT_READY','battle':'BATTLE_NOT_COMPLETE','boss':'BATTLE_NOT_COMPLETE','result':'BATTLE_COMPLETE'}
        phase=s.get('previousPhase',s['phase']) if s['phase']=='paused' else s['phase']
        runtime={'uidIndex':0,'gameFinish':s['phase']=='result','connectState':self.c.enum('Torappu.AutoChessPlayerConnectStateType','ONLINE'),
          'shopLv':p['store']['level'],'hp':p['hp'],'state':self.c.enum('Torappu.AutoChessPlayerGameStateType',runtime_state[phase])}
        deadline=s['battle'].get('deadlineTs',0) if s.get('battle') else 0
        if s.get('nativeSessionVersion') and s['phase'] not in ('battle','boss'):deadline=-1
        scene={'obUidIndex':0,'state':self.c.enum('Torappu.AutoChessGameStateType',PHASE[s['phase']]),
          'round':s['round'],'forceEndTs':deadline,'runtimePlayers':[runtime],'roundEnemies':template_rows(self.c,s),
          'specialPreparation':None,'preparationStatus':p,'bossRoundDetail':None,'selfBattleInfo':None,
          'helpBattleInfo':None,'bossBattleInfo':None,'specialEffects':self.special_effects(s['specialEffects']),
          'lastBattleResult':None,'settleBattleResult':None}
        if s['phase']=='special':
            choice=s['player']['selfChoice']
            sp_id=choice['id'] if choice['id'] in self.c.season['EffectChoiceInfoDict'] else 'buff_select_1'
            slots=[{'unitId':key,'slotId':index,'price':0} for index,key in enumerate(choice['options'])]
            draft=s.get('specialDraft')
            if draft is not None:
                definition=self.c.season['EffectChoiceInfoDict'][sp_id]
                if (draft['round']!=s['round'] or draft['choiceType']!=definition['ChoiceType'] or
                        draft['effectType']!=definition['EffectType'] or draft['slots']!=slots):
                    raise DomainError('invalid_special_draft_projection')
                slots=copy.deepcopy(draft['slots'])
            scene['specialPreparation']={'options':slots, 'playersQueue':[{'uidIndex':0,'choice':-1}],
                'spId':sp_id,'finTs':-1,'currQueueIndex':0}
        is_boss_round=self.c.turns(s['modeId']).get(s['round'],{}).get('IsBossTurn',False)
        if is_boss_round and s['phase'] in ('preparation','boss'):
            preview_boss=round_boss_id(self.c,s)
            scene['bossRoundDetail']={'isHiddenBoss':preview_boss==s.get('hiddenBossId'),'bossId':preview_boss,
                # CN DataConverter.Refresh copies this wire field directly to
                # BossRoundInfo.playerTotalHp, which the boss-round HUD renders
                # as the player/team HP. Boss health has its own battle field.
                'hp':p['hp'],
                'groupInfos':[{'group':BOSS_GROUP,'players':[0]}]}
        if s['battle'] and s['phase'] in ('battle','boss'):
            by_action={}
            for u in s['battle']['units']:by_action.setdefault((u['enemyId'],u['actionIndex']),[]).append(u['instId'])
            groups=[{'enemyId':key[0],'actionIndex':key[1],'instIdList':values} for key,values in by_action.items()]
            if s['phase']=='boss':
                scene['bossBattleInfo']={'group':BOSS_GROUP,'bossHp':s['battle']['bossHp'],
                    'enemyCount':len(s['battle']['units'])+s['battle'].get('nativeLocalBossCount',0),
                    'enemyGroup':[{'playerGroup':BOSS_GROUP,**row} for row in groups],
                    'players':[{field:copy.deepcopy(p[field]) for field in ('uidIndex','charChess','equipChess','trapChess','positions','bonds')}]}
            else:scene['selfBattleInfo']={'enemyInfos':groups}
        if s['completedReports']:
            report=s['completedReports'][str(max(map(int,s['completedReports'])))];scene['lastBattleResult']=[{'uidIndex':0,'hpCost':report['hpCost']}]
        if s['result']:
            boss_records=[{'bossId':report['bossId'],'isDead':report['bossDefeated'],'isHiddenBoss':report['bossId']==s.get('hiddenBossId')}
                for report in s['completedReports'].values() if 'bossDefeated' in report]
            scene['settleBattleResult']={'settleState':self.c.enum('Torappu.AutoChessSettleStateType','SUCCESS' if s['result']['success'] else 'FAILED'),'bossRecords':boss_records}
        if s['phase']=='boss':pending.append('native LOCAL boss combat acceptance; multiplayer fixed-step transport is outside this scope')
        squad=wire_squad(self.c,s)
        card={'nickName':'本地博士','nickNumber':'0000','avatarType':'DEFAULT','avatarId':'0','secretary':'char_002_amiya',
            'secretarySkinId':'char_002_amiya#1','secretarySkinSp':False,'level':120,'nameCardSkin':'','nameCardSkinTmpl':0}
        sync={'modeId':s['modeId'],'stageId':s['stageId'],'bossId':s['bossId'],'hiddenBossId':s['hiddenBossId'],
            'stageSeed':s['stageSeed'] if s['stageSeed']<0x80000000 else s['stageSeed']-0x100000000,'uidIndex':0,
            'bannedBonds':sorted(set(s.get('bannedBondIds',[]))|set(s.get('inactiveBondIds',self.c.mode(s['modeId'])['InactiveBondIdList'] or []))),
            'staticPlayers':[{'uid':'local-'+s['runId'],'index':0,'squad':squad,'bandId':s['bandId'],'cardInfo':card}],
            'sceneDetail':scene}
        return {'nativeReady':False,'pending':['native dispatch/transport and initialization sequence',*pending],
          'allStateSync':self.message('AutoChessBattleSceneAllStateSyncDn',{'allStateSyncData':sync}),
          'preparation':self.message('AutoChessScenePreparationStatusResp',{'code':0,'preparationStatus':p}),
          'sceneStatus':self.message('AutoChessBattleSceneChangeStatusDn',{'sceneStatus':scene})}
    def batch(self,before,after,op=None,ok=True):
        """Declaration DTO batch for a private native-object bridge, not wire bytes."""
        # Native AddBondCount already applied the delta. A preparation/full
        # snapshot here would reset an active Unity self battle and overwrite
        # later queued local increments. The envelope carries confirmed state.
        # CN declares no AddBondDn; do not invent an official downlink DTO.
        if op=='bond.add':return []
        # A rejected combat report must never send preparation data into a
        # still-running Unity battle. The caller receives its explicit error
        # and retries the durable report without resetting native models.
        if not ok and (before is not None and before['phase'] in ('battle','boss','result') or
                op in ('boss.finish','native.actions') or isinstance(op,str) and op.startswith('battle.') or
                op=='preparation.ready' and before is not None and before['phase'] in ('battle','boss')):return []
        projected=self.project(after)
        if before is None:return [projected['allStateSync']]
        if ok and op=='choice.pick' and before['phase']=='special' and after['phase']=='preparation':
            choice=after['lastSpecialChoice']
            return [self.message('AutoChessBattleSceneSpPreparationPickDn',{'updateInfo':{
                'playerChoice':{'uidIndex':0,'choice':choice['slotId']},'finTs':-1,'currQueueIndex':1}}),projected['sceneStatus']]
        if before['phase']!=after['phase'] or before['round']!=after['round']:
            return [projected['sceneStatus']]
        if ok and (op in ('battle.kill','battle.escape','battle.info','native.actions') or before==after):return []
        p=projected['preparation']['payload']['preparationStatus']
        if not ok:return [self.message('AutoChessScenePreparationStatusResp',{'code':1,'preparationStatus':p})]
        messages=[]
        if op=='board.move':messages.append(self.message('AutoChessBattleSceneChangePositionDn',{'boardStatus':{'uidIndex':0,'positions':p['positions'],'updatedBonds':p['bonds']}}))
        if op=='shop.freeze':messages.append(self.message('AutoChessBattleSceneShopFrozeDn',{'uidIdx':0,'isFrozen':all(v['isFrozen'] for v in p['store']['goods'])}))
        messages.append(projected['preparation'])
        return messages
    def decode(self,type_name,payload):
        if type(type_name)is int:
            full=next((k for k,v in self.c.constants['protocolIds'].items() if v==type_name and k.startswith(NS)),None)
            if not full:raise DomainError('unknown_protocol_id')
            type_name=full
        if not isinstance(type_name,str):raise DomainError('invalid_protocol_type')
        name=type_name.removeprefix(NS);data=copy.deepcopy(payload)
        if not isinstance(data,dict):raise DomainError('invalid_payload')
        if name=='AutoChessBattleSceneActionUp':raise DomainError('final_action_param_contract_pending')
        if name=='AutoChessBattleSceneSelfBattleKillEnemyUp':return 'battle.kill',data.get('record',{})
        if name=='AutoChessBattleScenePreparationReadyUp':
            if integer(data.get('readyType'),'readyType',0,1)!=self.c.enum('Torappu.UI.AutoChess.Server.AutoChessBattlePreparationReadyType','READY'):raise DomainError('cancel_ready_not_applicable_in_solo')
            return 'preparation.ready',{}
        if name not in UP:raise DomainError('protocol_adapter_pending',name)
        if name in ('AutoChessBattleSceneChangePositionUp','AutoChessBattleSceneUseMagicUp'):
            units=data.get('unitPositionInfo')
            if not isinstance(units,list):raise DomainError('invalid_positions')
            deployed=[];seen=set();hand=set();hand_positions=[];tokens=[]
            for unit in units:
                if not isinstance(unit,dict):raise DomainError('invalid_positions')
                ident=integer(unit.get('instId'),'instId');position=integer(unit.get('position'),'position',-2147483648)
                kind=integer(unit.get('chessType',0),'chessType',0,3)
                if kind==3:
                    tokens.append({'instId':ident,'position':position,'chessType':3,'direction':integer(unit.get('direction'),'direction',0,3)})
                    continue
                if ident in seen:raise DomainError('position_collision')
                seen.add(ident)
                if position<=0:
                    integer(position,'handPosition',1-HAND_COUNT,0)
                    if position in hand:raise DomainError('position_collision')
                    hand.add(position);hand_positions.append({'instId':ident,'position':position,'chessType':integer(unit.get('chessType',0),'chessType',0,3),
                        'direction':integer(unit.get('direction'),'direction',0,3)});continue
                deployed.append({'instId':ident,'position':decode_position(position),'chessType':integer(unit.get('chessType',0),'chessType',0,3),'direction':integer(unit.get('direction'),'direction',0,3)})
            declared=integer(data.get('deployCount'),'deployCount')
            # Native drag restores its old dummy counter before changing the
            # ChessMap and immediately submitting the request. The common
            # domain boundary compares against exact pre/post layout volumes;
            # this stateless decoder can only enforce the integer declaration.
            args={'unitPositionInfo':deployed,'handPositions':hand_positions,'nativeTokenPositions':tokens,'deployCount':declared}
            if name=='AutoChessBattleSceneUseMagicUp':return 'item.use',{'instId':integer(data.get('instId'),'instId',1),**args}
            return 'board.move',args
        if name=='AutoChessBattleSceneSelfBattleFinishUp':
            records=data.pop('killedInsts',None)
            if records is not None:
                if not isinstance(records,list) or any(not isinstance(v,dict) or type(v.get('enemyInstId'))is not int for v in records):raise DomainError('invalid_kill_records')
                data['killedInstIds']=[v['enemyInstId'] for v in records]
                data['killedInsts']=records
        return UP[name],data
