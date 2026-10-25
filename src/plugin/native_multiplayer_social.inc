// Private room result/social boundaries. NPC owns the four original social
// hooks and NPB owns preparation-page close; their early branches call these
// helpers, so no token is hooked twice.
#pragma once
#include <mutex>
inline constexpr CN_Method NMS_GET_PAGE={"Torappu.UI.State","GetPage",0x601adc8,0x141ec30ULL,0,false,"Torappu.UI.UIPage",{}};
inline constexpr CN_Method NMS_GET_ARGS={"Torappu.UI.UIPage","GetArguments",0x601ac23,0x140ee70ULL,0,false,"System.Object",{}};
inline constexpr CN_Method NMS_RESULT_REQUEST={"Torappu.UI.AutoChess.AutoChessSettleGameState","_SendSettleGameReq",0x602b798,0x278d0c0ULL,2,false,"System.Void",{"System.String","Torappu.UI.AutoChess.FromBattleSource"}};
inline constexpr CN_Method NMS_RESULT_PROCEED={"Torappu.UI.AutoChess.AutoChessSettleGameState","_OnSettleGameProceed",0x602b799,0x278ccd0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.AutoChessSettleGameResponse"}};
inline constexpr CN_Method NMS_RESULT_CLOSE={"Torappu.UI.AutoChess.AutoChessPrepareController","_LeavePreparePage",0x602b423,0x2726710ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NMS_RETURN_PARAMS={"Torappu.UI.AutoChess.AutoChessUtil","_CreatePreaparePageParamFromDataBundle",0x602b555,0x27652a0ULL,2,true,"Torappu.UI.AutoChess.AutoChessPreparePage+Params",{"Torappu.DataBundle","Torappu.Battle.AutoChess.AutoChessOutput"}};
inline constexpr CN_Method NMS_RETURN_STRING={"Torappu.DataBundle","GetString",0x600649d,0x1df79b0ULL,2,false,"System.String",{"System.String","System.String"}};
inline constexpr CN_Method NMS_RETURN_DESERIALIZE={"Torappu.UI.AutoChess.AutoChessPreparePage+Params","Deserialize",0x602b4c5,0x2766a50ULL,1,true,"Torappu.UI.AutoChess.AutoChessPreparePage+Params",{"System.String"}};
inline constexpr CN_Method NMS_PREPARE_SYNC={"Torappu.UI.AutoChess.AutoChessPrepareController","_HandleSyncInfoResponse",0x602b3cd,0x2725960ULL,1,false,"System.Void",{"Torappu.Activity.AutoChess.ActAutoChessSyncInfoResponse"}};
inline constexpr CN_Method NMS_PREPARE_ROUTE={"Torappu.UI.AutoChess.AutoChessPrepareController","_RouteToProperState",0x602b3d0,0x27292f0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.AutoChessPrepareStateViewType"}};
using NMS_PrepareRoute=void(*)(void*,int32_t,const void*);
NMS_PrepareRoute nms_old_prepare_route=nullptr;
thread_local void* nms_terminal_routing=nullptr;
using NMS_ResultRequest=void(*)(void*,void*,int32_t,const void*);
NMS_ResultRequest nms_old_result_request=nullptr;
using NMS_ResultProceed=void(*)(void*,void*,const void*);
NMS_ResultProceed nms_old_result_proceed=nullptr;
thread_local unsigned nms_presenting_result=0;
using NMS_ReturnParams=void*(*)(void*,void*,const void*);
NMS_ReturnParams nms_old_return_params=nullptr;
using NMS_ReturnDeserialize=void*(*)(void*,const void*);
NMS_ReturnDeserialize nms_old_return_deserialize=nullptr;
void native_multiplayer_entry_close_result(void*);
std::string nms_result_room,nms_result_uid;
// The personal result model independently reads the cached SDK UID, unlike
// the team model. Retain it before normalizing this private page's identity.
std::string nms_personal_sdk_uid;
void nms_capture_personal_uid(void* model){
    const auto uid=text(field_object(model,"<selfUid>k__BackingField"));
    if(!uid.empty()&&uid.size()<=128&&uid.find_first_not_of("0123456789")==std::string::npos)nms_personal_sdk_uid=uid;
}
preparation_view::Session nms_result_origin;
std::atomic<unsigned> nms_results{0},nms_failures{0};
std::atomic<unsigned> nms_decoder_calls{0},nms_decoder_matches{0},nms_deserialize_calls{0},nms_deserialize_matches{0},nms_result_requests{0},nms_result_forwards{0};
std::atomic<bool> nms_bundle_staged{false},nms_string_staged{false},nms_params_staged{false};
std::mutex nms_status_lock;std::string nms_first_error;
std::recursive_mutex nms_return_lock;
std::atomic<unsigned> nms_native_http_routes{0};
int64_t nms_return_interrupt_status=-1;
struct NMS_ReturnProof {preparation_view::Session origin;std::string uid,mode;int64_t seat=-1;uint64_t epoch=0;bool interrupted=false;} nms_return_proof;

void native_multiplayer_social_forget_return(){
    std::lock_guard<std::recursive_mutex> guard(nms_return_lock);
    nms_return_proof={};drop_cache("native-multiplayer-return-bundle");drop_cache("native-multiplayer-return-string");drop_cache("native-multiplayer-return-params");
    drop_cache("native-multiplayer-terminal-routed-params");
    nms_return_interrupt_status=-1;
    nms_bundle_staged=false;nms_string_staged=false;nms_params_staged=false;
}
void nms_failure(const char* stage,const std::exception& ex){
    ++nms_failures;{std::lock_guard<std::mutex> lock(nms_status_lock);if(nms_first_error.empty())nms_first_error=(std::string(stage)+": "+ex.what()).substr(0,512);}
    report((std::string("private-multiplayer-")+stage+"-rejected: "+ex.what()).c_str());
}
std::string native_multiplayer_social_status_json(){
    std::lock_guard<std::mutex> lock(nms_status_lock);cnjson::Value error;error.kind=cnjson::Value::String;error.string=nms_first_error;
    return "{\"results\":"+std::to_string(nms_results.load())+",\"failures\":"+std::to_string(nms_failures.load())+
        ",\"decoderCalls\":"+std::to_string(nms_decoder_calls.load())+",\"decoderMatches\":"+std::to_string(nms_decoder_matches.load())+
        ",\"deserializeCalls\":"+std::to_string(nms_deserialize_calls.load())+",\"deserializeMatches\":"+std::to_string(nms_deserialize_matches.load())+
        ",\"resultRequests\":"+std::to_string(nms_result_requests.load())+",\"forwardedResultRequests\":"+std::to_string(nms_result_forwards.load())+
        ",\"bundleStaged\":"+(nms_bundle_staged.load()?"true":"false")+",\"stringStaged\":"+(nms_string_staged.load()?"true":"false")+
        ",\"paramsStaged\":"+(nms_params_staged.load()?"true":"false")+",\"nativeHttpRoutes\":"+std::to_string(nms_native_http_routes.load())+",\"firstError\":"+cnjson::dump(error)+"}";
}

void* nms_arguments(void* page,TemporaryRoots& roots){return roots.retain(invoke(bound_methods.at(NMS_GET_ARGS.token),page));}
void nms_sync_terminal_input(void* page,void* args,TemporaryRoots& roots){
    // OnCreate copies Params into a value-type Input. LoadData subsequently
    // reads that copy, and a null sync battleInfo falls back to Input.battleInfo.
    // Keep both native inputs consistent; changing only the model is transient.
    auto input=native_battle_boxed_field(page,"m_inputData",roots);
    if(!input||api.il2cpp_object_get_class(input)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage+Input")||
       text(field_object(input,"actId"))!="act2autochess")throw std::runtime_error("private terminal page Input differs");
    for(const auto name:{"modeType","multiModeSubType","fromBattleSource"}){
        int32_t value=nb_native_int(args,name);set_field(input,profile_field(api.il2cpp_object_get_class(input),name),&value);
    }
    set_object_field(input,profile_field(api.il2cpp_object_get_class(input),"battleInfo"),nullptr);
    set_field(page,profile_field(api.il2cpp_object_get_class(page),"m_inputData"),unbox(input));
    set_object_field(args,profile_field(api.il2cpp_object_get_class(args),"battleInfo"),nullptr);
}
bool nms_result_arguments(void* args){
    return args&&api.il2cpp_object_get_class(args)==cls("Torappu.UI.AutoChess.AutoChessPreparePage+Params")&&
        text(field_object(args,"actId"))=="act2autochess"&&nb_native_int(args,"modeType")==2&&
        nb_native_int(args,"multiModeSubType")==2&&nb_native_int(args,"fromBattleSource")==1;
}
cnjson::Value nms_view(){
    auto view=native_multiplayer_transport_snapshot();
    if(native_multi_entry_string(view,"activityId")!="act2autochess"||native_multi_entry_string(view,"roomId").empty()||
       native_multi_entry_string(view,"roomId")!=native_multiplayer_transport_room())throw std::runtime_error("private result room identity unavailable");
    const auto& projection=nb_at(view,"dtoProjection");const auto uid=nb_string(projection,"privateSelfUid");
    if(uid.rfind("private-",0)!=0||uid.size()<=8)throw std::runtime_error("private result participant identity unavailable");
    return view;
}
bool nms_committed_interrupt_status(const cnjson::Value& view,int64_t status){
    if(status<0)return false;
    auto state=native_multi_entry_field(view,"selfState"),exit=native_multi_entry_field(view,"nativeSceneExit");
    // A committed personal QuitUp can precede the surviving team's result.
    // Use the recipient-bound departure and INTERRUPT row, not team phase.
    auto presence=native_multi_entry_field(view,"selfScenePresence");
    auto flag=[&](const char* name,bool expected){auto value=presence?native_multi_entry_field(*presence,name):nullptr;return value&&value->kind==cnjson::Value::Boolean&&value->boolean==expected;};
    if(!state||!exit||!presence||native_multi_entry_string(*state,"runId").empty()||
       !flag("quit",true)||!flag("sceneLeft",true)||!flag("returned",false)||
       nb_number(*presence,"seat")!=nb_number(view,"seat")||nb_number(*presence,"matchSequence")!=nb_number(view,"matchSequence")||
       native_multi_entry_string(*exit,"kind")!="voluntary"||
       native_multi_entry_string(*exit,"roomId")!=native_multi_entry_string(view,"roomId")||nb_number(*exit,"reason")!=7||
       nb_number(*exit,"seat")!=nb_number(view,"seat")||nb_number(*exit,"matchSequence")!=nb_number(view,"matchSequence")||
       native_multi_entry_string(view,"nativeSettleGameInfoType")!="Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo")return false;
    const auto& summary=nb_at(view,"nativeSettleGameInfo");const auto uid=nb_string(nb_at(view,"dtoProjection"),"privateSelfUid");
    for(const auto& row:nb_at(summary,"teamInfo").array)
        if(nb_string(row,"uid")==uid&&nb_number(row,"uidIndex")==nb_number(view,"seat")&&nb_number(row,"gameCode")==status)return true;
    return false;
}
bool nms_committed_interrupt(const cnjson::Value& view){
    nb_main_thread();
    return nms_committed_interrupt_status(view,nmd_enum(cls("Torappu.UI.AutoChess.AutoChessSettleGamePlayerStatus"),"INTERRUPT"));
}
bool nms_return_matches(const cnjson::Value& view){
    std::lock_guard<std::recursive_mutex> guard(nms_return_lock);
    return nms_return_proof.origin.valid()&&nms_return_proof.epoch==native_multi_entry_service_epoch&&
        (native_multi_entry_string(view,"phase")=="result"||(nms_return_proof.interrupted&&nms_committed_interrupt_status(view,nms_return_interrupt_status)))&&preparation_view::session(view)==nms_return_proof.origin&&
        native_multi_entry_string(view,"modeId")==nms_return_proof.mode&&nb_number(view,"seat")==nms_return_proof.seat&&
        nb_string(nb_at(view,"dtoProjection"),"privateSelfUid")==nms_return_proof.uid;
}
void native_multiplayer_social_stage_return(const preparation_view::Session& origin){
    std::lock_guard<std::recursive_mutex> guard(nms_return_lock);
    try{
    nb_main_thread();
    auto view=native_multiplayer_transport_snapshot();
    // An accepted Leave may already have cleared the transport before native
    // Service.Dispose runs. A nonterminal/lobby dispose needs no result proof.
    if(native_multiplayer_transport_room()!=origin.room){if(nms_return_proof.origin==origin)native_multiplayer_social_forget_return();return;}
    const bool interrupted=native_multiplayer_entry_voluntary_exit()&&nms_committed_interrupt(view);
    if(native_multi_entry_string(view,"phase")!="result"&&!interrupted){if(nms_return_proof.origin==origin)native_multiplayer_social_forget_return();return;}
    view=nms_view();
    if(!origin.valid()||!(preparation_view::session(view)==origin)||!native_multi_entry_mode_allowed(native_multi_entry_string(view,"modeId")))
        throw std::runtime_error("private terminal return origin differs");
    nb_main_thread();TemporaryRoots roots;
    auto singleton=roots.retain(invoke(method(cls("Torappu.Battle.BattleInOut"),"get_instance",0),nullptr));
    auto input=singleton?roots.retain(invoke(bound_methods.at(CN_BATTLE_IN_PARAMS.token),singleton)):nullptr;
    auto bundle=native_battle_boxed_field(input,"bundleToJumpBack",roots);
    if(!bundle||api.il2cpp_object_get_class(bundle)!=cls("Torappu.DataBundle"))throw std::runtime_error("private terminal return bundle unavailable");
    auto key=roots.retain(string_new("key_auto_chess_param"));void* args[]={key,nullptr};
    auto serialized=roots.retain(invoke(bound_methods.at(NMS_RETURN_STRING.token),bundle,args));
    if(!serialized||api.il2cpp_object_get_class(serialized)!=cls("System.String")||text(serialized).empty())throw std::runtime_error("private terminal return serialized Params unavailable");
    // DataBundle.GetString -> Get<string> returns the exact stored reference.
    // Do not let a hotfix that recreates strings turn content into ownership.
    if(invoke(bound_methods.at(NMS_RETURN_STRING.token),bundle,args)!=serialized)throw std::runtime_error("private terminal return string identity unstable");
    if(!nms_return_matches(view)||nb_retained("native-multiplayer-return-bundle")!=bundle||nb_retained("native-multiplayer-return-string")!=serialized)native_multiplayer_social_forget_return();
    nms_return_proof={origin,nb_string(nb_at(view,"dtoProjection"),"privateSelfUid"),nb_string(view,"modeId"),nb_number(view,"seat"),native_multi_entry_service_epoch,interrupted};
    nms_return_interrupt_status=interrupted?nmd_enum(cls("Torappu.UI.AutoChess.AutoChessSettleGamePlayerStatus"),"INTERRUPT"):-1;
    protect("native-multiplayer-return-bundle",bundle);protect("native-multiplayer-return-string",serialized);nms_bundle_staged=true;nms_string_staged=true;
    }catch(const std::exception& ex){nms_failure("return-stage",ex);throw;}
}
void nms_bind_return_params(void* params,const cnjson::Value& view){
    std::lock_guard<std::recursive_mutex> guard(nms_return_lock);
    if(!nms_return_matches(view)||!nms_result_arguments(params))throw std::runtime_error("private terminal return Params differ");
    protect("native-multiplayer-return-params",params);nms_params_staged=true;
}
std::string native_multiplayer_social_settlement_url(){
    // A native/hotfixed UI sender may issue settleGame without entering the
    // auxiliary _SendSettleGameReq detour. Keep its real HTTP response shape,
    // but authorize this route only from the exact terminal return Params.
    std::lock_guard<std::recursive_mutex> guard(nms_return_lock);
    auto view=nms_view();
    if(!nms_params_staged.load()||!nms_return_matches(view))throw std::runtime_error("private native settlement return proof unavailable");
    const auto& origin=nms_return_proof.origin;
    const auto valid_id=[](const std::string& value){return value.size()==32&&value.find_first_not_of("0123456789abcdef")==std::string::npos;};
    if(!valid_id(origin.room)||!valid_id(origin.run)||origin.match<=0||origin.party)throw std::runtime_error("private native settlement route identity invalid");
    ++nms_native_http_routes;
    return "/bridge/act2autochess/multiplayer/rooms/"+origin.room+"/native-settlement/"+std::to_string(origin.match)+"/"+origin.run;
}
void* nms_return_deserialize(void* serialized,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()||!can_read()){lease.release();return nms_old_return_deserialize(serialized,info);}
    ++nms_deserialize_calls;auto params=nms_old_return_deserialize(serialized,info);
    try{
        // GetSceneParam inlines _CreatePreaparePageParamFromDataBundle, but
        // still calls Params.Deserialize at 27622c9. Preserve that native path.
        // It can run before Dispose, so stage the same live bundle if needed.
        if(serialized&&serialized!=nb_retained("native-multiplayer-return-string")&&native_multiplayer_entry_active()){
            auto view=nms_view();if(native_multi_entry_string(view,"phase")=="result"||native_multiplayer_entry_voluntary_exit())native_multiplayer_social_stage_return(preparation_view::session(native_multi_entry_snapshot));
        }
        if(serialized&&serialized==nb_retained("native-multiplayer-return-string")){
            nb_main_thread();nms_bind_return_params(params,nms_view());++nms_deserialize_matches;
        }
    }catch(const std::exception& ex){nms_failure("return-deserialize",ex);}
    return params;
}
void* nms_return_params(void* bundle,void* output,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()||!can_read()){lease.release();return nms_old_return_params(bundle,output,info);}
    ++nms_decoder_calls;auto params=nms_old_return_params(bundle,output,info);
    try{
        // Some native return routes decode the bundle just before disposing the
        // old service. Bind that same input while it still has an entry owner.
        if(bundle&&bundle!=nb_retained("native-multiplayer-return-bundle")&&native_multiplayer_entry_active()){
            auto view=nms_view();if(native_multi_entry_string(view,"phase")=="result"||native_multiplayer_entry_voluntary_exit())native_multiplayer_social_stage_return(preparation_view::session(native_multi_entry_snapshot));
        }
        // This exact native decoder preserves the engine's normal result-page
        // route. Matching mode/source fields alone cannot claim another page.
        if(bundle&&bundle==nb_retained("native-multiplayer-return-bundle")){
            nb_main_thread();nms_bind_return_params(params,nms_view());++nms_decoder_matches;
        }
    }catch(const std::exception& ex){nms_failure("return",ex);}
    return params;
}
bool native_multiplayer_social_return_owner(void* self){
    if(!self||self!=nb_retained("native-multiplayer-result-controller")||!nms_return_proof.origin.valid())return false;
    TemporaryRoots roots;auto page=roots.retain(field_object(self,"m_page"));auto params=page?nms_arguments(page,roots):nullptr;
    return params&&params==nb_retained("native-multiplayer-return-params")&&nms_result_arguments(params)&&nms_return_matches(nms_view());
}
void native_multiplayer_social_resume_result(void* self,const cnjson::Value& snapshot){
    if(native_multi_entry_string(snapshot,"phase")!="result")return;
    nb_main_thread();
    if(native_multi_entry_string(snapshot,"activityId")!="act2autochess"||
       native_multi_entry_string(snapshot,"roomId")!=native_multiplayer_transport_room()||
       !native_multi_entry_mode_allowed(native_multi_entry_string(snapshot,"modeId")))throw std::runtime_error("private result resume room identity mismatch");
    const auto uid=nb_string(nb_at(snapshot,"dtoProjection"),"privateSelfUid");
    if(uid.rfind("private-",0)!=0||uid.size()<=8)throw std::runtime_error("private result resume participant missing");
    if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessPrepareController")||
       text(field_object(self,"m_actId"))!="act2autochess")throw std::runtime_error("private result resume controller mismatch");
    TemporaryRoots roots;auto page=roots.retain(field_object(self,"m_page"));
    if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage")||field_object(page,"_controller")!=self)
        throw std::runtime_error("private result resume page mismatch");
    auto args=nms_arguments(page,roots);auto model=roots.retain(field_object(self,"m_model"));
    if(!args||api.il2cpp_object_get_class(args)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage+Params")||text(field_object(args,"actId"))!="act2autochess"||
       !model||api.il2cpp_object_get_class(model)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||text(field_object(model,"<actId>k__BackingField"))!="act2autochess")
        throw std::runtime_error("private result resume model/Params mismatch");
    int32_t multi=2,team=2,source=1;
    const auto pending= snapshot.object.find("partyReturn");
    if(pending!=snapshot.object.end())for(const auto& seat:nb_at(pending->second,"returnedSeats").array)
        if(seat.kind==cnjson::Value::Integer&&seat.integer==nb_number(snapshot,"seat"))source=0;
    for(const auto& value:std::initializer_list<std::pair<const char*,int32_t*>>{{"modeType",&multi},{"multiModeSubType",&team},{"fromBattleSource",&source}})
        set_field(args,profile_field(api.il2cpp_object_get_class(args),value.first),value.second);
    nms_sync_terminal_input(page,args,roots);
    for(const auto& value:std::initializer_list<std::pair<const char*,int32_t*>>{{"<modeType>k__BackingField",&multi},{"<multiModeSubType>k__BackingField",&team},{"<fromBattleSource>k__BackingField",&source}})
        set_field(model,profile_field(api.il2cpp_object_get_class(model),value.first),value.second);
    nms_capture_personal_uid(model);
    set_object_field(model,profile_field(api.il2cpp_object_get_class(model),"<selfUid>k__BackingField"),roots.retain(string_new(uid.c_str())));
    // A fresh process resumes the committed result through an existing native
    // page, with no BattleInOut return bundle. Bind that exact validated page
    // Params to the authenticated terminal snapshot on the UI thread.
    std::lock_guard<std::recursive_mutex> guard(nms_return_lock);
    native_multiplayer_social_forget_return();
    if(source==1){
        auto view=nms_view();
        if(!nms_result_arguments(args)||!(preparation_view::session(view)==preparation_view::session(snapshot))||nb_number(view,"seat")!=nb_number(snapshot,"seat")||nb_string(nb_at(view,"dtoProjection"),"privateSelfUid")!=uid)
            throw std::runtime_error("private resumed terminal Params owner differs");
        nms_return_proof={preparation_view::session(view),uid,nb_string(view,"modeId"),nb_number(view,"seat"),native_multi_entry_service_epoch,false};
        protect("native-multiplayer-return-params",args);nms_params_staged=true;
    }
}
bool native_multiplayer_social_recover_terminal_page(void* self){
    // _InitViewType tests address/token before fromBattleSource. An ended
    // private room still has persisted reconnect metadata, so the returning
    // page can select RECONNECT instead of SETTLE. Repair only that exact live
    // return page; keep the persisted Continue entry for fresh-process recovery.
    if(native_battle_launch_in_flight.load()||native_battle_launch_pending.load())return false;
    nb_main_thread();
    if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessPrepareController")||text(field_object(self,"m_actId"))!="act2autochess")return false;
    TemporaryRoots roots;auto page=roots.retain(field_object(self,"m_page")),model=roots.retain(field_object(self,"m_model"));
    if(!page||!model||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage")||field_object(page,"_controller")!=self||
       api.il2cpp_object_get_class(model)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||text(field_object(model,"<actId>k__BackingField"))!="act2autochess"||
       nb_native_int(model,"<modeType>k__BackingField")!=2||nb_native_int(model,"<multiModeSubType>k__BackingField")!=2||nb_native_int(model,"<curViewType>k__BackingField")!=8)return false;
    auto args=nms_arguments(page,roots);if(!nms_result_arguments(args))return false;
    const bool returned=args==nb_retained("native-multiplayer-return-params");
    if(!native_multiplayer_entry_active()&&!returned)return false;
    auto view=nms_view();
    const bool proved=returned&&nms_return_matches(view);
    if(!proved&&(!native_multiplayer_entry_active()||native_multi_entry_string(view,"phase")!="result"||
       !(preparation_view::session(view)==preparation_view::session(native_multi_entry_snapshot))))return false;
    auto presence=roots.retain(field_object(model,"<battleInfo>k__BackingField"));
    if(!presence||api.il2cpp_object_get_class(presence)!=cls("Torappu.Activity.AutoChess.ActAutoChessSyncInfoBattleInfo")||
       text(field_object(presence,"sceneId"))!=nb_string(view,"roomId")||text(field_object(presence,"modeId"))!=nb_string(view,"modeId")||
       text(field_object(presence,"address"))!="private-http-room"||text(field_object(presence,"token"))!="private-in-process-adapter")return false;
    // Bind the authenticated return before the original handler synchronously
    // enters its native settlement state (including the installed Lua sender).
    if(nb_retained("native-multiplayer-terminal-routed-params")==args&&nms_return_matches(view))return true;
    if(!proved)native_multiplayer_social_resume_result(self,view);
    if(!nms_result_arguments(args))return false; // Already acknowledged party return.
    nms_sync_terminal_input(page,args,roots);
    auto response=build_value(api.il2cpp_class_get_type(cls("Torappu.Activity.AutoChess.ActAutoChessSyncInfoResponse")),cnjson::parse("{\"changed\":[],\"battleInfo\":null}"),roots).object;
    if(!response)throw std::runtime_error("private terminal sync response unavailable");
    struct Routing{void* previous;explicit Routing(void* value):previous(nms_terminal_routing){nms_terminal_routing=value;}~Routing(){nms_terminal_routing=previous;}} routing(self);
    void* values[]={response};PrivateDataScope scope(true);invoke(bound_methods.at(NMS_PREPARE_SYNC.token),self,values);
    if(nb_native_int(model,"<curViewType>k__BackingField")!=6)throw std::runtime_error("private terminal sync did not select native settlement");
    protect("native-multiplayer-terminal-routed-params",args);
    report("private-terminal-return-routed-to-native-settlement");
    return true;
}
void nms_prepare_route(void* self,int32_t viewType,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{
        // Choose the terminal route before RECONNECT starts its native state.
        // The return must not depend on a later page Update to undo that
        // selection. The original sync
        // handler reloads the model and reenters here with SETTLE (6).
        if(can_read()&&viewType==8){
            if(self&&self==nms_terminal_routing)throw std::runtime_error("private terminal sync reentered reconnect");
            if(native_multiplayer_social_recover_terminal_page(self))return;
        }
    }catch(const std::exception& ex){nms_failure("terminal-route",ex);return;}
    lease.release();nms_old_prepare_route(self,viewType,info);
}
bool native_multiplayer_social_normalize(void* self){
    const bool retained=self&&self==nb_retained("native-multiplayer-result-controller");
    if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessPrepareController")||
       text(field_object(self,"m_actId"))!="act2autochess"){
        if(retained)throw std::runtime_error("private retained controller identity changed");return false;
    }
    nb_main_thread();TemporaryRoots roots;auto page=roots.retain(field_object(self,"m_page"));
    if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage")||field_object(page,"_controller")!=self){
        if(retained)throw std::runtime_error("private retained page identity changed");return false;
    }
    auto model=roots.retain(field_object(self,"m_model"));
    if(!model||api.il2cpp_object_get_class(model)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||
       text(field_object(model,"<actId>k__BackingField"))!="act2autochess"||nb_native_int(model,"<modeType>k__BackingField")!=2||
       nb_native_int(model,"<multiModeSubType>k__BackingField")!=2){
        if(retained)throw std::runtime_error("private retained model identity changed");return false;
    }
    auto args=nms_arguments(page,roots);const auto uid=text(field_object(model,"<selfUid>k__BackingField"));
    if(retained){
        if(!args||args!=nb_retained("native-multiplayer-result-params"))throw std::runtime_error("private retained Params identity changed");
        if(!nms_result_arguments(args)||nms_result_room.empty()||nms_result_uid.empty()||uid!=nms_result_uid)
            throw std::runtime_error("private retained result identity changed");
        return true;
    }
    const bool returned=nms_result_arguments(args);
    const bool disposed_return=args&&args==nb_retained("native-multiplayer-return-params");
    if(!native_multiplayer_entry_active()&&!disposed_return)return false;
    if(self!=hold_get(native_multi_entry_controller_root)&&!returned)return false;
    auto view=nms_view();const auto expected=nb_string(nb_at(view,"dtoProjection"),"privateSelfUid");
    if(disposed_return&&(!returned||!nms_return_matches(view)))throw std::runtime_error("private terminal return receipt changed");
    // Native PrepareModel.LoadData unconditionally reads the SDK UID. The exact
    // active private MULTI/TEAM return page must recover its room actor before
    // _LoadFromSettleData searches teamInfo and builds the native team view.
    if(uid!=expected){
        if(!returned)throw std::runtime_error("private preparation participant identity changed");
        nms_capture_personal_uid(model);
        set_object_field(model,profile_field(api.il2cpp_object_get_class(model),"<selfUid>k__BackingField"),roots.retain(string_new(expected.c_str())));
    }
    if(returned){
        native_multi_entry_root(native_multi_entry_controller_root,self);
        // Only the result/Leave owner is restored. The retired mode and service
        // stay disposed; transport keeps its terminal receipt and journal.
        if(disposed_return){native_multi_entry_snapshot=view;native_multi_entry_room=nb_string(view,"roomId");}
    }
    return true;
}
bool native_multiplayer_social_controller(void* self,void* uid){
    // Numeric/ordinary account identities keep their original behavior.
    if(text(uid).rfind("private-",0)!=0)return false;
    return native_multiplayer_social_normalize(self);
}
bool native_multiplayer_social_state(void* self){
    if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessSettleGameState"))return false;
    nb_main_thread();TemporaryRoots roots;auto page=roots.retain(invoke(bound_methods.at(NMS_GET_PAGE.token),self));
    if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage"))return false;
    return native_multiplayer_social_normalize(field_object(page,"_controller"));
}
bool native_multiplayer_social_result_close(void* self,const void*){
    // This is a consumed private MULTI result, not an arbitrary preparation
    // page. The original BackToRoom edge remains QuitUp -> BackTeamUp.
    if(!can_read()||!self||self!=nb_retained("native-multiplayer-result-controller")||native_multiplayer_transport_room().empty())return false;
    try{
        if(!native_multiplayer_social_normalize(self))throw std::runtime_error("private result close controller identity changed");
        auto view=nms_view();
        if((nb_string(view,"phase")!="result"&&!native_multiplayer_entry_voluntary_exit()&&!native_multiplayer_social_return_owner(self))||nb_string(view,"roomId")!=nms_result_room||
           nb_string(nb_at(view,"dtoProjection"),"privateSelfUid")!=nms_result_uid)
            throw std::runtime_error("private result close belongs to another room");
        native_multiplayer_entry_close_result(self);
    }catch(const std::exception& ex){nms_failure("result-close",ex);}
    return true;
}
void nms_present_result(void* self){
        auto view=nms_view();if((nb_string(view,"phase")!="result"&&!native_multiplayer_entry_voluntary_exit()&&!(nms_return_proof.interrupted&&nms_return_matches(view)))||nb_string(view,"nativeSettleGameInfoType")!="Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo")
            throw std::runtime_error("private MULTI settlement is not committed");
        TemporaryRoots page_roots;auto result_page=page_roots.retain(invoke(bound_methods.at(NMS_GET_PAGE.token),self));
        if(field_object(result_page,"_controller")==nb_retained("native-multiplayer-result-controller")&&nms_result_origin.valid()&&
           !(preparation_view::session(view)==nms_result_origin))throw std::runtime_error("private retained settlement match changed");
        auto summary=nb_at(view,"nativeSettleGameInfo");const auto& projection=nb_at(view,"dtoProjection");const auto uid=nb_string(projection,"privateSelfUid");
        if(nb_string(summary,"modeId")!=nb_string(view,"modeId"))throw std::runtime_error("private settlement mode mismatch");
        const auto& teams=nb_at(summary,"teamInfo");if(teams.kind!=cnjson::Value::Array||teams.array.empty()||teams.array.size()>4)throw std::runtime_error("private settlement team count invalid");
        unsigned self_count=0;std::set<int64_t> seats;std::set<std::string> identities;
        for(const auto& row:teams.array){
            const auto index=nb_number(row,"uidIndex");const auto member=nb_string(row,"uid");
            if(index<0||index>3||member.rfind("private-",0)!=0||!seats.insert(index).second||!identities.insert(member).second)
                throw std::runtime_error("private settlement team identity invalid");
            bool roster_member=false;for(const auto& participant:nb_at(view,"roster").array)
                if(nb_number(participant,"seat")==index&&"private-"+nb_string(participant,"participantId")==member)roster_member=true;
            if(!roster_member)throw std::runtime_error("private settlement participant outside room");
            if(member==uid){if(index!=nb_number(view,"seat"))throw std::runtime_error("private settlement recipient seat mismatch");++self_count;}
        }
        if(self_count!=1)throw std::runtime_error("private settlement recipient missing");
        // Room cards already contain each participant's synchronized cached
        // display data. Keep the same projection for self and remote viewers.
        auto response=nb_object();response.object["result"]=nb_integer(0);response.object["gameSettleData"]=summary;
        TemporaryRoots roots;auto native=build_value(api.il2cpp_class_get_type(cls("Torappu.UI.AutoChess.AutoChessSettleGameResponse")),response,roots).object;
        if(!native)throw std::runtime_error("private settlement native response unavailable");
        auto page=roots.retain(invoke(bound_methods.at(NMS_GET_PAGE.token),self));auto args=nms_arguments(page,roots);auto controller=roots.retain(field_object(page,"_controller"));
        if(!nms_result_arguments(args))throw std::runtime_error("private settlement return Params changed");
        protect("native-multiplayer-result-params",args);protect("native-multiplayer-result-controller",controller);protect("native-multiplayer-result-response",native);
        nms_result_room=nb_string(view,"roomId");nms_result_uid=uid;nms_result_origin=preparation_view::session(view);
        PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;
        struct PresentScope{PresentScope(){++nms_presenting_result;}~PresentScope(){--nms_presenting_result;}} presenting;
        native_private_identifier_reset_projection_error();void* values[]={native};invoke(bound_methods.at(NMS_RESULT_PROCEED.token),self,values);native_private_identifier_require_projection_success();++nms_results;
}
void nms_result_request(void* self,void* activity,int32_t source,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(text(activity)!="act2autochess"){lease.release();nms_old_result_request(self,activity,source,info);return;}
    ++nms_result_requests;
    bool forward=false;
    try{
        forward=[&](){
        if(!native_multiplayer_social_state(self))return true;
        nb_main_thread();if(source!=1)throw std::runtime_error("private MULTI settlement source mismatch");
        nms_present_result(self);
        return false;}();
    }catch(const std::exception& ex){nms_failure("settlement",ex);}
    if(forward){++nms_result_forwards;lease.release();nms_old_result_request(self,activity,source,info);}
}
void nms_result_proceed(void* self,void* response,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(nms_presenting_result){lease.release();nms_old_result_proceed(self,response,info);return;}
    bool forward=false;
    try{
        forward=[&](){
            if(!can_read()||!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessSettleGameState"))return true;
            nb_main_thread();TemporaryRoots roots;auto page=roots.retain(invoke(bound_methods.at(NMS_GET_PAGE.token),self));
            if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage"))return true;
            auto args=nms_arguments(page,roots);
            if(!args||(args!=nb_retained("native-multiplayer-return-params")&&args!=nb_retained("native-multiplayer-result-params")))return true;
            auto view=nms_view();
            if(!nms_result_arguments(args)||!nms_return_matches(view)||!native_multiplayer_social_normalize(field_object(page,"_controller")))
                throw std::runtime_error("private native settlement callback owner differs");
            if(!response)return true;
            if(api.il2cpp_object_get_class(response)!=cls("Torappu.UI.AutoChess.AutoChessSettleGameResponse"))throw std::runtime_error("private native settlement callback response type differs");
            if(nb_native_int(response,"result")!=0)return true;
            auto summary=roots.retain(field_object(response,"gameSettleData"));
            if(!summary||api.il2cpp_object_get_class(summary)!=cls("Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo")||text(field_object(summary,"modeId"))!=nb_string(view,"modeId"))
                throw std::runtime_error("private native settlement callback summary differs");
            // Both native entry points share the same recipient normalization,
            // canonical immutable result and close-owner registration. The
            // narrow presenting guard delegates the inner invoke to native.
            nms_present_result(self);return false;
        }();
    }catch(const std::exception& ex){nms_failure("native-result-callback",ex);}
    if(forward){lease.release();nms_old_result_proceed(self,response,info);}
}
void native_multiplayer_social_setup(){
    bind_method(NMS_GET_PAGE);bind_method(NMS_GET_ARGS);bind_method(NMS_RETURN_STRING);bind_method(NMS_PREPARE_SYNC);
    for(const auto& row:std::initializer_list<std::tuple<const char*,const char*,const char*,size_t>>{
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_actId","System.String",64},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_page","Torappu.UI.AutoChess.AutoChessPreparePage",80},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_model","Torappu.UI.AutoChess.AutoChessPrepareModel",128},
        {"Torappu.UI.AutoChess.AutoChessPreparePage","_controller","Torappu.UI.AutoChess.AutoChessPrepareController",248},
        {"Torappu.UI.AutoChess.AutoChessPreparePage","m_inputData","Torappu.UI.AutoChess.AutoChessPreparePage+Input",264},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Input","actId","System.String",16},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Input","modeType","Torappu.ActAutoChessModeType",24},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Input","multiModeSubType","Torappu.ActAutoChessMultiModeSubType",28},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Input","fromBattleSource","Torappu.UI.AutoChess.FromBattleSource",32},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Input","battleInfo","Torappu.Activity.AutoChess.ActAutoChessSyncInfoBattleInfo",40},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","battleInfo","Torappu.Activity.AutoChess.ActAutoChessSyncInfoBattleInfo",40},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","actId","System.String",16},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","modeType","Torappu.ActAutoChessModeType",24},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","multiModeSubType","Torappu.ActAutoChessMultiModeSubType",28},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","fromBattleSource","Torappu.UI.AutoChess.FromBattleSource",32},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<actId>k__BackingField","System.String",32},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<modeType>k__BackingField","Torappu.ActAutoChessModeType",44},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<multiModeSubType>k__BackingField","Torappu.ActAutoChessMultiModeSubType",48},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<fromBattleSource>k__BackingField","Torappu.UI.AutoChess.FromBattleSource",52},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<selfUid>k__BackingField","System.String",72},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<battleInfo>k__BackingField","Torappu.Activity.AutoChess.ActAutoChessSyncInfoBattleInfo",56},
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<curViewType>k__BackingField","Torappu.UI.AutoChess.AutoChessPrepareStateViewType",64},
        {"Torappu.UI.AutoChess.AutoChessSettleGameResponse","result","System.Int32",40},
        {"Torappu.UI.AutoChess.AutoChessSettleGameResponse","gameSettleData","Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo",48}
    })nbb_field(cls(std::get<0>(row)),std::get<1>(row),std::get<2>(row),std::get<3>(row));
    hook(NMS_RESULT_REQUEST,nms_result_request,nms_old_result_request);
    hook(NMS_RESULT_PROCEED,nms_result_proceed,nms_old_result_proceed);
    hook(NMS_RETURN_PARAMS,nms_return_params,nms_old_return_params);
    hook(NMS_RETURN_DESERIALIZE,nms_return_deserialize,nms_old_return_deserialize);
    hook(NMS_PREPARE_ROUTE,nms_prepare_route,nms_old_prepare_route);
    // NPB already owns this hook and delegates consumed MULTI results above.
    // Validate the shared native contract without registering a second detour.
    auto selected=bind_method(NMS_RESULT_CLOSE);void* pointer=nullptr;
    if(!read(selected,pointer)||reinterpret_cast<uintptr_t>(pointer)-reinterpret_cast<uintptr_t>(runtime)!=NMS_RESULT_CLOSE.rva)
        throw std::runtime_error("private MULTI result close RVA differs");
}
