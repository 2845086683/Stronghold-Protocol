// Included by native_multiplayer_help.inc after native identity/terminal helpers.
// Readiness records the complete original per-character carry execution.
struct NMHCarryTarget {bool token=false,retreat=false;int32_t hp=0,tech=0,position=0,direction=0;};
struct NMHCarryPlan {std::string stamp;std::map<NMHKey,NMHCarryTarget> targets;int32_t generation=0;};
std::map<NMHKey,NMHCarryTarget> nmh_carry_targets;
std::set<NMHKey> nmh_carry_applied;
std::string nmh_carry_stamp;
uint64_t nmh_carry_serial=0;
int32_t nmh_carry_generation=0;
uint32_t nmh_carry_controller_root=0;
uint32_t nmh_carry_task_root=0;
cnjson::Value nmh_carry_first_failure;
cnjson::Value nmh_carry_before_apply;
std::string native_multiplayer_transport_room();

void nmh_carry_clear_locked(){
    nmh_carry_targets.clear();nmh_carry_applied.clear();nmh_carry_stamp.clear();nmh_carry_generation=0;nmh_carry_before_apply={};++nmh_carry_serial;
    if(nmh_carry_controller_root){hold_free(nmh_carry_controller_root);nmh_carry_controller_root=0;}
    if(nmh_carry_task_root){hold_free(nmh_carry_task_root);nmh_carry_task_root=0;}
}
NMHCarryPlan nmh_make_carry_plan(const cnjson::Value& snapshot,const std::string& phase,const std::string& room,const std::string& run,int32_t sequence,int32_t round,int32_t seat){
    NMHCarryPlan result;if(phase!="help")return result;
    const auto& plan=nb_at(snapshot,"jointDefense");const auto& segments=nb_at(plan,"segments");
    result.generation=nb_number(snapshot,"generation");
    if(run.empty()||result.generation<1||nb_number(plan,"generation")!=result.generation||nb_number(plan,"round")!=round||nb_string(plan,"status")!="battle")throw std::runtime_error("multiplayer carry plan identity invalid");
    for(const auto& segment:segments.array){const auto owner=nb_number(segment,"seat");
        auto unresolved=segment.object.find("unresolvedCarry");
        if(unresolved!=segment.object.end()&&(unresolved->second.kind!=cnjson::Value::Array||!unresolved->second.array.empty()))throw std::runtime_error("multiplayer carry has unresolved units");
        for(const auto& unit:nb_at(segment,"deployments").array){const auto kind=nb_number(unit,"chessType");if(kind!=0&&kind!=3)continue;
            const auto& carry=nb_at(unit,"carry");if(carry.kind!=cnjson::Value::Object)throw std::runtime_error("multiplayer carry status unresolved");
            NMHCarryTarget target;target.token=kind==3;target.tech=nb_number(carry,"tech");
            target.position=nmh_encode(nb_at(unit,"position"));target.direction=nb_number(unit,"direction");
            if(target.position<=0||target.direction<0||target.direction>3)throw std::runtime_error("multiplayer carry deployment position/direction invalid");
            if(target.tech<0||target.tech>100)throw std::runtime_error("multiplayer carry SP outside contract");
            if(!target.token){target.hp=nb_number(carry,"hp");target.retreat=nb_true(carry,"forceRetreat");
                if(target.hp<0||target.hp>100||target.retreat!=(target.hp==0))throw std::runtime_error("multiplayer operator carry HP/retreat inconsistent");}
            const NMHKey key{owner,nb_number(unit,"instId"),target.token?nmh_encode(nb_at(unit,"position")):0};
            if(key[1]<0||!result.targets.emplace(key,target).second)throw std::runtime_error("multiplayer carry unit identity ambiguous");
        }
    }
    auto identity=nb_object();identity.object["room"]=json_text(room);identity.object["run"]=json_text(run);identity.object["match"]=nb_integer(sequence);
    identity.object["round"]=nb_integer(round);identity.object["seat"]=nb_integer(seat);identity.object["generation"]=nb_integer(result.generation);
    identity.object["segments"]=segments;
    auto field=snapshot.object.find("nativeField");
    if(field!=snapshot.object.end()){
        auto hash=nb_string(field->second,"planHash");const auto& attempts=nb_at(field->second,"loadAttemptBySeat");
        auto attempt=attempts.object.find(std::to_string(seat));
        if(hash.empty()||attempts.kind!=cnjson::Value::Object||attempt==attempts.object.end()||attempt->second.kind!=cnjson::Value::Integer||attempt->second.integer<0||attempt->second.integer>INT32_MAX)throw std::runtime_error("multiplayer carry field load identity unavailable");
        identity.object["fieldPlanHash"]=json_text(hash);identity.object["fieldLoadAttempt"]=attempt->second;
    }
    result.stamp=cnjson::dump(identity);return result;
}
void nmh_accept_carry_locked(NMHCarryPlan plan){
    if(plan.stamp!=nmh_carry_stamp){nmh_carry_clear_locked();nmh_carry_failed=false;if(!nmh_failed.load())nmh_error.clear();}
    nmh_carry_stamp=std::move(plan.stamp);nmh_carry_generation=plan.generation;nmh_carry_targets=std::move(plan.targets);
}
// Capture once per failing native attempt. Later units see the latched failure;
// their refusal must not overwrite the actual first fault with a scope error.
void nmh_carry_record_failure(const std::string& error,const cnjson::Value& context){
    std::lock_guard<std::mutex> lock(nmh_mutex);
    if(!nmh_carry_failed.load()){
        nmh_error=error;nmh_carry_first_failure=context;
        nmh_carry_first_failure.object["error"]=json_text(error);
        nmh_carry_first_failure.object["roomId"]=json_text(nmh_room);
        nmh_carry_first_failure.object["round"]=nb_integer(nmh_round);
        nmh_carry_first_failure.object["seat"]=nb_integer(nmh_seat);
        nmh_carry_first_failure.object["generation"]=nb_integer(nmh_carry_generation);
        nmh_carry_first_failure.object["expectedUnits"]=nb_integer(nmh_carry_targets.size());
        nmh_carry_first_failure.object["appliedUnits"]=nb_integer(nmh_carry_applied.size());
        nmh_carry_first_failure.object["beforeApply"]=nmh_carry_before_apply;
    }
    nmh_carry_failed=true;++nmh_failures;
}
void* nmh_carry_controller(TemporaryRoots& roots){
    auto controller=roots.retain(invoke(method(cls("Torappu.Battle.BattleController"),"get_instance",0),nullptr));
    if(!controller)throw std::runtime_error("multiplayer carry controller unavailable");return controller;
}
std::string native_multiplayer_help_carry_plan_stamp(){
    nb_main_thread();std::lock_guard<std::mutex> lock(nmh_mutex);
    return nmh_ready.load()&&nmh_phase=="help"?nmh_carry_stamp:std::string{};
}
void native_multiplayer_help_carry_begin(void* controller,const std::string& stamp){
    nb_main_thread();if(!nmh_scope())throw std::runtime_error("multiplayer carry begin outside private native bridge");
    TemporaryRoots roots;auto current=nmh_carry_controller(roots);
    if(!controller||controller!=current)throw std::runtime_error("multiplayer carry begin controller is stale");
    std::lock_guard<std::mutex> lock(nmh_mutex);
    if(!nmh_ready.load()||nmh_failed.load()||nmh_carry_failed.load()||nmh_phase!="help"||stamp.empty()||stamp!=nmh_carry_stamp||
       native_multiplayer_transport_room()!=nmh_room||nmh_int(nb_centre(),"<selfPlayerIndex>k__BackingField")!=nmh_seat||
       nmh_int(nb_centre(),"<currentRound>k__BackingField")!=nmh_round)throw std::runtime_error("multiplayer carry begin plan is stale");
    if(nmh_carry_controller_root&&hold_get(nmh_carry_controller_root)==controller)return;
    auto root=hold_new(controller,false);if(!root)throw std::runtime_error("multiplayer carry controller strong root unavailable");
    if(nmh_carry_controller_root)hold_free(nmh_carry_controller_root);
    if(nmh_carry_task_root){hold_free(nmh_carry_task_root);nmh_carry_task_root=0;}
    nmh_carry_controller_root=root;nmh_carry_applied.clear();++nmh_carry_serial;
}
void native_multiplayer_help_carry_task_begin(void* task,void* controller,const std::string& stamp){
    nb_main_thread();if(!nmh_scope()||!task)throw std::runtime_error("multiplayer carry task outside private native bridge");
    TemporaryRoots roots;if(nmh_carry_controller(roots)!=controller)throw std::runtime_error("multiplayer carry task controller is stale");
    std::lock_guard<std::mutex> lock(nmh_mutex);
    if(nmh_failed.load()||nmh_carry_failed.load()||nmh_phase!="help"||stamp.empty()||stamp!=nmh_carry_stamp||!nmh_carry_controller_root||hold_get(nmh_carry_controller_root)!=controller)throw std::runtime_error("multiplayer carry task plan is stale");
    if(nmh_carry_task_root&&hold_get(nmh_carry_task_root)==task)return;
    auto root=hold_new(task,false);if(!root)throw std::runtime_error("multiplayer carry task strong root unavailable");
    if(nmh_carry_task_root)hold_free(nmh_carry_task_root);
    nmh_carry_task_root=root;nmh_carry_applied.clear();++nmh_carry_serial;
}
float nmh_ratio(void* character,uint32_t token,TemporaryRoots& roots){
    auto ratio=roots.retain(invoke(nmh_method(token),character));auto value=nmh_result<float>(invoke(nmh_method(0x6000323),unbox(ratio)));
    if(!std::isfinite(value))throw std::runtime_error("multiplayer carry native ratio nonfinite");return value;
}
bool nmh_matches_ratio(float actual,int32_t percent){return std::fabs(actual-float(percent)/100.0f)<=0.0001f;}
float nmh_float(void* object,const char* name){
    if(!object)throw std::runtime_error("multiplayer carry native status absent");auto field=profile_field(api.il2cpp_object_get_class(object),name);
    if(!matches_type(api.il2cpp_field_get_type(field),"System.Single"))throw std::runtime_error("multiplayer carry native float declaration changed");
    float value=0;api.il2cpp_field_get_value(object,field,&value);if(!std::isfinite(value))throw std::runtime_error("multiplayer carry native status nonfinite");return value;
}
void nmh_check_operator_status(void* help,int32_t inst,const NMHCarryTarget& target,TemporaryRoots& roots){
    auto statuses=roots.retain(field_object(help,"battleChessStatus"));if(!statuses)throw std::runtime_error("multiplayer carry native Help statuses absent");
    void* row=nullptr;void* args[]={&inst,&row};bool found=nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(statuses),"TryGetValue",2),statuses,args));
    if(target.retreat){if(found)throw std::runtime_error("retreated operator must be absent from native Help status dictionary");return;}
    row=roots.retain(row);
    if(!found||!row||nmh_int(row,"instId")!=inst||!nmh_matches_ratio(nmh_float(row,"hpRatio"),target.hp)||!nmh_matches_ratio(nmh_float(row,"spRatio"),target.tech))throw std::runtime_error("multiplayer carry native Help status differs from authenticated plan");
}

