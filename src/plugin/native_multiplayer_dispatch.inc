// Include inside cn after native_battle_data.inc/native_battle_launch.inc and
// before setup(). HTTP workers pass immutable JSON only. Every managed object
// and native callback below is touched on the observed Unity main thread.
#pragma once
bool native_multiplayer_dead_observation_fill(const cnjson::Value&,void*);
void native_multiplayer_dead_observation_reset();
cnjson::Value native_multiplayer_dead_observation_packet(const cnjson::Value&);
#include "native_multiplayer_dispatch_dtos.inc"
#include "native_multiplayer_local_combat_policy.h"
#include "native_multiplayer_help_outcome_policy.h"
#include "native_multiplayer_preparation_write_policy.h"

void* native_multiplayer_entry_service();
void* native_multiplayer_entry_mode();
bool native_multiplayer_enqueue(const std::string&,const cnjson::Value&);
bool native_multiplayer_enqueue_request(const std::string&,const cnjson::Value&,uint64_t);
bool native_multiplayer_transport_live();
int32_t native_multiplayer_transport_ping();
cnjson::Value native_multiplayer_help_capture_tokens();
void native_multiplayer_help_set_carry(const cnjson::Value&);
void native_multiplayer_help_require_ready();
void native_multiplayer_help_reset();
void native_multiplayer_field_prepare(const cnjson::Value&);
void native_multiplayer_field_apply(const cnjson::Value&);
void native_multiplayer_field_reset();
void native_multiplayer_field_require_ready();
void native_multiplayer_help_counter_prepare(const cnjson::Value&);
void native_multiplayer_help_counter_reset();
void native_multiplayer_help_completion_prepare(const cnjson::Value&);
void native_multiplayer_mode_runes_prepare(const cnjson::Value&);
void native_multiplayer_mode_runes_reset();
void native_multiplayer_mode_runes_require_applied();
void native_multiplayer_observation_prepare(const cnjson::Value&);
void native_multiplayer_observation_apply(const cnjson::Value&);
void native_multiplayer_observation_reset();
void native_multiplayer_observation_require_ready();
void native_multiplayer_boss_apply(const cnjson::Value&);
void native_multiplayer_boss_reset();
bool native_multiplayer_boss_up(const cnjson::Value&);
bool native_multiplayer_entry_complete_reconnect(uint64_t,const cnjson::Value&,bool);
void native_preparation_tokens_preflight();
void native_preparation_tokens_reset();

