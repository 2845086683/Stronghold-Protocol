// Included inside namespace cn after launch/DTO helpers. All managed operations
// run on the observed Unity thread; HTTP work belongs to the transport worker.
#pragma once
#include <initializer_list>
#include <tuple>
#include "native_multiplayer_entry_bindings.inc"
#include "native_multiplayer_phase_policy.h"
#include "native_multiplayer_departure_policy.h"
#include "native_multiplayer_error_trace.h"

bool native_multiplayer_transport_create(const std::string&,bool);
bool native_multiplayer_transport_join(const std::string&);
bool native_multiplayer_transport_command(const std::string&,const cnjson::Value&);
bool native_multiplayer_enqueue_request(const std::string&,const cnjson::Value&,uint64_t);
bool native_multiplayer_dispatch_request(void*,void*);
void native_multiplayer_dispatch_apply(const cnjson::Value&);
void native_multiplayer_dispatch_reset();
void native_multiplayer_dispatch_reset_scene();
bool native_multiplayer_dispatch_ready();
double native_multiplayer_transport_time();
void native_multiplayer_transport_detach(const preparation_view::Session&);
void native_multiplayer_transport_attach(const preparation_view::Session&);
cnjson::Value native_multiplayer_transport_snapshot();
std::string native_multiplayer_transport_room();
void native_multiplayer_drain_main();
void native_multiplayer_social_resume_result(void*,const cnjson::Value&);
bool native_multiplayer_social_recover_terminal_page(void*);
void native_multiplayer_social_stage_return(const preparation_view::Session&);
void native_multiplayer_social_forget_return();
bool native_multiplayer_social_return_owner(void*);

using NativeMultiEntryOne=void(*)(void*,void*,const void*);
using NativeMultiReconnectDone=void(*)(void*,bool,const void*);
NativeMultiReconnectDone native_multi_entry_old_reconnect_done=nullptr;
using NativeMultiEntryVoid=void(*)(void*,const void*);
using NativeMultiEntryTime=uint64_t(*)(void*,const void*);
using NativeMultiRoomRefresh=uint64_t(*)(void*,void*,void*,const void*);
NativeMultiRoomRefresh native_multi_entry_old_room_refresh=nullptr;
NativeMultiEntryOne native_multi_entry_old_join=nullptr,native_multi_entry_old_server_start=nullptr,native_multi_entry_old_request=nullptr;
NativeMultiEntryVoid native_multi_entry_old_start_game=nullptr,native_multi_entry_old_destroy=nullptr;
NativeMultiEntryVoid native_multi_entry_old_service_dispose=nullptr;
NativeMultiEntryVoid native_multi_entry_old_leave_request=nullptr;
NativeMultiEntryVoid native_multi_entry_old_prepare_update=nullptr;
using NativeMultiBattleException=void(*)(void*,int32_t,int32_t,const void*);
NativeMultiBattleException native_multi_entry_old_battle_exception=nullptr;
NativeMultiEntryTime native_multi_entry_old_service_time=nullptr,native_multi_entry_old_mode_time=nullptr;
uint32_t native_multi_entry_controller_root=0,native_multi_entry_mode_root=0,native_multi_entry_service_root=0;
std::string native_multi_entry_pending,native_multi_entry_room,native_multi_entry_team_signature;
std::string native_multi_entry_first_error;
cnjson::Value native_multi_entry_snapshot;
bool native_multi_entry_join_armed=false,native_multi_entry_callback_done=false,native_multi_entry_callback_success=false;
thread_local unsigned native_multi_entry_start_depth=0;
thread_local unsigned native_multi_entry_live_callback_depth=0;
std::map<uint64_t,uint32_t> native_multi_entry_live_callbacks;
uint64_t native_multi_entry_live_cookie=uint64_t(1)<<63;
std::atomic<unsigned> native_multi_entry_joins{0},native_multi_entry_requests{0},native_multi_entry_failures{0};
std::atomic<uint64_t> native_multi_entry_service_epoch{0};uint64_t native_multi_entry_disposed_epoch=0;
thread_local void* native_multi_entry_disposing=nullptr;
thread_local void* native_multi_entry_terminal_ui_service=nullptr;
preparation_view::Session native_multi_entry_return_origin;
uint64_t native_multi_entry_return_epoch=0,native_multi_entry_return_cookie=0;
bool native_multi_entry_result_close_pending=false;
bool native_multi_entry_scene_quit_pending=false;
std::string native_multi_entry_scene_exit_key;