struct NMHApplyProof {void* character=nullptr;unsigned hpCalls=0,spCalls=0,finishCalls=0;bool invalid=false,hpAccepted=false,spAccepted=false;};
struct NMHApplyReentry {void* self=nullptr;void* character=nullptr;int32_t inst=0;void* help=nullptr;};
thread_local NMHApplyProof* nmh_apply_proof=nullptr;
thread_local NMHApplyReentry* nmh_apply_reentry=nullptr;
using NMHModifier=uint8_t(*)(void*,void*,uint8_t,const void*);
using NMHForceFinish=void(*)(void*,const void*);
NMHModifier nmh_original_modifier=nullptr;NMHForceFinish nmh_original_force_finish=nullptr;
#ifndef NMH_CARRY_CALLER_ADDRESS
#define NMH_CARRY_CALLER_ADDRESS() __builtin_return_address(0)
#endif
uintptr_t nmh_caller_rva(void* address)noexcept{auto value=reinterpret_cast<uintptr_t>(address),base=reinterpret_cast<uintptr_t>(runtime);return value>=base?value-base:0;}
uint8_t nmh_carry_modifier(void* self,void* modifier,uint8_t force,const void* info){
    const auto caller=nmh_caller_rva(NMH_CARRY_CALLER_ADDRESS());auto accepted=nmh_original_modifier(self,modifier,force,info);
    auto proof=nmh_apply_proof;if(proof&&(caller==0xbfc2ee||caller==0xbfc3cd)){
        if(self!=proof->character||force)proof->invalid=true;
        if(caller==0xbfc2ee){++proof->hpCalls;proof->hpAccepted=accepted!=0;}else{++proof->spCalls;proof->spAccepted=accepted!=0;}}
    return accepted;
}
void nmh_carry_force_finish(void* self,const void* info){
    const auto caller=nmh_caller_rva(NMH_CARRY_CALLER_ADDRESS());nmh_original_force_finish(self,info);
    auto proof=nmh_apply_proof;if(proof&&caller==0xbfc3de){if(self!=proof->character)proof->invalid=true;++proof->finishCalls;}
}
void nmh_apply_original_protected(void* self,void* character,int32_t inst,void* help,NMHApplyProof& proof){
    NMHApplyReentry reentry{self,character,inst,help};auto previous=nmh_apply_reentry;auto previousProof=nmh_apply_proof;
    nmh_apply_reentry=&reentry;nmh_apply_proof=&proof;void* args[]={character,&inst,help};void* error=nullptr;
    try{api.il2cpp_runtime_invoke(nmh_method(0x6013d30),self,args,&error);}
    catch(...){nmh_apply_reentry=previous;nmh_apply_proof=previousProof;throw;}
    nmh_apply_reentry=previous;nmh_apply_proof=previousProof;
    if(error)throw std::runtime_error("multiplayer original carry application raised managed error");
}
bool native_multiplayer_help_carry_ready(){
    if(!nmh_scope())return false;
    try{
        TemporaryRoots roots;std::string room;int32_t seat,round;
        {std::lock_guard<std::mutex> lock(nmh_mutex);if(!nmh_ready.load()||nmh_failed.load()||nmh_carry_failed.load()||nmh_phase!="help"||nmh_carry_stamp.empty())return false;room=nmh_room;seat=nmh_seat;round=nmh_round;}
        if(native_multiplayer_transport_room()!=room||nmh_int(nb_centre(),"<selfPlayerIndex>k__BackingField")!=seat||nmh_int(nb_centre(),"<currentRound>k__BackingField")!=round)return false;
        auto controller=nmh_carry_controller(roots);
        std::lock_guard<std::mutex> lock(nmh_mutex);
        return nmh_ready.load()&&!nmh_failed.load()&&!nmh_carry_failed.load()&&nmh_phase=="help"&&!nmh_carry_stamp.empty()&&
            nmh_carry_controller_root&&nmh_carry_task_root&&hold_get(nmh_carry_controller_root)==controller&&nmh_carry_applied.size()==nmh_carry_targets.size();
    }catch(...){return false;}
}
bool native_multiplayer_help_carry_complete(){return native_multiplayer_help_carry_ready();}
