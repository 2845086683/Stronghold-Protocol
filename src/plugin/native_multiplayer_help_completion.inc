// Shared Help remains a native combat simulation. Only its selected defender
// can finish it; character deaths are not an outcome while enemies remain.
#pragma once
#include "native_multiplayer_help_outcome_policy.h"
static const CN_Method NMHO_METHODS[]={
{"Torappu.Battle.BattleController","FinishGame",0x600fc65,0x624450ULL,3,false,"System.Void",{"Torappu.Battle.BattleController+GameResult","System.Boolean","System.Action"}},
{"Torappu.UI.AutoChess.Battle.AutoChessBattleUICountdownViewModel","GetCurrentTimeInfo",0x602bfe8,0x2819f70ULL,1,false,"Torappu.UI.AutoChess.Battle.AutoChessBattleUICountdownViewModel+TimeInfo",{"System.DateTime"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer","_ReqHelpBattleFinish",0x6013626,0xb34c90ULL,2,false,"System.Void",{"System.Boolean","System.Int32"}},
};
using NMHOFinish=void(*)(void*,int32_t,bool,void*,const void*);
// Win64 IL2CPP returns this 16-byte value through caller-owned RCX, with this
// in RDX and DateTime's eight-byte value in R8 (602bfe8/2819f70).
struct NMHOTimeInfo{int64_t endTs;int32_t totalTime,hintTime;};
static_assert(sizeof(NMHOTimeInfo)==16,"native Help TimeInfo ABI");
using NMHOTime=NMHOTimeInfo*(*)(NMHOTimeInfo*,void*,uint64_t,const void*);
NMHOFinish nmho_old_finish=nullptr;NMHOTime nmho_old_time=nullptr;
using NMHORequest=void(*)(void*,bool,int32_t,const void*);
NMHORequest nmho_old_request=nullptr;
std::atomic<unsigned> nmho_deferred{0},nmho_countdowns{0};
cnjson::Value nmho_countdown_view;
void native_multiplayer_help_completion_prepare(const cnjson::Value& view){
    // Runs after stale-envelope validation and before native Fill/dirty events.
    // Countdown reads are synchronous inside those events; nmd_last_view is
    // still the preceding phase until the receive completes.
    nmho_countdown_view={};
    if(view.kind!=cnjson::Value::Object)return;
    nmho_countdown_view.kind=cnjson::Value::Object;
    for(const auto* key:{"roomId","phase","round"})nmho_countdown_view.object[key]=nb_at(view,key);
    const auto reconstruction=view.object.find("selfReconstruction");
    if(reconstruction!=view.object.end()&&reconstruction->second.kind==cnjson::Value::Object){nmho_countdown_view={};return;}
    if(nb_string(view,"phase")!="help")return;
    const auto& source=nb_at(view,"jointDefense");auto& plan=nmho_countdown_view.object["jointDefense"];
    plan.kind=cnjson::Value::Object;
    for(const auto* key:{"startedAtTs","deadlineTs","fieldLoad"}){
        auto found=source.object.find(key);if(found!=source.object.end())plan.object[key]=found->second;
    }
}
bool nmho_active(){
    if(closing.load()||!native_multiplayer_entry_active()||nmd_receive_depth||native_battle_data_projecting()||!nmh_scope())return false;
    if(nmd_last_view.kind!=cnjson::Value::Object||nb_string(nmd_last_view,"phase")!="help"||nb_string(nmd_last_view,"roomId")!=native_multiplayer_transport_room())return false;
    const auto reconstruction=nmd_last_view.object.find("selfReconstruction");
    if(reconstruction!=nmd_last_view.object.end()&&reconstruction->second.kind==cnjson::Value::Object)return false;
    return nb_native_int(nb_centre(),"<currentRound>k__BackingField")==nb_number(nmd_last_view,"round");
}
bool nmho_terminal_allowed(){
    return cnhelp_outcome::running(nmd_last_view)&&(cnhelp_outcome::journal_complete(nmd_last_view)||
        cnhelp_outcome::expired(nmd_last_view,native_multiplayer_transport_time()));
}
bool native_multiplayer_help_defer_manager_finish(void* manager,int32_t result){
    if(!nmho_active()||manager!=nb_manager()||(result!=1&&result!=2)||nmho_terminal_allowed())return false;
    // OnFinishGame sets battleFinished before calling BattleController. Block
    // at this earlier, already-hooked edge too, so a temporary all-downed
    // formation cannot stop waves or poison the redeployment state machine.
    ++nmho_deferred;return true;
}
void nmho_request(void* bridge,bool rejoin,int32_t round,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{
        if(nmho_active()&&bridge==native_multiplayer_dispatch_bridge()&&round==nb_number(nmd_last_view,"round")&&!nmho_terminal_allowed()){
            ++nmho_deferred;return;
        }
    }catch(const std::exception& ex){nmd_failed(ex);return;}
    lease.release();nmho_old_request(bridge,rejoin,round,info);
}
void nmho_finish(void* controller,int32_t result,bool silent,void* callback,const void* info){
    RuntimeLease lease(runtime_gate);
    if(lease){try{
        if(nmho_active()){
            nb_main_thread();TemporaryRoots roots;
            auto current=roots.retain(invoke(method(cls("Torappu.Battle.BattleController"),"get_instance",0),nullptr));
            if(controller==current&&(result==1||result==2)&&!nmho_terminal_allowed()){
                // Keep the original native engine, waves and resurrection
                // timers alive. A temporary all-downed formation (e.g. an
                // Abyssal team) is not failure while an enemy is unresolved.
                ++nmho_deferred;return;
            }
        }
    }catch(const std::exception& ex){nmd_failed(ex);return;}}
    lease.release();nmho_old_finish(controller,result,silent,callback,info);
}
NMHOTimeInfo* nmho_time(NMHOTimeInfo* output,void* model,uint64_t currentTime,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return output;
    auto result=nmho_old_time(output,model,currentTime,info);
    try{
        const auto& view=nmho_countdown_view;
        const bool own=native_multiplayer_entry_active()&&nmh_scope()&&view.kind==cnjson::Value::Object&&
            nb_string(view,"phase")=="help"&&nb_string(view,"roomId")==native_multiplayer_transport_room();
        if(!own||!model||!act(field_object(model,"m_actId"))||nb_native_int(model,"m_round")!=nb_number(view,"round"))return result;
        const auto& plan=nb_at(view,"jointDefense");
        const auto& field=nb_at(plan,"fieldLoad");
        // Native normal/Help TimeInfo uses separate timestamp differences.
        // A newly armed Help budget cannot reuse the preceding self field's
        // normalEnd/start. Supply the same original native value type with a
        // precise Help denominator, including its first integer server second.
        if(nb_string(field,"status")!="running")return result;
        const auto start=nb_number(plan,"startedAtTs"),end=nb_number(plan,"deadlineTs");
        if(end<=start||end-start>3600)throw std::runtime_error("Help countdown duration invalid");
        if(!output||result!=output)throw std::runtime_error("Help countdown native value return buffer changed");
        auto data=field_object(model,"m_autoChessData"),constants=field_object(data,"constData");
        const auto hint=nb_native_int(constants,"hintTimeDotPhase");
        if(hint<0||hint>3600)throw std::runtime_error("Help countdown native warning time invalid");
        *output=NMHOTimeInfo{end,int32_t(end-start),hint};
        ++nmho_countdowns;
    }catch(const std::exception& ex){nmd_failed(ex);}
    return result;
}
void native_multiplayer_help_completion_setup(){
    hook(NMHO_METHODS[0],nmho_finish,nmho_old_finish);
    hook(NMHO_METHODS[1],nmho_time,nmho_old_time);
    hook(NMHO_METHODS[2],nmho_request,nmho_old_request);
    auto type=cls("Torappu.UI.AutoChess.Battle.AutoChessBattleUICountdownViewModel+TimeInfo");uint32_t alignment=0;
    if(!api.il2cpp_class_is_valuetype(type)||api.il2cpp_class_value_size(type,&alignment)!=sizeof(NMHOTimeInfo))throw std::runtime_error("Help countdown native value ABI mismatch");
    const auto header=api.il2cpp_object_header_size();
    for(const auto& field:std::array<std::pair<const char*,size_t>,3>{{{"endTs",offsetof(NMHOTimeInfo,endTs)},{"totalTime",offsetof(NMHOTimeInfo,totalTime)},{"hintTime",offsetof(NMHOTimeInfo,hintTime)}}})
        if(api.il2cpp_field_get_offset(profile_field(type,field.first))!=header+field.second)throw std::runtime_error("Help countdown native return field layout changed");
    const struct{const char* owner;const char* field;const char* type;} fields[]={
        {"Torappu.UI.AutoChess.Battle.AutoChessBattleUICountdownViewModel","m_autoChessData","Torappu.AutoChessData"},
        {"Torappu.AutoChessData","constData","Torappu.AutoChessData+AutoChessConstData"},
        {"Torappu.AutoChessData+AutoChessConstData","hintTimeDotPhase","System.Int32"},
    };
    for(const auto& field:fields)if(!matches_type(api.il2cpp_field_get_type(profile_field(cls(field.owner),field.field)),field.type))throw std::runtime_error("Help countdown native value contract mismatch");
}
