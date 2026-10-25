# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Stronghold-Protocol contributors
# Economic lifecycle/empty upgrade-slot rules adapted from upstream
# server/match/player/{economy,round}.js and server/sim/content/bands/meta.js at
# cf7a9400270d5aa79b2e601e405dab782e25f120; CN parameters remain authoritative.
# See ../third_party/stronghold-protocol-reference/ for licence and notices.
"""Independent native-activity domain model; no browser simulation or game writes.

Unpublished server choices are versioned local policy. Native DTO transport and
client integration are separate from these tested economic/state transitions.
"""
import copy
import secrets
from .catalog import draw,weighted
from .special_choices import PREP_KEYS,GATE_KEYS,NATIVE_KEYS

POLICY={'version':1,'economyVersion':2,'initialCoins':0,'roundIncomeBase':3,'roundIncomeCap':12,'upgradePriceReductionPerRound':1,
        'draw':'uniform eligible tier; xorshift32-v1','hiddenSoloActivatedLayers':350,
        'nativeCombatAuthority':True,'exactOfficialEconomy':False}
NATIVE_EFFECTS={'env_gbuff_new','env_gbuff_new_with_verify','char_attribute_mul','char_dynamic_ability_new',
                'enemy_attribute_mul','enemy_attribute_add','char_respawntime_mul'}
USE_EFFECTS={'use_equip_upgrade_char','use_equip_reward_char_chess','use_equip_reward_random_char_chess_in_shop',
             'use_equip_reward_char_chess_with_same_bond','use_equip_reward_char_chess_bond_layer',
             'use_equip_gain_coin_when_next_round_start','equip_destory_gain_random_coin',
             'use_equip_reward_special_goods_char_chess','equip_destory_deployment_cnt_change'}
CHOICE_EFFECTS=PREP_KEYS|GATE_KEYS|NATIVE_KEYS

class DomainError(Exception):
    def __init__(self,code,detail=''):self.code=code;self.detail=detail;super().__init__(f'{code}: {detail}')

def integer(value,name,minimum=0,maximum=2147483647):
    if type(value)is not int or not minimum<=value<=maximum:raise DomainError('invalid_argument',name)
    return value

