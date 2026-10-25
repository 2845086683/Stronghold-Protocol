// Inside cn, after native_battle_launch.inc. This uses the existing native
// continue/give-up panels, and never opens the server address/token as a socket.
#pragma once
inline constexpr CN_Method CN_PRESENCE_RECONNECT={"Torappu.UI.AutoChess.AutoChessPrepareController","ReconnectBattle",0x602b3df,0x2721180ULL,1,false,"System.Void",{"Torappu.Activity.AutoChess.ActAutoChessSyncInfoBattleInfo"}};
inline constexpr CN_Method CN_PRESENCE_QUIT={"Torappu.Activity.AutoChess.ActAutoChessActivityController","OnSingleGameSettle",0x6031c11,0x2e32710ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_PRESENCE_START_LOCAL={"Torappu.UI.AutoChess.AutoChessPrepareController","_StartTrainingBattle",0x602b42f,0x2729f30ULL,0,false,"System.Void",{}};
using NativePresenceReconnect=void(*)(void*,void*,const void*);
NativePresenceReconnect nsp_old_reconnect=nullptr;
NativeBattleVoid nsp_old_quit=nullptr;
std::atomic<unsigned> nsp_syncs{0},nsp_reconnects{0},nsp_quits{0},nsp_failures{0},nsp_guard_rejections{0};
std::atomic<unsigned> nsp_direct_local_starts{0};
thread_local bool nsp_joining=false;
inline constexpr char NSP_ADDRESS[]="stronghold-private-local",NSP_TOKEN[]="stronghold-private-session";

bool nsp_run_id(const std::string& value){
    if(value.size()!=32)return false;
    for(char c:value)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))return false;
    return true;
}
void nsp_require_info(const cnjson::Value& value){
    if(value.kind!=cnjson::Value::Object||value.object.size()!=6||
       !nsp_run_id(nb_string(value,"sceneId"))||nb_string(value,"address")!=NSP_ADDRESS||
       nb_string(value,"token")!=NSP_TOKEN||!cncombat::mode_allowed(nb_string(value,"modeId"))||
       nb_number(value,"curRound")<0||nb_number(value,"curRound")>15||nb_number(value,"endTime")<=0)
        throw std::runtime_error("private session presence battle identity invalid");
}
cnjson::Value nsp_request(const wchar_t* route,const cnjson::Value& request){
    native_battle_launch_require_ui();const auto body=cnjson::dump(request);
    auto response=cnjson::parse(private_http(route,&body,true));
    if(response.kind!=cnjson::Value::Object||nb_number(response,"result")!=0)
        throw std::runtime_error("private session presence request not acknowledged");
    return response;
}
cnjson::Value nsp_sync(){
    auto request=nb_object();request.object["activityId"]=json_text(NATIVE_BATTLE_ACTIVITY);
    auto response=native_multiplayer_entry_sync();
    if(response.kind==cnjson::Value::Null)response=nsp_request(L"/activity/autochessSeason/syncInfo",request);
    const auto& changed=nb_at(response,"changed");const auto& info=nb_at(response,"battleInfo");
    if(changed.kind!=cnjson::Value::Array||changed.array.size()>4096)throw std::runtime_error("private session presence changed list invalid");
    for(const auto& row:changed.array)if(row.kind!=cnjson::Value::String)throw std::runtime_error("private session presence changed key invalid");
    if(info.kind!=cnjson::Value::Null&&!native_multiplayer_entry_presence(info))nsp_require_info(info);
    auto wire=nb_object();wire.object["changed"]=changed;wire.object["battleInfo"]=info;
    ++nsp_syncs;return wire;
}
void* native_session_presence_sync_response(){
    auto wire=nsp_sync();auto spec=bound_methods.at(CN_METHOD_handle_sync.token);
    auto response_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(spec,0));
    if(!response_class)throw std::runtime_error("private native sync response class missing");
    const std::string key="native-session-presence-sync";
    return json(response_class,cnjson::dump(wire),&key);
}
bool nsp_private_controller(void* self,const char* type,const char* identity_field){
    if(!self||api.il2cpp_object_get_class(self)!=cls(type))throw std::runtime_error("private session presence controller class mismatch");
    return native_battle_prepare_activity(self,identity_field)==NATIVE_BATTLE_ACTIVITY;
}
struct NativePresenceJoinScope{
    NativePresenceJoinScope(){if(nsp_joining)throw std::runtime_error("private native reconnect is already initializing");nsp_joining=true;}
    ~NativePresenceJoinScope(){nsp_joining=false;}
};
struct NativePresenceSavedRunScope{
    std::string previous;
    explicit NativePresenceSavedRunScope(const std::string& run):previous(nb_resume_expected_run){
        if(!previous.empty()||!nsp_run_id(run))throw std::runtime_error("private saved-run initialization scope invalid");
        nb_resume_expected_run=run;
    }
    ~NativePresenceSavedRunScope(){nb_resume_expected_run=std::move(previous);}
};
void nsp_reconnect(void* self,void* supplied,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()){lease.release();nsp_old_reconnect(self,supplied,info);return;}
    try{
        if(!nsp_private_controller(self,CN_PRESENCE_RECONNECT.type,"m_actId")){lease.release();nsp_old_reconnect(self,supplied,info);return;}
        if(native_multiplayer_entry_reconnect(self,supplied))return;
        native_battle_launch_require_ui();NativePresenceJoinScope joining;
        if(native_battle_launch_in_flight.load()||native_battle_launch_pending.load())throw std::runtime_error("a private native scene is already active or pending");
        if(!supplied||api.il2cpp_object_get_class(supplied)!=cls("Torappu.Activity.AutoChess.ActAutoChessSyncInfoBattleInfo"))
            throw std::runtime_error("private native reconnect battle info class mismatch");
        auto wire=nsp_sync();const auto& current=nb_at(wire,"battleInfo");nsp_require_info(current);
        if(text(field_object(supplied,"sceneId"))!=nb_string(current,"sceneId")||
           text(field_object(supplied,"modeId"))!=nb_string(current,"modeId")||
           text(field_object(supplied,"address"))!=NSP_ADDRESS||text(field_object(supplied,"token"))!=NSP_TOKEN)
            throw std::runtime_error("private native reconnect does not match the current saved scene");
        auto saved=nb_http(L"/bridge/act2autochess/native-session");const auto& state=nb_at(saved,"state");
        if(!nb_true(saved,"ok")||nb_string(state,"activityId")!=NATIVE_BATTLE_ACTIVITY||
           nb_string(state,"runId")!=nb_string(current,"sceneId")||nb_string(state,"modeId")!=nb_string(current,"modeId")||
           nb_string(state,"phase")=="result"||!cncombat::stage_allowed(nb_string(state,"stageId"),nb_string(state,"modeId"))||
           (state.object.count("nativeBandConfirmed")&&!nb_true(state,"nativeBandConfirmed")))
            throw std::runtime_error("private native reconnect authoritative run mismatch");
        const auto saved_band=nb_string(state,"bandId");if(saved_band.empty())throw std::runtime_error("private native reconnect has no saved strategy");
        if(!native_single_old_training||!bound_methods.count(CN_PRESENCE_START_LOCAL.token))throw std::runtime_error("private native reconnect LOCAL entry unavailable");
        NativePresenceSavedRunScope exact_run(nb_string(state,"runId"));
        TemporaryRoots roots;roots.retain(self);roots.retain(supplied);
        const auto previous_mode=nb_selected_mode;const bool previous_capture=native_single_mode_captured,previous_choices=native_single_team_choices_ready;
        nb_selected_mode=nb_string(state,"modeId");native_single_mode_captured=true;native_single_team_choices_ready=false;
        PrivateDataScope private_scope(true);
        try{native_single_old_training(self,bound_methods.at(CN_SINGLE_START_TRAINING.token));}
        catch(...){nb_selected_mode=previous_mode;native_single_mode_captured=previous_capture;native_single_team_choices_ready=previous_choices;throw;}
        // StartTraining's synchronous callback precedes its TeamData ctor.
        // Launch only after the complete original call returned and its wrapper
        // has marked team_ready. The saved band is already authoritative.
        if(!native_battle_launch_team_ready.load()||!native_battle_launch_session_prepared.load()||
           nb_string(native_battle_session_state(),"runId")!=nb_string(current,"sceneId")||
           nb_string(native_battle_session_state(),"modeId")!=nb_string(current,"modeId")||
           nb_string(native_battle_session_state(),"stageId")!=nb_string(state,"stageId")||
           nb_number(native_battle_session_state(),"revision")<nb_number(state,"revision")||
           nb_string(native_battle_session_state(),"phase")=="result"||
           (native_battle_session_state().object.count("nativeBandConfirmed")&&!nb_true(native_battle_session_state(),"nativeBandConfirmed"))||
           nb_string(native_battle_session_state(),"bandId")!=saved_band)
            throw std::runtime_error("private native reconnect LOCAL initialization did not retain the saved run");
        native_prebattle_select_band(saved_band.c_str());
        // The ordinary ChooseBand path queues _StartTrainingBattle behind the
        // prebattle AVG latch. A reconnect has already committed its strategy,
        // so call that original LOCAL launch directly. No ChooseStrategy event,
        // second private band transaction or deferred duplicate launch is made.
        invoke(bound_methods.at(CN_PRESENCE_START_LOCAL.token),self);
        if(!native_battle_launch_in_flight.load()&&!native_battle_launch_pending.load())
            throw std::runtime_error("private native reconnect LOCAL launch did not start");
        ++nsp_direct_local_starts;
        ++nsp_reconnects;report("private-native-saved-session-reconnect-routed-to-local");
    }catch(const std::exception& ex){++nsp_failures;native_battle_launch_reject(ex.what());}
}
// Called only for AutoChessService.StartReconnectBattle by guarded_start.
// The private UI hook above initializes LOCAL directly. Any other act2 call is
// consumed here instead of opening an official/multiplayer transport.
bool native_session_presence_reconnect(void* raw_entry,void* param,const void*){
    if(!param||!act(field_object(param,"activityId")))return false;
    ++nsp_guard_rejections;cnlaunch::CommonJoinEntryRaw entry;
    if(read(raw_entry,entry)&&entry.on_join_done)try{cancel_join(reinterpret_cast<void*>(uintptr_t(entry.on_join_done)));}catch(...){}
    native_battle_launch_reject("private reconnect must use the validated saved-session UI entry");return true;
}
void native_session_presence_open_result(const std::string& run);
void nsp_quit(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()){lease.release();nsp_old_quit(self,info);return;}
    try{
        if(!controller_target(self)){lease.release();nsp_old_quit(self,info);return;}
        if(api.il2cpp_object_get_class(self)!=cls(CN_PRESENCE_QUIT.type))throw std::runtime_error("private give-up controller class mismatch");
        native_battle_launch_require_ui();auto wire=nsp_sync();const auto& current=nb_at(wire,"battleInfo");
        std::string abandoned_run;
        if(current.kind!=cnjson::Value::Null){
            abandoned_run=nb_string(current,"sceneId");
            auto request=nb_object();request.object["activityId"]=json_text(NATIVE_BATTLE_ACTIVITY);request.object["sceneId"]=nb_at(current,"sceneId");
            auto response=nsp_request(L"/activity/autochessSeason/quitSingleGame",request);
            if(nb_at(response,"battleInfo").kind!=cnjson::Value::Null)throw std::runtime_error("private give-up response still contains an active scene");
        }
        if(!native_battle_launch_in_flight.load()&&!native_battle_launch_pending.load()){
            native_battle_session_cancel();native_single_mode_captured=false;native_single_team_choices_ready=false;
        }
        // Keep the native confirmation dialog, but do not call its original
        // success callback: that callback opens another Prepare page.
        TemporaryRoots roots;auto response=roots.retain(native_session_presence_sync_response());void* args[]={response};
        PrivateDataScope scope(true);invoke(bound_methods.at(CN_METHOD_handle_sync.token),self,args);
        // Open this exact acknowledged abandonment now. The existing result
        // loader records presentation only after its native DTO is constructed.
        if(!abandoned_run.empty())native_session_presence_open_result(abandoned_run);
        ++nsp_quits;report("private-native-saved-session-abandoned-and-entry-refreshed");
    }catch(const std::exception& ex){++nsp_failures;report((std::string("private-native-session-give-up-failed: ")+ex.what()).c_str());}
}
std::string native_session_presence_status_json(){
    return "{\"syncs\":"+std::to_string(nsp_syncs.load())+",\"reconnects\":"+std::to_string(nsp_reconnects.load())+
        ",\"quits\":"+std::to_string(nsp_quits.load())+",\"directLocalStarts\":"+std::to_string(nsp_direct_local_starts.load())+",\"guardRejections\":"+std::to_string(nsp_guard_rejections.load())+
        ",\"failures\":"+std::to_string(nsp_failures.load())+"}";
}
void native_session_presence_setup(){
    hook(CN_PRESENCE_RECONNECT,nsp_reconnect,nsp_old_reconnect);
    hook(CN_PRESENCE_QUIT,nsp_quit,nsp_old_quit);
    bind_method(CN_PRESENCE_START_LOCAL);
}