static const CN_Method NMD_METHODS[]={
{"Torappu.UI.AutoChess.Server.AutoChessServiceMode","SendRequest",0x602bd23,0x27f1d90ULL,1,false,"System.Void",{"Torappu.SocketNetwork.Msg.RequestHandler"}},
{"Torappu.UI.AutoChess.Server.AutoChessServiceMode","Update",0x602bd1f,0x27f2310ULL,0,false,"System.Void",{}},
{"Torappu.UI.AutoChess.Server.AutoChessServiceMode","get_ping",0x602bd13,0x27f3040ULL,0,false,"System.Int32",{}},
{"Torappu.UI.AutoChess.Server.AutoChessServiceMode","OnBattleStatusChanged",0x602bd30,0x27f1990ULL,0,false,"System.Void",{}},
{"Torappu.UI.AutoChess.Server.AutoChessServiceMode","TriggerEvent",0x602bd2a,0x27f2270ULL,2,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessServiceEvent","System.Object"}},
{"Torappu.UI.AutoChess.Server.AutoChessServiceBattleInfo","Fill",0x602bcfb,0x27f0370ULL,2,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessBattleAllStateSyncData","System.String"}},
{"Torappu.UI.AutoChess.Server.AutoChessServiceBattleInfo","Reset",0x602bcfc,0x27f04d0ULL,0,false,"System.Void",{}},
{"Torappu.UI.AutoChess.Server.AutoChessBattleServer","Start",0x602bea5,0x27f8fd0ULL,1,false,"System.Void",{"Torappu.SocketNetwork.SvrCom.CommonJoinEntry"}},
{"Torappu.SocketNetwork.ServerBase.Server","get_ping",0x6008890,0x2b5f440ULL,0,false,"System.Int32",{}},
{"Torappu.SocketNetwork.ServerBase.Server","get_connected",0x60088a0,0x2b5f2b0ULL,0,false,"System.Boolean",{}},
{"Torappu.SocketNetwork.ServerBase.Server","get_alive",0x60088a1,0x2b5f1c0ULL,0,false,"System.Boolean",{}},
};
using NMD_Request=void(*)(void*,void*,const void*);
using NMD_Void=void(*)(void*,const void*);
using NMD_Ping=int32_t(*)(void*,const void*);
using NMD_Bool=bool(*)(void*,const void*);
NMD_Request nmd_old_request=nullptr,nmd_old_battle_start=nullptr;
NMD_Void nmd_old_update=nullptr;
NMD_Ping nmd_old_ping=nullptr;
NMD_Ping nmd_old_server_ping=nullptr;
NMD_Bool nmd_old_connected=nullptr,nmd_old_alive=nullptr;
cnmulti_dispatch::Snapshot nmd_snapshot;
cnjson::Value nmd_last_view;
std::atomic<unsigned> nmd_local_reports{0},nmd_terminal_reports{0};
std::map<void*,const NMD_DtoSpec*> nmd_dtos;
struct NMD_Pending {uint32_t root=0;std::string response_type;};
std::map<uint64_t,NMD_Pending> nmd_pending;
std::vector<cnjson::Value> nmd_queued_broadcasts;
std::vector<cnjson::Value> nmd_queued_chats;
std::atomic<unsigned> nmd_chats{0},nmd_local_rejections{0};
uint64_t nmd_cookie=0;
unsigned nmd_receive_depth=0;
std::atomic<unsigned> nmd_full{0},nmd_scenes{0},nmd_preparations{0},nmd_players{0},nmd_boards{0},nmd_upstream{0},nmd_completions{0},nmd_broadcasts{0},nmd_failures{0},nmd_duplicates{0};
std::atomic<bool> nmd_setup_ready{false},nmd_seeded{false};
std::string nmd_error,nmd_first_error,nmd_last_failure;
std::mutex nmd_error_lock;
bool nmd_scope(void* mode){
    if(!mode||closing.load()||mode!=native_multiplayer_entry_mode())return false;
    auto service=native_multiplayer_entry_service();auto param=field_object(service,"<param>k__BackingField");
    return service&&param&&act(field_object(param,"activityId"))&&field_object(service,"m_mode")==mode;
}
void nmd_failed(const std::exception& ex){
 ++nmd_failures;{std::lock_guard<std::mutex> lock(nmd_error_lock);
  nmd_error=std::string(ex.what()).substr(0,2048);nmd_last_failure=nmd_error;
  if(nmd_first_error.empty())nmd_first_error=nmd_error;
 }
 report((std::string("private-multiplayer-dispatch-rejected: ")+ex.what()).c_str());
}
void* native_multiplayer_dispatch_scene(){auto mode=native_multiplayer_entry_mode();if(!nmd_scope(mode))return nullptr;return field_object(field_object(field_object(mode,"m_battleSvr"),"<battleInfo>k__BackingField"),"battleSceneData");}
void* native_multiplayer_dispatch_bridge(){
    auto bridge=field_object(nb_manager(),"dataBridge");
    return bridge&&api.il2cpp_object_get_class(bridge)==cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer")?bridge:nullptr;
}
bool native_multiplayer_dispatch_ready(){return nmd_setup_ready.load()&&nmd_seeded.load()&&!closing.load();}
void* nmd_method(uint32_t token){return bound_methods.at(token);}
void* nmd_generic_method(void* object,uint32_t token,const char* name,unsigned count){
    for(auto c=api.il2cpp_object_get_class(object);c;c=api.il2cpp_class_get_parent(c)){
        void* iterator=nullptr;
        while(auto candidate=api.il2cpp_class_get_methods(c,&iterator))if(api.il2cpp_method_get_token(candidate)==token&&
            !std::strcmp(api.il2cpp_method_get_name(candidate),name)&&api.il2cpp_method_get_param_count(candidate)==count){
            if(!matches_type(api.il2cpp_method_get_return_type(candidate),count==1&&!std::strcmp(name,"GetUpProtocol")?"Torappu.SocketNetwork.Msg.IProtocol":count==1&&!std::strcmp(name,"FillUpProtocol")?"System.Boolean":"System.Void"))
                throw std::runtime_error("multiplayer native request method return mismatch");
            return candidate;
        }
    }
    throw std::runtime_error(std::string("multiplayer native request method missing: ")+name);
}
int32_t nmd_enum(void* c,const char* name){
    if(!c||!api.il2cpp_class_is_enum(c))throw std::runtime_error("multiplayer native enum missing");
    auto field=profile_field(c,name);if(!(api.il2cpp_field_get_flags(field)&16))throw std::runtime_error("multiplayer native enum field mismatch");
    int32_t value=0;api.il2cpp_field_static_get_value(field,&value);return value;
}
cnjson::Value nmd_export(void* object,TemporaryRoots& roots,size_t& nodes,unsigned depth=0){
    if(depth>32||++nodes>100000)throw std::runtime_error("multiplayer upstream graph bound");
    if(!object)return {};roots.retain(object);auto c=api.il2cpp_object_get_class(object);auto type=api.il2cpp_class_get_type(c);
    auto kind=api.il2cpp_type_get_type(type);if(api.il2cpp_class_is_enum(c))kind=api.il2cpp_type_get_type(api.il2cpp_class_enum_basetype(c));
    if(kind==14)return json_text(text(object));
    if(kind==2){uint8_t value=0;if(!read(unbox(object),value))throw std::runtime_error("multiplayer upstream bool unreadable");return nb_boolean(value!=0);}
    if(kind>=4&&kind<=11){
        int64_t value=0;const auto bytes=kind<=5?1:(kind<=7?2:(kind<=9?4:8));std::array<uint8_t,8> raw{};
        SIZE_T used=0;if(!ReadProcessMemory(GetCurrentProcess(),unbox(object),raw.data(),bytes,&used)||used!=SIZE_T(bytes))throw std::runtime_error("multiplayer upstream integer unreadable");
        std::memcpy(&value,raw.data(),bytes);if(!(kind&1)&&bytes<8&&(raw[bytes-1]&128))value|=int64_t(~uint64_t(0)<<(bytes*8));
        if(kind==11&&value<0)throw std::runtime_error("multiplayer upstream UInt64 exceeds private JSON range");return nb_integer(value);
    }
    const std::string name=api.il2cpp_class_get_name(c);
    if(name=="List`1"){
        int32_t count=0;if(!read(unbox(invoke(method(c,"get_Count",0),object)),count)||count<0||count>16384)throw std::runtime_error("multiplayer upstream list bound");
        auto result=nb_array();auto getter=method(c,"get_Item",1);
        for(int32_t index=0;index<count;++index){void* args[]={&index};result.array.push_back(nmd_export(invoke(getter,object,args),roots,nodes,depth+1));}return result;
    }
    const auto found=nmd_dtos.find(c);if(found==nmd_dtos.end())throw std::runtime_error("multiplayer upstream class outside pinned DTO graph");
    auto result=nb_object();
    for(const auto& spec:found->second->fields){
        auto field=profile_field(c,spec.name);auto ft=api.il2cpp_field_get_type(field);auto fc=api.il2cpp_class_from_type(ft);
        if(!matches_type(ft,spec.type)||(api.il2cpp_field_get_flags(field)&16))throw std::runtime_error("multiplayer upstream field contract mismatch");
        void* value=nullptr;
        if(api.il2cpp_class_is_valuetype(fc)){
            uint32_t alignment=0;auto size=api.il2cpp_class_value_size(fc,&alignment);if(size<1||size>4096)throw std::runtime_error("multiplayer upstream value size bound");
            std::vector<uint8_t> bytes(size_t(size),uint8_t{});api.il2cpp_field_get_value(object,field,bytes.data());value=roots.retain(value_box(fc,bytes.data()));
        }else api.il2cpp_field_get_value(object,field,&value);
        result.object[spec.name]=nmd_export(value,roots,nodes,depth+1);
    }
    return result;
}
void nmd_send(void* protocol,uint64_t cookie=0){
    if(nmd_receive_depth||native_battle_data_projecting()){++nmd_duplicates;return;}
    auto c=api.il2cpp_object_get_class(protocol);auto found=nmd_dtos.find(c);
    if(found==nmd_dtos.end())throw std::runtime_error(std::string("multiplayer upstream request outside pinned declarations: ")+api.il2cpp_class_get_name(c));
    const auto type=std::string(found->second->type);
    const auto name=cnmulti_dispatch::short_type(type);
    // A single selected defender owns the shared native enemy journal. Other
    // clients render Help locally; their empty/late journals are not outcomes.
    if(!cookie&&cnhelp_outcome::display_only(nmd_last_view,name)){++nmd_local_reports;return;}
    if(name=="AutoChessBattleSceneHelpBattleFinishUp"&&cnhelp_outcome::premature_finish(nmd_last_view,name,native_multiplayer_transport_time())){++nmd_local_reports;return;}
    // The original engine has already processed these events on this field.
    // Avoid graph export, TOKEN scans, disk writes and HTTP per enemy/frame.
    if(cnlocal_combat::local_only(nmd_last_view,name,cookie)){++nmd_local_reports;return;}
    TemporaryRoots roots;size_t nodes=0;auto dto=nmd_export(protocol,roots,nodes);
    native_multiplayer_preparation_view_require_request(name);
    if(name=="AutoChessBattleScenePreparationReadyUp"&&nb_number(dto,"readyType")==1)native_preparation_tokens_preflight();
    if(name=="AutoChessBattleSceneActionUp"&&!native_multiplayer_boss_up(dto))return;
    if(name=="AutoChessBattleSceneSelfBattleFinishUp"){native_multiplayer_help_require_ready();native_multiplayer_field_require_ready();native_multiplayer_observation_require_ready();native_multiplayer_mode_runes_require_applied();}
    // The original Help engine can already have retired its carry task when
    // it sends FinishUp. Startup readiness must not veto its terminal journal.
    if(name=="AutoChessBattleSceneHelpBattleFinishUp")cnhelp_outcome::normalize_finish(dto);
    if(name=="AutoChessBattleSceneSelfBattleInfoUp"||(name=="AutoChessBattleSceneSelfBattleFinishUp"&&!nb_true(dto,"fromRejoin")))dto.object["tokenBattleStatusList"]=native_multiplayer_help_capture_tokens();
    auto up=cnmulti_dispatch::upstream(type,dto);
    if(!(cookie?native_multiplayer_enqueue_request(up.op,up.payload,cookie):native_multiplayer_enqueue(up.op,up.payload)))throw std::runtime_error("multiplayer private request queue rejected");
    if(name=="AutoChessBattleSceneSelfBattleFinishUp")++nmd_terminal_reports;
    ++nmd_upstream;
}
// Called by the sole ServiceRequest hook owned by native_multiplayer_entry.inc.
// A private request stays private even if export or enqueue fails.
bool native_multiplayer_dispatch_request(void* mode,void* request){
    if(!nmd_scope(mode))return false;
    try{nb_main_thread();PrivateDataScope scope(true);if(!request)throw std::runtime_error("multiplayer native request null");nmd_send(request);}
    catch(const std::exception& ex){nmd_failed(ex);}return true;
}
void native_multiplayer_dispatch_complete(uint64_t cookie,const cnjson::Value& reply,bool ok){
    if(native_multiplayer_entry_complete_reconnect(cookie,reply,ok))return;
    if(!cookie)return;nb_main_thread();auto item=nmd_pending.find(cookie);if(item==nmd_pending.end())return;
    auto pending=item->second;nmd_pending.erase(item);TemporaryRoots roots;auto request=roots.retain(hold_get(pending.root));hold_free(pending.root);
    if(!request)return;
    try{
        auto method=nmd_generic_method(request,0x6008993,"ProcDnProtocol",2);
        if(!matches_type(api.il2cpp_method_get_param(method,0),"Torappu.SocketNetwork.Msg.RRPRespCode")||!matches_type(api.il2cpp_method_get_param(method,1),"Torappu.SocketNetwork.Msg.IProtocol"))throw std::runtime_error("multiplayer response method contract mismatch");
        auto enum_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(method,0));int32_t code=nmd_enum(enum_class,ok?"SUC":"REQ_WRONG");void* response=nullptr;
        if(ok){
            const bool dead_response=pending.response_type=="Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessBattleSceneDeadAutoObDn";
            const auto packet=dead_response?native_multiplayer_dead_observation_packet(reply):nb_at(nb_at(reply,"dtoProjection"),"preparation");
            if(nb_string(packet,"type")!=pending.response_type)throw std::runtime_error("multiplayer response DTO type mismatch");
            response=build_value(api.il2cpp_class_get_type(cls(pending.response_type)),nb_at(packet,"payload"),roots).object;
            if(dead_response||reply.object.count("commandRejected")){
                // CN ReqBuyChess's onResponse tests only RRP.SUC, ignoring the
                // business RetCode. Consume the response normally, but never
                // run purchase-success callbacks for an acknowledged rejection.
                for(const auto* field:{"<onResponse>k__BackingField","<onSuccess>k__BackingField"})
                    set_object_field(request,profile_field(api.il2cpp_object_get_class(request),field),nullptr);
            }
        }
        // ProcDnProtocol invokes the game's onResponse/onSuccess chain, including
        // the original buy callback, only after the committed view was applied.
        PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;
        native_private_identifier_reset_projection_error();void* args[]={&code,response};invoke(method,request,args);native_private_identifier_require_projection_success();++nmd_completions;
    }catch(const std::exception& ex){nmd_failed(ex);}
}
void nmd_request(void* mode,void* request,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!nmd_scope(mode)){lease.release();nmd_old_request(mode,request,method_info);return;}
    uint64_t cookie=0;
    try{
        nb_main_thread();if(nmd_receive_depth||native_battle_data_projecting()){++nmd_duplicates;return;}if(!request)throw std::runtime_error("multiplayer native request handler null");
        PrivateDataScope scope(true);TemporaryRoots roots;roots.retain(request);
        auto get=nmd_generic_method(request,0x6008995,"GetUpProtocol",1),fill=nmd_generic_method(request,0x6008994,"FillUpProtocol",1);
        if(!matches_type(api.il2cpp_method_get_param(get,0),"Torappu.SocketNetwork.Msg.INetProtocolSuite")||!matches_type(api.il2cpp_method_get_param(fill,0),"Torappu.SocketNetwork.Msg.IProtocol"))throw std::runtime_error("multiplayer fill request parameter mismatch");
        auto suite=roots.retain(field_object(mode,"m_protocolSuite"));if(!suite)throw std::runtime_error("multiplayer native protocol suite missing");
        void* get_args[]={suite};auto up=roots.retain(invoke(get,request,get_args));if(!up)throw std::runtime_error("multiplayer native request did not allocate protocol");
        void* fill_args[]={up};uint8_t accepted=0;if(!read(unbox(invoke(fill,request,fill_args)),accepted)||!accepted)throw std::runtime_error("multiplayer native request fill declined");
        auto proc=nmd_generic_method(request,0x6008991,"OnProcStatus",1);char* type=api.il2cpp_type_get_name(api.il2cpp_method_get_param(proc,0));
        std::string response_type=type?type:"";if(type)api.il2cpp_free(type);
        const std::string preparation="Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessScenePreparationStatusResp";
        const std::string dead="Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessBattleSceneDeadAutoObDn";
        const std::string expected=normalized_type(response_type)==normalized_type(dead)?dead:preparation;
        if(normalized_type(response_type)!=normalized_type(expected))throw std::runtime_error("multiplayer response declaration is not implemented");
        if(nmd_pending.size()>=128)throw std::runtime_error("multiplayer native pending callback bound");
        cookie=++nmd_cookie;auto root=hold_new(request,false);if(!root)throw std::runtime_error("multiplayer native request root failed");nmd_pending.emplace(cookie,NMD_Pending{root,expected});
        nmd_send(up,cookie);
    }catch(const cnpreparation_write::Denied& ex){
        // A locally denied preparation write is a business rejection. Preserve
        // the guard, consume the original request once, and suppress its success
        // delegates just as for the server's RetCode.FAIL. Never enqueue/retry it.
        try{
            if(!cookie)throw std::runtime_error("local preparation rejection without pending request");
            auto reply=nmd_last_view;
            auto& payload=reply.object.at("dtoProjection").object.at("preparation").object.at("payload");
            payload.object["code"]=nb_integer(nmd_enum(cls("Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessScenePreparationStatusResp+RetCode"),"FAIL"));
            reply.object["commandRejected"]=nb_object();
            reply.object["commandRejected"].object["error"]=json_text("local_preparation_view_not_owned");
            ++nmd_local_rejections;report(ex.what());
            native_multiplayer_dispatch_complete(cookie,reply,true);
        }catch(const std::exception& failure){nmd_failed(failure);if(cookie)native_multiplayer_dispatch_complete(cookie,cnjson::Value{},false);}
    }catch(const std::exception& ex){nmd_failed(ex);if(cookie)native_multiplayer_dispatch_complete(cookie,cnjson::Value{},false);}
}
void nmd_update(void* mode,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(nmd_scope(mode))return; // No socket/retry timers own the private HTTP room.
    lease.release();nmd_old_update(mode,method_info);
}
int32_t nmd_ping(void* mode,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return -1;if(nmd_scope(mode))return native_multiplayer_transport_ping();lease.release();return nmd_old_ping(mode,method_info);}
bool nmd_server_scope(void* server){
    // ServerBase belongs to the whole application. Managed ownership checks
    // are permitted only on Unity's observed main thread and exact private mode.
    if(!server||!can_read()||unity_ui_thread.load()!=GetCurrentThreadId())return false;
    auto mode=native_multiplayer_entry_mode();if(!nmd_scope(mode))return false;
    return server==field_object(mode,"m_battleSvr")||server==field_object(mode,"m_team1")||server==field_object(mode,"m_team2");
}
int32_t nmd_server_ping(void* server,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return -1;if(nmd_server_scope(server))return native_multiplayer_transport_ping();lease.release();return nmd_old_server_ping(server,method_info);}
bool nmd_server_connected(void* server,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;if(nmd_server_scope(server))return native_multiplayer_transport_live();lease.release();return nmd_old_connected(server,method_info);}
bool nmd_server_alive(void* server,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;if(nmd_server_scope(server))return native_multiplayer_transport_live();lease.release();return nmd_old_alive(server,method_info);}
void nmd_battle_start(void* server,void* raw_entry,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    auto mode=native_multiplayer_entry_mode();
    if(!nmd_scope(mode)||field_object(mode,"m_battleSvr")!=server){lease.release();nmd_old_battle_start(server,raw_entry,method_info);return;}
    try{
        nb_main_thread();TemporaryRoots roots;cnlaunch::CommonJoinEntryRaw raw{};
        if(!raw_entry||!read(raw_entry,raw))throw std::runtime_error("multiplayer private battle join payload unreadable");
        auto entry=roots.retain(value_box(join_entry_class,&raw));const auto id=text(field_object(entry,"id"));
        if(!nmd_snapshot.room.empty()&&id!=nmd_snapshot.room)throw std::runtime_error("multiplayer private battle scene identity mismatch");
        auto info=field_object(server,"<battleInfo>k__BackingField");if(!info)throw std::runtime_error("multiplayer native battle info missing");
        native_multi_entry_store_join(server,raw,true);
        uint8_t yes=1;set_field(info,profile_field(api.il2cpp_object_get_class(info),"started"),&yes);
        // The persisted room view is filled by dispatch_apply; no socket join or
        // CommonJoinEntry server address/token is ever passed to the original.
        // StartBattle reuses the BattleServer allocated by ServiceMode.Init.
        // Keep its already-filled container and readiness through this callback:
        // the prepare controller may launch Unity synchronously before next poll.
    }catch(const std::exception& ex){nmd_failed(ex);}
}
void native_multiplayer_dispatch_apply(const cnjson::Value& view){
    nb_main_thread();auto mode=native_multiplayer_entry_mode();if(!nmd_scope(mode))return;
    auto plan=cnmulti_dispatch::plan(nmd_snapshot,view);if(plan.stale){++nmd_duplicates;return;}
    PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;
    struct Receive {Receive(){++nmd_receive_depth;}~Receive(){--nmd_receive_depth;}} receive;
    native_multiplayer_help_set_carry(view);
    native_multiplayer_help_completion_prepare(view);
    native_multiplayer_observation_prepare(view);
    native_multiplayer_help_counter_prepare(view);
    native_multiplayer_mode_runes_prepare(view);
    native_multiplayer_field_prepare(view);
    if(plan.available){
        TemporaryRoots roots;auto server=roots.retain(field_object(mode,"m_battleSvr"));auto info=roots.retain(field_object(server,"<battleInfo>k__BackingField"));auto scene=roots.retain(field_object(info,"battleSceneData"));
        if(!server||!info||!scene)throw std::runtime_error("multiplayer native scene ownership incomplete");
        const auto& all=nb_at(nb_at(nb_at(nb_at(view,"dtoProjection"),"allStateSync"),"payload"),"allStateSyncData");const auto& detail=nb_at(all,"sceneDetail");
        // A terminal SP choice precedes the following PREPARE SceneStatus.
        // Its durable outbox receipt survives coalescing the HTTP presentation.
        for(const auto& update:plan.special_updates)nb_fill(scene,0x602b5f9,update);
        if(plan.full){
            auto fill=nmd_method(0x602bcfb);auto data=build_value(api.il2cpp_method_get_param(fill,0),all,roots);
            auto scene_id=roots.retain(string_new(plan.next.room.c_str()));void* args[]={data.argument,scene_id};
            native_private_identifier_reset_projection_error();invoke(fill,info,args);native_private_identifier_require_projection_success();++nmd_full;
        }else{
            if(plan.scene){nb_fill(scene,0x602b5f5,detail);++nmd_scenes;}
            if(plan.preparation){nb_fill(scene,0x602b5f6,nb_at(detail,"preparationStatus"));++nmd_preparations;}
            if(plan.players){nb_fill(scene,0x602b5f7,nb_at(detail,"runtimePlayers"));++nmd_players;}
            if(plan.board){nb_fill(scene,0x602b5f8,plan.board_status);++nmd_boards;}
        }
        uint8_t yes=1;set_field(info,profile_field(api.il2cpp_object_get_class(info),"started"),&yes);
        const bool dead_filled=native_multiplayer_dead_observation_fill(view,scene);
        if(plan.full||plan.scene||plan.preparation||plan.players||plan.board||!plan.special_updates.empty()||dead_filled)invoke(nmd_method(0x602bd30),mode);
        nmd_seeded.store(true);
    }
    native_multiplayer_observation_apply(view);
    native_multiplayer_field_apply(view);
    cnmulti_dispatch::append_broadcast_tail(nmd_queued_broadcasts,plan.broadcasts);
    for(const auto& chat:plan.chats){if(nmd_queued_chats.size()==16)nmd_queued_chats.erase(nmd_queued_chats.begin());nmd_queued_chats.push_back(chat);}
    // Advance the immutable envelope cursor before entering arbitrary original
    // broadcast listeners. A managed exception cannot replay this same outbox.
    nmd_snapshot=std::move(plan.next);nmd_last_view=view;
    native_multiplayer_boss_apply(view);
    while(native_multiplayer_dispatch_bridge()&&!nmd_queued_chats.empty()){
        auto event=nmd_queued_chats.front();nmd_queued_chats.erase(nmd_queued_chats.begin());
        if(nb_number(event,"expiresTs")<=native_multiplayer_transport_time())continue;
        auto dto=nb_object();for(const auto* name:{"uid","emojiGroup","emojiId"})dto.object[name]=nb_at(event,name);
        TemporaryRoots roots;auto message=build_value(api.il2cpp_class_get_type(cls("Torappu.AutoChessChatData")),dto,roots).object;
        int32_t evt=nmd_enum(cls("Torappu.UI.AutoChess.Server.AutoChessServiceEvent"),"SCENE_CHAT");
        void* args[]={&evt,message};invoke(nmd_method(0x602bd2a),mode,args);++nmd_chats;
    }
    while(native_multiplayer_dispatch_bridge()&&!nmd_queued_broadcasts.empty()){
        auto event=nmd_queued_broadcasts.front();nmd_queued_broadcasts.erase(nmd_queued_broadcasts.begin());
        auto dto=nb_object();for(const auto* name:{"uIdx","broadcastId","strParams","intParams"})dto.object[name]=nb_at(event,name);
        TemporaryRoots roots;auto message=build_value(api.il2cpp_class_get_type(cls("Torappu.AutoChessBroadcast")),dto,roots).object;int32_t evt=9;void* args[]={&evt,message};invoke(nmd_method(0x602bd2a),mode,args);++nmd_broadcasts;
    }
}
bool native_multiplayer_dispatch_projection_applied(const cnjson::Value& view){
    nb_main_thread();if(!nmd_scope(native_multiplayer_entry_mode())||!nmd_seeded.load())return false;
    // Preparation consumes the exact module snapshot already filled by this
    // dispatcher. Replaying FullFill from its proof coordinator dirties every
    // module a second time and needlessly re-emits the service change event.
    const auto applied=cnmulti_dispatch::plan(nmd_snapshot,view);
    return applied.available&&!applied.stale&&!applied.full&&!applied.scene&&!applied.preparation&&!applied.players&&!applied.board&&applied.special_updates.empty();
}
void native_multiplayer_dispatch_reset(){
    native_multiplayer_dead_observation_reset();
    nb_main_thread();for(auto& item:nmd_pending)if(item.second.root)hold_free(item.second.root);nmd_pending.clear();
    native_multiplayer_help_reset();native_multiplayer_boss_reset();
    native_multiplayer_observation_reset();
    native_multiplayer_help_counter_reset();native_multiplayer_field_reset();native_multiplayer_mode_runes_reset();
    native_preparation_tokens_reset();
    nmd_snapshot={};nmd_last_view={};nmd_queued_broadcasts.clear();nmd_queued_chats.clear();nmd_seeded.store(false);{std::lock_guard<std::mutex> lock(nmd_error_lock);nmd_error.clear();}
}
void native_multiplayer_dispatch_reset_scene(){
    native_multiplayer_dead_observation_reset();
    nb_main_thread();auto mode=native_multiplayer_entry_mode();if(!nmd_scope(mode))throw std::runtime_error("private battle reset without owned service");
    auto info=field_object(field_object(mode,"m_battleSvr"),"<battleInfo>k__BackingField");
    if(!info)throw std::runtime_error("private battle reset container missing");
    PrivateDataScope scope(true);invoke(nmd_method(0x602bcfc),info);
    // A return stays in the same party. Keep acknowledged room/event cursors
    // so its historical outbox cannot replay when a new match starts.
    for(auto& item:nmd_pending)if(item.second.root)hold_free(item.second.root);nmd_pending.clear();
    nmd_snapshot.seeded=false;nmd_snapshot.immutable.clear();nmd_snapshot.scene.clear();nmd_snapshot.preparation.clear();nmd_snapshot.players.clear();
    nmd_queued_broadcasts.clear();nmd_queued_chats.clear();nmd_seeded.store(false);native_multiplayer_help_reset();native_multiplayer_boss_reset();native_multiplayer_observation_reset();native_multiplayer_help_counter_reset();native_multiplayer_field_reset();native_multiplayer_mode_runes_reset();
    native_preparation_tokens_reset();
}
void native_multiplayer_dispatch_setup(){
    // Preflight uses reflection only; Unity object work starts at dispatch_apply.
    for(const auto& spec:NMD_DTO_SPECS){auto c=cls(spec.type);if(!c)throw std::runtime_error("multiplayer DTO class missing");for(const auto& field:spec.fields){auto f=profile_field(c,field.name);if(!matches_type(api.il2cpp_field_get_type(f),field.type)||(api.il2cpp_field_get_flags(f)&16))throw std::runtime_error("multiplayer DTO field preflight mismatch");}nmd_dtos.emplace(c,&spec);}
    for(const auto& spec:NMD_METHODS){if(spec.token==0x602bd23)hook(spec,nmd_request,nmd_old_request);else if(spec.token==0x602bd1f)hook(spec,nmd_update,nmd_old_update);else if(spec.token==0x602bd13)hook(spec,nmd_ping,nmd_old_ping);else if(spec.token==0x602bea5)hook(spec,nmd_battle_start,nmd_old_battle_start);else if(spec.token==0x6008890)hook(spec,nmd_server_ping,nmd_old_server_ping);else if(spec.token==0x60088a0)hook(spec,nmd_server_connected,nmd_old_connected);else if(spec.token==0x60088a1)hook(spec,nmd_server_alive,nmd_old_alive);else bind_method(spec);}
    auto time_class=cls("System.DateTime");uint32_t align=0;if(!time_class||!api.il2cpp_class_is_valuetype(time_class)||api.il2cpp_class_value_size(time_class,&align)!=8)throw std::runtime_error("multiplayer native DateTime ABI mismatch");
    nmd_setup_ready.store(true);
}
std::string native_multiplayer_dispatch_status_json(){std::lock_guard<std::mutex> lock(nmd_error_lock);return "{\"bindingsReady\":"+std::string(nmd_setup_ready.load()?"true":"false")+",\"sceneSeeded\":"+(nmd_seeded.load()?"true":"false")+",\"fullStateFills\":"+std::to_string(nmd_full.load())+",\"sceneFills\":"+std::to_string(nmd_scenes.load())+",\"preparationFills\":"+std::to_string(nmd_preparations.load())+",\"boardFills\":"+std::to_string(nmd_boards.load())+",\"runtimePlayerFills\":"+std::to_string(nmd_players.load())+",\"upstream\":"+std::to_string(nmd_upstream.load())+",\"completions\":"+std::to_string(nmd_completions.load())+",\"broadcasts\":"+std::to_string(nmd_broadcasts.load())+",\"chats\":"+std::to_string(nmd_chats.load())+",\"localWriteRejections\":"+std::to_string(nmd_local_rejections.load())+",\"failures\":"+std::to_string(nmd_failures.load())+",\"lastError\":"+quote(nmd_error)+",\"firstError\":"+quote(nmd_first_error)+",\"lastFailure\":"+quote(nmd_last_failure)+"}";}
