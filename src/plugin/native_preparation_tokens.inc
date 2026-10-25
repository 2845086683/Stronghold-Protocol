// Private preparation-display supplement. Battle token creation remains native.
// Include after native_battle_data.inc and native battle launch globals.
#pragma once
#include "native_token_prepare_policy.h"
inline constexpr CN_Method NPT_QUERY={"Torappu.Battle.AutoChess.AutoChessBattleUtil","IsChessIdHasRelatedTokenInPrepare",0x601351a,0xb28010ULL,1,true,"System.Boolean",{"System.Int32"}};
inline constexpr CN_Method NPT_SQUAD={"Torappu.Battle.AutoChess.AutoChessBattleUtil","GetChessSquad",0x60134f1,0xb26b80ULL,1,true,"Torappu.Battle.AutoChess.ChessSquad",{"System.Int32"}};
inline constexpr CN_Method NPT_CONVERT={"Torappu.Battle.BattleDataConverter","ConvertToCharacterData",0x6013041,0xabb870ULL,5,true,"Torappu.Battle.BattleCharacterData",{"Torappu.AdvancedCharacterInst","System.Boolean","System.Boolean","Torappu.PlayerSide","Torappu.LevelData"}};
using NativePreparationTokenQuery=uint8_t(*)(int32_t,const void*);
NativePreparationTokenQuery npt_original_query=nullptr;
std::recursive_mutex npt_lock;
std::atomic<bool> npt_setup_ready{false},npt_failed{false};
std::atomic<unsigned> npt_queries{0},npt_forwarded{0},npt_suppressed{0},npt_failures{0},npt_preflights{0};
std::string npt_run,npt_error;

