// Included after native battle model helpers, before native_combat.inc.
// Existing CN Rune construction/execution only; shared tables remain unchanged.
#pragma once

using NBMR_Load=void(*)(void*,const void*);
NBMR_Load nbmr_original_load=nullptr;
std::recursive_mutex nbmr_lock;
std::atomic<unsigned> nbmr_preflights{0},nbmr_rebuilds{0},nbmr_added{0},nbmr_forwarded{0},nbmr_failures{0};
std::atomic<bool> nbmr_prepared{false},nbmr_applied{false},nbmr_failed{false};
std::string nbmr_run,nbmr_mode,nbmr_stage,nbmr_error,nbmr_last_effect;
int32_t nbmr_round=0;
std::vector<std::string> nbmr_expected,nbmr_applied_ids;

int32_t nbmr_version(const cnjson::Value& state){
    if(state.kind!=cnjson::Value::Object||state.object.find("modeStrengthVersion")==state.object.end())return 0;
    return nb_number(state,"modeStrengthVersion");
}

void* nbmr_owner(void* model){
    if(!model||!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||!private_battle_resources.load()||
       unity_ui_thread.load()!=GetCurrentThreadId()||nb_run_id.empty()||nb_state.kind!=cnjson::Value::Object||
       nb_string(nb_state,"activityId")!="act2autochess"||nb_string(nb_state,"runId")!=nb_run_id||
       !cncombat::stage_allowed(nb_string(nb_state,"stageId"),nb_string(nb_state,"modeId")))return nullptr;
    auto centre=nb_centre(),manager=nb_manager(),bridge=nb_retained("native-battle-local-player");
    if(!centre||!manager||!bridge||!act(field_object(centre,"<actId>k__BackingField"))||
       field_object(manager,"dataCenter")!=centre||field_object(manager,"dataBridge")!=bridge||
       field_object(centre,"battleData")!=model||
       api.il2cpp_object_get_class(model)!=cls("Torappu.Battle.AutoChess.AutoChessBattleDataModel")||
       api.il2cpp_object_get_class(bridge)!=cls("Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer"))return nullptr;
    return model;
}
std::vector<std::string> nbmr_ids(const cnjson::Value& state,bool battle){
    auto version=nbmr_version(state);
    if(version==0)return {}; // Preserve explicitly unversioned old private runs.
    if(version!=1)throw std::runtime_error("private mode Rune version unsupported");
    const auto& list=nb_at(battle?nb_at(state,"battle"):state,"nativeModeEffectIds");
    if(list.kind!=cnjson::Value::Array||list.array.empty()||list.array.size()>16)
        throw std::runtime_error("private mode Rune effect list missing or outside bound");
    std::vector<std::string> result;
    for(const auto& value:list.array){
        if(value.kind!=cnjson::Value::String||(value.string!="aceffect_enemy_1"&&value.string!="aceffect_enemy_3"&&
           value.string!="aceffect_enemy_4"&&value.string!="aceffect_enemy_5"))
            throw std::runtime_error("private mode Rune effect outside verified CN templates");
        result.push_back(value.string); // Repeated reinforcement stacks are intentional.
    }
    return result;
}
void* nbmr_packed(void* model,const std::string& effect,TemporaryRoots& roots){
    nbmr_last_effect=effect;
    auto key=roots.retain(string_new(effect.c_str()));void* args[]={key};
    auto packed=roots.retain(invoke(nb_method(0x60138ca),model,args));
    if(!packed||api.il2cpp_object_get_class(packed)!=cls("Torappu.RuneTable+PackedRuneData")||
       text(field_object(packed,"id"))!=effect)
        throw std::runtime_error("native mode packed Rune class/identity invalid");
    auto runes=field_object(packed,"runes");int32_t count=0;
    if(!runes||!read(unbox(invoke(method(api.il2cpp_object_get_class(runes),"get_Count",0),runes)),count)||count!=1)
        throw std::runtime_error("native mode Rune missing/unregistered (expected exactly one CN Rune)");
    int32_t index=0;void* item_args[]={&index};auto rune=roots.retain(invoke(method(api.il2cpp_object_get_class(runes),"get_Item",1),runes,item_args));
    if(!rune||api.il2cpp_object_get_class(rune)!=cls("Torappu.RuneData")||
       text(field_object(rune,"key"))!=(effect=="aceffect_enemy_1"?"env_gbuff_new":"enemy_attribute_mul"))
        throw std::runtime_error("native mode Rune key/class mismatch");
    return packed;
}
void nbmr_failure(const char* stage,const std::exception& error){
    nbmr_failed=true;nbmr_applied=false;++nbmr_failures;
    nbmr_error=std::string(stage)+" ["+nbmr_last_effect+"]: "+error.what();
    nb_error("native-mode-runes",std::runtime_error(nbmr_error));
}
void native_battle_mode_runes_preflight(){
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nbmr_lock);++nbmr_preflights;
    nbmr_prepared=false;nbmr_applied=false;nbmr_last_effect="<scope>";
    try{
        const auto mode=nb_string(nb_state,"modeId"),stage=nb_string(nb_state,"stageId");
        if(nb_string(nb_state,"phase")!="preparation"||!cncombat::preparation_resume("preparation",nb_number(nb_state,"round"),mode))
            throw std::runtime_error("private mode Rune preflight requires supported preparation");
        auto centre=nb_centre();auto model=centre?field_object(centre,"battleData"):nullptr;
        if(!nbmr_owner(model))throw std::runtime_error("private mode Rune model/LOCAL ownership unavailable");
        auto ids=nbmr_ids(nb_state,false);PrivateDataScope private_scope(true);NB_ProjectionScope projection;TemporaryRoots roots;
        // _GetPackedRuneData constructs fresh lists/blackboards and checks the
        // current native RuneManager. Never attach or cache these mutable packs.
        for(const auto& effect:ids)nbmr_packed(model,effect,roots);
        nbmr_run=nb_run_id;nbmr_mode=mode;nbmr_stage=stage;nbmr_round=nb_number(nb_state,"round");
        nbmr_expected=std::move(ids);nbmr_applied_ids.clear();nbmr_error.clear();nbmr_last_effect.clear();nbmr_failed=false;nbmr_prepared=true;
    }catch(const std::exception& error){nbmr_failure("preflight",error);throw std::runtime_error(nbmr_error);}
}
void nbmr_load(void* self,const void* method_info){
    if(native_multiplayer_mode_runes_load(self,method_info))return;
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load()||!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||
       !private_battle_resources.load()||nb_run_id.empty()||nb_state.kind!=cnjson::Value::Object){
        lease.release();nbmr_original_load(self,method_info);return;
    }
    auto activity=nb_state.object.find("activityId"),run=nb_state.object.find("runId");
    if(activity==nb_state.object.end()||activity->second.kind!=cnjson::Value::String||activity->second.string!="act2autochess"||
       run==nb_state.object.end()||run->second.kind!=cnjson::Value::String||run->second.string!=nb_run_id){
        ++nbmr_forwarded;lease.release();nbmr_original_load(self,method_info);return;
    }
    bool scoped=false;
    try{const auto phase=nb_string(nb_state,"phase");scoped=(phase=="battle"||phase=="boss")&&nbmr_owner(self)&&
            cncombat::state_allowed(phase,nb_number(nb_state,"round"),nb_string(nb_state,"modeId"));}
    catch(const std::exception& error){std::lock_guard<std::recursive_mutex> lock(nbmr_lock);nbmr_failure("scope",error);return;}
    if(!scoped){++nbmr_forwarded;lease.release();nbmr_original_load(self,method_info);return;}
    std::lock_guard<std::recursive_mutex> data_lock(nb_data_lock);std::lock_guard<std::recursive_mutex> lock(nbmr_lock);
    nbmr_applied=false;nbmr_last_effect="<original-load>";
    try{
        // Original _LoadBattleRunes resets AdvancedRuneHolder first (ba3871),
        // then rebuilds original band/equipment/character Rune data. Do not
        // skip, replace or append before that reset, including on a reload.
        nbmr_original_load(self,method_info);
        auto ids=nbmr_ids(nb_state,true);
        if(ids.empty())return;
        if(!nbmr_prepared.load()||nbmr_failed.load()||nbmr_run!=nb_run_id||nbmr_mode!=nb_string(nb_state,"modeId")||
           nbmr_stage!=nb_string(nb_state,"stageId")||nbmr_round!=nb_number(nb_state,"round")||
           ids!=nbmr_expected||ids!=nbmr_ids(nb_state,false))throw std::runtime_error("private mode Rune preflight identity/list changed");
        PrivateDataScope private_scope(true);NB_ProjectionScope projection;TemporaryRoots roots;
        uint8_t is_right=0;void* side_args[]={&is_right};int32_t side=0,index=0;
        if(!read(unbox(invoke(nb_method(0x601353e),nullptr,side_args)),side))throw std::runtime_error("native mode player side unavailable");
        for(const auto& effect:ids){
            auto packed=nbmr_packed(self,effect,roots);void* args[]={packed,&side,&index};
            invoke(nb_method(0x60138cd),self,args);++nbmr_added;
        }
        nbmr_applied_ids=std::move(ids);nbmr_applied=true;nbmr_last_effect.clear();++nbmr_rebuilds;
    }catch(const std::exception& error){nbmr_failure("load",error);}
}
void native_battle_mode_runes_require_applied(){
    if(nbmr_version(nb_state)==0)return;
    std::lock_guard<std::recursive_mutex> lock(nbmr_lock);
    if(nbmr_failed.load()||!nbmr_applied.load()||nbmr_run!=nb_run_id||nbmr_mode!=nb_string(nb_state,"modeId")||
       nbmr_stage!=nb_string(nb_state,"stageId")||nbmr_round!=nb_number(nb_state,"round")||
       nbmr_applied_ids!=nbmr_ids(nb_state,true))throw std::runtime_error("private native mode Runes were not applied: "+nbmr_error);
}
void native_battle_mode_runes_reset(){
    std::lock_guard<std::recursive_mutex> lock(nbmr_lock);nbmr_prepared=false;nbmr_applied=false;nbmr_failed=false;
    nbmr_run.clear();nbmr_mode.clear();nbmr_stage.clear();nbmr_error.clear();nbmr_last_effect.clear();nbmr_round=0;nbmr_expected.clear();nbmr_applied_ids.clear();
}
cnjson::Value native_battle_mode_runes_status(bool details){
    std::unique_lock<std::recursive_mutex> lock(nbmr_lock,std::try_to_lock);auto value=nb_object();
    value.object["snapshotBusy"]=nb_boolean(!lock.owns_lock());value.object["preflightPassed"]=nb_boolean(nbmr_prepared.load());
    value.object["applied"]=nb_boolean(nbmr_applied.load());value.object["failed"]=nb_boolean(nbmr_failed.load());
    value.object["preflights"]=nb_integer(nbmr_preflights.load());value.object["rebuilds"]=nb_integer(nbmr_rebuilds.load());
    value.object["packsAdded"]=nb_integer(nbmr_added.load());value.object["forwarded"]=nb_integer(nbmr_forwarded.load());value.object["failures"]=nb_integer(nbmr_failures.load());
    if(details&&lock.owns_lock()){
        value.object["runId"]=json_text(nbmr_run);value.object["modeId"]=json_text(nbmr_mode);value.object["stageId"]=json_text(nbmr_stage);value.object["round"]=nb_integer(nbmr_round);
        value.object["expectedEffects"]=nb_array();for(const auto& id:nbmr_expected)value.object["expectedEffects"].array.push_back(json_text(id));
        value.object["appliedEffects"]=nb_array();for(const auto& id:nbmr_applied_ids)value.object["appliedEffects"].array.push_back(json_text(id));
        value.object["lastEffect"]=json_text(nbmr_last_effect);value.object["lastError"]=json_text(nbmr_error);
    }return value;
}
void native_battle_mode_runes_setup(){nb_hook(0x60138c0,nbmr_load,nbmr_original_load);}
