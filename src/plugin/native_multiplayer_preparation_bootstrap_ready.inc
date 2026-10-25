// Included after native_multiplayer_entry_apply. This replays the existing
// registered start/ready events; native latch timing and MULTI stage guards
// remain authoritative. Filling BattleInfo before StartBattle suppresses its
// original evt5, so the private adapter must preserve that initialization too.
std::string nl_sha256(const std::vector<uint8_t>&);
struct NativeMultiBootstrapReady {
    uint64_t epoch=0;int64_t match=0;bool started=false,startNotified=false,notifying=false;
    std::set<std::string> attempted;
};
NativeMultiBootstrapReady native_multi_entry_bootstrap_ready;
bool native_multi_entry_bootstrap_wait_missing(){
    const auto& state=native_multi_entry_bootstrap_ready;
    if(!state.startNotified||state.started||state.notifying||!native_multiplayer_entry_active()||!native_multi_entry_controller_root)return false;
    TemporaryRoots roots;auto controller=roots.retain(hold_get(native_multi_entry_controller_root));
    if(!controller||!native_multi_entry_target(controller))return false;
    auto latch=roots.retain(field_object(controller,"m_battleStartLatch"));
    return latch&&api.il2cpp_object_get_class(latch)==cls("Torappu.LatchUtils+InvokeWhenUnlock")&&
        !native_multi_entry_number<uint8_t>(latch,"m_unlocked")&&field_object(latch,"m_callback")&&
        !field_object(controller,"m_multiStartBattleMaxWaitCoroutine");
}

std::string native_multi_entry_bootstrap_signature(const cnjson::Value& snapshot,const cnjson::Value& all){
    auto envelope=native_multi_entry_field(snapshot,"preparationView");cnjson::Value immutable;
    if(envelope&&envelope->kind==cnjson::Value::Object){
        immutable.kind=cnjson::Value::Object;
        for(const auto* name:{"planHash","attempt","targetRevision","targetProjectionSha256"}){
            auto value=native_multi_entry_field(*envelope,name);if(!value)return {};
            const bool number=std::string(name)=="attempt"||std::string(name)=="targetRevision";
            if(number){if(value->kind!=cnjson::Value::Integer||value->integer<0)return {};}
            else if(value->kind!=cnjson::Value::String||!cnfield::sha256(value->string))return {};
            immutable.object[name]=*value;
        }
    }else immutable=all;
    auto serialized=cnjson::dump(immutable);
    return nl_sha256(std::vector<uint8_t>(serialized.begin(),serialized.end()));
}

void native_multiplayer_entry_retry_scene_ready(const cnjson::Value& applied){
    if(!native_multiplayer_entry_active())return;native_battle_launch_require_ui();
    const auto origin=preparation_view::session(applied),current=preparation_view::session(native_multi_entry_snapshot);
    const uint64_t epoch=native_multi_entry_service_epoch;
    if(!epoch||!origin.valid()||!(origin==current)||origin.room!=native_multi_entry_room)return;
    TemporaryRoots roots;auto service=roots.retain(native_multiplayer_entry_service()),mode=roots.retain(native_multiplayer_entry_mode());
    if(!service||!mode)return;
    auto projection=native_multi_entry_field(applied,"dtoProjection");
    auto all=projection?native_multi_entry_field(*projection,"allStateSync"):nullptr;
    auto phase=roots.retain(field_object(service,"m_curPhase"));
    if(!native_multiplayer_phase_can_launch(native_multi_entry_string(applied,"phase"))||!all||all->kind!=cnjson::Value::Object||!native_multiplayer_dispatch_ready()||!phase||
       api.il2cpp_object_get_class(phase)!=cls("Torappu.UI.AutoChess.Server.AutoChessServiceBattlePhase"))return;
    // A party/new-match envelope may coexist with the previous world's flag.
    // Only this eligible native battle phase can observe success for its key.
    auto& state=native_multi_entry_bootstrap_ready;
    if(state.epoch!=epoch||state.match!=origin.match){state={};state.epoch=epoch;state.match=origin.match;}
    if(native_battle_launch_in_flight.load()){state.started=true;return;}
    if(state.started)return;
    if(state.notifying)return;
    // Reconnect/page routing can stop the original wait after evt5 was sent.
    // A queued Action behind a locked latch with no wait is not a live launch.
    // Rearm the original event pair only for this exact still-owned page; an
    // existing wait or an unlocked latch must never have its timer restarted.
    if(native_multi_entry_bootstrap_wait_missing()){
        state.startNotified=false;state.attempted.clear();
    }
    auto signature=native_multi_entry_bootstrap_signature(applied,*all);if(signature.empty()||!state.attempted.insert(signature).second)return;
    if(!state.startNotified){
        // _HandleSceneStartRet starts the original bounded wait coroutine.
        // Its Unlock releases _HandleSceneStartSucRet's queued launch Action.
        // Set first: original listeners may synchronously reenter this helper.
        struct Notify{bool& busy;explicit Notify(bool& value):busy(value){busy=true;}~Notify(){busy=false;}} notify(state.notifying);
        state.startNotified=true;int32_t start=5;void* args[]={&start,nullptr};invoke(bound_methods.at(CN_MULTI_ENTRY_SCENE_READY.token),mode,args);
        if(!native_multiplayer_entry_active()||native_multi_entry_service_epoch!=epoch||native_multiplayer_entry_service()!=service||native_multiplayer_entry_mode()!=mode||
           !(preparation_view::session(native_multi_entry_snapshot)==origin)||state.epoch!=epoch||state.match!=origin.match)return;
    }
    int32_t ready=6;void* args[]={&ready,nullptr};invoke(bound_methods.at(CN_MULTI_ENTRY_SCENE_READY.token),mode,args);
    if(native_multi_entry_service_epoch==epoch&&native_multiplayer_entry_service()==service&&native_multiplayer_entry_mode()==mode&&
       preparation_view::session(native_multi_entry_snapshot)==origin&&state.epoch==epoch&&state.match==origin.match&&native_battle_launch_in_flight.load())state.started=true;
}