void* npt_field(void* owner,const char* name,const char* type){
    if(!owner)throw std::runtime_error("private preparation token field owner missing");
    auto field=profile_field(api.il2cpp_object_get_class(owner),name);
    if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),type))
        throw std::runtime_error(std::string("private preparation token field contract: ")+name);
    return field;
}
bool npt_scene(){
    if(native_multiplayer_entry_active()){
        if(!can_read()||!private_view.load()||!private_battle_resources.load()||unity_ui_thread.load()!=GetCurrentThreadId()||
           !native_multiplayer_dispatch_scene()||!native_multiplayer_dispatch_ready()||nmo_snapshot.kind!=cnjson::Value::Object)return false;
        const auto latest=native_multiplayer_transport_snapshot();const auto& shown=nmo_snapshot;const auto& own=nb_at(shown,"selfState");const auto mode=nb_string(shown,"modeId");
        const auto& view=nb_at(nb_at(shown,"dtoProjection"),"view");
        if(nb_string(shown,"roomId")!=native_multiplayer_transport_room()||nb_string(shown,"roomId").empty()||
           !cnmp::mode(mode)||nb_string(own,"activityId")!="act2autochess"||nb_string(own,"modeId")!=mode||
           nb_string(own,"runId").empty()||!cncombat::stage_allowed(nb_string(own,"stageId"))||nb_number(shown,"seat")!=nmo_self||
           nb_string(latest,"roomId")!=nb_string(shown,"roomId")||nb_number(latest,"seat")!=nb_number(shown,"seat")||
           nb_number(latest,"matchSequence")!=nb_number(shown,"matchSequence")||nb_string(latest,"modeId")!=mode||
           nb_string(nb_at(latest,"selfState"),"runId")!=nb_string(own,"runId")||nmo_observed<0||nmo_observed>3||
           nb_number(view,"recipientSeat")!=nmo_self||nb_number(view,"observedSeat")!=nmo_observed)return false;
        auto manager=nb_manager(),dc=nb_centre(),bridge=native_multiplayer_dispatch_bridge();
        if(!manager||!dc||!bridge||!native_battle_manager_root||hold_get(native_battle_manager_root)!=manager||
           api.il2cpp_object_get_class(manager)!=cls("Torappu.Battle.AutoChess.AutoChessGameManager")||
           api.il2cpp_object_get_class(dc)!=cls("Torappu.Battle.DataCenter.AutoChessDataCenter")||
           api.il2cpp_object_get_class(bridge)!=cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer")||
           field_object(manager,"dataCenter")!=dc||field_object(manager,"dataBridge")!=bridge||
           !act(field_object(dc,"<actId>k__BackingField"))||text(field_object(dc,"<modeId>k__BackingField"))!=mode)return false;
        uint8_t local=1;api.il2cpp_field_get_value(manager,npt_field(manager,"<isTrainingMode>k__BackingField","System.Boolean"),&local);
        return local==0;
    }
    if(!can_read()||!private_view.load()||!private_battle_resources.load()||!nb_prepared.load()||!nb_started.load()||
       !native_battle_launch_session_prepared.load()||!native_single_mode_captured||
       unity_ui_thread.load()!=GetCurrentThreadId()||nb_run_id.empty()||nb_state.kind!=cnjson::Value::Object)return false;
    auto mode=nb_string(nb_state,"modeId");
    if(nb_string(nb_state,"activityId")!="act2autochess"||nb_string(nb_state,"runId")!=nb_run_id||
       mode!=nb_selected_mode||!cncombat::stage_allowed(nb_string(nb_state,"stageId"),mode))return false;
    auto manager=nb_manager(),dc=nb_centre(),bridge=nb_retained("native-battle-local-player");
    if(!manager||!dc||!bridge||!native_battle_manager_root||hold_get(native_battle_manager_root)!=manager||
       api.il2cpp_object_get_class(manager)!=cls("Torappu.Battle.AutoChess.AutoChessGameManager")||
       api.il2cpp_object_get_class(dc)!=cls("Torappu.Battle.DataCenter.AutoChessDataCenter")||
       api.il2cpp_object_get_class(bridge)!=cls("Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer")||
       field_object(manager,"dataCenter")!=dc||field_object(manager,"dataBridge")!=bridge||
       !act(field_object(dc,"<actId>k__BackingField"))||text(field_object(dc,"<modeId>k__BackingField"))!=mode)return false;
    uint8_t local=0;api.il2cpp_field_get_value(manager,npt_field(manager,"<isTrainingMode>k__BackingField","System.Boolean"),&local);
    return local!=0;
}
std::string npt_current_run(){
    if(native_multiplayer_entry_active())return "multi:"+nb_string(nmo_snapshot,"roomId")+":"+
        std::to_string(nb_number(nmo_snapshot,"matchSequence"))+":"+nb_string(nb_at(nmo_snapshot,"selfState"),"runId");
    return nb_run_id;
}
void npt_require_run(){
    const auto run=npt_current_run();if(npt_run.empty())npt_run=run;
    if(npt_run!=run)throw std::runtime_error("private preparation token run changed without reset");
}
std::set<int32_t> npt_owner_ids(){
    std::set<int32_t> result;
    const auto collect=[&](const cnjson::Value& rows){
        if(rows.kind!=cnjson::Value::Array||rows.array.size()>128)throw std::runtime_error("private preparation token owner list invalid");
        for(const auto& row:rows.array){auto id=nb_number(row,"instId");if(id<=0||id>INT32_MAX||!result.insert(int32_t(id)).second)throw std::runtime_error("private preparation token owner id invalid");}
    };
    if(native_multiplayer_entry_active()){
        // GetChessSquad resolves in the current presented player's view.
        // SINGLE nb_state is unrelated to MULTI and may contain a previous run.
        const auto& all=nb_at(nb_at(nb_at(nb_at(nmo_snapshot,"dtoProjection"),"allStateSync"),"payload"),"allStateSyncData");
        const auto& scene=nb_at(all,"sceneDetail");const auto& prep=nb_at(scene,"preparationStatus");
        if(prep.kind==cnjson::Value::Object){
            if(nb_number(prep,"uidIndex")!=nmo_observed)throw std::runtime_error("private preparation token viewed owner mismatch");
            collect(nb_at(prep,"charChess"));return result;
        }
        // HELP has no standalone preparationStatus. Its left-first paired
        // deployments carry their own owner; BOSS uses the same owned rows.
        // Never substitute the actor-only preparation response while watching.
        if(prep.kind!=cnjson::Value::Null)throw std::runtime_error("private preparation token presentation invalid");
        const auto phase=nb_string(nmo_snapshot,"phase");
        if(phase!="help"&&phase!="boss")throw std::runtime_error("private preparation token presentation missing");
        const auto& players=nb_at(nb_at(scene,phase=="help"?"helpBattleInfo":"bossBattleInfo"),"players");
        if(players.kind!=cnjson::Value::Array||players.array.empty()||players.array.size()>2)throw std::runtime_error("private preparation token player list invalid");
        std::set<int64_t> seats;
        for(const auto& player:players.array){const auto& deployment=phase=="help"?nb_at(player,"deployment"):player;auto seat=nb_number(deployment,"uidIndex");if(seat<0||seat>3||!seats.insert(seat).second)throw std::runtime_error("private preparation token player owner invalid");if(seat==nmo_observed)collect(nb_at(deployment,"charChess"));}
        if(!seats.count(nmo_observed))throw std::runtime_error("private preparation token viewed owner missing");
    }else collect(nb_at(nb_at(nb_state,"player"),"charChess"));
    return result;
}
bool npt_owned_character(int32_t id){return npt_owner_ids().count(id)!=0;}
void* npt_common(TemporaryRoots& roots){
    // ConstTable<ActivityTable,ActivityDB>.get_data is inherited and generic;
    // the ordinary method resolver must search the actual closed parent type.
    auto getter=method(cls("Torappu.ActivityDB"),"get_data",0);uint32_t impl=0;
    if(!getter||!(api.il2cpp_method_get_flags(getter,&impl)&16)||
       !matches_type(api.il2cpp_method_get_return_type(getter),"Torappu.ActivityTable"))
        throw std::runtime_error("private preparation token ActivityDB getter contract");
    auto table=roots.retain(invoke(getter,nullptr));
    if(!table||api.il2cpp_object_get_class(table)!=cls("Torappu.ActivityTable"))throw std::runtime_error("private preparation token ActivityTable missing");
    npt_field(table,"autoChessData","Torappu.AutoChessData");auto common=roots.retain(field_object(table,"autoChessData"));
    if(!common||api.il2cpp_object_get_class(common)!=cls("Torappu.AutoChessData"))throw std::runtime_error("private preparation token common data missing");
    npt_field(common,"shopStateTokenDict","System.Collections.Generic.Dictionary`2<System.String,Torappu.AutoChessData+AutoChessShopStateTokenData>");
    auto dictionary=roots.retain(field_object(common,"shopStateTokenDict"));
    if(!dictionary)throw std::runtime_error("private preparation token display dictionary missing");
    return dictionary;
}
bool npt_explicit(void* dictionary,void* key,TemporaryRoots& roots){
    auto contains=method(api.il2cpp_object_get_class(dictionary),"ContainsKey",1);uint32_t impl=0;
    if(!contains||(api.il2cpp_method_get_flags(contains,&impl)&16)||
       !matches_type(api.il2cpp_method_get_return_type(contains),"System.Boolean")||
       !matches_type(api.il2cpp_method_get_param(contains,0),"System.String"))
        throw std::runtime_error("private preparation token dictionary lookup contract");
    void* args[]={key};auto boxed=roots.retain(invoke(contains,dictionary,args));uint8_t value=0;
    if(!boxed||!read(unbox(boxed),value))throw std::runtime_error("private preparation token display lookup result missing");
    return value!=0;
}
void npt_failure(const std::exception& error){
    // Preserve the first contract failure. Repeated Ready attempts must not
    // recursively wrap this saved error or flood diagnostics inside one run.
    if(npt_failed.exchange(true))return;
    ++npt_failures;npt_error=error.what();nb_error("native-preparation-tokens",error);
}
uint8_t npt_query(int32_t id,const void* info){
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load()){lease.release();return npt_original_query(id,info);}
    // Preserve original false and original exceptions, including foreign calls.
    auto native=npt_original_query(id,info);if(!native)return native;
    try{
        if(!npt_scene()){++npt_forwarded;return native;}
        std::lock_guard<std::recursive_mutex> lock(npt_lock);npt_require_run();
        if(!npt_owned_character(id)){++npt_forwarded;return native;}
        ++npt_queries;if(npt_failed.load())return 0;
        TemporaryRoots roots;void* squad_args[]={&id};auto squad=roots.retain(invoke(bound_methods.at(NPT_SQUAD.token),nullptr,squad_args));
        if(!squad||api.il2cpp_object_get_class(squad)!=cls("Torappu.Battle.AutoChess.ChessSquad"))throw std::runtime_error("private preparation token squad missing");
        npt_field(squad,"inst","Torappu.AdvancedCharacterInst");auto slot=roots.retain(field_object(squad,"inst"));
        if(!slot||api.il2cpp_object_get_class(slot)!=cls("Torappu.AdvancedCharacterInst"))throw std::runtime_error("private preparation token squad inst missing");
        uint8_t is_token=0,assist=0;int32_t side=0;
        // runtime_invoke reference arguments are object pointers, not &slot.
        void* convert_args[]={slot,&is_token,&assist,&side,nullptr};
        auto host=roots.retain(invoke(bound_methods.at(NPT_CONVERT.token),nullptr,convert_args));
        if(!host||api.il2cpp_object_get_class(host)!=cls("Torappu.Battle.BattleCharacterData"))throw std::runtime_error("private preparation token host conversion missing");
        uint8_t token_host=0;api.il2cpp_field_get_value(host,npt_field(host,"isToken","System.Boolean"),&token_host);
        npt_field(host,"tokenOrHostKey","System.String");auto key=roots.retain(field_object(host,"tokenOrHostKey"));auto value=text(key);
        if(token_host||value.empty()||value.size()>256)throw std::runtime_error("private preparation token original host contract changed");
        auto dictionary=npt_common(roots);bool explicit_entry=npt_explicit(dictionary,key,roots);
        // No saved TLS/owner decisions: a nested foreign scene cannot borrow
        // the outer private scene. Recheck ownership after managed consumers.
        if(!npt_scene()||npt_run!=npt_current_run()||!npt_owned_character(id))throw std::runtime_error("private preparation token scene changed during conversion");
        if(explicit_entry||!cntokenprepare::classified_marker_key(value))return native;
        if(npt_suppressed.fetch_add(1)==0)report("private-native-preparation-hidden-marker-suppressed");
        return 0;
    }catch(const std::exception& error){std::lock_guard<std::recursive_mutex> lock(npt_lock);npt_failure(error);return 0;}
}
void native_preparation_tokens_preflight(){
    RuntimeLease lease(runtime_gate);std::lock_guard<std::recursive_mutex> lock(npt_lock);++npt_preflights;
    try{
        if(!lease||closing.load()||!npt_setup_ready.load()||(!native_multiplayer_entry_active()&&!nb_loaded.load())||!npt_scene())throw std::runtime_error("private preparation token Ready scope incomplete");
        npt_require_run();if(npt_failed.load())throw std::runtime_error("private preparation token prior contract failure: "+npt_error);
        TemporaryRoots roots;npt_common(roots);
        for(auto id:npt_owner_ids())npt_query(id,nullptr);
        if(npt_failed.load())throw std::runtime_error("private preparation token Ready contract failed: "+npt_error);
    }catch(const std::exception& error){npt_failure(error);throw;}
}
void native_preparation_tokens_reset(){std::lock_guard<std::recursive_mutex> lock(npt_lock);npt_failed=false;npt_run.clear();npt_error.clear();}
std::string native_preparation_tokens_status_json(){
    std::unique_lock<std::recursive_mutex> lock(npt_lock,std::try_to_lock);
    std::string value="{\"setup\":"+std::string(npt_setup_ready.load()?"true":"false")+",\"failed\":"+(npt_failed.load()?"true":"false")+
        ",\"queries\":"+std::to_string(npt_queries.load())+",\"forwarded\":"+std::to_string(npt_forwarded.load())+
        ",\"suppressed\":"+std::to_string(npt_suppressed.load())+",\"preflights\":"+std::to_string(npt_preflights.load())+
        ",\"failures\":"+std::to_string(npt_failures.load())+",\"snapshotBusy\":"+(lock.owns_lock()?"false":"true");
    if(lock.owns_lock())value+=",\"runId\":"+quote(npt_run)+",\"lastError\":"+quote(npt_error);
    return value+"}";
}
void native_preparation_tokens_setup(){
    // NB_DATA_METHODS may already have bound the predicate. nb_hook still
    // installs its one owner; the generic hook helper would skip a bound token.
    nb_hook(0x601351a,npt_query,npt_original_query);bind_method(NPT_SQUAD);bind_method(NPT_CONVERT);npt_setup_ready=true;
}
