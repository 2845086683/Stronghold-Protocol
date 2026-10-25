// Include after native_multiplayer_dispatch.inc; original Unity step host owns
// time/frame execution. The worker only reads two atomic receipt cursors.
#pragma once
#include "native_multiplayer_boss_policy.h"

static const CN_Method NMB_METHODS[]={
{"Torappu.UI.AutoChess.Server.AutoChessServiceBattlePhase","RevStep",0x602bd69,0x27f0f10ULL,1,false,"System.Void",{"Torappu.AutoChessBattleStepData"}},
{"Torappu.UI.AutoChess.Server.AutoChessServiceObjPool","AllocateStepData",0x602bd39,0x27f33b0ULL,0,true,"Torappu.AutoChessBattleStepData",{}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","get_inStepMode",0x6013684,0xb47680ULL,0,false,"System.Boolean",{}},
};
cnmulti_boss::Journal nmb_journal;
cnjson::Value nmb_view;
void* nmb_phase=nullptr;
std::atomic<int64_t> nmb_received{-1},nmb_epoch{0};
std::atomic<unsigned> nmb_frames{0},nmb_suppressed{0};
std::atomic<unsigned> nmb_fast_acks{0};
std::atomic<unsigned> nmb_windows{0};
bool nmb_window_dirty=false;
std::atomic<bool> nmb_ready{false};
void* nmb_ack_owner=nullptr;std::string nmb_ack_identity;
std::set<int32_t> nmb_known_acks,nmb_pending_acks;
void nmb_reset_acknowledgements(){nmb_ack_owner=nullptr;nmb_ack_identity.clear();nmb_known_acks.clear();nmb_pending_acks.clear();}
int64_t native_multiplayer_boss_received_step(){return nmb_received.load();}
int64_t native_multiplayer_boss_epoch(){return nmb_epoch.load();}
void native_multiplayer_boss_reset(){
    nb_main_thread();nmb_journal={};nmb_view={};nmb_phase=nullptr;nmb_received.store(-1);nmb_epoch.store(0);nmb_window_dirty=false;nmb_reset_acknowledgements();
}
void nmb_receive_acknowledgements(const cnmulti_boss::Window& plan){
    const auto& owner=plan.next;
    const auto identity=owner.room+":"+owner.timeline+":"+std::to_string(owner.epoch)+":"+std::to_string(owner.group);
    if(identity!=nmb_ack_identity){nmb_reset_acknowledgements();nmb_ack_identity=identity;}
    // MAX_STEPS=18000 is validated by the policy. Preserve explicit future
    // acknowledgements after the server's bounded 128-entry window rolls on.
    for(auto seq:plan.acknowledged)if(nmb_known_acks.insert(seq).second)nmb_pending_acks.insert(seq);
}
void* nmb_remove_method(void* dictionary){
    auto type=api.il2cpp_object_get_class(dictionary);
    if(!matches_type(api.il2cpp_class_get_type(type),"Torappu.ListDict`2<System.Int32,System.Int32>"))throw std::runtime_error("private native boss prediction cache type mismatch");
    void* iterator=nullptr;
    while(auto method=api.il2cpp_class_get_methods(type,&iterator))if(api.il2cpp_method_get_token(method)==0x60001a7){
        uint32_t implementation=0;
        if(std::strcmp(api.il2cpp_method_get_name(method),"Remove")||api.il2cpp_method_get_param_count(method)!=1||
            api.il2cpp_method_is_generic(method)||(api.il2cpp_method_get_flags(method,&implementation)&16)||
            !matches_type(api.il2cpp_method_get_return_type(method),"System.Boolean"))throw std::runtime_error("private native boss prediction removal declaration mismatch");
        auto param=api.il2cpp_method_get_param(method,0);
        if(api.il2cpp_type_is_byref(param)||(!matches_type(param,"System.Int32")&&api.il2cpp_type_get_type(param)!=19))throw std::runtime_error("private native boss prediction removal argument mismatch");
        return method;
    }
    throw std::runtime_error("private native boss prediction removal method absent");
}
void nmb_apply_acknowledgements(void* manager){
    // Remove only explicitly server-accepted, already completed native seqs.
    // Do not apply future StepDn HP early, clear unknown predictions, raise the
    // original 30-step timeout, or change native healing/rebirth behavior.
    auto owner=field_object(manager,"stepModeManager");
    if(!owner||api.il2cpp_object_get_class(owner)!=cls("Torappu.Battle.AutoChess.AutoChessStepModeManager"))return;
    if(nb_native_int(owner,"m_playerGroup")!=nmb_journal.group)return;
    if(owner!=nmb_ack_owner){nmb_ack_owner=owner;nmb_pending_acks=nmb_known_acks;}
    const auto played=nb_native_int(owner,"m_seq");
    auto cache=field_object(owner,"m_cachedCostHpInfo");if(!cache)return;
    void* remove=nullptr;
    for(auto it=nmb_pending_acks.begin();it!=nmb_pending_acks.end()&&*it<played;){
        auto seq=*it;
        if(!remove)remove=nmb_remove_method(cache);
        void* args[]={&seq};uint8_t removed=0;
        if(!read(unbox(invoke(remove,cache,args)),removed))throw std::runtime_error("private native boss prediction removal result missing");
        if(removed)++nmb_fast_acks;
        it=nmb_pending_acks.erase(it);
    }
}
bool native_multiplayer_boss_up(const cnjson::Value& dto){
    nb_main_thread();if(!nmb_ready.load()||!nmb_journal.epoch)throw std::runtime_error("private native boss upstream before clock");
    const auto seq=nb_number(dto,"seq");if(seq<0||seq>nmb_received.load())throw std::runtime_error("private native boss upstream frame not received");
    if(!cnmulti_boss::reports(nmb_journal)){++nmb_suppressed;return false;}
    const auto& actions=nb_at(dto,"actions");
    if(actions.kind!=cnjson::Value::Array)throw std::runtime_error("private native boss upstream actions type differs");
    // Empty fixed steps carry no prediction/damage/outcome to acknowledge.
    // Avoid ten HTTP mutations and durable receipt writes per idle second.
    if(actions.array.empty()){++nmb_suppressed;return false;}
    return true;
}
void native_multiplayer_boss_tick(){
    nb_main_thread();if(nmb_view.kind==cnjson::Value::Null||!nmb_ready.load())return;
    if(!nmb_window_dirty){
        // Played prediction ACKs still advance locally, but an already queued
        // window must not copy/reparse the entire committed history each UI tick.
        // Native teardown can precede the next HTTP snapshot. Its old ACKs
        // belong only to the same live private BattlePhase and step owner.
        auto service=native_multiplayer_entry_service();auto phase=field_object(service,"m_curPhase");
        if(!phase||api.il2cpp_object_get_class(phase)!=cls("Torappu.UI.AutoChess.Server.AutoChessServiceBattlePhase")||
            !native_multiplayer_dispatch_scene()||!native_multiplayer_dispatch_bridge())return;
        auto manager=nb_manager();uint8_t in_step=0;
        if(!manager||!read(unbox(invoke(bound_methods.at(0x6013684),manager)),in_step)||!in_step||!field_object(phase,"m_serverStepModeHelper"))return;
        if(nmb_phase!=phase){nmb_window_dirty=true;return;}
        if(nmb_journal.epoch)nmb_apply_acknowledgements(manager);
        return;
    }
    ++nmb_windows;
    auto plan=cnmulti_boss::accept(nmb_journal,nmb_view);
    if(!plan.active){if(!plan.next.epoch)native_multiplayer_boss_reset();return;}
    if(plan.next.epoch!=nmb_journal.epoch){nmb_received.store(-1);nmb_epoch.store(0);nmb_phase=nullptr;nmb_reset_acknowledgements();}
    nmb_receive_acknowledgements(plan);
    // Keep policy/authority even while the native battlefield is loading.
    nmb_journal=std::move(plan.next);
    auto service=native_multiplayer_entry_service();auto phase=field_object(service,"m_curPhase");
    if(!phase||api.il2cpp_object_get_class(phase)!=cls("Torappu.UI.AutoChess.Server.AutoChessServiceBattlePhase")||
        !native_multiplayer_dispatch_scene()||!native_multiplayer_dispatch_bridge())return;
    auto manager=nb_manager();uint8_t in_step=0;
    if(!manager||!read(unbox(invoke(bound_methods.at(0x6013684),manager)),in_step)||!in_step||!field_object(phase,"m_serverStepModeHelper"))return;
    if(nmb_phase&&nmb_phase!=phase){
        // A new native scene must restart at seq0. The next GET uses -1 and
        // requests its bounded first replay window instead of skipping history.
        nmb_journal.committed.clear();nmb_received.store(-1);nmb_phase=phase;nmb_view={};nmb_ack_owner=nullptr;return;
    }
    nmb_phase=phase;nmb_epoch.store(nmb_journal.epoch);
    nmb_apply_acknowledgements(manager);
    for(const auto& step:plan.steps){
        TemporaryRoots roots;auto object=roots.retain(invoke(bound_methods.at(0x602bd39),nullptr));
        if(!object||api.il2cpp_object_get_class(object)!=cls("Torappu.AutoChessBattleStepData"))throw std::runtime_error("private native boss pool allocation mismatch");
        for(const auto* name:{"seq","duration","actions"}){
            auto field=profile_field(api.il2cpp_object_get_class(object),name);auto type=api.il2cpp_field_get_type(field);
            auto child=build_value(type,nb_at(step,name),roots);
            if(api.il2cpp_class_is_valuetype(api.il2cpp_class_from_type(type)))set_field(object,field,child.argument);else set_object_field(object,field,child.object);
        }
        // Original RevStep retains the object in ServerStepModeHelper's queue;
        // original ReleaseStepData recycles it only after UpdateSeq/ApplyAction.
        void* args[]={object};invoke(bound_methods.at(0x602bd69),phase,args);
        cnmulti_boss::commit(nmb_journal,step);nmb_received.store(nb_number(step,"seq"));++nmb_frames;
    }
    nmb_window_dirty=false;
}
void native_multiplayer_boss_apply(const cnjson::Value& view){
    // The native frame adapter needs no board, audit history or duplicate DTOs.
    // Keep the same validated identity/window across asynchronous field loading.
    nmb_view={};nmb_view.kind=cnjson::Value::Object;
    for(const auto* key:{"roomId","phase","seat","round","bossNative","selfReconstruction"}){
        auto item=view.object.find(key);if(item!=view.object.end())nmb_view.object.emplace(key,item->second);
    }
    nmb_window_dirty=true;native_multiplayer_boss_tick();
}
void native_multiplayer_boss_setup(){
    for(const auto& spec:NMB_METHODS)bind_method(spec);
    for(const auto& item:std::array<std::pair<const char*,const char*>,9>{{{"Torappu.AutoChessBattleStepData","seq"},{"Torappu.AutoChessBattleStepData","duration"},{"Torappu.AutoChessBattleStepData","actions"},{"Torappu.UI.AutoChess.Server.AutoChessService","m_curPhase"},{"Torappu.UI.AutoChess.Server.AutoChessServiceBattlePhase","m_serverStepModeHelper"},{"Torappu.Battle.AutoChess.AutoChessGameManager","stepModeManager"},{"Torappu.Battle.AutoChess.AutoChessStepModeManager","m_playerGroup"},{"Torappu.Battle.AutoChess.AutoChessStepModeManager","m_seq"},{"Torappu.Battle.AutoChess.AutoChessStepModeManager","m_cachedCostHpInfo"}}})profile_field(cls(item.first),item.second);
    nmb_ready.store(true);
}
std::string native_multiplayer_boss_status_json(){return "{\"bindingsReady\":"+std::string(nmb_ready.load()?"true":"false")+",\"epoch\":"+std::to_string(nmb_epoch.load())+",\"receivedStep\":"+std::to_string(nmb_received.load())+",\"framesQueued\":"+std::to_string(nmb_frames.load())+",\"confirmedPredictionAcks\":"+std::to_string(nmb_fast_acks.load())+",\"nonAuthorityReportsSuppressed\":"+std::to_string(nmb_suppressed.load())+",\"nativeAccepted\":false}";}
