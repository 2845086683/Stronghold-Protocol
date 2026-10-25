// Private eliminated-player observation over the declared native DeadAutoObDn.
// Include after native_multiplayer_dispatch.inc. No scene loader or combat replay.
#pragma once
static const CN_Method NMDO_METHODS[]={
{"Torappu.Battle.DataCenter.AutoChessDataCenter","get_hasDeadObIndex",0x601343e,0xb13ce0ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","get_realPlayerIndex",0x601343d,0xb147c0ULL,0,false,"System.Int32",{}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData+AutoChessBattleSceneStateData","Fill",0x602b603,0x276bee0ULL,2,false,"System.Void",{"Torappu.AutoChessGameStateType","System.Int32"}},
};
using NMDOHas=bool(*)(void*,const void*);
using NMDOReal=int32_t(*)(void*,const void*);
NMDOHas nmdo_old_has=nullptr;
NMDOReal nmdo_old_real=nullptr;
struct NMDOLease{
    std::string room,run,key;
    int64_t match=0,round=0,seat=-1,target=-1;
    uint32_t mode_root=0;
};
NMDOLease nmdo_lease;
unsigned nmdo_fills=0,nmdo_zero_reads=0;
bool nmdo_flag(const cnjson::Value& value,const char* name){
    auto found=value.object.find(name);
    return found!=value.object.end()&&found->second.kind==cnjson::Value::Boolean&&found->second.boolean;
}