class Engine:
    def __init__(self,catalog):
        self.c=catalog
        from .garrison_effects import GarrisonEffects
        from .item_effects import ItemEffects
        from .economy_effects import EconomyEffects
        from .shop_rewards import ShopRewards
        from .acquisition_effects import AcquisitionEffects
        from .setup_rules import SetupRules
        from .band_rules import BandRules
        from .finite_pool import FinitePool
        from .special_choices import SpecialChoices
        from .broadcasts import BattlefieldBroadcasts
        self.effects=GarrisonEffects(self)
        self.items=ItemEffects(self)
        self.economy=EconomyEffects(self)
        self.shop_rewards=ShopRewards(self)
        self.acquisitions=AcquisitionEffects(self)
        self.setup=SetupRules(self)
        self.bands=BandRules(self)
        self.pool=FinitePool(self)
        self.special=SpecialChoices(self)
        self.broadcasts=BattlefieldBroadcasts(self)
    def create(self,run_id,args):
        return self._create(run_id,args,False)
    def create_multiplayer_player(self,run_id,args):
        """Room coordinator only; public solo routes retain their mode guard."""
        return self._create(run_id,args,True)
    def _create(self,run_id,args,multiplayer):
        if not isinstance(args,dict):raise DomainError('invalid_payload')
        mode=args.get('modeId','mode_single_funny')
        if mode not in self.c.season['ModeDataDict']:raise DomainError('unknown_mode',mode)
        if (self.c.mode(mode)['ModeType'] not in (0,1)) != multiplayer:raise DomainError('multiplayer_not_implemented' if not multiplayer else 'multiplayer_mode_required')
        seed=integer(args['seed'] if 'seed' in args else secrets.randbits(32),'seed',0,0xffffffff) or 1
        pool=args.get('chessPool')
        if pool is None:pool=[k for k,v in self.c.season['CharShopChessDatas'].items() if not v['IsHidden']]
        if not isinstance(pool,list) or not pool or any(k not in self.c.season['CharShopChessDatas'] for k in pool):raise DomainError('invalid_pool')
        diy=args.get('diyChessPool',{})
        if not isinstance(diy,dict):raise DomainError('invalid_diy_pool')
        for chess_id,choice in diy.items():
            if chess_id not in self.c.season['DiyChessDict'] or not isinstance(choice,dict):raise DomainError('invalid_diy_slot')
            char=self.c.data['operators'].get(choice.get('charId'))
            if not char or not choice.get('charId','').startswith('char_'):raise DomainError('unknown_cn_operator')
            integer(choice.get('skillIndex',0),'skillIndex',0,len(char.get('Skills') or [])-1)
            equip=choice.get('currentEquip')
            if equip is not None:
                owner=self.c.equipment_owner(equip)
                if owner is None:raise DomainError('unknown_cn_module')
                if owner!=choice['charId']:raise DomainError('invalid_operator_equipment')
            if chess_id not in pool:pool.append(chess_id)
        s={'schemaVersion':1,'catalogHash':self.c.hash,'policyVersion':POLICY['version'],'runId':run_id,
           'revision':0,'rngState':seed,'modeId':mode,'round':0,'phase':'band','nextInstId':self.c.number('Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer','LOCAL_ID_START'),
           'seq':0,'history':[],'completedReports':{},'scheduled':[],'chessPool':sorted(set(pool)),'diyChessPool':copy.deepcopy(diy),
           'bandId':None,'stageId':None,'bossId':None,'hiddenBossId':None,'stageSeed':seed,'specialEffects':[],'forceActiveBonds':False,'battle':None,'result':None,
           'player':{'uidIndex':0,'hp':0,'round':0,'store':{'level':1,'coin':POLICY['initialCoins'],'upgradePrice':0,'refreshPrice':self.c.season['ConstData']['ShopRefreshPrice'],
             'isSpecialGoods':False,'freeRefreshCnt':0,'specialRefresh':False,'goods':[],'refreshSeq':0},
             'charChess':[],'equipChess':[],'trapChess':[],'positions':[],'bonds':[],'selfChoice':None,
             'maxDeploymentCnt':self.c.season['ConstData']['MaxBattleChessCnt'],'effectEnemies':[],
             'roundAnalytics':{'chessGained':[],'coinCost':0}}}
        self.broadcasts.initialize(s)
        self.setup.initialize(s)
        self.pool.initialize(s)
        self._bonds(s)
        return s
    def _phase(self,s,*phases):
        if s['phase'] not in phases:raise DomainError('wrong_phase',s['phase'])
    def _instance(self,s,ident,fields=('charChess','equipChess','trapChess')):
        integer(ident,'instanceId')
        for field in fields:
            for obj in s['player'][field]:
                if obj['instId']==ident:return obj,field
        raise DomainError('unknown_instance',str(ident))
    def _id(self,s):
        if s.get('multiplayerInstanceNamespace') is not None:
            from .multiplayer_instance_ids import allocate
            return allocate(s)
        s['nextInstId']+=1;return s['nextInstId']
    def _definition(self,obj):return self.c.chess[self.c.data['identifiers'][str(obj['chessIdentifier'])]]
    def _debit(self,s,amount):
        p=s['player'];store=p['store']
        if store['coin']<amount:raise DomainError('insufficient_funds')
        store['coin']-=amount;p['roundAnalytics']['coinCost']+=amount
    def _reprice(self,s):
        for good in s['player']['store']['goods']:
            key=self.c.data['identifiers'][str(good['chessIdentifier'])]
            shop=self.c.season['CharShopChessDatas'].get(key)
            if shop:
                good['price']=self._price(s,key)
    def _price(self,s,key):
        base=self.c.season['ShopCharChessInfoData'][str(self.c.season['CharShopChessDatas'][key]['ChessLevel'])][0]['PurchasePrice']
        return self.economy.price(s,key,self.effects.price(s,key,base))
    def _gain(self,s,chess_id,merge=True,stock='fixed'):
        if chess_id not in self.c.chess:raise DomainError('unknown_chess',chess_id)
        self.pool.require(s,chess_id,fixed=stock=='fixed',merge=merge)
        d=self.c.chess[chess_id];is_char=chess_id in self.c.season['CharChessDataDict']
        field='charChess' if is_char else 'equipChess' if d['ItemType']==1 else 'trapChess'
        obj={'instId':self._id(s),'chessIdentifier':d['Identifier']}
        if is_char:obj['equipInstIds']=[]
        s['player'][field].append(obj)
        return self.acquisitions.gain(s,obj,field,d,merge)
    def _merge(self,s,field,d):
        if not d.get('UpgradeChessId') or not d.get('UpgradeNum'):return
        matches=[o for o in s['player'][field] if o['chessIdentifier']==d['Identifier']]
        if len(matches)<d['UpgradeNum']:return
        use=matches[:d['UpgradeNum']];keep=use[0]
        if field=='charChess':
            positions={row['instId']:row['position'] for row in s['player']['positions']}
            deployed=[obj for obj in use if obj['instId'] in positions]
            if deployed:keep=min(deployed,key=lambda obj:(positions[obj['instId']]['x'],-positions[obj['instId']]['y']))
        consumed={obj['instId'] for obj in use};others=consumed-{keep['instId']}
        keep['chessIdentifier']=self.c.chess[d['UpgradeChessId']]['Identifier']
        if field=='charChess':
            # Equipment is already owned inventory; detaching all consumed
            # copies returns it to the hand without deleting or duplicating it.
            for obj in use:obj['equipInstIds']=[]
        elif field=='equipChess':
            for char in s['player']['charChess']:char['equipInstIds']=[v for v in char['equipInstIds'] if v not in consumed]
        s['player'][field]=[o for o in s['player'][field] if o['instId'] not in others]
        s['player']['positions']=[p for p in s['player']['positions'] if p['instId'] not in others]
        if field=='charChess':self.shop_rewards.merged(s,d['ChessId'],keep)
        self.acquisitions.merged(s,use,keep)
        self._merge(s,field,d)
        self._merge(s,field,self._definition(keep))
        return keep
    def _refresh(self,s,keep_frozen=False):
        store=s['player']['store'];lv=store['level'];config=self.c.shop_levels(s['modeId'])[lv]
        frozen=s.get('shopFrozen',bool(store['goods']) and all(good.get('isFrozen',False) for good in store['goods']))
        chars=[k for k in s['chessPool'] if self.c.season['CharShopChessDatas'][k]['ChessLevel']<=lv]
        items=[k for k,v in self.c.season['TrapShopChessDatas'].items() if v['ItemLevel']<=lv and
               (self.pool.shop_item(k) if self.pool.enabled(s) else not v['HideInShop'])]
        old={v['slotId']:v for v in store['goods'] if v.get('isFrozen')}
        goods=[]
        for slot in range(config['CharChessCount']+config['ItemCount']):
            if keep_frozen and slot in old:goods.append(old[slot]);continue
            char=slot<config['CharChessCount']
            candidates=self.pool.candidates(s,chars if char else items,shop=True)
            if not candidates:continue
            key=self.pool.draw(s,candidates,shop=True) if self.pool.enabled(s) else draw(s,candidates);d=self.c.chess[key]
            price=self.c.season['ShopCharChessInfoData'][str(self.c.season['CharShopChessDatas'][key]['ChessLevel'])][0]['PurchasePrice'] if char else d['PurchasePrice']
            if char:price=self._price(s,key)
            goods.append({'chessIdentifier':d['Identifier'],'slotId':slot,'price':price,'isFrozen':frozen and not keep_frozen})
        store['goods']=goods;store['refreshSeq']+=1
    def _open_shop_slots(self,s,previous_level):
        # Original goods keep their identities/frozen state. Only the item
        # segment moves when an extra character slot opens; new slots are empty.
        store=s['player']['store'];levels=self.c.shop_levels(s['modeId'])
        old_chars=levels[previous_level]['CharChessCount'];new_chars=levels[store['level']]['CharChessCount']
        for good in store['goods']:
            if good['slotId']>=old_chars:good['slotId']+=new_chars-old_chars
    def _band_effects(self,s):
        return self.c.effects(self.c.season['BandDataListDict'][s['bandId']]['EffectId'])
    def _income(self,s):
        # CN gives strategy overrides, but not the complete base-income table.
        # Use the pinned reference's explicit deterministic curve and cap.
        income=min(POLICY['roundIncomeBase']+s['round'],POLICY['roundIncomeCap'])
        for effect in self._band_effects(s):
            bb=effect['Blackboard'] or {}
            if effect['Key']=='give_coin_in_round' and int(bb['round'])==s['round']:income=int(bb['coin'])
        for effect in self._band_effects(s):
            if effect['Key']!='coin_carry_over':continue
            bb=effect['Blackboard'] or {};capital=int(bb.get('capital',0))
            if capital>0:income+=min(int(bb.get('max',2147483647)),s['player']['store']['coin']//capital*int(bb.get('interest',0)))
        return max(0,income)
    def _bonds(self,s):
        p=s['player'];old={o['bondIdentifier']:o for o in p['bonds']};deployed={v['instId'] for v in p['positions']}
        eligible=[*self.effects.board(s),*self.effects.hand(s)]
        result=[]
        # Random setup bans filter recruitment cards, not owned bond effects.
        # Only the mode's supported/inactive sets gate native activation.
        mode=self.c.mode(s['modeId']);active=set(mode['ActiveBondIdList'] or [])-set(s.get('inactiveBondIds',[]))
        raw={}
        for key,d in self.c.season['BondInfoDict'].items():
            condition=d['ActiveCondition']
            pool=eligible if condition==1 else [o for o in eligible if (o['instId'] in deployed)==(condition!=2)]
            count=len({self.c.season['ChessNormalIdLookupDict'][self._definition(o)['ChessId']] for o in pool if key in self.effects.bonds(s,o)})
            if condition==3:count=sum(bool(self._definition(o)['IsGolden']) for o in pool)
            raw[key]=count
        harmony=0
        for key,d in self.c.season['BondInfoDict'].items():
            if d['ActiveType']==2 and key in active and raw[key]>=d['ActiveCount']:
                harmony+=sum(int((e['Blackboard'] or {}).get('count',0)) for e in self.c.effects(d['EffectId']) if e['Key']=='other_bond_add_trigger_cnt')
        for key,d in self.c.season['BondInfoDict'].items():
            count=raw[key]
            if count and self.c.common['BondInfoDict'].get(key,{}).get('BondType')==2:count+=harmony
            ident=d['Identifier'];layer=0 if d['NoStack'] else old.get(ident,{}).get('stackCnt',0)
            # Non-count activation templates remain explicit local policy, not inferred equality.
            condition=d['ActiveConditionTemplate']
            activated=key in active and (0<count<=d['ActiveCount'] if condition=='count_threshold_downward' else count>=d['ActiveCount'])
            if s.get('forceActiveBonds') and key in active:activated=True
            result.append({'bondIdentifier':ident,'stackCnt':layer,'charCnt':count,'isActive':activated})
        p['bonds']=result
    def _layers(self,s,ident,delta):
        bond=next((b for b in s['player']['bonds'] if b['bondIdentifier']==ident),None)
        if bond is None:raise DomainError('unknown_bond')
        definition=next(d for d in self.c.season['BondInfoDict'].values() if d['Identifier']==ident)
        if definition['NoStack']:return
        updated=min(self.c.number('Torappu.Battle.AutoChessBattleConst','MAX_GARRISON_STACK'),max(0,bond['stackCnt']+delta))
        if updated==bond['stackCnt']:return
        bond['stackCnt']=updated
        self.effects.settle_bonds(s)
    def _round_start(self,s):
        s['round']+=1;s['battle']=None;s['roundEffectStats']={'manualRefreshes':0};p=s['player'];p['round']=s['round'];p['roundAnalytics']={'chessGained':[],'coinCost':0}
        from .mode_strength import project
        project(s)
        for item in list(p['equipChess']):
            if item in p['equipChess']:self._merge(s,'equipChess',self._definition(item))
        p['store']['coin']+=self._income(s);s['economyVersion']=POLICY['economyVersion']
        config=self.c.shop_levels(s['modeId'])[p['store']['level']]
        p['store']['upgradePrice']=(config['InitialUpgradePrice'] if s['round']==1 else
            max(0,p['store']['upgradePrice']-POLICY['upgradePriceReductionPerRound']))
        self.items.round_start(s)
        self._refresh(s,True);self._bonds(s)
        s['shopFrozen']=False
        for good in p['store']['goods']:good['isFrozen']=False
        self.economy.event(s,'ROUND_START')
        s['phase']='preparation'
        self.effects.dispatch(s,'SERVER_PREP_START')
        self.effects.settle_bonds(s)
        self.bands.event(s,'ROUND_START')
        self._bonds(s)
        if s.get('multiplayerCoordinatorVersion')==1:
            from .multiplayer_policy import round_start
            round_start(self,s)
        else:self.special.round_start(s)
    def _promote(self,obj):
        d=self._definition(obj)
        if d.get('UpgradeChessId'):obj['chessIdentifier']=self.c.chess[d['UpgradeChessId']]['Identifier']
    def _battle(self,s):
        turn=self.c.turns(s['modeId'])[s['round']];variants=self.c.rounds(s['modeId'])[s['round']]
        wanted=s['hiddenBossId'] if s['round']==max(self.c.rounds(s['modeId'])) and s['round']>min(r for r,t in self.c.turns(s['modeId']).items() if t['IsBossTurn']) else s['bossId']
        matching=[v for v in variants if v['BossId']==wanted] if turn['IsBossTurn'] else variants
        variant=self.setup.variant(s) if s.get('setupVersion',0)>=2 else draw(s,matching or variants)
        s['phase']='boss' if turn['IsBossTurn'] else 'battle'
        if variant['BossId']:s['bossId']=variant['BossId']
        actions=self.setup.actions(s,variant['LevelId'])
        units=[]
        for index,a in enumerate(actions):
            for _ in range(a['Count']):
                unit={'instId':self._id(s),'enemyId':a['Key'],'actionIndex':a.get('nativeActionIndex',index)}
                if a.get('bandKillCoin'):unit['bandKillCoin']=a['bandKillCoin']
                units.append(unit)
        difficulty=self.c.mode(s['modeId'])['ModeDifficulty'];boss=self.c.season['BossInfoDict'][s['bossId']]
        hp=boss[{1:'BloodPoint',2:'BloodPointNormal',3:'BloodPointHard',4:'BloodPointAbyss'}.get(difficulty,'BloodPoint')]
        s['battle']={'round':s['round'],'levelId':variant['LevelId'],'actions':actions,'units':units,'killed':[],
                     'escaped':[],'bossHp':hp if s['phase']=='boss' else None,'reports':{},'nativeEngineRequired':True,
                     'enemySubstitutionPolicy':('cn-random-picks-v2' if s.get('setupVersion',0)>=2 else
                         'cn-random-picks-v1' if s.get('setupVersion') else 'legacy-template-native-ids')}
        from .mode_strength import effect_ids,describe
        if s.get('multiplayerCoordinatorVersion')==1:
            from .multiplayer_policy import strengths,project
            project(s)
            s['battle']['nativeModeEffectIds']=list(s['nativeModeEffectIds'])
            s['battle']['nativeModeStrength']=strengths(s)
        else:
            s['battle']['nativeModeEffectIds']=effect_ids(s)
            s['battle']['nativeModeStrength']=describe(self.c,s)
        self.items.prepare_battle(s)
    def _finish(self,s,args,coordinated_loss=None):
        round_id=integer(args.get('round'),'round',1)
        # Validate before replay comparison as well, so malformed duplicate
        # reports cannot reach set() with unhashable or boolean identities.
        for field in ('escapedInstIds','killedInstIds','escapedTokenInstIds'):
            values=args.get(field,[])
            if not isinstance(values,list):raise DomainError('invalid_report')
            if any(type(value)is not int or value<0 for value in values):
                raise DomainError('invalid_escaped_token_ids' if field=='escapedTokenInstIds' else 'invalid_enemy_ids')
        if str(round_id) in s['completedReports']:
            old=s['completedReports'][str(round_id)]
            if (set(args.get('escapedInstIds',old['escaped']))!=set(old['escaped']) or
                    set(args.get('killedInstIds',old['killed']))!=set(old['killed']) or
                    set(args.get('escapedTokenInstIds',old.get('escapedTokens',[])))!=set(old.get('escapedTokens',[]))):raise DomainError('conflicting_round_report')
            return {'duplicateRound':True}
        self._phase(s,'battle','boss')
        if round_id!=s['round']:raise DomainError('stale_round')
        battle=s['battle'];known={u['instId'] for u in battle['units']}
        escaped=args.get('escapedInstIds',battle['escaped']);killed=args.get('killedInstIds',battle['killed']);tokens=args.get('escapedTokenInstIds',[])
        if not isinstance(escaped,list) or not isinstance(killed,list) or not isinstance(tokens,list):raise DomainError('invalid_report')
        if any(type(x)is not int for x in escaped+killed) or not set(escaped+killed)<=known or set(escaped)&set(killed):raise DomainError('invalid_enemy_ids')
        # Tokens use the native summoned-enemy namespace independently of the
        # server-issued enemies; equal numeric IDs across namespaces are legal.
        if any(type(x)is not int or x<0 for x in tokens):raise DomainError('invalid_escaped_token_ids')
        if s['phase']=='boss' and battle['bossHp']>0 and not args.get('defeat'):raise DomainError('boss_still_alive')
        loss=min(len(set(escaped))+len(set(tokens)),self.c.season['ConstData']['CostPlayerHpLimit'])
        if coordinated_loss is not None:
            if s.get('multiplayerCoordinatorVersion')!=1:raise DomainError('multiplayer_room_required')
            loss=integer(coordinated_loss,'coordinatedHpCost',0,self.c.season['ConstData']['CostPlayerHpLimit'])
        overtime=integer(args.get('bossTimeoutHpCost',0),'bossTimeoutHpCost',0,86400)
        if overtime and s['phase']!='boss':raise DomainError('boss_timeout_requires_boss_round')
        loss+=min(overtime,max(0,s['player']['hp']-loss))
        if 'bossPlayerHp' in args:
            if s['phase']!='boss':raise DomainError('boss_hp_requires_boss_round')
            remaining=integer(args['bossPlayerHp'],'bossPlayerHp',0,max(0,s['player']['hp']-overtime))
            # MockBossBattleManager re-spawns first escapes; later escapes can
            # damage team HP repeatedly for the same issued identity. Its real
            # final team HP is authoritative, not a unique-ID normal loss cap.
            loss=s['player']['hp']-remaining
        s['player']['hp']=0 if args.get('defeat') else max(0,s['player']['hp']-loss)
        s['completedReports'][str(round_id)]={'escaped':escaped,'killed':killed,'escapedTokens':sorted(set(tokens)),'hpCost':loss}
        s['completedReports'][str(round_id)]['nativeModeEffectIds']=copy.deepcopy(battle.get('nativeModeEffectIds',[]))
        s['completedReports'][str(round_id)]['nativeModeStrength']=copy.deepcopy(battle.get('nativeModeStrength',{}))
        self.bands.battle_result(s,killed)
        self.items.battle_result(s)
        # Rooms own the all-seat barrier. Rewards and HP are committed once,
        # but no individual may start another rest period ahead of teammates.
        if s.get('multiplayerCoordinatorVersion')==1:
            s['phase']='preparation'
            return {'hpCost':loss,'awaitingTeamBarrier':True}
        rounds=self.c.rounds(s['modeId']);limit=max(rounds)
        # The optional R15 unlock is local policy; the first boss is the terminal baseline.
        boss_rounds=[r for r,t in self.c.turns(s['modeId']).items() if t['IsBossTurn']]
        last=min(boss_rounds) if boss_rounds else limit
        # Beating the baseline boss wins the run. The optional hidden boss can
        # improve the record, but cannot revoke that earned clear on defeat.
        baseline=s['completedReports'].get(str(last),{})
        baseline_won=(baseline.get('bossDefeated') is True or
            round_id==last and s['phase']=='boss' and battle['bossHp']<=1 and not args.get('defeat'))
        hidden_unlocked=(s['round']==last and limit>last and self.c.mode(s['modeId'])['ModeDifficulty']>=2 and s['player']['hp']>1 and
                         sum(b['stackCnt'] for b in s['player']['bonds'] if b['isActive'])>POLICY['hiddenSoloActivatedLayers'])
        if s['player']['hp']<=0 or s['round']>=limit or s['round']>=last and not hidden_unlocked:
            passed=s['round'] if s['player']['hp']>0 or s['phase']=='boss' and battle['bossHp']<=1 and not args.get('defeat') else s['round']-1
            s['phase']='result';s['result']={'success':baseline_won or s['player']['hp']>0,
                'roundsPassed':max(last if baseline_won else 0,passed),'hp':s['player']['hp']}
        else:self._round_start(s)
        return {'hpCost':loss}
    def apply(self,s,op,args):
        if not isinstance(args,dict):raise DomainError('invalid_payload')
        p=s['player'];store=p['store'];extra={}
        if op=='band.choose':
            self._phase(s,'band');key=args.get('bandId');band=self.c.season['BandDataListDict'].get(key)
            if not band:raise DomainError('unknown_band')
            s['bandId']=key;p['hp']=band['TotalHp'];s['phase']='loading'
        elif op=='loading.ready':self._phase(s,'loading');self._round_start(s)
        elif op=='shop.refresh':
            self._phase(s,'preparation')
            self.shop_rewards.require_regular_shop(s)
            snapshot=[*self.effects.board(s),*self.effects.hand(s)]
            paid=0
            if store['freeRefreshCnt']:store['freeRefreshCnt']-=1
            else:paid=store['refreshPrice'];self._debit(s,paid)
            self._refresh(s,False)
            self.economy.event(s,'MANUAL_REFRESH')
            stats=s.setdefault('roundEffectStats',{});stats['manualRefreshes']=stats.get('manualRefreshes',0)+1
            self.bands.event(s,'MANUAL_REFRESH')
            self.effects.refresh_bonds(s)
            self.effects.dispatch(s,'SERVER_REFRESH_SHOP',snapshot=snapshot)
            self.economy.event(s,'SPEND',amount=paid)
        elif op=='shop.freeze':
            self._phase(s,'preparation');flag=args.get('isFrozen')
            self.shop_rewards.require_regular_shop(s)
            if type(flag)is not bool:raise DomainError('invalid_frozen_flag')
            s['shopFrozen']=flag
            for good in store['goods']:good['isFrozen']=flag
        elif op=='shop.upgrade':
            self._phase(s,'preparation');levels=self.c.shop_levels(s['modeId'])
            self.shop_rewards.require_regular_shop(s)
            if store['level']==max(levels):raise DomainError('max_shop_level')
            paid=store['upgradePrice'];self._debit(s,paid);previous_level=store['level'];store['level']+=1
            store['upgradePrice']=levels[store['level']]['InitialUpgradePrice']
            self._open_shop_slots(s,previous_level)
            self.broadcasts.event(s,'SHOP_LEVEL',level=store['level'])
            self.economy.event(s,'LEVEL_UP',level=store['level'])
            self.economy.event(s,'SPEND',amount=paid)
        elif op=='shop.buy':
            self._phase(s,'preparation');slot=integer(args.get('slotId'),'slotId')
            special=args.get('isSpecial',False)
            if type(special)is not bool:raise DomainError('invalid_special_flag')
            from .hand_layout import require_purchase_space
            require_purchase_space(self.c,s)
            if special:
                extra=self._trait_choice(s,{'slotId':slot})
            else:
                self.shop_rewards.require_regular_shop(s)
                self._reprice(s)
                good=next((v for v in store['goods'] if v['slotId']==slot),None)
                if not good:raise DomainError('empty_shop_slot')
                paid=good['price'];key=self.c.data['identifiers'][str(good['chessIdentifier'])]
                if self.pool.enabled(s) and not self.pool.available(s,key,shop=True):raise DomainError('stock_exhausted',key)
                self._debit(s,paid);store['goods'].remove(good)
                self.economy.event(s,'BUY',chessId=key)
                self.bands.event(s,'BUY',chessId=key)
                acquired=self._gain(s,key,stock='strict')
                self.special.after_purchase(s,acquired)
                self.economy.event(s,'SPEND',amount=paid)
        elif op=='board.move':
            self._phase(s,'preparation');updates=args.get('unitPositionInfo')
            if not isinstance(updates,list):raise DomainError('invalid_positions')
            from .native_tokens import deployment_volume
            from .board_terrain import level_id as preparation_level
            terrain_level=preparation_level(self.c,s)
            previous_deployment_volume=deployment_volume(self.c,s)
            positions=[];seen=set()
            for unit in updates:
                if not isinstance(unit,dict):raise DomainError('invalid_positions')
                obj,_=self._instance(s,unit.get('instId'),('charChess','trapChess'))
                pos=unit.get('position',{})
                if not isinstance(pos,dict):raise DomainError('native_position_codec_pending')
                x=integer(pos.get('x'),'x');y=integer(pos.get('y'),'y')
                tile=self.c.level_tile(terrain_level,y,x)
                if not self.c.tile_allows_deployment(tile):
                    raise DomainError('invalid_tile',{'stageId':s['stageId'],'position':{'row':y,'col':x},
                        'tileKey':tile['TileKey'] if tile else None,'buildableType':tile['BuildableType'] if tile else None,
                        'reason':'outside_map' if not tile else 'not_buildable' if tile['BuildableType']==0 else 'hand_tile_not_battlefield'})
                if obj['instId'] in seen or (x,y) in {(u['position']['x'],u['position']['y']) for u in positions}:raise DomainError('position_collision')
                kind=0 if obj in p['charChess'] else self._definition(obj)['ItemType']
                seen.add(obj['instId']);positions.append({'instId':obj['instId'],'position':{'x':x,'y':y},'chessType':kind,'direction':integer(unit.get('direction',0),'direction',0,3)})
            p['positions']=positions
            from .hand_layout import reconcile
            reconcile(self.c,s,args.get('handPositions'),token_positions=args.get('nativeTokenPositions',[]))
            from .native_tokens import accept_tokens,require_deployment_capacity
            accept_tokens(self.c,s,args.get('nativeTokenPositions',[]))
            self._bonds(s)
            require_deployment_capacity(self.c,s,args.get('deployCount'),previous_deployment_volume)
        elif op=='board.destroy':
            self._phase(s,'preparation');obj,field=self._instance(s,args.get('chessInstId'))
            sale_coin=0
            if field=='charChess':
                d=self._definition(obj);normal=self.c.season['ChessNormalIdLookupDict'].get(d['ChessId'],d['ChessId']);shop=self.c.season['CharShopChessDatas'].get(normal)
                sale_coin=self.c.season['ShopCharChessInfoData'][str(shop['ChessLevel'])][0]['ChessSoldPrice'] if shop else 1
            p[field].remove(obj);p['positions']=[v for v in p['positions'] if v['instId']!=obj['instId']]
            if field=='charChess':obj['equipInstIds']=[]
            for char in p['charChess']:char['equipInstIds']=[i for i in char['equipInstIds'] if i!=obj['instId']]
            self._bonds(s)
            if field=='charChess':
                store['coin']+=self.bands.sale(s,obj,sale_coin)
                self.effects.dispatch(s,'SERVER_CHESS_SOLD',obj)
                self.items.event(s,'SERVER_CHESS_SOLD',obj)
            elif field=='trapChess':self.items.destroy(s,obj)
            self.effects.settle_bonds(s)
        elif op=='board.equip':extra=self._equip(s,args)
        elif op=='item.use':
            from .native_tokens import require_deployment_capacity
            require_deployment_capacity(self.c,s)
            extra=self.items.magic(s,args)
            require_deployment_capacity(self.c,s)
        elif op=='bond.add':
            # Native TriggerGarrisonStatusRefresh only adds combat layers in
            # SELF_BATTLE (4), before AddBondCount; BOSS_BATTLE (6) is excluded.
            # Preparation/end-of-rest and reward events use separate dispatch.
            self._phase(s,'battle');source,_=self._instance(s,args.get('charInstId'),('charChess',));delta=integer(args.get('layerDelta'),'layerDelta',1,999)
            values=args.get('bondIndexList')
            known={d['Identifier'] for d in self.c.season['BondInfoDict'].values()}
            if not isinstance(values,list) or not values or len(values)>len(known) or any(type(v)is not int or v not in known for v in values) or len(set(values))!=len(values):raise DomainError('invalid_bond_identifiers')
            native_owned=(s.get('battle') or {}).get('nativeOwnedChessIds')
            if native_owned is not None and source['instId'] not in native_owned:raise DomainError('source_not_in_native_battle')
            for ident in values:self._layers(s,ident,delta)
        elif op=='preparation.ready':
            self._phase(s,'preparation')
            from .native_tokens import require_deployment_capacity
            require_deployment_capacity(self.c,s)
            self.shop_rewards.preparation_ready(s)
            if s.get('itemChoice'):raise DomainError('item_choice_selection_required')
            self.effects.prep_end_bonds(s)
            self.effects.dispatch(s,'SERVER_PREP_FIN')
            for effect in self.c.effects(self.c.season['BandDataListDict'][s['bandId']]['EffectId']):
                if effect['Key']=='prep_finish_char_bond_add_layer':
                    deployed={u['instId'] for u in p['positions']};groups={}
                    for char in p['charChess']:
                        if char['instId'] in deployed:
                            normal=self.c.season['ChessNormalIdLookupDict'][self._definition(char)['ChessId']]
                            tier=self.c.season['CharShopChessDatas'][normal]['ChessLevel'];groups.setdefault(tier,[]).append(char)
                    for values in groups.values():
                        selected=draw(s,values)
                        for bond in self.effects.bonds(s,selected):self._layers(s,self.c.season['BondInfoDict'][bond]['Identifier'],int(effect['Blackboard']['layer']))
            from .hand_layout import finish_preparation
            discarded=finish_preparation(self.c,s)
            if discarded:self._bonds(s);extra={'discardedOverflowInstIds':discarded}
            require_deployment_capacity(self.c,s)
            self.special.preparation_ready(s)
            # Only a strategy with the explicit carry-over effect preserves
            # leftover funds. PREP_FIN milestone funds were scheduled already.
            if not any(effect['Key']=='coin_carry_over' for effect in self._band_effects(s)):store['coin']=0
            store['goods']=[good for good in store['goods'] if good.get('isFrozen')]
            store['specialRefresh']=False
            self._battle(s)
        elif op in ('battle.kill','battle.escape'):
            self._phase(s,'battle','boss');ident=integer(args.get('enemyInstId'),'enemyInstId');known={u['instId'] for u in s['battle']['units']}
            if ident not in known:raise DomainError('unknown_enemy')
            field='killed' if op=='battle.kill' else 'escaped';other='escaped' if field=='killed' else 'killed'
            if ident in s['battle'][other]:raise DomainError('conflicting_enemy_result')
            if ident not in s['battle'][field]:s['battle'][field].append(ident)
        elif op=='boss.damage':
            self._phase(s,'boss');damage=integer(args.get('damage'),'damage',0,self.c.number('Torappu.Battle.AutoChessBattleConst','MAX_BATTLE_DAMAGE'))
            s['battle']['bossHp']=max(0,s['battle']['bossHp']-damage)
        elif op=='battle.finish':extra=self._finish(s,args)
        elif op=='multiplayer.finish':
            if s.get('multiplayerCoordinatorVersion')!=1:raise DomainError('multiplayer_room_required')
            extra=self._finish(s,args,integer(args.get('_roomHpCost'),'coordinatedHpCost',0,self.c.season['ConstData']['CostPlayerHpLimit']))
        elif op=='battle.info':self._phase(s,'battle','boss');s['battle']['reports']=copy.deepcopy(args)
        elif op=='pause':
            self._phase(s,'preparation','battle','boss','special');s['previousPhase']=s['phase'];s['phase']='paused'
        elif op=='resume':self._phase(s,'paused');s['phase']=s.pop('previousPhase')
        elif op=='quit':
            if s['phase']=='result':return {'duplicateQuit':True}
            bosses=[r for r,t in self.c.turns(s['modeId']).items() if t['IsBossTurn']]
            baseline=min(bosses) if bosses else max(self.c.rounds(s['modeId']))
            cleared=s['completedReports'].get(str(baseline),{}).get('bossDefeated') is True
            s['phase']='result';s['result']={'success':cleared,'roundsPassed':len(s['completedReports']),
                'hp':p['hp'],'abandoned':not cleared}
        elif op=='choice.pick':
            extra=(self._choice(s,args) if s['phase']=='special' else
                   self._trait_choice(s,args) if s.get('traitRewardOffers') else
                   self.items.choice(s,args) if s.get('itemChoice') else self._choice(s,args))
        elif op=='history':
            after=integer(args.get('seq',0),'seq')
            return {'localDomainHistory':[copy.deepcopy(v) for v in s['history'] if v['seq']>after],
                    'officialStepHistoryImplemented':False}
        else:raise DomainError('unknown_operation',op)
        # A move or equipment swap can activate already-stacked bonds. Their
        # unpaid milestones must settle now, even without a new layer delta.
        self.effects.settle_bonds(s)
        self._reprice(s)
        self.pool.prune_displayed(s)
        from .hand_layout import reconcile
        reconcile(self.c,s)
        s['revision']+=1;s['seq']+=1;s['history'].append({'seq':s['seq'],'op':op,'payload':copy.deepcopy(args),'round':s['round'],'phase':s['phase']})
        return extra
    def _trait_choice(self,s,args):
        self._phase(s,'preparation');offers=s.get('traitRewardOffers') or []
        if not offers:raise DomainError('no_trait_reward_offer')
        from .hand_layout import require_purchase_space
        require_purchase_space(self.c,s)
        slot=integer(args.get('slotId'),'slotId',0,len(offers[0]['options'])-1)
        selected=offers[0]['options'][slot]
        self.pool.require(s,selected)
        offers.pop(0);self._gain(s,selected,stock='strict')
        return {'selectedChessId':selected,'remainingOffers':len(offers)}
    def _equip(self,s,args):
        return self.items.equip(s,args)
    def _choice(self,s,args):
        return self.special.pick(s,args)
