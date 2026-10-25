// Include native_private_liveness_policy.h before namespace cn; include this
// file after native_battle_data.inc and native_battle_launch.inc. No valid hook.
// setup(enable) before hook activation; confirm_current() immediately after a
// successful server session prepare; shutdown() before runtime gate teardown.
#pragma once

inline constexpr CN_Method NPL_UI_UPDATE={"Torappu.UI.AutoChess.Battle.AutoChessBattleUIController","Update",0x602bee3,0x27fd780ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPL_SERVICE_INFO={"Torappu.UI.AutoChess.Server.AutoChessService","get_battleInfo",0x602bcdf,0x27f4e40ULL,0,false,"Torappu.UI.AutoChess.Server.AutoChessServiceBattleInfo",{}};
inline constexpr CN_Method NPL_INFO_CTOR={"Torappu.UI.AutoChess.Server.AutoChessServiceBattleInfo",".ctor",0x602bcfd,0x27f05a0ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPL_INFO_VALID={"Torappu.UI.AutoChess.Server.AutoChessServiceBattleInfo","get_valid",0x602bcfa,0x27f0640ULL,0,false,"System.Boolean",{}};
inline constexpr CN_Method NPL_SINGLE_CLICK={"Torappu.Activity.AutoChess.ActAutoChessEntryGameStartPlugin","EventOnSingleClicked",0x6031c5a,0x2e37490ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPL_ENTRY_MODEL={"Torappu.Activity.AutoChess.ActAutoChessEntryGameStartPlugin","_GetActViewModel",0x6031c53,0x2e37920ULL,0,false,"Torappu.Activity.AutoChess.ActAutoChessEntryViewModel",{}};
inline constexpr CN_Method NPL_RESULT_PARAMS={"Torappu.UI.AutoChess.AutoChessPreparePage+Params",".ctor",0x602b4c6,0x2766c20ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPL_RESULT_TRAINING={"Torappu.Battle.AutoChess.TrainingModeSettleData",".ctor",0x601359e,0xb3ed30ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPL_RESULT_OPEN={"Torappu.UI.UIPageController","OpenPage",0x601ab28,0x1407f00ULL,2,true,"System.Void",{"System.String","Torappu.UI.UIPageOption"}};
inline constexpr CN_Method NPL_RESULT_PAGE={"Torappu.UI.State","GetPage",0x601adc8,0x141ec30ULL,0,false,"Torappu.UI.UIPage",{}};
inline constexpr CN_Method NPL_RESULT_ARGS={"Torappu.UI.UIPage","GetArguments",0x601ac23,0x140ee70ULL,0,false,"System.Object",{}};
inline constexpr CN_Method NPL_RESULT_UID={"Torappu.UI.AutoChess.AutoChessPrepareModel","get_selfUid",0x602b490,0x275e6a0ULL,0,false,"System.String",{}};
inline constexpr CN_Method NPL_RESULT_AUTO_TEAM={"Torappu.UI.AutoChess.AutoChessSettleGameState","_AutoJumpToTeamViewAfterWaitTime",0x602b79c,0x278c110ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPL_RESULT_NEXT={"Torappu.UI.AutoChess.AutoChessSettleGameState","_OnPersonalViewNextBtnClick",0x602b791,0x278c810ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPL_RESULT_LEAVE={"Torappu.UI.AutoChess.AutoChessPrepareController","_LeavePreparePage",0x602b423,0x2726710ULL,0,false,"System.Void",{}};

using NPLVoid=void(*)(void*,const void*);
using NPLGetter=void*(*)(void*,const void*);
NPLVoid npl_old_update=nullptr;NPLGetter npl_old_info=nullptr;
NPLVoid npl_old_single=nullptr;
NPLVoid npl_old_auto_team=nullptr,npl_old_result_next=nullptr;
std::mutex npl_mutex;
cnliveness::Identity npl_identity;
cnliveness::Lease npl_lease;
std::string npl_last_error;
unsigned npl_generation=0,npl_confirmations=0,npl_failures=0;
std::atomic<unsigned> npl_served{0},npl_invalid{0};
std::atomic<bool> npl_setup{false},npl_stop{false};
HANDLE npl_thread=nullptr,npl_wake=nullptr;
thread_local bool npl_ui_scope=false;
thread_local bool npl_result_opening=false;
std::string npl_result_shown;
std::string npl_present_pending_run;
int32_t npl_present_pending_revision=0,npl_present_pending_attempt=0;
cnjson::Value npl_result_reply;
std::atomic<unsigned> npl_result_opens{0},npl_result_loads{0},npl_result_failures{0};

// This worker boundary never invokes IL2CPP, Unity, POST or autostart. Both
// identities must agree before extending a lease for one specific private run.
std::string npl_get(const std::wstring& route,size_t limit){
    if(nbb_participant_profile&&route.rfind(L"/bridge/act2autochess/native-session",0)==0){auto raw=cnjson::dump(native_multiplayer_solo_http(route.c_str(),nullptr));if(raw.size()>limit)throw std::runtime_error("participant solo liveness size limit");return raw;}
    const auto began=GetTickCount64();
    NBBInternet session(WinHttpOpen(L"StrongholdCNPrivateLiveness/1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value||!WinHttpSetTimeouts(session.value,250,250,250,250))throw std::runtime_error("private liveness HTTP session unavailable");
    NBBInternet connection(private_backend_connect(session.value));
    NBBInternet request(connection.value?WinHttpOpenRequest(connection.value,L"GET",route.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,private_backend_request_flags()):nullptr);
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,disabled=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
    if(!request.value||!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))||
        !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))||!private_backend_headers(request.value)||
        !WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.value,nullptr))
        throw std::runtime_error("private liveness loopback GET unavailable");
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)
        throw std::runtime_error("private liveness loopback GET rejected");
    std::string result;
    while(!npl_stop.load()&&!closing.load()&&GetTickCount64()-began<2000){
        DWORD available=0;if(!WinHttpQueryDataAvailable(request.value,&available))throw std::runtime_error("private liveness GET read failed");
        if(!available)return result;if(result.size()+available>limit)throw std::runtime_error("private liveness GET size limit");
        const auto previous=result.size();result.resize(previous+available);DWORD used=0;
        if(!WinHttpReadData(request.value,result.data()+previous,available,&used)||!used)throw std::runtime_error("private liveness GET incomplete");result.resize(previous+used);
    }
    throw std::runtime_error("private liveness GET cancelled or timed out");
}
DWORD WINAPI npl_worker(void*){
    while(!npl_stop.load()&&!closing.load()){
        WaitForSingleObject(npl_wake,2000);if(npl_stop.load()||closing.load())break;
        cnliveness::Identity expected;unsigned generation=0;
        {std::lock_guard<std::mutex> lock(npl_mutex);expected=npl_identity;generation=npl_generation;}
        if(expected.run.empty())continue;
        bool revoke=false,confirmed=false;std::string error;cnliveness::Identity observed;
        try{
            auto health=cnbackend::validate_health(npl_get(L"/health",65536),CN_BACKEND_CATALOG_HASH,"");
            if(health.kind!=cnbackend::ProbeKind::healthy){revoke=true;throw std::runtime_error("private liveness backend identity mismatch");}
            auto raw=npl_get(L"/bridge/act2autochess/native-session/"+std::wstring(expected.run.begin(),expected.run.end()),4*1024*1024);
            // A durable result is still this scene's canonical session while
            // native END/settlement tasks consume it. Wrong identity, malformed
            // summary or a backwards revision still revokes immediately.
            revoke=true;observed=cnliveness::validate_reply(cnjson::parse(raw),expected);confirmed=true;
        }catch(const std::exception& ex){error=ex.what();}
        bool changed=false;
        {std::lock_guard<std::mutex> lock(npl_mutex);if(generation!=npl_generation)continue;
            const bool was_valid=npl_lease.valid(GetTickCount64());
            if(confirmed&&observed.revision<npl_identity.revision)continue;
            if(confirmed&&!cnliveness::can_advance(npl_identity,observed)){confirmed=false;revoke=true;error="private liveness terminal proof changed";}
            if(confirmed){npl_identity=observed;npl_lease.confirm(GetTickCount64());++npl_confirmations;npl_last_error.clear();}
            else{++npl_failures;if(revoke)npl_lease.revoke();npl_last_error=error;}
            changed=was_valid!=npl_lease.valid(GetTickCount64());}
        if(changed)report(confirmed?"private-native-liveness-session-confirmed":"private-native-liveness-session-lost");
    }
    return 0;
}
void native_private_liveness_shutdown(){
    // Host serializes this after setup_busy has cleared. In particular, never
    // wait for the worker while holding npl_mutex, nb_data_lock or report_lock.
    npl_setup.store(false);npl_stop.store(true);if(npl_wake)SetEvent(npl_wake);
    if(npl_thread){WaitForSingleObject(npl_thread,INFINITE);CloseHandle(npl_thread);npl_thread=nullptr;}
    if(npl_wake){CloseHandle(npl_wake);npl_wake=nullptr;}
    std::lock_guard<std::mutex> lock(npl_mutex);npl_lease.revoke();npl_identity={};++npl_generation;
}
// Call only immediately after prepare's freshly accepted private GET/POST and
// typed DTO construction. This initial lease prevents an asynchronous first
// heartbeat race; it is not renewed by UI frames or cached status reads.
void native_private_liveness_confirm_current(){
    nb_main_thread();std::lock_guard<std::recursive_mutex> data_lock(nb_data_lock);
    if(!npl_setup.load()||!nb_prepared.load()||!nbb_healthy.load())throw std::runtime_error("private liveness prepare proof missing");
    auto expected=cnliveness::identity(nb_state);cnliveness::validate_reply(nb_initial_reply,expected);
    TemporaryRoots roots;auto c=cls("Torappu.UI.AutoChess.Server.AutoChessServiceBattleInfo");
    auto valid=roots.retain(object_new(c));invoke(bound_methods.at(NPL_INFO_CTOR.token),valid);
    auto invalid=roots.retain(object_new(c));invoke(bound_methods.at(NPL_INFO_CTOR.token),invalid);
    uint8_t started=1;set_field(valid,profile_field(c,"started"),&started);
    for(const auto& item:std::initializer_list<std::pair<const char*,std::string>>{{"stageId",expected.stage},{"modeId",expected.mode},{"sceneId",expected.run}})
        set_object_field(valid,profile_field(c,item.first),roots.retain(string_new(item.second.c_str())));
    set_object_field(valid,profile_field(c,"battleSceneData"),nb_ui());
    uint8_t is_valid=0,is_invalid=1;
    if(!read(unbox(invoke(bound_methods.at(NPL_INFO_VALID.token),valid)),is_valid)||!is_valid||
        !read(unbox(invoke(bound_methods.at(NPL_INFO_VALID.token),invalid)),is_invalid)||is_invalid)
        throw std::runtime_error("native battle info valid contract failed");
    protect("private-liveness-valid-info",valid);protect("private-liveness-invalid-info",invalid);
    {std::lock_guard<std::mutex> lock(npl_mutex);npl_identity=expected;npl_lease.confirm(GetTickCount64());++npl_generation;++npl_confirmations;npl_last_error.clear();}
    if(npl_wake)SetEvent(npl_wake);report("private-native-liveness-bound-to-confirmed-session");
}
void* npl_get_info(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;
    if(!npl_ui_scope){lease.release();return npl_old_info(self,method_info);}
    try{
        const auto service_activity=text(field_object(field_object(self,"<param>k__BackingField"),"activityId"));
        if(service_activity.empty())throw std::runtime_error("private liveness service activity unresolved");
        if(service_activity!="act2autochess"){lease.release();return npl_old_info(self,method_info);}
        nb_main_thread();std::lock_guard<std::recursive_mutex> data_lock(nb_data_lock);
        const auto current=cnliveness::terminal_identity(nb_state,nb_settle_game_info);
        bool live=false;
        {std::lock_guard<std::mutex> lock(npl_mutex);
            live=cnliveness::scope(nb_prepared.load(),native_battle_launch_in_flight.load(),native_battle_launch_scene_entered.load(),"act2autochess",service_activity)&&
                npl_identity.run==nb_run_id&&npl_identity.same_session(current)&&npl_lease.valid(GetTickCount64())&&
                (!current.terminal||!npl_identity.terminal||current.terminal_proof==npl_identity.terminal_proof);
            // The freshly accepted finish ACK can precede the next GET. It
            // binds the result proof but never renews the eight-second lease.
            if(live&&current.revision>=npl_identity.revision&&(!npl_identity.terminal||current.terminal))npl_identity=current;}
        auto result=nb_retained(live?"private-liveness-valid-info":"private-liveness-invalid-info");
        if(!result)throw std::runtime_error("private liveness native model missing");
        if(live&&field_object(result,"battleSceneData")!=nb_ui())set_object_field(result,profile_field(api.il2cpp_object_get_class(result),"battleSceneData"),nb_ui());
        if(live)++npl_served;else ++npl_invalid;return result;
    }catch(const std::exception& ex){++npl_invalid;{std::lock_guard<std::mutex> lock(npl_mutex);npl_lease.revoke();npl_last_error=ex.what();}
        report("private-native-liveness-model-failed");return nb_retained("private-liveness-invalid-info");}
}
void npl_update(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    const bool target=can_read()&&unity_ui_thread.load()==GetCurrentThreadId()&&nb_prepared.load()&&native_battle_launch_in_flight.load()&&
        native_battle_launch_scene_entered.load()&&act(field_object(self,"m_actId"));
    struct Scope{bool previous;explicit Scope(bool target_scope):previous(npl_ui_scope){npl_ui_scope=target_scope;}~Scope(){npl_ui_scope=previous;}} scope(target);
    if(!target){lease.release();npl_old_update(self,method_info);return;}
    // Original modeType, original valid, original disconnect dialog and all
    // native panel ticks remain responsible for their existing behavior.
    PrivateDataScope private_scope(true);npl_old_update(self,method_info);
}
std::string npl_result_key(const std::string& run,int32_t revision,int32_t attempt){return run+":"+std::to_string(revision)+":"+std::to_string(attempt);}
int32_t npl_result_attempt(const cnjson::Value& state){
    const auto rounds=state.object.find("nativeBattleAttempts");if(rounds==state.object.end())return 0;
    if(rounds->second.kind!=cnjson::Value::Object)throw std::runtime_error("private presented result attempt map malformed");
    const auto found=rounds->second.object.find(std::to_string(nb_number(state,"round")));if(found==rounds->second.object.end())return 0;
    if(found->second.kind!=cnjson::Value::Integer||found->second.integer<0||found->second.integer>2147483647)
        throw std::runtime_error("private presented result attempt malformed");
    return int32_t(found->second.integer);
}
void npl_flush_presented(){
    if(npl_present_pending_run.empty())return;
    const auto run=npl_present_pending_run;const auto revision=npl_present_pending_revision,attempt=npl_present_pending_attempt;
    auto body=nb_object();body.object["activityId"]=json_text("act2autochess");body.object["revision"]=nb_integer(revision);body.object["battleAttempt"]=nb_integer(attempt);
    cnjson::Value reply;for(unsigned retry=0;;++retry)try{
        reply=nb_http(L"/bridge/act2autochess/native-session/"+std::wstring(run.begin(),run.end())+L"/presented",&body);break;
    }catch(...){if(retry)throw;}
    cnliveness::boolean(reply,"ok",true);cnliveness::boolean(reply,"officialFallback",false);cnliveness::boolean(reply,"presented",true);
    if(cnliveness::string(reply,"activityId")!="act2autochess"||cnliveness::string(reply,"runId")!=run||
       cnliveness::number(reply,"revision")!=revision||cnliveness::number(reply,"battleAttempt")!=attempt)
        throw std::runtime_error("private presented result ACK identity changed");
    npl_present_pending_run.clear();npl_present_pending_revision=0;npl_present_pending_attempt=0;
}
void npl_result_presented(const std::string& run,int32_t revision,int32_t attempt){
    npl_result_shown=npl_result_key(run,revision,attempt);
    npl_present_pending_run=run;npl_present_pending_revision=revision;npl_present_pending_attempt=attempt;
    // A transport failure must not black out the successfully constructed
    // native page. Keep the receipt pending and require retry before new entry.
    try{npl_flush_presented();}catch(const std::exception& ex){++npl_result_failures;{std::lock_guard<std::mutex> lock(npl_mutex);npl_last_error=ex.what();}report("private-native-result-presentation-ack-pending");}
}
void* npl_result_data(void* self,void* activity){
    if(closing.load()||!can_read()||text(activity)!="act2autochess"||!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessSettleGameState"))return nullptr;
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);TemporaryRoots roots;
    auto retained=nb_retained("private-liveness-result-params");if(!retained)return nullptr;
    auto page=roots.retain(invoke(bound_methods.at(NPL_RESULT_PAGE.token),self));
    if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage"))return nullptr;
    auto args=roots.retain(invoke(bound_methods.at(NPL_RESULT_ARGS.token),page));if(args!=retained)return nullptr;
    // Only the page created by this GET-backed result entry may replace the
    // training loader. Every unrelated native result keeps its original path.
    if(text(field_object(args,"actId"))!="act2autochess"||nb_native_int(args,"modeType")!=1||nb_native_int(args,"fromBattleSource")!=2)
        throw std::runtime_error("private durable result page identity changed");
    const auto& state=nb_at(npl_result_reply,"state");auto expected=cnliveness::identity(state);expected.terminal=false;cnliveness::validate_reply(npl_result_reply,expected);
    auto controller=field_object(page,"_controller"),model=field_object(controller,"m_model");
    if(!controller||!model||api.il2cpp_object_get_class(model)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel"))throw std::runtime_error("private durable result model missing");
    auto uid=text(roots.retain(invoke(bound_methods.at(NPL_RESULT_UID.token),model)));if(uid.empty())throw std::runtime_error("private durable result native self UID missing");
    auto summary=nb_at(npl_result_reply,"nativeSettleGameInfo");summary.object.at("teamInfo").array[0].object["uid"]=json_text(uid);
    native_broadcast_project_settlement(summary,uid);
    PrivateDataScope private_scope(true);auto result=build_value(api.il2cpp_method_get_return_type(nb_method(0x602b79f)),summary,roots).object;
    if(!result)throw std::runtime_error("private durable result DTO conversion returned no object");
    protect("private-liveness-result-summary",result);npl_result_presented(expected.run,expected.revision,npl_result_attempt(state));
    ++npl_result_loads;report("private-native-durable-result-summary-projected");return result;
}
// The page itself and its exact Params must have consumed our sole-player
// summary. A historical GET is not a current-battle exit proof.
void* npl_owned_result_page_controller(void* page){
    nb_main_thread();TemporaryRoots roots;
    if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage"))return nullptr;
    auto args=roots.retain(invoke(bound_methods.at(NPL_RESULT_ARGS.token),page));if(!args)return nullptr;
    const bool live=args==nb_retained("native-private-settlement-params")&&nb_retained("native-private-settlement");
    const bool history=args==nb_retained("private-liveness-result-params")&&nb_retained("private-liveness-result-summary");
    if(!live&&!history)return nullptr;
    if(api.il2cpp_object_get_class(args)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage+Params")||text(field_object(args,"actId"))!="act2autochess"||nb_native_int(args,"fromBattleSource")!=2||nb_native_int(args,"modeType")!=(live?0:1))
        throw std::runtime_error("private result navigation Params changed");
    auto controller=field_object(page,"_controller");
    if(!controller||api.il2cpp_object_get_class(controller)!=cls("Torappu.UI.AutoChess.AutoChessPrepareController")||
       field_object(controller,"m_page")!=page||text(field_object(controller,"m_actId"))!="act2autochess")
        throw std::runtime_error("private result navigation controller missing");
    return controller;
}
void* npl_owned_result_controller(void* self){
    if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessSettleGameState"))return nullptr;
    TemporaryRoots roots;auto page=roots.retain(invoke(bound_methods.at(NPL_RESULT_PAGE.token),self));
    return npl_owned_result_page_controller(page);
}
bool native_solo_result_leave_request(void* controller){
    std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    if(!controller||npl_owned_result_page_controller(field_object(controller,"m_page"))!=controller)return false;
    npl_flush_presented();PrivateDataScope private_scope(true);invoke(bound_methods.at(NPL_RESULT_LEAVE.token),controller);
    report("private-native-solo-result-returned-home");return true;
}
void npl_result_auto_team(void* self,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{std::lock_guard<std::recursive_mutex> lock(nb_data_lock);if(npl_owned_result_controller(self)){report("private-native-solo-result-auto-team-skipped");return;}}
    catch(const std::exception& ex){++npl_result_failures;{std::lock_guard<std::mutex> lock(npl_mutex);npl_last_error=ex.what();}report("private-native-result-navigation-failed");return;}
    lease.release();npl_old_auto_team(self,method_info);
}
void npl_result_next(void* self,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{std::lock_guard<std::recursive_mutex> lock(nb_data_lock);if(auto controller=npl_owned_result_controller(self)){
        npl_flush_presented();PrivateDataScope private_scope(true);invoke(bound_methods.at(NPL_RESULT_LEAVE.token),controller);report("private-native-solo-result-returned-home");return;}}
    catch(const std::exception& ex){++npl_result_failures;{std::lock_guard<std::mutex> lock(npl_mutex);npl_last_error=ex.what();}report("private-native-result-navigation-failed");return;}
    lease.release();npl_old_result_next(self,method_info);
}
void npl_open_result_page(const cnjson::Value& fresh){
    TemporaryRoots roots;
    PrivateDataScope private_scope(true);auto params=roots.retain(object_new(cls(NPL_RESULT_PARAMS.type)));invoke(bound_methods.at(NPL_RESULT_PARAMS.token),params);
    auto training=roots.retain(object_new(cls(NPL_RESULT_TRAINING.type)));invoke(bound_methods.at(NPL_RESULT_TRAINING.token),training);
    int32_t single=1,source=2,passed=nb_number(nb_at(nb_at(fresh,"state"),"result"),"roundsPassed");uint8_t win=nb_true(nb_at(nb_at(fresh,"state"),"result"),"success");
    auto type=api.il2cpp_object_get_class(params);set_object_field(params,profile_field(type,"actId"),roots.retain(string_new("act2autochess")));
    set_field(params,profile_field(type,"modeType"),&single);set_field(params,profile_field(type,"fromBattleSource"),&source);set_object_field(params,profile_field(type,"trainingModeSettleData"),training);
    auto training_type=api.il2cpp_object_get_class(training);set_field(training,profile_field(training_type,"gameWin"),&win);set_field(training,profile_field(training_type,"finishRound"),&passed);
    auto option_type=api.il2cpp_method_get_param(bound_methods.at(NPL_RESULT_OPEN.token),1);auto option=build_value(option_type,nb_object(),roots);
    auto option_class=api.il2cpp_object_get_class(option.object);api.il2cpp_field_static_get_value(profile_field(option_class,"DEFAULT"),option.argument);
    set_object_field(option.object,profile_field(option_class,"args"),params);
    auto name=roots.retain(string_new("auto_chess_prepare_page"));void* args[]={name,option.argument};
    npl_result_reply=fresh;protect("private-liveness-result-params",params);
    invoke(bound_methods.at(NPL_RESULT_OPEN.token),nullptr,args);++npl_result_opens;report("private-native-durable-result-opened-without-battle");
}
// The activity give-up callback already has a canonical quit receipt. Reuse
// the same durable result page, scoped to that exact run rather than latest.
void native_session_presence_open_result(const std::string& run){
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    if(npl_result_opening||native_battle_launch_in_flight.load()||native_battle_launch_pending.load())
        throw std::runtime_error("private abandoned result page is already active or pending");
    struct Opening{Opening(){npl_result_opening=true;}~Opening(){npl_result_opening=false;}} opening;
    npl_flush_presented();
    if(run.size()!=32||run.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("private abandoned result run invalid");
    auto fresh=nb_http(L"/bridge/act2autochess/native-session/"+std::wstring(run.begin(),run.end()));
    const auto& state=nb_at(fresh,"state");auto expected=cnliveness::identity(state);
    if(expected.run!=run||!expected.terminal||nb_string(state,"nativeSessionEndReason")!="quit"||
       nb_true(nb_at(state,"result"),"success")||!nb_true(nb_at(state,"result"),"abandoned"))
        throw std::runtime_error("private abandoned result does not match the acknowledged quit");
    expected.terminal=false;cnliveness::validate_reply(fresh,expected);
    if(npl_result_key(run,expected.revision,npl_result_attempt(state))==npl_result_shown)return;
    npl_open_result_page(fresh);
}
void npl_single(void* self,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()||!self||api.il2cpp_object_get_class(self)!=cls(NPL_SINGLE_CLICK.type)){lease.release();npl_old_single(self,method_info);return;}
    bool forward=false;
    try{
        forward=[&](){
        nb_main_thread();TemporaryRoots roots;auto entry=roots.retain(invoke(bound_methods.at(NPL_ENTRY_MODEL.token),self));
        if(!entry||api.il2cpp_object_get_class(entry)!=cls("Torappu.Activity.AutoChess.ActAutoChessEntryViewModel"))throw std::runtime_error("private result entry model unavailable");
        if(text(field_object(entry,"actId"))!="act2autochess")return true;
        if(nb_prepared.load()||native_battle_launch_in_flight.load()||native_battle_launch_pending.load())return true;
        if(npl_result_opening)throw std::runtime_error("private result page is already opening");
        struct Opening{Opening(){npl_result_opening=true;}~Opening(){npl_result_opening=false;}} opening;
        npl_flush_presented();
        if(cnbackend::validate_health(npl_get(L"/health",65536),CN_BACKEND_CATALOG_HASH,"").kind!=cnbackend::ProbeKind::healthy)
            throw std::runtime_error("private result backend identity mismatch");
        auto latest=nb_http(L"/bridge/act2autochess/native-session/latest-result");
        cnliveness::boolean(latest,"ok",true);cnliveness::boolean(latest,"officialFallback",false);
        if(cnliveness::string(latest,"activityId")!="act2autochess")throw std::runtime_error("private latest result activity mismatch");
        auto empty=latest.object.find("latestResult");
        if(empty!=latest.object.end()){if(empty->second.kind!=cnjson::Value::Null)throw std::runtime_error("private latest result index malformed");return true;}
        auto expected=cnliveness::identity(nb_at(latest,"state"));if(!expected.terminal)throw std::runtime_error("private latest result is not terminal");
        expected.terminal=false;auto indexed=cnliveness::validate_reply(latest,expected);
        if(npl_result_key(indexed.run,indexed.revision,npl_result_attempt(nb_at(latest,"state")))==npl_result_shown)return true;
        auto fresh=nb_http(L"/bridge/act2autochess/native-session/"+std::wstring(indexed.run.begin(),indexed.run.end()));
        auto confirmed=cnliveness::validate_reply(fresh,indexed);if(!confirmed.terminal)throw std::runtime_error("private durable result confirmation missing");
        npl_open_result_page(fresh);return false;
        }();
    }catch(const std::exception& ex){++npl_result_failures;{std::lock_guard<std::mutex> lock(npl_mutex);npl_last_error=ex.what();}report("private-native-durable-result-open-failed");}
    // Original exceptions must retain their original behavior. Do not classify
    // a foreign or normal new-game failure as our read-only result failure.
    if(forward){lease.release();npl_old_single(self,method_info);}
}
std::string native_private_liveness_status_json(){
    std::unique_lock<std::mutex> lock(npl_mutex,std::try_to_lock);
    if(!lock.owns_lock())return "{\"busy\":true}";
    return "{\"setup\":"+std::string(npl_setup.load()?"true":"false")+",\"leaseValid\":"+(npl_lease.valid(GetTickCount64())?"true":"false")+
        ",\"runId\":"+quote(npl_identity.run)+",\"terminal\":"+(npl_identity.terminal?"true":"false")+",\"leaseMs\":8000,\"confirmations\":"+std::to_string(npl_confirmations)+",\"failures\":"+std::to_string(npl_failures)+
        ",\"uiReads\":"+std::to_string(npl_served.load())+",\"invalidUiReads\":"+std::to_string(npl_invalid.load())+",\"resultOpens\":"+std::to_string(npl_result_opens.load())+",\"resultLoads\":"+std::to_string(npl_result_loads.load())+",\"resultFailures\":"+std::to_string(npl_result_failures.load())+",\"lastError\":"+quote(npl_last_error)+"}";
}
void npl_result_setup(){
    hook(NPL_SINGLE_CLICK,npl_single,npl_old_single);
    hook(NPL_RESULT_AUTO_TEAM,npl_result_auto_team,npl_old_auto_team);hook(NPL_RESULT_NEXT,npl_result_next,npl_old_result_next);
    for(const auto* spec:{&NPL_ENTRY_MODEL,&NPL_RESULT_PARAMS,&NPL_RESULT_TRAINING,&NPL_RESULT_OPEN,&NPL_RESULT_PAGE,&NPL_RESULT_ARGS,&NPL_RESULT_UID})bind_method(*spec);
    bind_method(NPL_RESULT_LEAVE);
    auto option=cls("Torappu.UI.UIPageOption");native_battle_launch_check_value(option,24,"private result UIPageOption");
    const struct{const char* type;const char* name;const char* field_type;size_t offset;} fields[]={
        {"Torappu.Activity.AutoChess.ActAutoChessEntryViewModel","actId","System.String",32},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","actId","System.String",16},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","modeType","Torappu.ActAutoChessModeType",24},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","fromBattleSource","Torappu.UI.AutoChess.FromBattleSource",32},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","trainingModeSettleData","Torappu.Battle.AutoChess.TrainingModeSettleData",56},
        {"Torappu.UI.AutoChess.AutoChessPreparePage","_controller","Torappu.UI.AutoChess.AutoChessPrepareController",248},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_model","Torappu.UI.AutoChess.AutoChessPrepareModel",128},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_page","Torappu.UI.AutoChess.AutoChessPreparePage",80},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_actId","System.String",64},
        {"Torappu.UI.UIPageOption","args","System.Object",24}};
    for(const auto& item:fields){auto c=cls(item.type);if(!c)throw std::runtime_error("private result native type missing");auto field=profile_field(c,item.name);if(api.il2cpp_field_get_offset(field)!=item.offset||!matches_type(api.il2cpp_field_get_type(field),item.field_type))throw std::runtime_error("private result field contract mismatch");}
    auto defaults=profile_field(option,"DEFAULT");if(!(api.il2cpp_field_get_flags(defaults)&16)||!matches_type(api.il2cpp_field_get_type(defaults),"Torappu.UI.UIPageOption"))throw std::runtime_error("private result default page option mismatch");
    nbc_restore_result=npl_result_data;
    nbc_result_presented=npl_result_presented;
}
void native_private_liveness_setup(bool enable){
    if(npl_thread||npl_wake)throw std::runtime_error("private liveness setup already owns worker handles");
    hook(NPL_UI_UPDATE,npl_update,npl_old_update);hook(NPL_SERVICE_INFO,npl_get_info,npl_old_info);
    bind_method(NPL_INFO_CTOR);bind_method(NPL_INFO_VALID);
    npl_result_setup();
    auto ui=cls("Torappu.UI.AutoChess.Battle.AutoChessBattleUIController");
    if(!ui)throw std::runtime_error("private liveness BattleUI class missing");
    auto activity=profile_field(ui,"m_actId");
    if(api.il2cpp_field_get_offset(activity)!=136||!matches_type(api.il2cpp_field_get_type(activity),"System.String"))throw std::runtime_error("private liveness BattleUI activity identity field mismatch");
    auto c=cls("Torappu.UI.AutoChess.Server.AutoChessServiceBattleInfo");if(!c||api.il2cpp_class_is_valuetype(c))throw std::runtime_error("private liveness BattleInfo class mismatch");
    const char* names[]={"started","stageId","sceneId","modeId","battleSceneData"};
    const char* types[]={"System.Boolean","System.String","System.String","System.String","Torappu.UI.AutoChess.AutoChessBattleSceneData"};
    const size_t offsets[]={16,24,32,40,48};
    for(size_t i=0;i<5;++i){auto field=profile_field(c,names[i]);if(api.il2cpp_field_get_offset(field)!=offsets[i]||!matches_type(api.il2cpp_field_get_type(field),types[i]))throw std::runtime_error("private liveness BattleInfo field mismatch");}
    // Even a failed prepare proof must return a native object whose original
    // valid property is false; BattleUI.Update dereferences the result.
    {TemporaryRoots roots;auto invalid=roots.retain(object_new(c));invoke(bound_methods.at(NPL_INFO_CTOR.token),invalid);protect("private-liveness-invalid-info",invalid);}
    if(!enable){npl_setup.store(true);return;}
    try{
        npl_stop.store(false);npl_wake=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!npl_wake)throw std::runtime_error("private liveness wake event unavailable");
        npl_thread=CreateThread(nullptr,0,npl_worker,nullptr,0,nullptr);if(!npl_thread)throw std::runtime_error("private liveness worker unavailable");
        npl_setup.store(true);
    }catch(...){native_private_liveness_shutdown();throw;}
}