const cnjson::Value* native_multi_entry_field(const cnjson::Value& value,const char* key){
    if(value.kind!=cnjson::Value::Object)return nullptr;auto found=value.object.find(key);return found==value.object.end()?nullptr:&found->second;
}
std::string native_multi_entry_string(const cnjson::Value& value,const char* key){auto found=native_multi_entry_field(value,key);return found&&found->kind==cnjson::Value::String?found->string:std::string();}
bool native_multi_entry_mode_allowed(const std::string& mode){return mode=="mode_multi_funny"||mode=="mode_multi_normal"||mode=="mode_multi_hard"||mode=="mode_multi_abyss";}
void native_multiplayer_capture_display_profile(cnjson::Value& payload){
    native_battle_launch_require_ui();auto card=native_broadcast_cached_identity();
    if(card.kind!=cnjson::Value::Object||!card.object.count("nickName")||!card.object.count("nickNumber"))return;
    // PrepareModel already cached the SDK UID during ordinary login. Read that
    // field before private room startup replaces the session's actor identity.
    auto controller=native_multi_entry_controller_root?hold_get(native_multi_entry_controller_root):nullptr;
    auto model=field_object(controller,"m_model");const auto uid=text(field_object(model,"<selfUid>k__BackingField"));
    if(!uid.empty()&&uid.size()<=128&&uid.find_first_not_of("0123456789")==std::string::npos){cnjson::Value id;id.kind=cnjson::Value::String;id.string=uid;card.object["displayUid"]=std::move(id);}
    payload.object["displayProfile"]=std::move(card);
}
template<class T>T native_multi_entry_number(void* object,const char* field){T value{};api.il2cpp_field_get_value(object,profile_field(api.il2cpp_object_get_class(object),field),&value);return value;}
void native_multi_entry_root(uint32_t& root,void* value){if(root)hold_free(root);root=value?hold_new(value,false):0;if(value&&!root)throw std::runtime_error("private room native root unavailable");}
std::shared_ptr<void> native_multi_entry_retain(void* value){
    if(!value)return {};auto handle=hold_new(value,false);if(!handle)throw std::runtime_error("private retiring object root unavailable");
    return std::shared_ptr<void>(value,[handle](void*){hold_free(handle);});
}
void* native_multiplayer_entry_service(){return native_multi_entry_service_root?hold_get(native_multi_entry_service_root):nullptr;}
void* native_multiplayer_entry_mode(){return native_multi_entry_mode_root?hold_get(native_multi_entry_mode_root):nullptr;}
bool native_multiplayer_entry_active(){return !native_multi_entry_room.empty()&&native_multiplayer_entry_mode()!=nullptr;}
void native_multi_entry_battle_exception(void* server,int32_t exception,int32_t parameter,const void* info){
    // Observe only the exact participant-owned native battle server. Foreign,
    // solo, retired and off-thread calls always retain the original behavior.
    RuntimeLease lease(runtime_gate);
    if(lease&&can_read()&&!closing.load())try{
        native_battle_launch_require_ui();auto service=native_multiplayer_entry_service(),mode=native_multiplayer_entry_mode();
        if(native_multiplayer_entry_active()&&server&&service&&mode&&field_object(mode,"m_battleSvr")==server&&field_object(service,"m_mode")==mode){
            auto core=field_object(service,"m_coreImpl"),param=field_object(service,"<param>k__BackingField");
            if(core&&param&&field_object(mode,"m_serviceCore")==core&&field_object(core,"m_service")==service&&act(field_object(param,"activityId"))&&
               native_multi_entry_string(native_multi_entry_snapshot,"roomId")==native_multi_entry_room)
                cnmp_trace::native_exception(native_multi_entry_snapshot,native_multi_entry_service_epoch.load(),exception,parameter);
        }
    }catch(...){} // Never replace an original exception with a diagnostic fault.
    lease.release();native_multi_entry_old_battle_exception(server,exception,parameter,info);
}
void native_multi_entry_store_join(void* server,const cnlaunch::CommonJoinEntryRaw& raw,bool battle){
    native_battle_launch_require_ui();auto completed=raw;completed.on_join_done=0;
    // Original Join stores the base entry; TryTriggerJoinResult clears its
    // callback before invoking it. Reproduce that completed local state only.
    set_field(server,profile_field(cls("Torappu.SocketNetwork.SvrCom.ServerWithinJoin"),"m_entry"),&completed);
    if(battle)set_field(server,profile_field(cls("Torappu.UI.AutoChess.Server.AutoChessBattleServer"),"m_entry"),&completed);
}
void native_multi_entry_fail(const std::string& error){++native_multi_entry_failures;if(native_multi_entry_first_error.empty())native_multi_entry_first_error=error;native_battle_launch_reject(("private room: "+native_multi_entry_first_error+(error==native_multi_entry_first_error?"":"; "+error)).c_str());}
void native_multiplayer_entry_reset_now(){
    native_multiplayer_dispatch_reset();
    native_multi_entry_result_close_pending=false;
    native_multi_entry_scene_quit_pending=false;native_multi_entry_scene_exit_key.clear();
    native_multi_entry_return_origin={};native_multi_entry_return_epoch=native_multi_entry_return_cookie=0;
    for(const auto& value:native_multi_entry_live_callbacks)hold_free(value.second);native_multi_entry_live_callbacks.clear();
    native_multi_entry_join_armed=false;native_multi_entry_callback_done=false;native_multi_entry_callback_success=false;native_multi_entry_pending.clear();native_multi_entry_room.clear();native_multi_entry_team_signature.clear();native_multi_entry_snapshot={};
    native_multi_entry_root(native_multi_entry_controller_root,nullptr);native_multi_entry_root(native_multi_entry_mode_root,nullptr);native_multi_entry_root(native_multi_entry_service_root,nullptr);
}
void native_multiplayer_entry_reset(){
    if(native_multiplayer_preparation_view_finalizing()||!native_multiplayer_entry_active()){native_multiplayer_entry_reset_now();return;}
    const auto origin=preparation_view::session(native_multi_entry_snapshot);const uint64_t epoch=native_multi_entry_service_epoch;
    if(!native_multiplayer_preparation_view_retire(origin,[origin,epoch](){
        if(epoch!=native_multi_entry_service_epoch)throw std::runtime_error("private entry reset service epoch is stale");
        if(native_multiplayer_entry_active()&&!(preparation_view::session(native_multi_entry_snapshot)==origin))throw std::runtime_error("private entry reset no longer owns its session");
        native_multiplayer_entry_reset_now();
    },"entry-reset:"+std::to_string(epoch)))throw std::runtime_error("private entry reset lacks a retiring scene owner");
}
bool native_multi_entry_target(void* controller){return controller&&act(field_object(controller,"m_actId"));}
bool native_multi_entry_return_snapshot(const cnjson::Value& snapshot){
    const auto phase=native_multi_entry_string(snapshot,"phase");if(phase=="building")return true;if(phase!="result")return false;
    auto party=native_multi_entry_field(snapshot,"partyReturn"),seat=native_multi_entry_field(snapshot,"seat");if(!party||!seat||seat->kind!=cnjson::Value::Integer)return false;
    auto returned=native_multi_entry_field(*party,"returnedSeats");if(!returned||returned->kind!=cnjson::Value::Array)return false;
    for(const auto& value:returned->array)if(value.kind==cnjson::Value::Integer&&value.integer==seat->integer)return true;return false;
}
void native_multi_entry_capture(void* self,const char* operation){
    native_battle_launch_require_ui();if(!native_multi_entry_pending.empty()||native_multiplayer_entry_active())throw std::runtime_error("a private room operation is already active");
    if(native_battle_launch_in_flight.load()||native_battle_launch_pending.load())throw std::runtime_error("a native battle is already active or pending");
    auto prepare=field_object(self,"m_model");if(!prepare||native_multi_entry_number<int32_t>(prepare,"<multiModeSubType>k__BackingField")!=2)throw std::runtime_error("custom joint simulation TEAM page is required");
    native_multiplayer_social_forget_return();
    native_multi_entry_first_error.clear();native_multi_entry_root(native_multi_entry_controller_root,self);native_multi_entry_pending=operation;
}
// Called by existing native_single_create before the solo-only mode check.
bool native_multiplayer_entry_create(void* self,void* raw_mode,bool precise,bool /*match*/,const void*){
    const auto selected=text(raw_mode);if(!native_multi_entry_mode_allowed(selected))return false;
    try{native_multi_entry_capture(self,"create");if(!native_multiplayer_transport_create(selected,precise))throw std::runtime_error("create queue unavailable");}
    catch(const std::exception& e){native_multi_entry_pending.clear();native_multi_entry_fail(e.what());}
    return true; // The custom room never submits CreateTeam to the official API.
}
void native_multi_entry_join(void* self,void* code,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()){lease.release();native_multi_entry_old_join(self,code,info);return;}
    try{if(!native_multi_entry_target(self)){lease.release();native_multi_entry_old_join(self,code,info);return;}native_multi_entry_capture(self,"join");const auto selected=text(code);if(selected.size()!=6||selected.find_first_not_of("0123456789")!=std::string::npos)throw std::runtime_error("six digit private room code required");
        if(!native_multiplayer_transport_join(selected))throw std::runtime_error("join queue unavailable");}
    catch(const std::exception& e){native_multi_entry_pending.clear();native_multi_entry_fail(e.what());}
}
void native_multiplayer_entry_apply(const cnjson::Value& snapshot){
    if(!native_multiplayer_entry_active())return;native_battle_launch_require_ui();
    if(native_multi_entry_string(snapshot,"roomId")!=native_multi_entry_room)throw std::runtime_error("private team snapshot room mismatch");
    auto projection=native_multi_entry_field(snapshot,"dtoProjection");if(!projection)throw std::runtime_error("private team projection missing");
    auto status=native_multi_entry_field(*projection,"teamStatus");if(!status)throw std::runtime_error("private native TeamStatus missing");
    const auto phase=native_multi_entry_string(snapshot,"phase");
    const bool phase_changed=phase!=native_multi_entry_string(native_multi_entry_snapshot,"phase");
    const auto signature=cnjson::dump(*status);native_multi_entry_snapshot=snapshot;if(signature==native_multi_entry_team_signature)return;
    TemporaryRoots roots;auto mode=roots.retain(native_multiplayer_entry_mode());auto team=roots.retain(field_object(mode,"<teamInfo>k__BackingField"));if(!team)throw std::runtime_error("native ServiceMode.teamInfo unavailable");
    auto team_id=roots.retain(string_new(native_multi_entry_string(*projection,"privateTeamId").c_str()));
    auto boxed=build_value(api.il2cpp_class_get_type(cls("Torappu.UI.AutoChess.Server.AutoChessTeamStatus")),*status,roots);
    void* args[]={team_id,boxed.argument};invoke(bound_methods.at(CN_MULTI_ENTRY_TEAM_FILL.token),team,args);
    native_multi_entry_team_signature=signature;
    invoke(bound_methods.at(CN_MULTI_ENTRY_TEAM_CHANGED.token),mode);
    if(phase_changed)report(("private-room-phase-"+phase).c_str());
}
#include "native_multiplayer_scene_exit.inc"
// Consume only the exact join assembled by the successful private callback.
// old_start keeps the native singleton, service-started flag, Core and Mode.Init.
bool native_multiplayer_entry_service_start(void* raw_entry,void* param,const void* info){
    if(!native_multi_entry_join_armed)return false;
    TemporaryRoots roots;void* callback=nullptr;try{
    native_battle_launch_require_ui();cnlaunch::CommonJoinEntryRaw raw{};if(!read(raw_entry,raw))throw std::runtime_error("unreadable private CommonJoinEntry");
    auto entry=roots.retain(value_box(join_entry_class,&raw));callback=roots.retain(field_object(entry,"onJoinDone"));
    if(text(field_object(entry,"id"))!=native_multi_entry_string(native_multi_entry_snapshot,"roomCode")||text(field_object(entry,"svrAddress"))!="private-http-room"||text(field_object(entry,"svrToken"))!="private-in-process-adapter")throw std::runtime_error("private join identity mismatch");
    struct Scope{Scope(){++native_multi_entry_start_depth;}~Scope(){--native_multi_entry_start_depth;}} scope;
    old_start(raw_entry,param,info);
    if(!native_multi_entry_callback_done)throw std::runtime_error("native private team startup did not consume its join");
    }catch(const std::exception& e){if(callback&&!native_multi_entry_callback_done){native_multi_entry_callback_done=true;try{cancel_join(callback);}catch(...){}}native_multi_entry_callback_success=false;native_multi_entry_fail(e.what());}
    return true;
}
bool native_multiplayer_entry_service_reconnect(void* raw_entry,void* param,const void*){
    if(!native_multiplayer_entry_active())return false;
    TemporaryRoots roots;void* callback=nullptr;uint64_t cookie=0;
    try{
        native_battle_launch_require_ui();cnlaunch::CommonJoinEntryRaw raw{};
        if(!param||!act(field_object(param,"activityId"))||!raw_entry||!read(raw_entry,raw))throw std::runtime_error("private live rejoin identity unreadable");
        auto entry=roots.retain(value_box(join_entry_class,&raw));callback=roots.retain(field_object(entry,"onJoinDone"));
        if(text(field_object(entry,"id"))!=native_multi_entry_room||text(field_object(entry,"svrAddress"))!="private-http-room"||text(field_object(entry,"svrToken"))!="private-in-process-adapter")throw std::runtime_error("private live rejoin scene identity mismatch");
        auto service=native_multiplayer_entry_service();if(!service||field_object(service,"m_mode")!=native_multiplayer_entry_mode())throw std::runtime_error("private live rejoin owner changed");
        if(!native_battle_launch_in_flight.load())throw std::runtime_error("private lost-scene rejoin requires validated preparation entry");
        if(native_multi_entry_live_callbacks.size()>=8)throw std::runtime_error("private live rejoin callback bound");
        cookie=++native_multi_entry_live_cookie;if(callback){auto root=hold_new(callback,false);if(!root)throw std::runtime_error("private live rejoin callback root unavailable");native_multi_entry_live_callbacks.emplace(cookie,root);}
        if(!native_multiplayer_enqueue_request("reconnect",cnjson::parse("{\"unityLost\":false}"),cookie))throw std::runtime_error("private live rejoin queue unavailable");
    }catch(const std::exception& error){
        auto pending=native_multi_entry_live_callbacks.find(cookie);if(pending!=native_multi_entry_live_callbacks.end()){hold_free(pending->second);native_multi_entry_live_callbacks.erase(pending);}
        if(callback){struct Scope{Scope(){++native_multi_entry_live_callback_depth;}~Scope(){--native_multi_entry_live_callback_depth;}} scope;try{cancel_join(callback);}catch(...){}}
        native_multi_entry_fail(error.what());
    }
    return true;
}
bool native_multiplayer_entry_complete_reconnect(uint64_t cookie,const cnjson::Value& reply,bool success){
    auto found=native_multi_entry_live_callbacks.find(cookie);if(found==native_multi_entry_live_callbacks.end())return false;
    native_battle_launch_require_ui();TemporaryRoots roots;auto callback=roots.retain(hold_get(found->second));hold_free(found->second);native_multi_entry_live_callbacks.erase(found);
    if(callback){
        struct Scope{Scope(){++native_multi_entry_live_callback_depth;}~Scope(){--native_multi_entry_live_callback_depth;}} scope;
        uint8_t accepted=success&&native_multiplayer_entry_active()&&native_multi_entry_string(reply,"roomId")==native_multi_entry_room;void* args[]={&accepted};invoke(method(api.il2cpp_object_get_class(callback),"Invoke",1),callback,args);
    }return true;
}
void native_multi_entry_reconnect_done(void* self,bool success,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(can_read()&&native_multi_entry_live_callback_depth&&native_multiplayer_entry_active()&&native_multi_entry_target(self))return;
    lease.release();native_multi_entry_old_reconnect_done(self,success,info);
}
#include "native_multiplayer_preparation_bootstrap_ready.inc"
void native_multi_entry_server_start(void* self,void* raw_entry,const void* info){
    if(!native_multi_entry_start_depth){native_multi_entry_old_server_start(self,raw_entry,info);return;}
    TemporaryRoots roots;cnlaunch::CommonJoinEntryRaw raw{};void* callback=nullptr;
    try{native_battle_launch_require_ui();
    if(!read(raw_entry,raw))throw std::runtime_error("native team entry unavailable");auto entry=roots.retain(value_box(join_entry_class,&raw));
    callback=roots.retain(field_object(entry,"onJoinDone"));if(!callback||native_multi_entry_callback_done)throw std::runtime_error("native team callback unavailable or duplicate");
    auto mode=roots.retain(field_object(self,"m_client"));if(!mode)throw std::runtime_error("native TeamServer client unavailable");
    auto service_class=cls("Torappu.UI.AutoChess.Server.AutoChessService");auto service=roots.retain(invoke(method(service_class,"get_s_instance",0),nullptr));if(!service)throw std::runtime_error("native private service singleton unavailable");
    native_multi_entry_root(native_multi_entry_mode_root,mode);native_multi_entry_root(native_multi_entry_service_root,service);++native_multi_entry_service_epoch;
    native_multi_entry_room=native_multi_entry_string(native_multi_entry_snapshot,"roomId");if(native_multi_entry_room.empty())throw std::runtime_error("private room identity missing");
    uint8_t silent=0;set_field(self,profile_field(api.il2cpp_object_get_class(self),"<isSilent>k__BackingField"),&silent);
    native_multi_entry_store_join(self,raw,false);
    // The original native callback queries selfUid when deciding room ownership.
    auto controller=roots.retain(hold_get(native_multi_entry_controller_root));auto model=roots.retain(field_object(controller,"m_model"));
    auto projection=native_multi_entry_field(native_multi_entry_snapshot,"dtoProjection");if(!projection)throw std::runtime_error("private team projection unavailable");
    auto uid=roots.retain(string_new(native_multi_entry_string(*projection,"privateSelfUid").c_str()));set_object_field(model,profile_field(api.il2cpp_object_get_class(model),"<selfUid>k__BackingField"),uid);
    // Resume set the page mode before entering this native service. Its final
    // terminal HTTP proof must bind the now-established service epoch before
    // the callback can synchronously open a result state.
    if(native_multi_entry_string(native_multi_entry_snapshot,"phase")=="result")native_multiplayer_social_resume_result(controller,native_multi_entry_snapshot);
    // A resume can already be IN_BATTLE. The original callbacks below may
    // synchronously start Unity, so seed the existing native BattleInfo first.
    native_multiplayer_dispatch_apply(native_multiplayer_preparation_view_effective(native_multi_entry_snapshot));
    native_multiplayer_entry_apply(native_multi_entry_snapshot);
    native_single_mode_captured=false;native_single_team_choices_ready=false;
    native_multi_entry_callback_done=true;native_multi_entry_callback_success=true;uint8_t success=1;void* args[]={&success};invoke(method(api.il2cpp_object_get_class(callback),"Invoke",1),callback,args);
    native_multiplayer_entry_retry_scene_ready(native_multi_entry_snapshot);
    ++native_multi_entry_joins;
    }catch(const std::exception& e){
        if(callback&&!native_multi_entry_callback_done){native_multi_entry_callback_done=true;try{cancel_join(callback);}catch(...){}}
        native_multi_entry_callback_success=false;native_multi_entry_fail(e.what());
    }
}
void native_multiplayer_entry_return_completed(uint64_t cookie,bool success,const cnjson::Value& receipt,const std::string& error){
    if(!cookie||cookie!=native_multi_entry_return_cookie)return;
    const auto origin=native_multi_entry_return_origin;const auto epoch=native_multi_entry_return_epoch;
    native_multi_entry_return_origin={};native_multi_entry_return_epoch=native_multi_entry_return_cookie=0;
    if(!success){native_multi_entry_fail(error);return;}
    const auto acknowledged=preparation_view::session(receipt);
    if(!acknowledged.valid()||acknowledged.room!=origin.room||(!acknowledged.party&&!(acknowledged==origin))||(acknowledged.party&&(acknowledged.match!=origin.match&&acknowledged.match!=origin.match+1)))throw std::runtime_error("private room return ACK is not this party transition");
    // The cookie is correlated to the original endpoint-bound queued request.
    // Its successful receipt retires only that native lifecycle, never an own-view proof.
    if(!origin.valid()||!native_multiplayer_entry_active()||epoch!=native_multi_entry_service_epoch||!(preparation_view::session(native_multi_entry_snapshot)==origin))return;
    auto mode=native_multi_entry_retain(native_multiplayer_entry_mode());
    auto done=[origin,acknowledged,epoch,mode](){
        if(epoch!=native_multi_entry_service_epoch)throw std::runtime_error("private room return service epoch is stale");
        if(!native_multiplayer_entry_active())return;
        if(!(preparation_view::session(native_multi_entry_snapshot)==origin)||native_multiplayer_entry_mode()!=mode.get())throw std::runtime_error("private room return no longer owns its native mode");
        native_multiplayer_dispatch_reset_scene();invoke(bound_methods.at(CN_MULTI_ENTRY_BACK_TO_TEAM.token),mode.get());
        native_multi_entry_scene_exit_key.clear();native_multi_entry_scene_quit_pending=false;native_multiplayer_transport_attach(acknowledged);
    };
    if(!native_multiplayer_preparation_view_retire(origin,std::move(done),"room-return:"+origin.room+":"+std::to_string(origin.match)+":"+std::to_string(cookie)))throw std::runtime_error("private room return lacks its original scene owner");
}
void native_multiplayer_entry_completed(const char* operation,bool success,const cnjson::Value& snapshot,const std::string& error){
    std::string op=operation?operation:"";
    if(op=="reconnect"&&native_multi_entry_pending=="resume")op="resume";
    if(op=="room.return")return; // Correlated by its original cookie, not the newest snapshot/op name.
    if(op=="scene.quit"){
        if(!success){native_multi_entry_scene_quit_pending=false;native_multi_entry_fail(error);return;}
        native_multi_entry_scene_exit(snapshot);return;
    }
    if(op!="create"&&op!="join"&&op!="resume"){
        if(!success){if(op=="leave")native_multi_entry_result_close_pending=false;native_multi_entry_fail(error);return;}
        if(auto left=native_multi_entry_field(snapshot,"left");left&&left->kind==cnjson::Value::Boolean&&left->boolean){
            const auto origin=preparation_view::session(native_multi_entry_snapshot);
            const uint64_t epoch=native_multi_entry_service_epoch;
            if(native_multi_entry_string(snapshot,"roomId")!=origin.room)throw std::runtime_error("private leave completion belongs to another room");
            auto controller=native_multi_entry_retain(native_multi_entry_controller_root?hold_get(native_multi_entry_controller_root):nullptr);
            const bool closed=native_multi_entry_field(snapshot,"roomClosed")!=nullptr;
            const bool close_result=native_multi_entry_result_close_pending;
            auto done=[origin,controller,epoch,closed,close_result](){
                if(epoch!=native_multi_entry_service_epoch)throw std::runtime_error("private leave completion service epoch is stale");
                if(native_multiplayer_entry_active()&&!(preparation_view::session(native_multi_entry_snapshot)==origin))throw std::runtime_error("private leave completion no longer owns its session");
                native_multiplayer_social_forget_return();
                if(controller){
                    // A server-pushed dissolution has no locally rented Leave
                    // request/callback. The Lua response callback is unsuitable
                    // here; use the original page-close + Service.Stop edge.
                    if(closed||close_result)invoke(bound_methods.at(CN_MULTI_ENTRY_LEAVE_PAGE.token),controller.get());
                    else{void* args[]={nullptr};invoke(bound_methods.at(CN_MULTI_ENTRY_LEAVE_DONE.token),controller.get(),args);}
                }
                if(epoch!=native_multi_entry_service_epoch||(native_multiplayer_entry_active()&&!(preparation_view::session(native_multi_entry_snapshot)==origin)))throw std::runtime_error("private leave callback replaced its service owner");
                native_multiplayer_entry_reset_now();
            };
            if(native_multiplayer_preparation_view_finalizing()){done();return;}
            if(!native_multiplayer_preparation_view_retire(origin,std::move(done),"leave-done:"+origin.room+":"+std::to_string(origin.match)))throw std::runtime_error("private leave lacks its original scene owner");
            return;
        }
        if(native_multiplayer_entry_active()){
            native_multiplayer_entry_apply(snapshot);
        }return;
    }
    if(native_multi_entry_pending!=op){if(success&&op!="resume")native_multiplayer_transport_command("leave",cnjson::parse("{}"));return;}
    if(!success){native_multi_entry_pending.clear();native_multi_entry_fail(error);return;}
    try{
        native_battle_launch_require_ui();TemporaryRoots roots;auto controller=roots.retain(hold_get(native_multi_entry_controller_root));if(!controller)throw std::runtime_error("prepare page closed before room join completed");
        native_multi_entry_snapshot=snapshot;
        if(native_multiplayer_preparation_view_finalizing()&&!native_multiplayer_preparation_view_claim_initial_owner(preparation_view::session(snapshot),native_multi_entry_service_epoch))throw std::runtime_error("private initial join lacks a quiescent lifecycle owner");
        native_multi_entry_pending.clear();
        if(op=="resume")native_multiplayer_social_resume_result(controller,snapshot);native_multi_entry_join_armed=true;native_multi_entry_callback_done=false;native_multi_entry_callback_success=false;
        cnjson::Value response=cnjson::parse("{\"teamId\":\"\",\"serverAddress\":\"private-http-room\",\"serverToken\":\"private-in-process-adapter\"}");response.object["teamId"].string=native_multi_entry_string(snapshot,"roomCode");
        auto team=roots.retain(json(cls("Torappu.UI.AutoChess.AutoChessTeamInfo"),cnjson::dump(response)));int32_t result=0;void* args[]={&result,team};
        PrivateDataScope private_scope(true);invoke(bound_methods.at(CN_MULTI_ENTRY_RESPONSE.token),controller,args);native_multi_entry_join_armed=false;
        if(!native_multi_entry_callback_done||!native_multi_entry_callback_success)throw std::runtime_error("native team join callback did not succeed");
    }catch(const std::exception& e){native_multi_entry_pending.clear();native_multi_entry_join_armed=false;native_multi_entry_fail(e.what());
        // Failed scene reconstruction is still a passive interruption. Keep
        // the protected participant/room and let the next Continue retry it.
        native_multiplayer_transport_command(op=="resume"?"disconnect":"leave",cnjson::parse("{}"));
        if(op=="resume")native_multiplayer_transport_detach(preparation_view::session(snapshot));}
}
bool native_multiplayer_entry_presence(const cnjson::Value& info){
    if(info.kind!=cnjson::Value::Object||info.object.size()!=6)return false;
    const auto id=native_multi_entry_string(info,"sceneId");
    if(id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos||
       !native_multi_entry_mode_allowed(native_multi_entry_string(info,"modeId"))||
       native_multi_entry_string(info,"address")!="private-http-room"||native_multi_entry_string(info,"token")!="private-in-process-adapter")return false;
    auto round=native_multi_entry_field(info,"curRound"),end=native_multi_entry_field(info,"endTime");
    return round&&round->kind==cnjson::Value::Integer&&round->integer>=0&&round->integer<=15&&end&&end->kind==cnjson::Value::Integer&&end->integer>0;
}
cnjson::Value native_multiplayer_entry_sync(){
    auto snapshot=native_multiplayer_transport_snapshot();const auto id=native_multi_entry_string(snapshot,"roomId");
    if(id.empty())return {};
    if(cn_departure::scene_departed(snapshot))return cnjson::parse("{\"changed\":[],\"battleInfo\":null}");
    auto wire=cnjson::parse("{\"changed\":[],\"battleInfo\":{\"sceneId\":\"\",\"address\":\"private-http-room\",\"token\":\"private-in-process-adapter\",\"modeId\":\"\",\"endTime\":1,\"curRound\":0}}");
    auto& info=wire.object["battleInfo"];info.object["sceneId"].string=id;info.object["modeId"].string=native_multi_entry_string(snapshot,"modeId");
    if(auto round=native_multi_entry_field(snapshot,"round"))info.object["curRound"]=*round;
    auto deadline=native_multi_entry_field(snapshot,"deadlineTs"),clock=native_multi_entry_field(snapshot,"serverTime");
    if(deadline&&deadline->kind==cnjson::Value::Integer&&deadline->integer>0)info.object["endTime"]=*deadline;
    else if(clock&&clock->kind==cnjson::Value::Integer)info.object["endTime"].integer=clock->integer+3600;
    if(!native_multiplayer_entry_presence(info))throw std::runtime_error("private persisted room presence is invalid");return wire;
}
bool native_multiplayer_entry_resume(void* self,const cnjson::Value& snapshot){
    native_battle_launch_require_ui();if(native_multiplayer_entry_active())return false;
    if(!native_multi_entry_target(self)||!native_multi_entry_mode_allowed(native_multi_entry_string(snapshot,"modeId")))throw std::runtime_error("private room resume identity mismatch");
    auto model=field_object(self,"m_model");if(!model)throw std::runtime_error("private room resume model unavailable");auto model_class=api.il2cpp_object_get_class(model);int32_t multi=2,team=2;uint8_t local=0;
    set_field(model,profile_field(model_class,"<modeType>k__BackingField"),&multi);set_field(model,profile_field(model_class,"<multiModeSubType>k__BackingField"),&team);set_field(model,profile_field(model_class,"<isLocalMode>k__BackingField"),&local);
    native_multi_entry_capture(self,"resume");native_multiplayer_entry_completed("resume",true,snapshot,{});return native_multiplayer_entry_active();
}
bool native_multiplayer_entry_reconnect(void* self,void* supplied){
    if(!supplied||!native_multi_entry_mode_allowed(text(field_object(supplied,"modeId"))))return false;
    try{
        native_battle_launch_require_ui();if(!native_multi_entry_target(self))throw std::runtime_error("private rejoin activity mismatch");
        const auto id=text(field_object(supplied,"sceneId"));auto snapshot=native_multiplayer_transport_snapshot();
        if(id.size()!=32||id.find_first_not_of("0123456789abcdef")!=std::string::npos||text(field_object(supplied,"address"))!="private-http-room"||text(field_object(supplied,"token"))!="private-in-process-adapter")throw std::runtime_error("private rejoin scene identity mismatch");
        if(native_multiplayer_transport_room()!=id)throw std::runtime_error("private rejoin does not match protected room identity");
        if(snapshot.kind==cnjson::Value::Object&&!snapshot.object.empty()&&(native_multi_entry_string(snapshot,"roomId")!=id||native_multi_entry_string(snapshot,"modeId")!=text(field_object(supplied,"modeId"))))throw std::runtime_error("private rejoin does not match persisted room");
        cnjson::Value payload=cnjson::parse("{\"unityLost\":true}");
        if(native_multiplayer_entry_active()&&native_battle_launch_in_flight.load())payload.object["unityLost"].boolean=false;
        else{
            auto model=field_object(self,"m_model");if(!model)throw std::runtime_error("private room resume model unavailable");auto model_class=api.il2cpp_object_get_class(model);int32_t multi=2,team=2;uint8_t local=0;
            set_field(model,profile_field(model_class,"<modeType>k__BackingField"),&multi);set_field(model,profile_field(model_class,"<multiModeSubType>k__BackingField"),&team);set_field(model,profile_field(model_class,"<isLocalMode>k__BackingField"),&local);
            native_multi_entry_capture(self,"resume");
        }
        native_multiplayer_capture_display_profile(payload);
        if(!native_multiplayer_transport_command("reconnect",payload))throw std::runtime_error("private rejoin queue unavailable");
    }catch(const std::exception& e){native_multi_entry_pending.clear();native_multi_entry_fail(e.what());}
    return true;
}
std::string native_multi_entry_leave_operation(){
    const auto view=native_multiplayer_transport_snapshot();
    auto seat=native_multi_entry_field(view,"seat"),owner=native_multi_entry_field(view,"ownerSeat");
    if(native_multi_entry_return_snapshot(view)&&seat&&owner&&seat->kind==cnjson::Value::Integer&&owner->kind==cnjson::Value::Integer&&seat->integer==owner->integer)return "room.dissolve";
    return "leave";
}
bool native_solo_result_leave_request(void*);
void native_multiplayer_entry_close_result(void* self){
    native_battle_launch_require_ui();
    const bool returned=native_multiplayer_social_return_owner(self);
    if((!native_multiplayer_entry_active()&&!returned)||!native_multi_entry_controller_root||self!=hold_get(native_multi_entry_controller_root)||
       (native_multi_entry_string(native_multi_entry_snapshot,"phase")!="result"&&!native_multiplayer_entry_voluntary_exit()&&!returned)||
       native_multi_entry_room!=native_multiplayer_transport_room())throw std::runtime_error("private result close has no current room owner");
    if(native_multi_entry_result_close_pending)return;
    // A native result confirmation can call _LeavePreparePage directly when
    // GetTeamInfo is absent. Keep the page and service alive until the same
    // authenticated room acknowledges Leave, including transport retries.
    if(!native_multiplayer_transport_command("leave",cnjson::parse("{}")))throw std::runtime_error("private result close queue unavailable");
    native_multi_entry_result_close_pending=true;++native_multi_entry_requests;report("private-result-close-awaiting-leave");
}
void native_multi_entry_leave_request(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()||!native_multi_entry_target(self)){
        lease.release();native_multi_entry_old_leave_request(self,info);return;
    }
    try{native_battle_launch_require_ui();if(native_solo_result_leave_request(self))return;if(!native_multiplayer_entry_active()||!native_multi_entry_controller_root||self!=hold_get(native_multi_entry_controller_root))throw std::runtime_error("private room leave has no current owner");if(!native_multiplayer_transport_command(native_multi_entry_leave_operation(),cnjson::parse("{}")))throw std::runtime_error("private room leave queue unavailable");++native_multi_entry_requests;report("private-room-leave-queued");}
    catch(const std::exception& error){native_multi_entry_fail(error.what());}
}
bool native_multiplayer_entry_request(void* request){
    if(!native_multiplayer_entry_active())return false;native_battle_launch_require_ui();
    const auto type=api.il2cpp_object_get_class(request);const std::string name=api.il2cpp_class_get_name(type);cnjson::Value payload=cnjson::parse("{}");std::string op;
    if(name=="AutoChessTeamReadyInRoomUp"){op="room.ready";cnjson::Value ready;ready.kind=cnjson::Value::Boolean;ready.boolean=native_multi_entry_number<uint8_t>(request,"ready")!=0;payload.object["ready"]=ready;}
    else if(name=="AutoChessTeamEnemyAssignReadyUp")op="confirm";
    else if(name=="AutoChessTeamChooseStrategyUp"){
        if(native_multi_entry_number<uint8_t>(request,"skip"))throw std::runtime_error("private strategy skip is not available");op="band.choose";cnjson::Value band;band.kind=cnjson::Value::String;band.string=text(field_object(request,"strategy"));payload.object["bandId"]=band;
    }else if(name=="LeaveUp")op=native_multi_entry_leave_operation();
    else if(name=="BackTeamUp")op="room.return";
    else if(name=="AutoChessBattleSceneQuitUp")op="scene.quit";
    else if(name=="ProtoIDAutoChessTeamChangeModeUp"){op="room.mode";cnjson::Value mode;mode.kind=cnjson::Value::String;mode.string=text(field_object(request,"modeId"));payload.object["modeId"]=mode;}
    else return false;
    if(op=="room.return"){
        if(native_multi_entry_return_cookie)throw std::runtime_error("private room return is already pending");
        auto origin=preparation_view::session(native_multi_entry_snapshot);if(!origin.valid()||origin.party)throw std::runtime_error("private room return has no original game owner");
        const auto cookie=++native_multi_entry_live_cookie;
        if(!native_multiplayer_enqueue_request(op,payload,cookie))throw std::runtime_error("private room return queue unavailable");
        native_multi_entry_return_origin=std::move(origin);native_multi_entry_return_epoch=native_multi_entry_service_epoch;native_multi_entry_return_cookie=cookie;
    }else{
        if(op=="scene.quit"&&native_multi_entry_scene_quit_pending){invoke(method(type,"Recycle",0),request);return true;}
        if(!native_multiplayer_transport_command(op,payload))throw std::runtime_error("private room command queue unavailable");
        if(op=="scene.quit")native_multi_entry_scene_quit_pending=true;
    }
    invoke(method(type,"Recycle",0),request);++native_multi_entry_requests;return true;
}
void native_multi_entry_request(void* self,void* request,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()||self!=native_multiplayer_entry_mode()){lease.release();native_multi_entry_old_request(self,request,info);return;}
    try{if(native_multiplayer_entry_request(request)||native_multiplayer_dispatch_request(self,request))return;throw std::runtime_error("private room request has no connected handler");}
    catch(const std::exception& e){try{if(request)invoke(method(api.il2cpp_object_get_class(request),"Recycle",0),request);}catch(...){}native_multi_entry_fail(e.what());}
}
void native_multi_entry_start_game(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()||!native_multiplayer_entry_active()||self!=hold_get(native_multi_entry_controller_root)){lease.release();native_multi_entry_old_start_game(self,info);return;}
    try{native_battle_launch_require_ui();if(!native_multiplayer_transport_command("start",cnjson::parse("{}")))throw std::runtime_error("private room start queue unavailable");}
    catch(const std::exception& e){native_multi_entry_fail(e.what());}
}
void native_multi_entry_destroy(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(native_multi_entry_controller_root&&self==hold_get(native_multi_entry_controller_root)){
        // Closing the preparation page for native battle loading keeps the
        // room/mode alive. A pending HTTP join can no longer touch that page.
        native_multi_entry_pending.clear();native_multi_entry_root(native_multi_entry_controller_root,nullptr);
    }
    native_multi_entry_old_destroy(self,info);
}
void native_multi_entry_service_dispose(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(self&&self==native_multi_entry_disposing)return;
    const bool target=native_multiplayer_entry_active()&&self==native_multiplayer_entry_service();
    if(!target){lease.release();native_multi_entry_old_service_dispose(self,info);return;}
    try{
        native_battle_launch_require_ui();const uint64_t epoch=native_multi_entry_service_epoch;
        if(native_multi_entry_disposed_epoch==epoch)return;
        const auto origin=preparation_view::session(native_multi_entry_snapshot);
        auto retained=native_multi_entry_retain(self);
        auto done=[retained,info,origin,epoch](){
            if(native_multi_entry_disposed_epoch==epoch)return;
            if(epoch!=native_multi_entry_service_epoch)throw std::runtime_error("private retiring service epoch is stale");
            if(native_multiplayer_entry_active()&&(!(preparation_view::session(native_multi_entry_snapshot)==origin)||native_multiplayer_entry_service()!=retained.get()))throw std::runtime_error("private retiring service no longer owns its session");
            // The native battle return creates a new PreparePage after this
            // service and its entry roots have gone away. Keep only its exact
            // recover bundle and authenticated terminal identity across reset.
            native_multiplayer_social_stage_return(origin);
            if(!origin.party&&native_multi_entry_string(native_multi_entry_snapshot,"phase")!="result"&&
               !cn_departure::scene_departed(native_multi_entry_snapshot))
                native_multiplayer_transport_command("disconnect",cnjson::parse("{}"));
            // Mark before entering the original body, which can reenter Dispose.
            native_multi_entry_disposed_epoch=epoch;
            {struct Disposing{void* previous;explicit Disposing(void* value):previous(native_multi_entry_disposing){native_multi_entry_disposing=value;}~Disposing(){native_multi_entry_disposing=previous;}} disposing(retained.get());
             native_multi_entry_old_service_dispose(retained.get(),info);}
            if(epoch!=native_multi_entry_service_epoch||(native_multiplayer_entry_active()&&(native_multiplayer_entry_service()!=retained.get()||!(preparation_view::session(native_multi_entry_snapshot)==origin))))throw std::runtime_error("private disposal cannot reset a newer service");
            native_multiplayer_transport_detach(origin);native_multiplayer_entry_reset_now();
        };
        if(native_multiplayer_preparation_view_finalizing()){done();return;}
        if(!native_multiplayer_preparation_view_retire(origin,std::move(done),"service-dispose:"+std::to_string(epoch)))throw std::runtime_error("private disposal lacks its original scene owner");
    }catch(const std::exception& error){native_multi_entry_fail(error.what());}
}
struct NativeMultiTerminalUiScope {
    void* previous=native_multi_entry_terminal_ui_service;
    explicit NativeMultiTerminalUiScope(void* controller){
        native_multi_entry_terminal_ui_service=nullptr;
        auto model=field_object(controller,"m_model");
        if(model&&native_multi_entry_number<int32_t>(model,"<curViewType>k__BackingField")==6&&
           native_multiplayer_social_return_owner(controller))
            native_multi_entry_terminal_ui_service=native_multiplayer_entry_service();
    }
    ~NativeMultiTerminalUiScope(){native_multi_entry_terminal_ui_service=previous;}
};
void native_multi_entry_prepare_update(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()){lease.release();native_multi_entry_old_prepare_update(self,info);return;}
    try{
        if(!native_multi_entry_target(self)){lease.release();native_multi_entry_old_prepare_update(self,info);return;}
        native_battle_launch_require_ui();
        if(native_multiplayer_entry_active()){
            // Drain close receipts before native Update interprets ping=-1 as
            // a retryable connection loss. Never update the page after closure.
            const uint64_t epoch=native_multi_entry_service_epoch;
            native_multiplayer_drain_main();
            if(!native_multiplayer_entry_active()||epoch!=native_multi_entry_service_epoch)return;
        }
        native_multiplayer_social_recover_terminal_page(self);
        // Native Update treats ping < 0 as a reconnect request even in SETTLE.
        // A verified personal terminal return deliberately detached transport;
        // suppress only that page's connection-mask predicate during Update.
        {PrivateDataScope scope(true);NativeMultiTerminalUiScope terminal(self);native_multi_entry_old_prepare_update(self,info);}
        // Before the first create/join ACK there is no AutoChessService yet.
        // The original preparation page's Update is the main-thread pump that
        // can complete that ACK and create the original native service.
        native_multiplayer_drain_main();
        // A Help handoff may be waiting for this first Unity world, so its
        // deferred HTTP callbacks cannot be the only pump for the start latch.
        if(native_multiplayer_entry_active()&&native_multi_entry_controller_root&&
           hold_get(native_multi_entry_controller_root)==self&&!native_battle_launch_in_flight.load()&&native_multi_entry_bootstrap_wait_missing())
            native_multiplayer_entry_retry_scene_ready(native_multi_entry_snapshot);
    }catch(const std::exception& e){native_multi_entry_fail(e.what());}
}
uint64_t native_multi_entry_time_value(){
    // The original countdown converts its deadline with DateTimeUtil, whose
    // START_TIME includes the CN UTC+8 bias. Use that same original converter
    // for the private clock; a UTC DateTime leaves eight hours on every timer.
    const auto seconds=native_multiplayer_transport_time();if(!std::isfinite(seconds)||seconds<=0||seconds>253402300799.0)throw std::runtime_error("private server clock unavailable");
    int64_t stamp=int64_t(seconds);void* args[]={&stamp};TemporaryRoots roots;
    auto boxed=roots.retain(invoke(bound_methods.at(CN_MULTI_ENTRY_TIMESTAMP.token),nullptr,args));uint64_t result=0;
    if(!boxed||!read(unbox(boxed),result))throw std::runtime_error("private server native timestamp conversion unreadable");return result;
}
uint64_t native_multi_entry_service_time(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return 0;
    if(!can_read()||!native_multiplayer_entry_active()||self!=native_multiplayer_entry_service()){lease.release();return native_multi_entry_old_service_time(self,info);}
    try{return native_multi_entry_time_value();}catch(const std::exception& e){native_multi_entry_fail(e.what());return 0;}
}
uint64_t native_multi_entry_mode_time(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return 0;
    if(!can_read()||!native_multiplayer_entry_active()||self!=native_multiplayer_entry_mode()){lease.release();return native_multi_entry_old_mode_time(self,info);}
    try{return native_multi_entry_time_value();}catch(const std::exception& e){native_multi_entry_fail(e.what());return 0;}
}
void native_multi_entry_single_room_start(void* self,void* prepare){
    if(!can_read()||!native_multiplayer_entry_active()||!self||!prepare||
       !native_multi_entry_controller_root||field_object(hold_get(native_multi_entry_controller_root),"m_model")!=prepare||
       api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessRoomViewModel")||
       api.il2cpp_object_get_class(prepare)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||
       text(field_object(self,"actId"))!="act2autochess"||text(field_object(prepare,"<actId>k__BackingField"))!="act2autochess"||
       native_multi_entry_number<int32_t>(prepare,"<modeType>k__BackingField")!=2||
       native_multi_entry_number<int32_t>(prepare,"<multiModeSubType>k__BackingField")!=2||
       !native_multi_entry_number<uint8_t>(self,"isSelfHost")||native_multi_entry_number<uint8_t>(self,"matchFlag")||native_multi_entry_number<int32_t>(self,"playerCount")!=1)return;
    native_battle_launch_require_ui();const auto snapshot=native_multiplayer_transport_snapshot();
    if(native_multi_entry_string(snapshot,"roomId")!=native_multi_entry_room||native_multi_entry_string(snapshot,"phase")!="building")return;
    auto seat=native_multi_entry_field(snapshot,"seat"),owner=native_multi_entry_field(snapshot,"ownerSeat"),party=native_multi_entry_field(snapshot,"partyReturn");
    if(!seat||!owner||seat->kind!=cnjson::Value::Integer||owner->kind!=cnjson::Value::Integer||seat->integer!=owner->integer||!party)return;
    auto pending=native_multi_entry_field(*party,"pendingSeats");if(!pending||pending->kind!=cnjson::Value::Array||!pending->array.empty())return;
    auto projection=native_multi_entry_field(snapshot,"dtoProjection");if(!projection)return;
    const auto uid=native_multi_entry_string(*projection,"privateSelfUid");auto team=native_multi_entry_field(*projection,"teamStatus");if(!team||uid.rfind("private-",0)!=0)return;
    auto players=native_multi_entry_field(*team,"players");
    if(!players||players->kind!=cnjson::Value::Array||players->array.size()!=1||native_multi_entry_string(players->array[0],"uid")!=uid||
       native_multi_entry_string(*team,"ownerUid")!=uid||text(field_object(self,"selfUID"))!=uid||text(field_object(prepare,"<selfUid>k__BackingField"))!=uid)return;
    if(native_multi_entry_number<int32_t>(self,"readyButtonState")!=4)return;
    // Original RefreshData forbids a single-player TEAM (2783e1a..2783e2f).
    // Change only its private owner's display state, never the native roster,
    // maxPlayerCnt or readiness rules for two through four real participants.
    int32_t start_game=5;set_field(self,profile_field(api.il2cpp_object_get_class(self),"readyButtonState"),&start_game);
}
uint64_t native_multi_entry_room_refresh(void* self,void* prepare,void* leaves,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return 0;
    const auto result=native_multi_entry_old_room_refresh(self,prepare,leaves,info);
    try{native_multi_entry_single_room_start(self,prepare);}catch(const std::exception& e){native_multi_entry_fail(e.what());}
    return result; // Preserve the original eight-byte RefreshDataResult ABI.
}
void native_multiplayer_entry_setup(){
    bind_method(CN_MULTI_ENTRY_LEAVE_PAGE);
    auto date=cls("System.DateTime");uint32_t alignment=0;if(!date||api.il2cpp_class_value_size(date,&alignment)!=8)throw std::runtime_error("native DateTime ABI differs");
    bind_method(CN_MULTI_ENTRY_TIMESTAMP);
    hook(CN_MULTI_ENTRY_BATTLE_EXCEPTION,native_multi_entry_battle_exception,native_multi_entry_old_battle_exception);bind_method(CN_MULTI_ENTRY_BATTLE_LOST);
    bind_method(CN_MULTI_ENTRY_RESPONSE);bind_method(CN_MULTI_ENTRY_TEAM_FILL);bind_method(CN_MULTI_ENTRY_TEAM_CHANGED);bind_method(CN_MULTI_ENTRY_LEAVE_DONE);bind_method(CN_MULTI_ENTRY_BACK_TO_TEAM);bind_method(CN_MULTI_ENTRY_SCENE_READY);
    hook(CN_MULTI_ENTRY_JOIN,native_multi_entry_join,native_multi_entry_old_join);
    hook(CN_MULTI_ENTRY_SERVER_START,native_multi_entry_server_start,native_multi_entry_old_server_start);
    hook(CN_MULTI_ENTRY_MODE_REQUEST,native_multi_entry_request,native_multi_entry_old_request);
    hook(CN_MULTI_ENTRY_START_GAME,native_multi_entry_start_game,native_multi_entry_old_start_game);
    hook(CN_MULTI_ENTRY_DESTROY,native_multi_entry_destroy,native_multi_entry_old_destroy);
    hook(CN_MULTI_ENTRY_SERVICE_TIME,native_multi_entry_service_time,native_multi_entry_old_service_time);
    hook(CN_MULTI_ENTRY_MODE_TIME,native_multi_entry_mode_time,native_multi_entry_old_mode_time);
    hook(CN_MULTI_ENTRY_SERVICE_DISPOSE,native_multi_entry_service_dispose,native_multi_entry_old_service_dispose);
    hook(CN_MULTI_ENTRY_PREPARE_UPDATE,native_multi_entry_prepare_update,native_multi_entry_old_prepare_update);
    hook(CN_MULTI_ENTRY_RECONNECT_DONE,native_multi_entry_reconnect_done,native_multi_entry_old_reconnect_done);
    hook(CN_MULTI_ENTRY_LEAVE_REQUEST,native_multi_entry_leave_request,native_multi_entry_old_leave_request);
    auto refreshed=cls("Torappu.UI.AutoChess.AutoChessRoomViewModel+RefreshDataResult");
    if(!refreshed||api.il2cpp_class_value_size(refreshed,&alignment)!=8)throw std::runtime_error("native room refresh result ABI differs");
    for(const auto& row:std::initializer_list<std::tuple<const char*,const char*,const char*,size_t>>{
        {"Torappu.LatchUtils+InvokeWhenUnlock","m_unlocked","System.Boolean",16},
        {"Torappu.LatchUtils+InvokeWhenUnlock","m_callback","System.Action",24},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_battleStartLatch","Torappu.LatchUtils+InvokeWhenUnlock",184},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_multiStartBattleMaxWaitCoroutine","UnityEngine.Coroutine",160},
        {"Torappu.UI.AutoChess.AutoChessRoomViewModel","actId","System.String",16},
        {"Torappu.UI.AutoChess.AutoChessRoomViewModel","selfUID","System.String",104},
        {"Torappu.UI.AutoChess.AutoChessRoomViewModel","playerCount","System.Int32",48},
        {"Torappu.UI.AutoChess.AutoChessRoomViewModel","isSelfHost","System.Boolean",94},
        {"Torappu.UI.AutoChess.AutoChessRoomViewModel","matchFlag","System.Boolean",92},
        {"Torappu.UI.AutoChess.AutoChessRoomViewModel","readyButtonState","Torappu.UI.AutoChess.AutoChessRoomViewModel+ReadyButtonState",96},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<actId>k__BackingField","System.String",32},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<modeType>k__BackingField","Torappu.ActAutoChessModeType",44},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<multiModeSubType>k__BackingField","Torappu.ActAutoChessMultiModeSubType",48},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<selfUid>k__BackingField","System.String",72}
    })nbb_field(cls(std::get<0>(row)),std::get<1>(row),std::get<2>(row),std::get<3>(row));
    hook(CN_MULTI_ENTRY_ROOM_REFRESH,native_multi_entry_room_refresh,native_multi_entry_old_room_refresh);
}
