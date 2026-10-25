// Concrete candidate inside namespace cn, after transport/entry/help/observation
// and before mode_runes. Global header: native_multiplayer_own_start_policy.h.
// Reuses 0x6013d15 only. No additional hook and no worker managed operations.
#pragma once
bool native_multiplayer_preparation_view_may_start_own_battle();
bool native_multiplayer_preparation_view_may_start_observed_battle(const cnjson::Value&);
bool native_multiplayer_transport_live();
bool native_multiplayer_field_transition_pending();

namespace native_multi_own_start {
using namespace cnmulti_own_start;
std::atomic<unsigned> retirements{0},retiredCallbacks{0},failures{0};
std::mutex errorMutex;std::string firstError,lastError;
struct Heap { Gate gate; Heap():gate({
    [](){return native_multiplayer_transport_live()&&!native_multiplayer_field_transition_pending()&&(cndead_battle::valid(nmo_snapshot,true)?native_multiplayer_preparation_view_may_start_observed_battle(nmo_snapshot):native_multiplayer_preparation_view_may_start_own_battle());},
    [](void* value){return hold_new(value,false);},
    [](uint32_t handle){return hold_get(handle);},
    [](uint32_t handle){hold_free(handle);},
    [](void* value){return hold_weak(value,false);}}){} };
// Deliberately heap-owned, never a process-static managed-root destructor.
Heap*& storage(){static Heap* value=nullptr;return value;}
Heap& heap(){auto& value=storage();if(!value)value=new Heap;return *value;}
void release(){auto& value=storage();if(value){value->gate.reset();delete value;value=nullptr;}}
const cnjson::Value& at(const cnjson::Value& v,const char* name){return cnmp::at(v,name);}
std::string str(const cnjson::Value& v,const char* name){return cnmp::text(v,name);}
int64_t num(const cnjson::Value& v,const char* name){return cnmp::integer(v,name);}
bool flag(const cnjson::Value& v,const char* name,bool fallback=false){auto p=v.object.find(name);if(p==v.object.end())return fallback;if(p->second.kind!=cnjson::Value::Boolean)throw std::runtime_error("own start Boolean field differs");return p->second.boolean;}
int32_t scalar(void* object,const char* name,const char* type){
    if(!object)throw std::runtime_error("own start native field owner missing");
    auto f=profile_field(api.il2cpp_object_get_class(object),name);
    if(!matches_type(api.il2cpp_field_get_type(f),type))throw std::runtime_error("own start native scalar ABI differs");
    int32_t value=0;api.il2cpp_field_get_value(object,f,&value);return value;
}
Owner owner(void* iterator){
    auto task=field_object(iterator,"<>4__this");auto actual=task?api.il2cpp_object_get_class(task):nullptr;
    if(actual==cls("Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInBattle"))return Owner::Normal;
    if(actual==cls("Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInBossBattle"))return Owner::Boss;
    if(actual==cls("Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle"))return Owner::Help;
    return Owner::Unknown;
}
struct Authority { Stamp stamp;std::string mode,phase;bool waiting=false; };
void verify_binding(const cnjson::Value& view,const std::string& participant,bool active){
    if(!flag(view,"ok")||flag(view,"officialFallback",true)||str(view,"activityId")!="act2autochess")throw std::runtime_error("own start lacks private activity acknowledgement");
    const auto seat=num(view,"seat");if(seat<0||seat>3)throw std::runtime_error("own start actor seat invalid");
    const auto& roster=at(view,"roster");if(roster.kind!=cnjson::Value::Array||roster.array.empty()||roster.array.size()>4)throw std::runtime_error("own start roster invalid");
    unsigned found=0;for(const auto& row:roster.array)if(num(row,"seat")==seat&&str(row,"participantId")==participant){if(active&&(flag(row,"quit")||flag(row,"left")||flag(row,"teamLeft")))throw std::runtime_error("own start actor left this game");++found;}
    if(found!=1||str(at(view,"dtoProjection"),"privateSelfUid")!="private-"+participant)throw std::runtime_error("own start participant/seat/UID binding differs");
}
void verify_actor(const cnjson::Value& view,const std::string& participant){
    verify_binding(view,participant,true);
    if(cndead_battle::valid(view))return; // Presentation role; never a settlement authority.
    const auto& self=at(view,"selfState");const auto& hp=at(at(self,"player"),"hp");
    if(hp.kind!=cnjson::Value::Integer||hp.integer<=0||num(self,"round")!=num(view,"round")||str(self,"runId").empty()||flag(view,"observerOnly"))throw std::runtime_error("own start is not a live current-round actor");
}
Authority authority(bool starting){
    // Snapshot is copied under the transport lock; token is validated in place
    // and is never copied into a report, callback payload or native DTO.
    cnjson::Value latest;std::string participant,room;int64_t visible=0;
    {std::lock_guard<std::mutex> lock(nmp_mutex);
        if(!nmp_state.enrolled()||!cnmp::hex_id(nmp_state.participant)||!cnmp::token_valid(nmp_state.token)||nmp_state.origin!=private_backend_origin())throw std::runtime_error("own start transport identity is not endpoint-bound");
        participant=nmp_state.participant;room=nmp_state.room;latest=nmp_state.latest;visible=nmp_state.visible_generation;
    }
    if(!cnmp::hex_id(room)||room!=native_multi_entry_room||!native_multiplayer_entry_active())throw std::runtime_error("own start service/transport room differs");
    const auto& shown=nmo_snapshot;
    verify_actor(shown,participant);
    // Workers can fetch a newer authoritative phase before Unity presents it.
    // Only state0 grants a new start. A granted iterator resumes against its
    // displayed world until the UI projection explicitly retires that lease.
    if(starting)verify_actor(latest,participant);else verify_binding(latest,participant,false);
    if(str(latest,"roomId")!=room||str(shown,"roomId")!=room||num(shown,"generation")>visible||num(latest,"generation")<num(shown,"generation"))throw std::runtime_error("own start room view was not presented");
    bool projectionPending=false;
    if(starting){
        for(const auto* key:{"matchSequence","round","seat"})if(num(latest,key)!=num(shown,key))throw std::runtime_error("own start displayed domain owner is stale");
        if(str(latest,"modeId")!=str(shown,"modeId"))throw std::runtime_error("own start displayed phase/mode is stale");
        if(str(latest,"phase")!=str(shown,"phase")){
            const auto phase=str(shown,"phase"),next=str(latest,"phase");
            projectionPending=(phase=="preparation"||phase=="special"||phase=="battle_countdown")&&(next=="preparation"||next=="battle_countdown"||next=="battle"||next=="boss");
            if(!projectionPending)throw std::runtime_error("own start displayed phase/mode is stale");
        }
        if(cndead_battle::execution_run(latest)!=cndead_battle::execution_run(shown))throw std::runtime_error("own start displayed run is stale");
    }
    Authority a;a.mode=str(shown,"modeId");a.phase=str(shown,"phase");
    if(!cnmp::mode(a.mode))throw std::runtime_error("own start mode outside private MULTI");
    a.stamp.room=room;a.stamp.run=cndead_battle::execution_run(shown);a.stamp.match=num(shown,"matchSequence");a.stamp.round=int32_t(num(shown,"round"));a.stamp.seat=int32_t(num(shown,"seat"));a.stamp.privateMulti=true;
    auto reconstruction=shown.object.find("selfReconstruction");
    a.waiting=(cndead_battle::valid(shown)&&!cndead_battle::valid(shown,true))||projectionPending||(reconstruction!=shown.object.end()&&reconstruction->second.kind==cnjson::Value::Object)||a.phase=="preparation"||a.phase=="special"||a.phase=="battle_countdown";
    if(!a.waiting&&a.phase!="battle"&&a.phase!="boss")throw std::runtime_error("own start authority is outside its battle phase");
    return a;
}
World world(void* iterator,const Authority& a,TemporaryRoots& roots){
    nb_main_thread();if(!nmh_scope()||nmo_failed)throw std::runtime_error("own start lost its native private world");
    auto service=roots.retain(native_multiplayer_entry_service()),mode=roots.retain(native_multiplayer_entry_mode());
    if(!service||!mode||field_object(service,"m_mode")!=mode)throw std::runtime_error("own start service mode root differs");
    auto task=roots.retain(field_object(iterator,"<>4__this"));
    auto controller=roots.retain(nmo_controller());auto centre=roots.retain(nb_centre()),manager=roots.retain(nb_manager());
    auto map=roots.retain(invoke(bound_methods.at(0x600fbe8),controller));
    auto bridge=roots.retain(field_object(manager,"dataBridge"));
    if(!task||!controller||!map||!manager||!centre||!bridge||field_object(manager,"dataCenter")!=centre||bridge!=native_multiplayer_dispatch_bridge()||api.il2cpp_object_get_class(bridge)!=cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer")||!act(field_object(centre,"<actId>k__BackingField")))throw std::runtime_error("own start world is not the current private MULTI owner");
    if(scalar(centre,"<selfPlayerIndex>k__BackingField","System.Int32")!=a.stamp.seat)throw std::runtime_error("own start native actor seat differs");
    auto modeData=roots.retain(field_object(centre,"<currentModeData>k__BackingField"));
    if(!modeData||api.il2cpp_object_get_class(modeData)!=cls("Torappu.ActAutoChessData+ActAutoChessModeData")||scalar(modeData,"modeType","Torappu.ActAutoChessModeType")!=2||text(field_object(modeData,"modeId"))!=a.mode||text(field_object(centre,"<modeId>k__BackingField"))!=a.mode)throw std::runtime_error("own start native mode is not authenticated MULTI");
    // No LevelData, MapData or InParams level equality here: original
    // StartBattle legitimately changes those values before later resumes.
    return {iterator,task,controller,map,manager,bridge,nullptr};
}
void yield_null(void* iterator){
    auto f=profile_field(api.il2cpp_object_get_class(iterator),"<>2__current");
    if(!matches_type(api.il2cpp_field_get_type(f),"System.Object"))throw std::runtime_error("own start yield ABI differs");
    set_object_field(iterator,f,nullptr);
}
}

// Native parent start coroutines legitimately remain yielded through combat.
// Retire from the authenticated recipient projection before native state events
// create a next-round iterator. A retired callback completes without becoming a
// COMMON Rune failure and without releasing a different current iterator lease.
void native_multiplayer_own_start_observe(const cnjson::Value& view){
    nb_main_thread();using namespace native_multi_own_start;auto h=storage();if(!h||(!h->gate.leased()&&!h->gate.waiting()))return;
    std::string participant,room;
    {std::lock_guard<std::mutex> lock(nmp_mutex);
        if(!nmp_state.enrolled()||!cnmp::hex_id(nmp_state.participant)||!cnmp::token_valid(nmp_state.token)||nmp_state.origin!=private_backend_origin())throw std::runtime_error("own start retirement lacks endpoint-bound identity");
        participant=nmp_state.participant;room=nmp_state.room;
    }
    verify_binding(view,participant,false);
    if(!native_multiplayer_entry_active()||room!=native_multi_entry_room||str(view,"roomId")!=room)throw std::runtime_error("own start retirement room differs");
    Stamp current;current.room=room;current.match=num(view,"matchSequence");current.round=int32_t(num(view,"round"));current.seat=int32_t(num(view,"seat"));current.privateMulti=true;
    const auto& self=at(view,"selfState");if(self.kind==cnjson::Value::Object)current.run=cndead_battle::execution_run(view);
    const auto phase=str(view,"phase");current.nativeState=phase=="boss"?6:4;
    if(h->gate.observe(current,phase=="battle"||phase=="boss",phase=="battle"||phase=="boss"||phase=="preparation"||phase=="special"||phase=="battle_countdown"))++retirements;
}
bool native_multiplayer_own_start_cancel_retired(void* iterator){
    if(!iterator||unity_ui_thread.load()!=GetCurrentThreadId())return false;
    auto h=native_multi_own_start::storage();if(!h||!h->gate.complete_retired(iterator))return false;
    ++native_multi_own_start::retiredCallbacks;if(!h->gate.leased()&&!h->gate.waiting()&&!h->gate.retired_count())native_multi_own_start::release();return true;
}
void native_multiplayer_own_start_failure(const std::string& error){
    using namespace native_multi_own_start;std::lock_guard<std::mutex> lock(errorMutex);lastError=error.substr(0,2048);if(firstError.empty())firstError=lastError;++failures;
}
cnjson::Value native_multiplayer_own_start_status(){
    using namespace native_multi_own_start;std::lock_guard<std::mutex> lock(errorMutex);
    auto result=cnmp::object();result.object["retirements"]=cnmp::number(retirements.load());result.object["retiredCallbacks"]=cnmp::number(retiredCallbacks.load());result.object["failures"]=cnmp::number(failures.load());result.object["firstError"]=cnmp::string(firstError);result.object["lastError"]=cnmp::string(lastError);return result;
}

// Returns true only when this frame was consumed as a state0 yield.
bool native_multiplayer_own_start_before(void* iterator,int32_t state){
    nb_main_thread();using namespace native_multi_own_start;const auto kind=owner(iterator);
    if(kind==Owner::Help){if(auto h=storage())h->gate.retire();return false;}
    if(kind==Owner::Unknown)throw std::runtime_error("own start shared iterator owner not registered");
    auto auth=authority(state==0);
    auth.stamp.nativeState=kind==Owner::Boss?6:4;
    if(auth.waiting){if(state!=0)throw std::runtime_error("own start old task resumed during preparation");heap().gate.wait(kind,auth.stamp,iterator);yield_null(iterator);return true;}
    TemporaryRoots roots;roots.retain(iterator);auto physical=world(iterator,auth,roots);
    auto status=roots.retain(field_object(physical.manager,"gameStatus"));
    if(!status||api.il2cpp_object_get_class(status)!=cls("Torappu.Battle.AutoChess.AutoChessGameStatus"))throw std::runtime_error("own start native state owner differs");
    auth.stamp.nativeState=nmh_result<int32_t>(invoke(bound_methods.at(0x60136f5),status));
    if((kind==Owner::Normal&&auth.phase!="battle")||(kind==Owner::Boss&&auth.phase!="boss"))throw std::runtime_error("own start task and private phase differ");
    const auto decision=heap().gate.before(kind,state,auth.stamp,physical);
    if(decision==Decision::YieldNull){yield_null(iterator);return true;}
    if(decision==Decision::Stop)throw std::runtime_error("own start lease invalidated: "+heap().gate.failure()+"; iteratorState="+std::to_string(state)+"; nativeState="+std::to_string(auth.stamp.nativeState)+"; phase="+auth.phase+"; round="+std::to_string(auth.stamp.round));
    return false;
}
void native_multiplayer_own_start_after(uint8_t value){if(auto h=native_multi_own_start::storage())h->gate.after(value);}
// Called only while the enclosing native RuntimeLease still protects the VM.
// It releases GC roots without asking the now-closing UI model for permission.
void native_multiplayer_own_start_abort(){auto h=native_multi_own_start::storage();if(!h)return;h->gate.reset_active();if(!h->gate.retired_count())native_multi_own_start::release();}
void native_multiplayer_own_start_reset(){nb_main_thread();auto h=native_multi_own_start::storage();if(!h)return;h->gate.retire();if(!h->gate.retired_count())native_multi_own_start::release();}
// An already granted old task must not fall through the pre-existing !nmh_scope
// passthrough after losing its world. No new ownership is inferred in this path.
bool native_multiplayer_own_start_cancel_unscoped(void* iterator){if(unity_ui_thread.load()!=GetCurrentThreadId())return false;auto h=native_multi_own_start::storage();if(!h||!h->gate.owns(iterator))return false;h->gate.reset_active();if(!h->gate.retired_count())native_multi_own_start::release();return true;}
void native_multiplayer_own_start_shutdown(){
    if(!closing.load()||nmp_running.load()||nmp_worker_thread.joinable())throw std::runtime_error("own start shutdown lacks runtime/worker drain");
    native_multi_own_start::release();
}
