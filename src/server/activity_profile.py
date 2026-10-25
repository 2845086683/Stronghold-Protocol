"""Independent activity ownership/progression, never official account data.

Native DTO projections use CN declarations. HTTP envelope/native acceptance are
tracked separately; no network client, credential forwarding or fallback exists.
"""
import copy
import json
from .engine import DomainError
from .storage import dump
from .session_presence import SessionPresence

ACTIVITY='act2autochess'
PREFIX='/activity/autochessSeason/'
POLICY_VERSION=2
ACTIVITY_POLICY={'operators':'all CN operators at maximum cultivation',
    'modes':'all unlocked','bands':'all unlocked','training':'completed',
    'rewards':'ignored','medals':'ignored','officialFallback':False}


class ActivityProfile:
    def __init__(self,store,clock=None):
        self.store=store;self.c=store.engine.c
        self.presence=SessionPresence(store,ACTIVITY,clock)
        with store.lock,store.db:
            store.db.execute('CREATE TABLE IF NOT EXISTS activity_profiles(id TEXT PRIMARY KEY,catalog_hash TEXT NOT NULL,data TEXT NOT NULL)')
        self._roster=self._make_roster()
    def _make_roster(self):
        result={}
        for index,(char_id,data) in enumerate(self.c.data['operators'].items()):
            phases=data['Phases'];phase=len(phases)-1
            available={key:{'hide':False,'locked':False,'level':self.c.equipment_max_level(key)}
                       for key in self.c.equipment_ids(char_id)}
            skills=[{'skillId':s['SkillId'],'unlock':True,'specializeLevel':3}
                    for s in data.get('Skills') or []]
            inst=200000+index
            result[str(inst)]={'instId':inst,'charId':char_id,'level':phases[phase]['MaxLevel'],
                'exp':0,'evolvePhase':phase,'potentialRank':5,'favorPoint':25570,'mainSkillLvl':7,
                'gainTime':0,'starMark':0,'skills':skills,'defaultSkillIndex':max(0,len(skills)-1),
                'skin':char_id+'#1','currentEquip':next((key for key in available if not self.c.equipment_is_original(key)),next(iter(available),None)),'equip':available,
                'voiceLan':0,'currentTmpl':None,'tmpl':{}}
        return {'troopCapacity':len(result),'curSquadCount':0,'curCharInstCount':len(result),
                'chars':result,'squads':{},'addon':{},'charMission':{},'spOperator':{}}
    def defaults(self):
        squad={}
        for chess_id,entry in self.c.season['CharShopChessDatas'].items():
            if entry.get('IsHidden'):continue
            char_id=entry.get('CharId') or entry.get('BackupCharId')
            if char_id not in self.c.data['operators']:continue
            squad[chess_id]={'chessId':chess_id,'charId':char_id,'tmplId':entry.get('TmplId'),
                'diyBackupChessId':None,'cultivateEffect':self.c.cultivate_effect(char_id),'currentEquip':entry.get('DefaultUniEquipId'),
                'skin':char_id+'#1','type':0,'potentialRank':5,'skillIndex':entry.get('DefaultSkillIndex',0),'assistInfo':None}
        return {'schemaVersion':1,'policyVersion':POLICY_VERSION,'activityId':ACTIVITY,'activeRun':None,
            'playerActivity':{'mode':{k:{'unlock':True,'completeCnt':0} for k in self.c.season['ModeDataDict']},
                'band':{k:{'state':1,'progress':{'value':1,'target':1},'passCnt':0} for k in self.c.season['BandDataListDict']},
                'dailyMission':{'process':0,'state':0},'protectTs':0,'trophyNum':0,
                'milestone':{'point':0,'got':[]},'match':{'bannedUntilTs':0},'scene':{'lastMate':[]},
                'globalBan':False,'chessSquad':squad},
            'autoChessPerm':{'band':{k:1 for k in self.c.season['BandDataListDict']},
                'trainingModeFin':{k:1 for k,v in self.c.season['ModeDataDict'].items() if v['ModeType']==0}}}
    def _apply_unlock_policy(self,profile):
        # This applies on every read, including saves created by older policy
        # versions. Character/DIY/skill/module selections are deliberately kept.
        profile['policyVersion']=POLICY_VERSION
        activity=profile['playerActivity']
        modes=activity.setdefault('mode',{})
        for key in self.c.season['ModeDataDict']:
            mode=modes.setdefault(key,{'completeCnt':0})
            mode['unlock']=True;mode.setdefault('completeCnt',0);mode.pop('locked',None)
        bands=activity.setdefault('band',{})
        for key in self.c.season['BandDataListDict']:
            band=bands.setdefault(key,{'passCnt':0})
            band['state']=1;band.setdefault('passCnt',0);band.pop('locked',None)
            progress=band.setdefault('progress',{})
            target=progress.get('target',1)
            if type(target)is not int or target<1:target=1
            value=progress.get('value',0)
            if type(value)is not int:value=0
            progress.update(value=max(value,target),target=target)
        permission=profile.setdefault('autoChessPerm',{})
        permission.setdefault('band',{}).update({key:1 for key in self.c.season['BandDataListDict']})
        permission.setdefault('trainingModeFin',{}).update({key:1 for key,value in self.c.season['ModeDataDict'].items() if value['ModeType']==0})
        for entry in activity.get('chessSquad',{}).values():
            if entry.get('cultivateEffect') is None:entry['cultivateEffect']=self.c.cultivate_effect(entry.get('charId'))
        return profile
    def get(self):
        with self.store.lock:
            row=self.store.db.execute('SELECT catalog_hash,data FROM activity_profiles WHERE id=?',(ACTIVITY,)).fetchone()
            if row:
                if row[0]!=self.c.hash:raise DomainError('activity_profile_catalog_mismatch')
                profile=self._apply_unlock_policy(json.loads(row[1]));self.presence.synchronize(profile);normalized=dump(profile)
                if normalized!=row[1]:
                    with self.store.db:self.store.db.execute('UPDATE activity_profiles SET data=? WHERE id=?',(normalized,ACTIVITY))
                return profile
            profile=self.defaults()
            with self.store.db:self.store.db.execute('INSERT INTO activity_profiles VALUES(?,?,?)',(ACTIVITY,self.c.hash,dump(profile)))
            return profile
    def player_model(self):
        profile=self.get()
        return {'troop':copy.deepcopy(self._roster),'autoChessPerm':profile['autoChessPerm'],
            'emoticon':{'unlockTheme':list(self.c.common['EnabledEmoticonThemeIdList'])},
            'activity':{'actAutoChessActivityList':{ACTIVITY:profile['playerActivity']}},
            'status':{'nickName':'本地博士','nickNumber':'0000','level':120},
            'inventory':{},'skin':{'characterSkins':{},'skinTs':{},'skinSp':{}}}
    def bootstrap(self):
        p=self.get()
        return {'activityId':ACTIVITY,'catalogHash':self.c.hash,'profile':p,
            'activityPolicy':copy.deepcopy(ACTIVITY_POLICY),
            'equipmentDefinitionsSha256':self.c.equipment_hash,
            'operatorsUnlocked':len(self._roster['chars']),'playerDataSource':'private CN catalog; no official account',
            'nativePlayerActivity':p['playerActivity'],'nativePlayerModel':self.player_model(),
            'nativeAutoChessPerm':p['autoChessPerm'],'officialFallback':False,'nativeAccepted':False}
    def invoke(self,path,body):
        with self.store.lock:
            return self._invoke(path,body)
    def _invoke(self,path,body):
        if not path.startswith(PREFIX):raise DomainError('outside_private_activity')
        ids=[body[k] for k in ('actId','activityId') if k in body]
        if not ids or any(v!=ACTIVITY for v in ids):raise DomainError('wrong_private_activity')
        operation=path[len(PREFIX):]
        profile=self.get()
        if operation=='syncInfo':return {'changed':[],'battleInfo':self.presence.battle_info(profile),'privateProfile':profile}
        if operation=='quitSingleGame':
            if set(body)!={'activityId','sceneId'}:raise DomainError('invalid_quit_single_game_request')
            return self.presence.quit(profile,body['sceneId'])
        if operation=='getFriendCharAssistList':return {'assistList':[],'source':'private roster; all operators unlocked'}
        if operation in ('setChessPoolDiyChar','setChessPoolDeploy','removeChessPoolChar'):
            self.apply_configuration(profile,operation,body)
            with self.store.lock,self.store.db:
                self.store.db.execute('UPDATE activity_profiles SET data=? WHERE id=?',(dump(profile),ACTIVITY))
            return {'changed':['activity.autoChessSeason.'+ACTIVITY],'privateProfile':profile}
        # Other native service shapes remain explicit pending adapters. Activity
        # requests terminate here, including unknown endpoints; none fall back.
        raise DomainError('private_activity_adapter_pending',operation)

    def apply_configuration(self,profile,operation,body):
        """Validate a private roster in memory; persistence belongs to its owner."""
        if operation not in ('setChessPoolDiyChar','setChessPoolDeploy','removeChessPoolChar'):raise DomainError('private_configuration_operation_required')
        if operation=='removeChessPoolChar':
            chess_id=body.get('chessId')
            if not isinstance(chess_id,str) or chess_id not in self.c.season['CharShopChessDatas']:raise DomainError('invalid_chess_id')
            profile['playerActivity']['chessSquad'].pop(chess_id,None)
        else:
            # CN declarations contain maps, not the former flat debug body:
            # AutoChessSetChessPoolDeployRequest.chessPool<string,Deploy>,
            # AutoChessSetChessPoolDiyCharRequest.diyChessPool<string,DiyCharDeploy>.
            is_diy=operation=='setChessPoolDiyChar'
            pool=body.get('diyChessPool' if is_diy else 'chessPool')
            if not isinstance(pool,dict):raise DomainError('invalid_chess_pool')
            validated={}
            for chess_id,deploy in pool.items():
                if chess_id not in self.c.season['CharShopChessDatas']:raise DomainError('invalid_chess_id')
                if not isinstance(deploy,dict):raise DomainError('invalid_deploy')
                entry=self.c.season['CharShopChessDatas'][chess_id]
                if is_diy and entry.get('CharId') is not None:raise DomainError('not_a_diy_slot')
                existing=profile['playerActivity']['chessSquad'].get(chess_id,{})
                char_id=deploy.get('diyChar') if is_diy else existing.get('charId',entry.get('CharId'))
                if not isinstance(char_id,str) or char_id not in self.c.data['operators']:raise DomainError('unknown_cn_operator')
                skill=deploy.get('skillIndex')
                if type(skill)is not int or not 0<=skill<max(1,len(self.c.data['operators'][char_id].get('Skills') or [])):
                    raise DomainError('invalid_skill_index')
                equip=deploy.get('currentEquip')
                if not self.c.equipment_allowed(char_id,equip):raise DomainError('invalid_operator_equipment')
                backup=deploy.get('origChessId') if is_diy else existing.get('diyBackupChessId')
                if backup is not None and (not isinstance(backup,str) or backup not in self.c.season['CharShopChessDatas']):raise DomainError('invalid_backup_chess_id')
                cultivate=deploy.get('cultivateEffect',existing.get('cultivateEffect') if existing.get('charId')==char_id else None)
                if not self.c.valid_cultivate_effect(cultivate):raise DomainError('invalid_cultivate_effect')
                if cultivate is None:cultivate=self.c.cultivate_effect(char_id)
                validated[chess_id]={'chessId':chess_id,'charId':char_id,'tmplId':entry.get('TmplId'),
                    'diyBackupChessId':backup,'cultivateEffect':cultivate,'currentEquip':equip,'skin':char_id+'#1',
                    'type':3 if is_diy else existing.get('type',0),'potentialRank':5,'skillIndex':skill,'assistInfo':None}
            squad=profile['playerActivity']['chessSquad']
            if is_diy:
                # The native selection helper returns the complete DIY pool,
                # including retained slots in other levels. Empty means clear
                # DIY choices. Validate the whole batch before removing any.
                squad={key:value for key,value in squad.items() if value.get('type')!=3}
                profile['playerActivity']['chessSquad']=squad
            squad.update(validated)
