// Include inside namespace cn after nb_hook, before native_combat.inc.
// Private LOCAL combat uses the original tutorial Start's non-training branch.
// The real GM training flag, its state machine and task queue are never edited.
#pragma once

using NBTut_Start=void(*)(void*,void*,const void*);
using NBTut_Training=bool(*)(void*,const void*);
NBTut_Start nbtut_original_start=nullptr;
NBTut_Training nbtut_original_training=nullptr;
thread_local void* nbtut_training_owner=nullptr;
struct NBTut_QueryScope{
    void* previous;
    explicit NBTut_QueryScope(void* owner):previous(nbtut_training_owner){nbtut_training_owner=owner;}
    ~NBTut_QueryScope(){nbtut_training_owner=previous;}
};
std::atomic<unsigned> nbtut_configurations{0},nbtut_training_queries{0},nbtut_native_releases{0},nbtut_forwarded{0};
std::atomic<bool> nbtut_configured{false},nbtut_failed{false};
std::recursive_mutex nbtut_lock;
std::string nbtut_run,nbtut_error;

bool nbtut_flag(void* object,const char* name){
    uint8_t value=0;auto field=profile_field(api.il2cpp_object_get_class(object),name);
    if(!matches_type(api.il2cpp_field_get_type(field),"System.Boolean"))throw std::runtime_error("private tutorial Boolean field contract mismatch");
    api.il2cpp_field_get_value(object,field,&value);return value!=0;
}
void* nbtut_owner(void* tutorial){
    // GM.Start initializes DC.actId and constructs/retains its LOCAL bridge
    // before Tutorial.Start (b44e1a). Bridge.Start follows at b44e51, so
    // nb_started/nb_loaded are necessarily false at this ownership boundary.
    if(!tutorial||!nb_prepared.load()||!private_battle_resources.load()||nb_run_id.empty()||
       unity_ui_thread.load()!=GetCurrentThreadId()||nb_state.kind!=cnjson::Value::Object)return nullptr;
    if(nb_string(nb_state,"runId")!=nb_run_id||nb_string(nb_state,"activityId")!="act2autochess"||
       !cncombat::stage_allowed(nb_string(nb_state,"stageId"),nb_string(nb_state,"modeId")))return nullptr;
    auto centre=nb_centre(),manager=nb_manager(),bridge=nb_retained("native-battle-local-player");
    if(!centre||!manager||!bridge||!act(field_object(centre,"<actId>k__BackingField"))||
       field_object(manager,"dataCenter")!=centre||field_object(manager,"dataBridge")!=bridge||
       api.il2cpp_object_get_class(bridge)!=cls("Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer")||
       field_object(manager,"tutorialManager")!=tutorial||
       api.il2cpp_object_get_class(tutorial)!=cls("Torappu.Battle.AutoChess.AutoChessTutorialManager"))return nullptr;
    return manager;
}
bool nbtut_training(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);
    if(lease&&!closing.load()&&self&&nbtut_training_owner==self){++nbtut_training_queries;return false;}
    lease.release();return nbtut_original_training(self,method_info);
}
void nbtut_release_existing(void* tutorial){
    auto task=field_object(tutorial,"m_blockBattleEndStateTask");if(!task)return;
    if(api.il2cpp_object_get_class(task)!=cls("Torappu.Battle.AutoChess.AutoChessTutorialManager+BlockSubStateTask"))
        throw std::runtime_error("private tutorial END task class mismatch");
    if(!nbtut_flag(task,"m_blockState"))return;
    // This is exactly the native AVG completion callback's release operation
    // (60138a0/b6a3b0). Let TaskManager observe taskFinished and advance normally.
    uint8_t block=0;void* args[]={&block};invoke(nb_method(0x60138a3),task,args);
    if(nbtut_flag(task,"m_blockState"))throw std::runtime_error("native tutorial END release did not complete");
    ++nbtut_native_releases;
}
void nbtut_start(void* self,void* config,const void* method_info){
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load()){lease.release();nbtut_original_start(self,config,method_info);return;}
    void* owner=nullptr;
    try{owner=nbtut_owner(self);}catch(const std::exception& error){nb_error("native-tutorial-scope",error);return;}
    if(!owner){++nbtut_forwarded;lease.release();nbtut_original_start(self,config,method_info);return;}
    try{
        nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nbtut_lock);
        nbtut_configured=false;nbtut_failed=false;
        // Start calls the original GM getter at b6a072, stores the answer in
        // its own m_isTrainningMode, then returns on false at b6a083..85. Only
        // that exact nested getter receives false; all other readers retain
        // the real LOCAL training mode for combat, loading and settlement.
        {NBTut_QueryScope query(owner);nbtut_original_start(self,config,method_info);}
        if(nbtut_flag(self,"m_isTrainningMode"))throw std::runtime_error("native tutorial Start did not take its non-training branch");
        nbtut_release_existing(self);
        nbtut_run=nb_run_id;nbtut_error.clear();nbtut_configured=true;++nbtut_configurations;
        report("private-native-tutorial-non-training-branch-configured");
    }catch(const std::exception& error){
        {std::lock_guard<std::recursive_mutex> lock(nbtut_lock);nbtut_failed=true;nbtut_error=error.what();}
        nb_error("native-tutorial-start",error);
    }
}
void native_battle_tutorial_preflight(){
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nbtut_lock);
    if(!nb_started.load()||!nb_loaded.load())throw std::runtime_error("private tutorial Ready requires completed native preload");
    auto manager=nb_manager();auto tutorial=field_object(manager,"tutorialManager");
    if(!tutorial||!nbtut_owner(tutorial)||!nbtut_configured.load()||nbtut_failed.load()||nbtut_run!=nb_run_id||
       nbtut_flag(tutorial,"m_isTrainningMode"))throw std::runtime_error("private native tutorial initialization incomplete: "+nbtut_error);
    auto task=field_object(tutorial,"m_blockBattleEndStateTask");
    if(task&&nbtut_flag(task,"m_blockState"))throw std::runtime_error("private native tutorial END task unexpectedly blocked");
}
void native_battle_tutorial_reset(){
    std::lock_guard<std::recursive_mutex> lock(nbtut_lock);nbtut_configured=false;nbtut_failed=false;nbtut_run.clear();nbtut_error.clear();
}
cnjson::Value native_battle_tutorial_status(bool details){
    std::unique_lock<std::recursive_mutex> lock(nbtut_lock,std::try_to_lock);auto value=nb_object();
    value.object["snapshotBusy"]=nb_boolean(!lock.owns_lock());value.object["configured"]=nb_boolean(nbtut_configured.load());
    value.object["failed"]=nb_boolean(nbtut_failed.load());value.object["configurations"]=nb_integer(nbtut_configurations.load());
    value.object["scopedTrainingQueries"]=nb_integer(nbtut_training_queries.load());value.object["releasedNativeTasks"]=nb_integer(nbtut_native_releases.load());
    value.object["forwardedStarts"]=nb_integer(nbtut_forwarded.load());
    if(details&&lock.owns_lock()){value.object["runId"]=json_text(nbtut_run);value.object["lastError"]=json_text(nbtut_error);}return value;
}
void native_battle_tutorial_setup(){
    nb_hook(0x6013680,nbtut_training,nbtut_original_training);
    nb_hook(0x601387f,nbtut_start,nbtut_original_start);
}