void native_multiplayer_dead_observation_reset(){
    if(nmdo_lease.mode_root)hold_free(nmdo_lease.mode_root);
    nmdo_lease={};
}
bool nmdo_departed(const cnjson::Value& view){
    if(nmdo_flag(view,"left"))return true;
    auto found=view.object.find("selfScenePresence");
    if(found==view.object.end()||found->second.kind!=cnjson::Value::Object)return false;
    const auto& presence=found->second;
    if(nb_number(presence,"seat")!=nb_number(view,"seat")||
       nb_number(presence,"matchSequence")!=nb_number(view,"matchSequence"))
        throw std::runtime_error("dead observation presence recipient mismatch");
    return nb_true(presence,"quit")||nb_true(presence,"sceneLeft")||nb_true(presence,"returned");
}
bool nmdo_actor(const cnjson::Value& view){
    if(!nmdo_flag(view,"observerOnly")||nmdo_departed(view))return false;
    const auto& state=nb_at(view,"selfState");
    return state.kind==cnjson::Value::Object&&nb_number(nb_at(state,"player"),"hp")==0;
}
NMDOLease nmdo_binding(const cnjson::Value& view,int64_t target){
    NMDOLease value;
    value.room=nb_string(view,"roomId");value.match=nb_number(view,"matchSequence");
    value.round=nb_number(view,"round");value.seat=nb_number(view,"seat");value.target=target;
    value.run=nb_string(nb_at(view,"selfState"),"runId");
    if(value.room.empty()||value.run.empty()||value.match<1||value.round<1||value.seat<0||value.seat>3||
       target<0||target>3||target==value.seat)throw std::runtime_error("dead observation binding invalid");
    bool recipient=false,target_live=false;
    for(const auto& row:nb_at(view,"roster").array){
        if(nb_number(row,"seat")==value.seat)recipient=true;
        if(nb_number(row,"seat")==target&&!nb_true(row,"left")&&nb_number(row,"hp")>0)target_live=true;
    }
    if(!recipient||!target_live)throw std::runtime_error("dead observation target is not a live room member");
    return value;
}
cnjson::Value native_multiplayer_dead_observation_packet(const cnjson::Value& view){
    if(!nmdo_actor(view))throw std::runtime_error("dead observation response has no eliminated recipient");
    auto envelope=view.object.find("nativeDeadAutoObservation");
    if(envelope!=view.object.end()&&envelope->second.kind==cnjson::Value::Object){
        const auto& value=envelope->second;auto binding=nmdo_binding(view,nb_number(value,"targetSeat"));
        if(nb_number(value,"version")!=1||nb_string(value,"roomId")!=binding.room||
           nb_number(value,"matchSequence")!=binding.match||nb_number(value,"round")!=binding.round||
           nb_number(value,"seat")!=binding.seat||nb_string(value,"runId")!=binding.run||
           nb_string(value,"targetRunId").empty())throw std::runtime_error("dead observation envelope belongs to another scene");
        auto prepared=view.object.find("preparationView");
        if(prepared!=view.object.end()&&prepared->second.kind==cnjson::Value::Object){
            const auto& plan=nb_at(prepared->second,"plan");
            if(nb_number(plan,"targetSeat")!=binding.target||
               nb_string(plan,"targetRunId")!=nb_string(value,"targetRunId"))
                throw std::runtime_error("dead observation envelope target run mismatch");
        }
        const auto& packet=nb_at(value,"protocol");const auto& payload=nb_at(packet,"payload");
        if(nb_string(packet,"type")!="Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessBattleSceneDeadAutoObDn"||
           nb_number(packet,"protocolId")!=360||nb_number(payload,"obIndex")!=binding.target||
           nb_number(nb_at(payload,"preparation"),"uidIndex")!=binding.target)
            throw std::runtime_error("dead observation original DTO target mismatch");
        return packet;
    }
    // The native request's callback may complete as its physical preparation
    // is being acknowledged. Its pending target projection is already bound
    // to the same actor/run/round/attempt; use that declared original DTO.
    const auto& pending=nb_at(view,"preparationView");const auto& plan=nb_at(pending,"plan");
    auto binding=nmdo_binding(view,nb_number(plan,"targetSeat"));
    if(!nb_true(plan,"observerOnly")||nb_string(plan,"roomId")!=binding.room||
       nb_number(plan,"matchSequence")!=binding.match||nb_number(plan,"round")!=binding.round||
       nb_number(plan,"seat")!=binding.seat||nb_string(plan,"runId")!=binding.run||
       nb_string(plan,"targetRunId").empty())throw std::runtime_error("dead observation pending target is stale");
    const auto& projection=nb_at(pending,"targetProjection");
    const auto& detail=nb_at(nb_at(nb_at(projection,"allStateSync"),"payload"),"allStateSyncData").object.at("sceneDetail");
    auto packet=nb_object();packet.object["type"]=json_text("Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessBattleSceneDeadAutoObDn");
    packet.object["protocolId"]=nb_integer(360);packet.object["payload"]=nb_object();
    auto& payload=packet.object["payload"];payload.object["obIndex"]=nb_integer(binding.target);
    payload.object["state"]=nb_at(detail,"state");payload.object["preparation"]=nb_at(detail,"preparationStatus");
    if(nb_number(nb_at(payload,"preparation"),"uidIndex")!=binding.target)
        throw std::runtime_error("dead observation pending preparation target mismatch");
    return packet;
}
bool native_multiplayer_dead_observation_fill(const cnjson::Value& view,void* scene){
    nb_main_thread();
    if(!nmdo_actor(view)){native_multiplayer_dead_observation_reset();return false;}
    if(nmdo_lease.mode_root&&(nmdo_lease.room!=nb_string(view,"roomId")||
       nmdo_lease.match!=nb_number(view,"matchSequence")||nmdo_lease.seat!=nb_number(view,"seat")||
       nmdo_lease.run!=nb_string(nb_at(view,"selfState"),"runId")))
        native_multiplayer_dead_observation_reset();
    const auto& projection=nb_at(view,"dtoProjection");
    const auto phase=nb_string(view,"phase");
    if(phase!="preparation"&&phase!="special"&&phase!="help"&&phase!="boss"&&!(phase=="battle"&&cndead_battle::valid(view)))return false;
    const auto target=nb_number(nb_at(projection,"view"),"observedSeat");
    if(target==nb_number(view,"seat"))return false; // Its desired view has not been committed yet.
    auto next=nmdo_binding(view,target);
    const auto& detail=nb_at(nb_at(nb_at(projection,"allStateSync"),"payload"),"allStateSyncData").object.at("sceneDetail");
    const auto state=nb_number(detail,"state");
    if((phase=="preparation"&&state!=3)||(phase=="special"&&state!=2)||
       (phase=="help"&&state!=5)||(phase=="boss"&&state!=6)||
       (phase=="battle"&&state!=(cndead_battle::valid(view,true)?4:3)))
        throw std::runtime_error("dead observation original scene phase mismatch");
    next.key=next.room+":"+std::to_string(next.match)+":"+std::to_string(next.round)+":"+
        next.run+":"+std::to_string(next.seat)+":"+std::to_string(target)+":"+std::to_string(state);
    if(nmdo_lease.key==next.key&&hold_get(nmdo_lease.mode_root)==native_multiplayer_entry_mode())return false;
    const auto& all=nb_at(nb_at(nb_at(projection,"allStateSync"),"payload"),"allStateSyncData");
    bool native_target_present=false;
    for(const auto& row:nb_at(all,"staticPlayers").array)if(nb_number(row,"index")==target)native_target_present=true;
    if(!native_target_present)throw std::runtime_error("dead observation target is absent from native roster");
    if(phase=="preparation"||phase=="special"||phase=="battle"){
        auto packet=native_multiplayer_dead_observation_packet(view);const auto& payload=nb_at(packet,"payload");
        if(nb_number(payload,"obIndex")!=target||nb_number(payload,"state")!=state)return false;
        auto module=field_object(scene,"sceneStateData");
        if(!module||api.il2cpp_object_get_class(module)!=cls("Torappu.UI.AutoChess.AutoChessBattleSceneData+AutoChessBattleSceneStateData"))
            throw std::runtime_error("dead observation original scene-state module missing");
        // Exact _HandleDeadAutoObDn order. The dispatcher performs its single
        // ordinary OnBattleStatusChanged after these original Fill calls.
        nb_fill(scene,0x602b5f6,nb_at(payload,"preparation"));
        int32_t native_state=int32_t(state),native_target=int32_t(target);void* args[]={&native_state,&native_target};
        invoke(bound_methods.at(0x602b603),module,args);
    }
    next.mode_root=hold_new(native_multiplayer_entry_mode(),false);
    if(!next.mode_root)throw std::runtime_error("dead observation private mode retention failed");
    native_multiplayer_dead_observation_reset();nmdo_lease=std::move(next);++nmdo_fills;
    return true;
}
bool nmdo_zero_target(void* self){
    if(!nmdo_lease.mode_root||nmdo_lease.target!=0||!nmh_scope()||self!=nb_centre()||
       hold_get(nmdo_lease.mode_root)!=native_multiplayer_entry_mode())return false;
    // ServiceMode can survive a party return. The retained native pointer is
    // necessary but not sufficient: require this recipient's room/match/run.
    if(native_multi_entry_snapshot.kind!=cnjson::Value::Object||
       nb_string(native_multi_entry_snapshot,"roomId")!=nmdo_lease.room||
       nb_number(native_multi_entry_snapshot,"matchSequence")!=nmdo_lease.match||
       nb_number(native_multi_entry_snapshot,"seat")!=nmdo_lease.seat||
       nb_string(nb_at(native_multi_entry_snapshot,"selfState"),"runId")!=nmdo_lease.run)return false;
    return nmh_int(self,"<selfPlayerIndex>k__BackingField")==nmdo_lease.seat&&nmh_int(self,"m_deadObIndex")==0;
}
bool nmdo_has(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;
    try{if(nmdo_zero_target(self)){++nmdo_zero_reads;return true;}}catch(const std::exception& ex){nb_error("multiplayer-dead-observation",ex);}
    lease.release();return nmdo_old_has(self,info);
}
int32_t nmdo_real(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return -1;
    try{if(nmdo_zero_target(self)){++nmdo_zero_reads;return 0;}}catch(const std::exception& ex){nb_error("multiplayer-dead-observation",ex);}
    lease.release();return nmdo_old_real(self,info);
}
void native_multiplayer_dead_observation_setup(){
    for(const auto& spec:NMDO_METHODS){
        if(spec.token==0x601343e)hook(spec,nmdo_has,nmdo_old_has);
        else if(spec.token==0x601343d)hook(spec,nmdo_real,nmdo_old_real);
        else bind_method(spec);
    }
}
