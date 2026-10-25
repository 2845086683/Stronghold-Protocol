// Include native_battle_launch_policy.h before namespace cn in cn_activity.cpp.
// Include this file inside namespace cn, after bind_method()/hook(), before setup().
// Configure the data bridge callbacks and call native_battle_launch_setup() before
// activating the queued MinHook hooks. The existing three-argument training guard
// delegates act2 requests to native_battle_launch_training(); no second hook is
// installed on AutoChessService.StartTraining here.

inline constexpr CN_Method CN_BATTLE_UTIL_START={"Torappu.UI.AutoChess.AutoChessUtil","StartBattle",0x602b550,0x2765030ULL,1,true,"System.Void",{"Torappu.UI.AutoChess.AutoChessUtil+StartBattleParam"}};
inline constexpr CN_Method CN_BATTLE_ACT_DATA={"Torappu.UI.AutoChess.AutoChessUtil","GetActAutoChessData",0x602b516,0x2761300ULL,1,true,"Torappu.ActAutoChessData",{"System.String"}};
inline constexpr CN_Method CN_BATTLE_MODE_DATA={"Torappu.UI.AutoChess.AutoChessUtil","GetModeData",0x602b549,0x2761e00ULL,2,true,"Torappu.ActAutoChessData+ActAutoChessModeData",{"System.String","System.String"}};
inline constexpr CN_Method CN_BATTLE_CONTROLLER_START={"Torappu.BattleStartController","StartBattle",0x6008427,0x283d640ULL,2,true,"Torappu.BattleStartController+Handler",{"Torappu.BattleStartController+Param","Torappu.BattleStartController+IPlugin"}};
inline constexpr CN_Method CN_BATTLE_CONTROLLER_LOCAL={"Torappu.BattleStartController","StartLocalBattle",0x6008428,0x283dbb0ULL,2,true,"Torappu.BattleStartController+Handler",{"Torappu.BattleStartController+Param","Torappu.BattleStartController+IPlugin"}};
inline constexpr CN_Method CN_BATTLE_SERVICE_UPDATE={"Torappu.UI.AutoChess.Server.AutoChessService","OnUpdate",0x602bce3,0x27f42c0ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_BATTLE_IN_PARAMS={"Torappu.Battle.BattleInOut","get_input",0x600f5cc,0x5d9990ULL,0,false,"Torappu.Battle.BattleInOut+InParams",{}};
inline constexpr CN_Method CN_BATTLE_MANAGER_START={"Torappu.Battle.AutoChess.AutoChessGameManager","Start",0x6013697,0xb44920ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_BATTLE_MANAGER_LOAD={"Torappu.Battle.AutoChess.AutoChessGameManager","DoLoadGameInternal",0x60136b3,0xb3fd40ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_BATTLE_MANAGER_FINISH={"Torappu.Battle.AutoChess.AutoChessGameManager","OnFinishGame",0x601369c,0xb41160ULL,1,false,"System.Void",{"Torappu.Battle.BattleController+GameResult"}};
inline constexpr CN_Method CN_BATTLE_MANAGER_DISPOSE={"Torappu.Battle.AutoChess.AutoChessGameManager","Dispose",0x6013691,0xb3fcb0ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_BATTLE_LOADING_MOVE={"Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+PreloadDataInLoading+<CoroutineTask>d__2","MoveNext",0x6013cb5,0xbe56c0ULL,0,false,"System.Boolean",{}};
inline constexpr CN_Method CN_BATTLE_BAND_LOAD={"Torappu.UI.AutoChess.AutoChessBandChooseViewModel","LoadData",0x602b5c7,0x27588d0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.AutoChessPrepareModel"}};
inline constexpr CN_Method CN_BATTLE_BAND_REFRESH={"Torappu.UI.AutoChess.AutoChessBandChooseViewModel","RefreshPlayerData",0x602b5c8,0x2758af0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.AutoChessPrepareModel"}};
inline constexpr CN_Method CN_BATTLE_CHOOSE_BAND={"Torappu.UI.AutoChess.AutoChessPrepareController","_EventOnChooseBand",0x602b417,0x2722410ULL,1,false,"System.Void",{"System.String"}};
inline constexpr CN_Method CN_BATTLE_BAND_STATE_ENTER={"Torappu.UI.AutoChess.AutoChessBandChooseState","OnStateEnter",0x602b599,0x2756a30ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_BATTLE_CUSTOM_OPERATION={"Torappu.AVG.AVGUtils","TrigCustomOperation",0x600d8bf,0x3fa34c0ULL,1,true,"System.Boolean",{"System.String"}};

using NativeBattlePrepare=bool(*)(const char* stage_id);
using NativeBattleReady=bool(*)();
using NativeBattleTick=void(*)();
using NativeBattleUtilStart=void(*)(void* raw_param,const void* method_info);
using NativeBattleControllerStart=void*(*)(void* raw_param,void* plugin,const void* method_info);
using NativeBattleVoid=void(*)(void* self,const void* method_info);
using NativeBattleFinish=void(*)(void* self,int32_t result,const void* method_info);
using NativeBattleMove=bool(*)(void* self,const void* method_info);
using NativeBattleBandModel=void(*)(void* self,void* prepare_model,const void* method_info);
using NativeBattleCustomOperation=bool(*)(void* operation,const void* method_info);
// Defined by native_battle_data.inc; a rejected private choice must never enter
// the original training handler or any official activity request path.
bool native_battle_select_band(const char* band_id);
NativeBattlePrepare native_battle_prepare_callback=nullptr;
NativeBattleReady native_battle_ready_callback=nullptr;
NativeBattleTick native_battle_tick_callback=nullptr,native_battle_loaded_callback=nullptr;
NativeBattleUtilStart native_battle_old_util_start=nullptr;
NativeBattleControllerStart native_battle_old_controller_start=nullptr,native_battle_local_start=nullptr;
NativeBattleVoid native_battle_old_service_update=nullptr,native_battle_old_manager_start=nullptr,native_battle_old_manager_load=nullptr,native_battle_old_manager_dispose=nullptr;
NativeBattleFinish native_battle_old_manager_finish=nullptr;
NativeBattleMove native_battle_old_loading_move=nullptr;
NativeBattleBandModel native_battle_old_band_load=nullptr,native_battle_old_band_refresh=nullptr,native_battle_old_choose_band=nullptr;
NativeBattleVoid native_battle_old_band_state_enter=nullptr;
NativeBattleCustomOperation native_battle_old_custom_operation=nullptr;
std::atomic<bool> native_battle_launch_bindings_ready{false},native_battle_launch_session_prepared{false},native_battle_launch_team_ready{false};
std::atomic<bool> native_battle_launch_pending{false},native_battle_launch_in_flight{false},native_battle_launch_scene_entered{false},native_battle_launch_loading_complete{false};
std::atomic<unsigned> native_battle_launch_attempts{0},native_battle_launch_rejections{0},native_battle_launch_local_starts{0};
std::mutex native_battle_launch_error_lock;
std::string native_battle_launch_error,native_battle_launch_first_error;
thread_local unsigned native_battle_training_depth=0,native_battle_utility_depth=0;
thread_local void* native_battle_band_entering_state=nullptr;
void* native_battle_start_param_class=nullptr;
void* native_battle_controller_param_class=nullptr;
void *native_battle_act_data_class=nullptr,*native_battle_const_data_class=nullptr;
void *native_battle_act_const_field=nullptr,*native_battle_training_stage_field=nullptr,*native_battle_training_mode_field=nullptr;
uint32_t native_battle_manager_root=0;
cnlaunch::SceneEpoch native_battle_scene_epoch;
void* nmo_controller();
void* nb_manager();
cnlaunch::SceneTransition native_battle_scene_begin(){
    cnlaunch::SceneTransition saved;saved.before=native_battle_scene_epoch;saved.complete=native_battle_launch_loading_complete.load();saved.entered=native_battle_launch_scene_entered.load();
    if(native_battle_manager_root){auto manager=hold_get(native_battle_manager_root);saved.managerRoot=hold_new(manager,false);if(!saved.managerRoot)throw std::runtime_error("native scene rollback manager root unavailable");}
    native_battle_scene_epoch={};native_battle_scene_epoch.epoch=saved.before.epoch+1;native_battle_scene_epoch.pending=true;
    native_battle_launch_loading_complete.store(false);native_battle_launch_scene_entered.store(false);return saved;
}
void native_battle_scene_accept(cnlaunch::SceneTransition& saved){
    if(saved.consumed)throw std::runtime_error("native scene transaction already consumed");saved.consumed=true;
    for(auto root:{saved.before.controllerRoot,saved.before.iteratorRoot,saved.managerRoot})if(root)hold_free(root);
}
void native_battle_scene_rollback(cnlaunch::SceneTransition& saved){
    if(saved.consumed)throw std::runtime_error("native scene transaction already consumed");saved.consumed=true;
    for(auto root:{native_battle_scene_epoch.controllerRoot,native_battle_scene_epoch.iteratorRoot,native_battle_manager_root})if(root)hold_free(root);
    native_battle_scene_epoch=saved.before;native_battle_manager_root=saved.managerRoot;
    native_battle_launch_loading_complete.store(saved.complete);native_battle_launch_scene_entered.store(saved.entered);
}
void native_battle_scene_bind_manager(void* manager){
    auto controller=nmo_controller();if(!controller||!manager)throw std::runtime_error("private scene manager/controller unavailable");
    if(!native_battle_scene_epoch.pending){auto previous=native_battle_scene_begin();native_battle_scene_accept(previous);}
    auto root=hold_new(controller,false);if(!root)throw std::runtime_error("private scene controller root unavailable");
    for(auto old:{native_battle_scene_epoch.controllerRoot,native_battle_scene_epoch.iteratorRoot})if(old)hold_free(old);
    native_battle_scene_epoch.controllerRoot=root;native_battle_scene_epoch.iteratorRoot=0;native_battle_scene_epoch.manager=manager;native_battle_scene_epoch.managerEpoch=native_battle_scene_epoch.epoch;native_battle_scene_epoch.completedEpoch=0;
    native_battle_launch_loading_complete.store(false);
}
// Only an accepted replacement whose new manager has not started can keep the
// old manager's Dispose from retiring the launch. A first preload's own manager
// can still be closed normally by the user.
bool native_battle_scene_pending(){return native_battle_scene_epoch.pending&&native_battle_scene_epoch.managerEpoch!=native_battle_scene_epoch.epoch;}
bool native_battle_scene_ready(void* controller,void* manager){
    const auto& state=native_battle_scene_epoch;
    return native_battle_launch_in_flight.load()&&state.epoch&&state.managerEpoch==state.epoch&&state.completedEpoch==state.epoch&&!state.pending&&native_battle_launch_loading_complete.load()&&native_battle_manager_root&&hold_get(native_battle_manager_root)==manager&&state.manager==manager&&state.controllerRoot&&hold_get(state.controllerRoot)==controller;
}
bool native_battle_scene_preload_owner(void* iterator){
    auto& state=native_battle_scene_epoch;
    if(!state.epoch||state.managerEpoch!=state.epoch||!state.manager||!native_battle_manager_root||hold_get(native_battle_manager_root)!=state.manager||nb_manager()!=state.manager||!state.controllerRoot||hold_get(state.controllerRoot)!=nmo_controller())return false;
    auto task=field_object(iterator,"<>4__this");if(!task||field_object(task,"m_coroutine")!=iterator)return false;
    if(!state.iteratorRoot){int32_t current=-1;api.il2cpp_field_get_value(iterator,profile_field(api.il2cpp_object_get_class(iterator),"<>1__state"),&current);if(current!=0)return false;state.iteratorRoot=hold_new(iterator,false);if(!state.iteratorRoot)throw std::runtime_error("private preload iterator root unavailable");}
    return hold_get(state.iteratorRoot)==iterator;
}
bool native_battle_scene_preload_finished(void* iterator){
    if(!native_battle_scene_preload_owner(iterator))return false;
    uint8_t finished=0;auto task=field_object(iterator,"<>4__this");api.il2cpp_field_get_value(task,profile_field(api.il2cpp_object_get_class(task),"m_coroutineFinished"),&finished);
    if(!finished)return false;native_battle_scene_epoch.completedEpoch=native_battle_scene_epoch.epoch;native_battle_scene_epoch.pending=false;return true;
}
void native_battle_scene_release(){
    for(auto root:{native_battle_scene_epoch.controllerRoot,native_battle_scene_epoch.iteratorRoot})if(root)hold_free(root);
    const auto epoch=native_battle_scene_epoch.epoch;native_battle_scene_epoch={};native_battle_scene_epoch.epoch=epoch;
}
std::string native_battle_scene_status_json(){
    const auto& s=native_battle_scene_epoch;return "{\"epoch\":"+std::to_string(s.epoch)+",\"managerEpoch\":"+std::to_string(s.managerEpoch)+",\"completedEpoch\":"+std::to_string(s.completedEpoch)+",\"pending\":"+(s.pending?"true":"false")+",\"managerRoot\":"+std::to_string(native_battle_manager_root)+",\"controllerRoot\":"+std::to_string(s.controllerRoot)+",\"iteratorRoot\":"+std::to_string(s.iteratorRoot)+"}";
}
std::string native_battle_training_stage,native_battle_training_mode;
inline constexpr char NATIVE_BATTLE_ACTIVITY[]="act2autochess";
inline constexpr char NATIVE_BATTLE_STAGE[]="act1autochess_m01";
inline constexpr char NATIVE_BATTLE_MODE[]="mode_single_funny";
// Pinned CN server-data/cn-autochess.json season.ConstData, not values inferred
// from whichever ActivityDB instance happens to exist during bootstrap.
inline constexpr char NATIVE_BATTLE_TRAINING_STAGE[]="act1autochess_m01";
inline constexpr char NATIVE_BATTLE_TRAINING_MODE[]="mode_training_1";
inline constexpr char NATIVE_BATTLE_BAND_TUTORIAL[]="mode_training_1_band";

struct NativeBattleDepth {
    unsigned& value;
    explicit NativeBattleDepth(unsigned& target):value(target){++value;}
    ~NativeBattleDepth(){--value;}
};
struct NativeBattlePending {
    std::array<uint32_t,7> references{};
    int32_t mode_type=0,multi_mode_sub_type=0;
    ULONGLONG queued_at=0;
} native_battle_queued;

void native_battle_launch_configure(NativeBattlePrepare prepare,NativeBattleReady is_ready){
    if(active.load())throw std::runtime_error("native battle callbacks must be configured before hook activation");
    native_battle_prepare_callback=prepare;native_battle_ready_callback=is_ready;
}
void native_battle_launch_set_tick(NativeBattleTick tick){
    if(active.load())throw std::runtime_error("native battle tick must be configured before hook activation");
    native_battle_tick_callback=tick;
}
void native_battle_launch_set_loaded(NativeBattleTick loaded){
    if(active.load())throw std::runtime_error("native battle load callback must be configured before hook activation");
    native_battle_loaded_callback=loaded;
}
bool native_battle_launch_ready(){
    if(closing.load()||!native_battle_launch_bindings_ready.load()||!native_battle_launch_session_prepared.load()||!native_battle_ready_callback)return false;
    try{return native_battle_ready_callback();}catch(...){return false;}
}
void native_battle_launch_require_ui(){
    if(!can_read()||!unity_ui_thread.load()||unity_ui_thread.load()!=GetCurrentThreadId())throw std::runtime_error("native battle operation outside observed Unity UI thread");
}
std::string native_battle_last_launch_error(){
    std::lock_guard<std::mutex> lock(native_battle_launch_error_lock);return native_battle_launch_error;
}
std::string native_battle_first_launch_error(){
    std::lock_guard<std::mutex> lock(native_battle_launch_error_lock);return native_battle_launch_first_error;
}
void native_battle_launch_reject(const char* detail){
    ++native_battle_launch_rejections;++blocked_battles;
    const std::string error=detail&&*detail?detail:"unspecified native battle launch rejection";
    // report() can immediately acquire its own lock and call the getter. Release
    // this independent lock first, and preserve the error across asset/success
    // reports, session preparation and disposal until a later rejection replaces it.
    {std::lock_guard<std::mutex> lock(native_battle_launch_error_lock);native_battle_launch_error=error;if(native_battle_launch_first_error.empty())native_battle_launch_first_error=error;}
    report((std::string("private-native-battle-launch-rejected: ")+error).c_str());
}
// field_object is for reference fields only. Value fields must be boxed using
// their actual metadata value size; reading actMeta into a void* would overflow.
void* native_battle_boxed_field(void* object,const char* name,TemporaryRoots& roots){
    if(!object)return nullptr;auto field=profile_field(api.il2cpp_object_get_class(object),name);
    auto type=api.il2cpp_class_from_type(api.il2cpp_field_get_type(field));if(!type)throw std::runtime_error("native battle field class missing");
    if(!api.il2cpp_class_is_valuetype(type)){void* value=nullptr;api.il2cpp_field_get_value(object,field,&value);return roots.retain(value);}
    uint32_t alignment=0;auto size=api.il2cpp_class_value_size(type,&alignment);if(size<1||size>4096)throw std::runtime_error("native battle field size bound");
    std::vector<uint8_t> bytes(size_t(size),uint8_t{});api.il2cpp_field_get_value(object,field,bytes.data());return roots.retain(value_box(type,bytes.data()));
}
std::string native_battle_controller_activity(void* raw_param,TemporaryRoots& roots){
    cnlaunch::ControllerRaw copy;if(!native_battle_controller_param_class||!read(raw_param,copy))throw std::runtime_error("unreadable native battle controller payload");
    auto boxed=roots.retain(value_box(native_battle_controller_param_class,copy.bytes.data()));
    auto meta=native_battle_boxed_field(boxed,"actMeta",roots);return text(field_object(meta,"activityId"));
}
std::string native_battle_current_activity(){
    TemporaryRoots roots;auto c=cls("Torappu.Battle.BattleInOut");if(!c)throw std::runtime_error("BattleInOut class unavailable");
    auto singleton=roots.retain(invoke(method(c,"get_instance",0),nullptr));if(!singleton)throw std::runtime_error("BattleInOut singleton unavailable");
    auto input=roots.retain(invoke(bound_methods.at(CN_BATTLE_IN_PARAMS.token),singleton));
    auto meta=native_battle_boxed_field(input,"actMeta",roots);return text(field_object(meta,"activityId"));
}
bool native_battle_manager_is_private(void* self){
    if(native_battle_manager_root&&hold_get(native_battle_manager_root)==self)return true;
    auto center=field_object(self,"dataCenter");return center&&act(field_object(center,"<actId>k__BackingField"));
}
void native_battle_release_pending(){
    native_battle_launch_pending.store(false);
    for(auto& handle:native_battle_queued.references)if(handle){hold_free(handle);handle=0;}
    native_battle_queued.queued_at=0;
}
void native_battle_queue_launch(const cnlaunch::RawStartBattleParam& value){
    // Managed pointers cannot be kept in a raw stack copy across UI frames.
    // Only GC handles survive the defer; rebuild the 64-byte payload on dispatch.
    native_battle_release_pending();
    const uint64_t values[]={value.act_id,value.stage_id,value.scene_id,value.mode_id,value.stage_name,value.stage_code,value.loading_pic_id};
    try{
        for(size_t i=0;i<7;++i)if(values[i]){auto object=reinterpret_cast<void*>(uintptr_t(values[i]));auto h=hold_new(object,false);if(!h)throw std::runtime_error("cannot retain pending native battle parameter");native_battle_queued.references[i]=h;}
        native_battle_queued.mode_type=value.mode_type;native_battle_queued.multi_mode_sub_type=value.multi_mode_sub_type;
        native_battle_queued.queued_at=GetTickCount64();native_battle_launch_pending.store(true);
    }catch(...){native_battle_release_pending();throw;}
}
void native_battle_prepare_session(){
    native_battle_launch_require_ui();
    native_battle_launch_session_prepared.store(false);
    if(!native_battle_launch_bindings_ready.load()||!native_battle_prepare_callback||!native_battle_ready_callback)throw std::runtime_error("native battle data bridge is not configured");
    if(!native_battle_prepare_callback(NATIVE_BATTLE_STAGE)||!native_battle_ready_callback())throw std::runtime_error("private battle session or typed initial snapshot unavailable");
    const bool resources_were_enabled=private_battle_resources.exchange(true);
    try{ensure_bundles();}catch(...){if(!resources_were_enabled&&!native_battle_launch_in_flight.load())private_battle_resources.store(false);throw;}
    native_battle_launch_session_prepared.store(true);
}
void native_battle_launch_preflight_training_fields(){
    // PlayerData readiness can precede ActivityDB loading act2. Bootstrap must
    // validate metadata only, without invoking GetActAutoChessData or touching
    // a live activity instance. Missing live data is a click-time rejection.
    native_battle_act_data_class=cls("Torappu.ActAutoChessData");
    native_battle_const_data_class=cls("Torappu.ActAutoChessData+ActAutoChessConstData");
    if(!native_battle_act_data_class||!native_battle_const_data_class)throw std::runtime_error("CN native training metadata classes unavailable");
    native_battle_act_const_field=profile_field(native_battle_act_data_class,"constData");
    native_battle_training_stage_field=profile_field(native_battle_const_data_class,"trStageId");
    native_battle_training_mode_field=profile_field(native_battle_const_data_class,"trainingModeId");
    if((api.il2cpp_field_get_flags(native_battle_act_const_field)&16)||!matches_type(api.il2cpp_field_get_type(native_battle_act_const_field),"Torappu.ActAutoChessData+ActAutoChessConstData"))throw std::runtime_error("CN native training constData metadata mismatch");
    for(auto field:{native_battle_training_stage_field,native_battle_training_mode_field})
        if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),"System.String"))throw std::runtime_error("CN native training string-field metadata mismatch");
}
void native_battle_launch_read_training_config(){
    native_battle_launch_require_ui();
    native_battle_training_stage.clear();native_battle_training_mode.clear();
    if(!native_battle_act_const_field||!native_battle_training_stage_field||!native_battle_training_mode_field)throw std::runtime_error("CN native training metadata preflight incomplete");
    TemporaryRoots roots;auto id=roots.retain(string_new(NATIVE_BATTLE_ACTIVITY));void* args[]={id};
    auto data=roots.retain(invoke(bound_methods.at(CN_BATTLE_ACT_DATA.token),nullptr,args));
    if(!data)throw std::runtime_error("CN native training data=null: act2autochess ActivityDB entry is not loaded");
    if(api.il2cpp_object_get_class(data)!=native_battle_act_data_class)throw std::runtime_error("CN native training data class mismatch");
    void* constants=nullptr;api.il2cpp_field_get_value(data,native_battle_act_const_field,&constants);roots.retain(constants);
    if(!constants)throw std::runtime_error("CN native training constData=null");
    if(api.il2cpp_object_get_class(constants)!=native_battle_const_data_class)throw std::runtime_error("CN native training constData class mismatch");
    void* stage_id=nullptr;void* mode_id=nullptr;
    api.il2cpp_field_get_value(constants,native_battle_training_stage_field,&stage_id);roots.retain(stage_id);
    api.il2cpp_field_get_value(constants,native_battle_training_mode_field,&mode_id);roots.retain(mode_id);
    const auto stage_id_text=text(stage_id),mode_id_text=text(mode_id);
    if(stage_id_text.empty())throw std::runtime_error("CN native training trStageId value is empty");
    if(mode_id_text.empty())throw std::runtime_error("CN native training trainingModeId value is empty");
    if(stage_id_text!=NATIVE_BATTLE_TRAINING_STAGE)throw std::runtime_error("CN native training trStageId differs from pinned CN catalog: "+stage_id_text);
    if(mode_id_text!=NATIVE_BATTLE_TRAINING_MODE)throw std::runtime_error("CN native training trainingModeId differs from pinned CN catalog: "+mode_id_text);
    native_battle_training_stage=stage_id_text;native_battle_training_mode=mode_id_text;
}
std::string native_battle_prepare_activity(void* object,const char* field_name){
    if(!object)throw std::runtime_error("native prepare identity owner missing");
    auto field=profile_field(api.il2cpp_object_get_class(object),field_name);
    if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),"System.String"))throw std::runtime_error("native prepare identity field metadata mismatch");
    void* id=nullptr;api.il2cpp_field_get_value(object,field,&id);
    if(!id)throw std::runtime_error("native prepare activity identity missing");
    auto activity=text(id);if(activity.empty())throw std::runtime_error("native prepare activity identity empty or unreadable");
    return activity;
}
#include "native_single_entry.inc"
#include "native_prebattle_presentation.inc"

// These UI state/selection calls occur outside AutoChessService.OnUpdate.
// The native list and confirm handler independently read PlayerData.autoChessPerm;
// without this exact activity scope they see the ordinary account's empty bands.
void native_battle_band_load(void* self,void* prepare_model,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{
        auto activity=native_battle_prepare_activity(prepare_model,"<actId>k__BackingField");
        if(!cnlaunch::target_activity(activity)){lease.release();native_battle_old_band_load(self,prepare_model,method_info);return;}
        native_battle_launch_require_ui();PrivateDataScope scope(true);
        if(native_multiplayer_entry_active()){native_battle_old_band_load(self,prepare_model,method_info);return;}
        NativePrebattleUiScope presentation(prepare_model);native_battle_old_band_load(self,prepare_model,method_info);
    }catch(const std::exception& ex){report((std::string("private-native-band-load-rejected: ")+ex.what()).c_str());}
}
void native_battle_band_refresh(void* self,void* prepare_model,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{
        auto activity=native_battle_prepare_activity(prepare_model,"<actId>k__BackingField");
        if(!cnlaunch::target_activity(activity)){lease.release();native_battle_old_band_refresh(self,prepare_model,method_info);return;}
        native_battle_launch_require_ui();PrivateDataScope scope(true);
        if(native_multiplayer_entry_active()){native_battle_old_band_refresh(self,prepare_model,method_info);return;}
        NativePrebattleUiScope presentation(prepare_model);native_battle_old_band_refresh(self,prepare_model,method_info);
    }catch(const std::exception& ex){report((std::string("private-native-band-refresh-rejected: ")+ex.what()).c_str());}
}
void native_battle_choose_band(void* self,void* band_id,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{
        auto activity=native_battle_prepare_activity(self,"m_actId");
        if(!cnlaunch::target_activity(activity)){lease.release();native_battle_old_choose_band(self,band_id,method_info);return;}
        native_battle_launch_require_ui();PrivateDataScope scope(true);
        if(native_multiplayer_entry_active()){native_battle_old_choose_band(self,band_id,method_info);return;}
        auto selected_band=text(band_id);
        // The private loading session must accept the exact UI choice and
        // rebuild its typed snapshot before the native training service emits
        // SceneStartSuc. Even an empty choice reaches the private rejection
        // path; no rejected request is forwarded into the native state change.
        const bool accepted=native_battle_select_band(selected_band.c_str());
        if(!accepted||selected_band.empty()){report("private-native-band-choice-not-accepted");return;}
        native_prebattle_select_band(selected_band.c_str());
        native_battle_old_choose_band(self,band_id,method_info);
    }catch(const std::exception& ex){report((std::string("private-native-band-choice-rejected: ")+ex.what()).c_str());}
}
struct NativeBattleBandEnterScope {
    void* previous;
    explicit NativeBattleBandEnterScope(void* state):previous(native_battle_band_entering_state){native_battle_band_entering_state=state;}
    ~NativeBattleBandEnterScope(){native_battle_band_entering_state=previous;}
};
void native_battle_band_state_enter(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(native_multiplayer_entry_active()){PrivateDataScope scope(true);native_battle_old_band_state_enter(self,method_info);return;}
    // The original still builds the band list, binds the state and handles its
    // selection callbacks. This stack-only context identifies its final AVG call.
    NativeBattleBandEnterScope context(self);NativePrebattleStateScope presentation;native_battle_old_band_state_enter(self,method_info);
}
bool native_battle_custom_operation(void* operation,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;
    if(native_multiplayer_entry_active()){lease.release();return native_battle_old_custom_operation(operation,method_info);}
    if(!native_battle_band_entering_state||text(operation)!=NATIVE_BATTLE_BAND_TUTORIAL){lease.release();return native_battle_old_custom_operation(operation,method_info);}
    try{
        native_battle_launch_require_ui();
        TemporaryRoots roots;auto bean=roots.retain(field_object(native_battle_band_entering_state,"m_stateBean"));
        auto property=roots.retain(field_object(bean,"property"));if(!property)throw std::runtime_error("native band tutorial property missing");
        auto getter=method(api.il2cpp_object_get_class(property),"get_Value",0);
        if(!getter||!matches_type(api.il2cpp_method_get_return_type(getter),"Torappu.UI.AutoChess.AutoChessBandChooseViewModel"))throw std::runtime_error("native band tutorial value getter metadata mismatch");
        auto view_model=roots.retain(invoke(getter,property));auto activity=native_battle_prepare_activity(view_model,"actId");
        if(!cnlaunch::target_activity(activity)){lease.release();return native_battle_old_custom_operation(operation,method_info);}
        ++blocked_guidebooks;report("private-native-band-tutorial-disabled-before-start");
        // This one-argument AVG API has no completion delegate. OnStateEnter
        // ignores its bool result, so declining it creates no mask or pending
        // callback and leaves the actual strategy selection fully operational.
        return false;
    }catch(const std::exception& ex){report((std::string("private-native-band-tutorial-scope-rejected: ")+ex.what()).c_str());return false;}
}
void native_battle_prepare_preflight(){
    for(const auto& entry:std::array<std::pair<const char*,const char*>,3>{{
        {"Torappu.UI.AutoChess.AutoChessPrepareModel","<actId>k__BackingField"},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_actId"},
        {"Torappu.UI.AutoChess.AutoChessBandChooseViewModel","actId"}}}){
        auto c=cls(entry.first);if(!c)throw std::runtime_error("native prepare identity class unavailable");
        auto field=profile_field(c,entry.second);
        if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),"System.String"))throw std::runtime_error("native prepare identity metadata mismatch");
    }
    auto state=cls("Torappu.UI.AutoChess.AutoChessBandChooseState"),bean=cls("Torappu.UI.AutoChess.AutoChessBandChooseStateBean");
    if(!state||!bean)throw std::runtime_error("native band tutorial metadata unavailable");
    if(!matches_type(api.il2cpp_field_get_type(profile_field(state,"m_stateBean")),"Torappu.UI.AutoChess.AutoChessBandChooseStateBean")||
       !matches_type(api.il2cpp_field_get_type(profile_field(bean,"property")),"Torappu.UI.AutoChess.AutoChessBandChooseProperty"))throw std::runtime_error("native band tutorial field metadata mismatch");
}
// Called from the existing StartTraining three-argument guard. Returns true if
// the request was consumed; false leaves a positively identified unrelated act
// to the existing guard/original. Missing identity is rejected, never inferred
// from private_view (which changes as pages open and close).
bool native_battle_launch_training(void* param,void* callback,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return true;
    bool called_original=false;
    try{
        if(!param)throw std::runtime_error("training parameter missing");auto id=field_object(param,"activityId");
        if(!id)throw std::runtime_error("training activity identity missing");auto activity=text(id);
        if(activity.empty())throw std::runtime_error("training activity identity empty or unreadable");
        if(!cnlaunch::target_activity(activity))return false;
        native_battle_launch_require_ui();if(!old_training_start)throw std::runtime_error("native training trampoline unavailable");
        if(native_battle_launch_in_flight.load()||native_battle_launch_pending.load()){
            native_battle_launch_reject("a private native battle launch is already pending or active");
            try{cancel_join(callback);}catch(...){report("private-native-training-duplicate-callback-failed");}return true;
        }
        native_battle_launch_read_training_config();
        native_battle_launch_team_ready.store(false);native_battle_launch_scene_entered.store(false);native_battle_launch_loading_complete.store(false);
        native_battle_prepare_session();
        TemporaryRoots roots;roots.retain(param);roots.retain(callback);
        PrivateDataScope private_scope(true);NativeBattleDepth depth(native_battle_training_depth);
        called_original=true;old_training_start(param,callback,method_info);
        // The native function invokes its callback before constructing team data.
        // Do not dispatch any queued utility launch until this call has returned.
        native_battle_launch_team_ready.store(true);report("private-native-training-team-initialized");return true;
    }catch(const std::exception& ex){
        native_battle_launch_team_ready.store(false);native_battle_launch_session_prepared.store(false);
        native_battle_release_pending();native_battle_launch_reject(ex.what());
        if(!called_original&&unity_ui_thread.load()==GetCurrentThreadId())try{cancel_join(callback);}catch(...){report("private-native-training-reject-callback-failed");}
        return true;
    }
}
void native_battle_util_start(void* raw_param,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    cnlaunch::RawStartBattleParam input;
    try{
        if(!read(raw_param,input))throw std::runtime_error("unreadable 64-byte AutoChess launch parameter");
        auto activity=text(reinterpret_cast<void*>(uintptr_t(input.act_id)));
        if(activity.empty())throw std::runtime_error("AutoChess launch activity identity missing");
        if(!cnlaunch::target_activity(activity)){lease.release();native_battle_old_util_start(raw_param,method_info);return;}
        if(native_multiplayer_launch_util(raw_param,method_info))return;
        ++native_battle_launch_attempts;native_battle_launch_require_ui();
        if(native_battle_launch_in_flight.load()){native_battle_launch_reject("a private native battle launch is already active");return;}
        if(!cnlaunch::is_private_local_launch(activity,input.mode_type))throw std::runtime_error("only private LOCAL training launch is enabled");
        auto incoming_stage=text(reinterpret_cast<void*>(uintptr_t(input.stage_id))),incoming_mode=text(reinterpret_cast<void*>(uintptr_t(input.mode_id)));
        if(!text(reinterpret_cast<void*>(uintptr_t(input.scene_id))).empty())throw std::runtime_error("LOCAL launch cannot consume a remote scene/session identity");
        bool original_training=incoming_stage==native_battle_training_stage&&incoming_mode==native_battle_training_mode;
        bool approved_map=cncombat::stage_allowed(incoming_stage,incoming_mode);
        if(!original_training&&!approved_map)throw std::runtime_error("activity stage/mode is outside the approved native battle launch");
        if(!native_battle_launch_team_ready.load()&&!native_battle_training_depth)throw std::runtime_error("native training team is not initialized");
        native_battle_prepare_session();
        const auto selected_mode=nb_string(native_battle_session_state(),"modeId"),selected_stage=nb_string(native_battle_session_state(),"stageId");
        if(!cncombat::stage_allowed(selected_stage,selected_mode))throw std::runtime_error("private launch mode/map differs from its prepared session");
        TemporaryRoots roots;auto id=roots.retain(string_new(NATIVE_BATTLE_ACTIVITY));auto mode=roots.retain(string_new(selected_mode.c_str()));
        void* args[]={id,mode};auto mode_data=roots.retain(invoke(bound_methods.at(CN_BATTLE_MODE_DATA.token),nullptr,args));if(!mode_data)throw std::runtime_error("CN private mode configuration unavailable");
        auto target_stage=roots.retain(string_new(selected_stage.c_str()));
        input.stage_id=uint64_t(reinterpret_cast<uintptr_t>(target_stage));input.mode_id=uint64_t(reinterpret_cast<uintptr_t>(mode));
        // LOCAL training writes System.String.Empty here. It is a server scene
        // identity used by multi-battle service configs, not a map/stage ID.
        // Retain the native empty reference; never replace it with stageId/runId.
        input.multi_mode_sub_type=0;
        input.stage_name=uint64_t(reinterpret_cast<uintptr_t>(roots.retain(field_object(mode_data,"name"))));
        input.stage_code=uint64_t(reinterpret_cast<uintptr_t>(roots.retain(field_object(mode_data,"code"))));
        input.loading_pic_id=uint64_t(reinterpret_cast<uintptr_t>(roots.retain(field_object(mode_data,"loadingPicId"))));
        if(native_battle_training_depth){native_battle_queue_launch(input);report("private-native-battle-launch-deferred-until-team-ready");return;}
        if(!native_battle_launch_ready())throw std::runtime_error("private native battle bridge is not ready");
        native_battle_launch_in_flight.store(true);native_battle_launch_loading_complete.store(false);
        PrivateDataScope scope(true);NativeBattleDepth utility_depth(native_battle_utility_depth);
        native_battle_old_util_start(&input,method_info);
    }catch(const std::exception& ex){native_battle_launch_in_flight.store(false);native_battle_launch_reject(ex.what());}
}
void* native_battle_controller_start(void* raw_param,void* plugin,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;
    try{
        TemporaryRoots roots;auto activity=native_battle_controller_activity(raw_param,roots);
        if(native_battle_utility_depth&&!cnlaunch::target_activity(activity))throw std::runtime_error("private battle controller lost its activity identity");
        if(!cnlaunch::target_activity(activity)){lease.release();return native_battle_old_controller_start(raw_param,plugin,method_info);}
        void* multiplayer_handler=nullptr;if(native_multiplayer_launch_controller(raw_param,plugin,method_info,multiplayer_handler))return multiplayer_handler;
        native_battle_launch_require_ui();
        if(!native_battle_utility_depth||!native_battle_launch_in_flight.load()||!native_battle_launch_ready())throw std::runtime_error("private battle controller called outside validated LOCAL launch");
        PrivateDataScope scope(true);++native_battle_launch_local_starts;
        // Both static methods receive Param indirectly in RCX, plugin in RDX and
        // hidden MethodInfo in R8. The result is the current cached Handler;
        // _OnSucceed clears that cache before invoking its completion action.
        // A synchronous successful local dispatch can therefore return null.
        // Keep in_flight for the later GameManager.Start/preload validation;
        // null alone does not distinguish success from a rejected context.
        auto handler=native_battle_local_start(raw_param,plugin,bound_methods.at(CN_BATTLE_CONTROLLER_LOCAL.token));
        report("private-native-local-battle-dispatched");return handler;
    }catch(const std::exception& ex){native_battle_launch_in_flight.store(false);native_battle_launch_reject(ex.what());return nullptr;}
}
void native_battle_launch_tick(){
    native_battle_launch_require_ui();
    native_multiplayer_drain_main();
    native_multiplayer_field_tick();
    native_multiplayer_boss_tick();
    native_multiplayer_hp_preview_tick();
    native_multiplayer_deadline_tick();
    if(native_battle_tick_callback&&native_battle_launch_session_prepared.load()){PrivateDataScope scope(true);native_battle_tick_callback();}
    if(!native_battle_launch_pending.load())return;
    if(GetTickCount64()-native_battle_queued.queued_at>15000){native_battle_release_pending();native_battle_launch_reject("deferred native launch timed out");return;}
    if(!native_battle_launch_team_ready.load()||native_battle_training_depth||!native_battle_launch_ready())return;
    cnlaunch::RawStartBattleParam input;
    uint64_t* values[]={&input.act_id,&input.stage_id,&input.scene_id,&input.mode_id,&input.stage_name,&input.stage_code,&input.loading_pic_id};
    TemporaryRoots roots;
    for(size_t i=0;i<7;++i)if(native_battle_queued.references[i]){auto value=roots.retain(hold_get(native_battle_queued.references[i]));if(!value)throw std::runtime_error("pending native battle parameter lost its GC root");*values[i]=uint64_t(reinterpret_cast<uintptr_t>(value));}
    input.mode_type=native_battle_queued.mode_type;input.multi_mode_sub_type=native_battle_queued.multi_mode_sub_type;
    native_battle_release_pending();native_battle_util_start(&input,bound_methods.at(CN_BATTLE_UTIL_START.token));
}
void native_battle_service_update(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    bool target=false;
    try{auto param=field_object(self,"<param>k__BackingField");target=param&&act(field_object(param,"activityId"));}
    catch(const std::exception& ex){native_battle_launch_reject(ex.what());return;}
    if(!(target&&native_multiplayer_entry_active())){PrivateDataScope scope(target);native_battle_old_service_update(self,method_info);}
    if(!target)return;
    try{native_battle_launch_tick();}catch(const std::exception& ex){native_battle_release_pending();native_battle_launch_reject(ex.what());}
}
void native_battle_manager_start(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{
        bool target=cnlaunch::target_activity(native_battle_current_activity());
        if(!target){lease.release();native_battle_old_manager_start(self,method_info);return;}
        native_battle_launch_require_ui();if(!native_battle_launch_in_flight.load()||!(native_multiplayer_entry_active()?native_multiplayer_dispatch_ready():native_battle_launch_ready()))throw std::runtime_error("private Unity battle entered without a prepared native session");
        if(native_battle_manager_root)hold_free(native_battle_manager_root);native_battle_manager_root=hold_new(self,false);if(!native_battle_manager_root)throw std::runtime_error("cannot retain private native battle manager");
        if(native_multiplayer_entry_active())native_battle_scene_bind_manager(self);
        PrivateDataScope scope(true);native_battle_old_manager_start(self,method_info);
        native_battle_launch_scene_entered.store(true);report("private-native-autochess-manager-started");
        // Readiness is signalled when the native preload coroutine completes,
        // not here: Start() precedes unit pool/level resource loading.
    }catch(const std::exception& ex){native_battle_launch_in_flight.store(false);native_battle_launch_reject(ex.what());}
}
void native_battle_manager_load(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{bool target=native_battle_manager_is_private(self);if(target){native_battle_launch_require_ui();if(!(native_multiplayer_entry_active()?native_multiplayer_dispatch_ready():native_battle_launch_ready()))throw std::runtime_error("private battle data unavailable before Unity game load");}PrivateDataScope scope(target);native_battle_old_manager_load(self,method_info);}
    catch(const std::exception& ex){native_battle_launch_reject(ex.what());}
}
bool native_multiplayer_help_defer_manager_finish(void*,int32_t);
void native_battle_manager_finish(void* self,int32_t result,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{bool target=native_battle_manager_is_private(self);if(target)native_battle_launch_require_ui();PrivateDataScope scope(target);if(target&&native_multiplayer_help_defer_manager_finish(self,result))return;native_battle_old_manager_finish(self,result,method_info);}
    catch(const std::exception& ex){native_battle_launch_reject(ex.what());}
}
void native_battle_manager_dispose(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    bool target=false,owned=false;
    try{target=native_battle_manager_is_private(self);owned=native_battle_manager_root&&hold_get(native_battle_manager_root)==self;if(target&&owned)native_fault_capture_scope(false);PrivateDataScope scope(target);native_battle_old_manager_dispose(self,method_info);}
    catch(const std::exception& ex){native_battle_launch_reject(ex.what());}
    // A delayed Dispose from the previous controller cannot revoke the newly
    // launched manager (or a launch whose manager Start has not run yet).
    if(target&&owned){if(native_battle_manager_root&&hold_get(native_battle_manager_root)!=self)return;
        if(native_battle_manager_root){hold_free(native_battle_manager_root);native_battle_manager_root=0;}
        if(native_multiplayer_entry_active()&&(native_battle_scene_pending()||native_multiplayer_observation_reloading()||native_multiplayer_observation_starting_reload()||native_multiplayer_preparation_view_owns_scene_transition())){native_battle_launch_loading_complete.store(false);return;}
        native_battle_scene_release();
        native_battle_release_pending();native_battle_launch_in_flight.store(false);native_battle_launch_scene_entered.store(false);native_battle_launch_loading_complete.store(false);native_battle_launch_team_ready.store(false);native_battle_launch_session_prepared.store(false);}
}
bool native_battle_loading_move(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;
    bool target=false;
    try{
        target=native_battle_manager_root&&native_battle_launch_in_flight.load()&&cnlaunch::target_activity(native_battle_current_activity());
        if(target){native_battle_launch_require_ui();if(!(native_multiplayer_entry_active()?native_multiplayer_dispatch_ready():native_battle_launch_ready()))throw std::runtime_error("private bridge unavailable during native resource preload");}
        const bool multiplayer=target&&native_multiplayer_entry_active();const bool owned=!multiplayer||native_battle_scene_preload_owner(self);
        PrivateDataScope scope(target);bool waiting=native_battle_old_loading_move(self,method_info);
        if(target&&owned&&!waiting&&(!multiplayer||native_battle_scene_preload_finished(self))&&!native_battle_launch_loading_complete.exchange(true)){
            if(native_battle_loaded_callback&&!native_multiplayer_entry_active())native_battle_loaded_callback();
            report("private-native-autochess-preload-complete");
            if(native_multiplayer_entry_active())native_multiplayer_preparation_view_loader_completed();
        }return waiting;
    }catch(const std::exception& ex){native_battle_launch_reject(ex.what());return false;}
}
void native_battle_launch_check_value(void* type,size_t expected,const char* label){
    uint32_t alignment=0;auto size=type?api.il2cpp_class_value_size(type,&alignment):0;
    if(!type||!api.il2cpp_class_is_valuetype(type)||size!=int32_t(expected))throw std::runtime_error(std::string(label)+" native indirect value ABI mismatch");
}
#include "native_band_range.inc"
void native_battle_launch_setup(){
    if(native_battle_launch_bindings_ready.load())return;
    if(!native_battle_prepare_callback||!native_battle_ready_callback||!native_battle_tick_callback||!native_battle_loaded_callback)throw std::runtime_error("native launch requires configured data bridge callbacks");
    static_assert(sizeof(void*)==8&&sizeof(cnlaunch::RawStartBattleParam)==64&&sizeof(cnlaunch::ControllerRaw)==680,"pinned CN Win64 launch ABI");
    // hook() must run before any bind_method() for its target because the host
    // hook helper intentionally skips an already-bound token.
    hook(CN_BATTLE_UTIL_START,native_battle_util_start,native_battle_old_util_start);
    hook(CN_BATTLE_CONTROLLER_START,native_battle_controller_start,native_battle_old_controller_start);
    hook(CN_BATTLE_SERVICE_UPDATE,native_battle_service_update,native_battle_old_service_update);
    hook(CN_BATTLE_MANAGER_START,native_battle_manager_start,native_battle_old_manager_start);
    hook(CN_BATTLE_MANAGER_LOAD,native_battle_manager_load,native_battle_old_manager_load);
    hook(CN_BATTLE_MANAGER_FINISH,native_battle_manager_finish,native_battle_old_manager_finish);
    hook(CN_BATTLE_MANAGER_DISPOSE,native_battle_manager_dispose,native_battle_old_manager_dispose);
    hook(CN_BATTLE_LOADING_MOVE,native_battle_loading_move,native_battle_old_loading_move);
    hook(CN_BATTLE_BAND_LOAD,native_battle_band_load,native_battle_old_band_load);
    hook(CN_BATTLE_BAND_REFRESH,native_battle_band_refresh,native_battle_old_band_refresh);
    hook(CN_BATTLE_CHOOSE_BAND,native_battle_choose_band,native_battle_old_choose_band);
    hook(CN_BATTLE_BAND_STATE_ENTER,native_battle_band_state_enter,native_battle_old_band_state_enter);
    hook(CN_BATTLE_CUSTOM_OPERATION,native_battle_custom_operation,native_battle_old_custom_operation);
    auto local_method=bind_method(CN_BATTLE_CONTROLLER_LOCAL);void* local_pointer=nullptr;if(!read(local_method,local_pointer)||!local_pointer)throw std::runtime_error("native StartLocalBattle target unavailable");std::memcpy(&native_battle_local_start,&local_pointer,sizeof(native_battle_local_start));
    bind_method(CN_BATTLE_ACT_DATA);bind_method(CN_BATTLE_MODE_DATA);bind_method(CN_BATTLE_IN_PARAMS);
    native_battle_start_param_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(CN_BATTLE_UTIL_START.token),0));
    native_battle_controller_param_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(CN_BATTLE_CONTROLLER_START.token),0));
    native_battle_launch_check_value(native_battle_start_param_class,64,"AutoChessUtil.StartBattleParam");native_battle_launch_check_value(native_battle_controller_param_class,680,"BattleStartController.Param");
    if(api.il2cpp_class_from_type(api.il2cpp_method_get_param(local_method,0))!=native_battle_controller_param_class)throw std::runtime_error("StartLocalBattle parameter class differs from StartBattle");
    const char* names[]={"actId","stageId","sceneId","modeId","stageName","stageCode","loadingPicId","modeType","multiModeSubType"};
    const size_t offsets[]={16,24,32,40,48,56,64,72,76};
    for(size_t i=0;i<9;++i){auto field=profile_field(native_battle_start_param_class,names[i]);if(api.il2cpp_field_get_offset(field)!=offsets[i]||!matches_type(api.il2cpp_field_get_type(field),i<7?"System.String":(i==7?"Torappu.ActAutoChessModeType":"Torappu.ActAutoChessMultiModeSubType")))throw std::runtime_error("AutoChess launch field layout mismatch");}
    auto result_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(CN_BATTLE_MANAGER_FINISH.token),0));if(!api.il2cpp_class_is_enum(result_class)||api.il2cpp_type_get_type(api.il2cpp_class_enum_basetype(result_class))!=8)throw std::runtime_error("native battle result enum ABI mismatch");
    native_battle_launch_preflight_training_fields();
    native_battle_prepare_preflight();
    native_single_entry_setup();
    native_prebattle_presentation_setup();
    native_single_band_range_setup();
    native_battle_launch_bindings_ready.store(true);
}
