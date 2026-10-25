// Included inside namespace cn after native_combat.inc. No training-domain or
// official network method is invoked for a private garrison report.
#pragma once
inline constexpr CN_Method NBG_ACT_DATA={"Torappu.UI.AutoChess.AutoChessUtil","GetActAutoChessData",0x602b516,0x2761300ULL,1,true,"Torappu.ActAutoChessData",{"System.String"}};
inline constexpr CN_Method NBG_SELF_DATA={"Torappu.Battle.DataCenter.AutoChessDataCenter","get_selfPlayerData",0x601345f,0xb14a30ULL,0,false,"Torappu.Battle.AutoChess.AutoChessPlayerDataModel+ScenePlayerData",{}};
inline constexpr CN_Method NBG_GET_STACK={"Torappu.Battle.AutoChess.AutoChessPlayerDataModel+ScenePlayerData","GetBondStackCount",0x6013926,0xbb0a40ULL,1,false,"System.Int32",{"System.String"}};
using NBG_Add=void(*)(void*,int32_t,void*,int32_t,const void*);NBG_Add nbg_old_add=nullptr;
using NBG_Int=void(*)(void*,int32_t,const void*);NBG_Int nbg_old_magic=nullptr;
cngarrison::Queue nbg_queue;
std::map<int32_t,std::string> nbg_touched;
std::atomic<unsigned> nbg_captured{0},nbg_confirmed{0},nbg_failures{0},nbg_echoes{0},nbg_verified{0};
std::atomic<bool> nbg_failed{false};
bool nbg_journal_dirty=false;
std::string nbg_last_error,nbg_incarnation,nbg_journal_bytes;
struct NBG_IOInput {
    std::string run,mode,stage,phase,metadata,assembly;
    int32_t round=0,revision=0;
    uint64_t generation=0;
    std::vector<cngarrison::Event> events;
};
struct NBG_IOOutput {
    uint64_t generation=0,elapsed_ms=0;
    std::string run,journal,error;
    std::vector<std::string> acknowledged;
    cnjson::Value reply;
    bool committed=false,erased=false,permanent=false;
};
cngarrison::AsyncJob<NBG_IOInput,NBG_IOOutput> nbg_io;
uint64_t nbg_generation=0;
std::atomic<unsigned> nbg_jobs{0};
std::atomic<uint64_t> nbg_io_ms{0};

void native_garrison_reset(){++nbg_generation;nbg_queue.clear();nbg_touched.clear();nbg_failed=false;nbg_journal_dirty=false;nbg_last_error.clear();nbg_journal_bytes.clear();}
// The host calls this after RuntimeGate.close_and_drain(), without data/report
// locks. The IO worker never needs either lock or a main-thread callback.
void native_garrison_shutdown(){nbg_io.shutdown();}
// Native action callbacks may run inside a managed state transition. Record
// failures in memory here; diagnostics and journal IO belong to safe UI ticks.
void nbg_error(const char* step,const std::exception& ex){std::lock_guard<std::recursive_mutex> lock(nb_data_lock);nbg_failed=true;++nbg_failures;nbg_last_error=std::string(step)+": "+ex.what();nb_last_error=nbg_last_error;}
cnjson::Value nbg_payload(const cngarrison::Event& event){
    auto payload=nb_object();payload.object["charInstId"]=nb_integer(event.source);payload.object["layerDelta"]=nb_integer(event.count);payload.object["bondIndexList"]=nb_array();
    for(auto id:event.bonds)payload.object["bondIndexList"].array.push_back(nb_integer(id));return payload;
}
cnjson::Value nbg_request(const cngarrison::Event& event){
    auto request=nb_object();request.object["actId"]=json_text("act2autochess");request.object["requestId"]=json_text(event.request);request.object["type"]=json_text("AutoChessBattleSceneSelfBattleAddBondUp");request.object["round"]=nb_integer(event.round);request.object["payload"]=nbg_payload(event);return request;
}
std::string nbg_document(const std::string& run,const std::string& metadata,const std::string& assembly,const std::vector<cngarrison::Event>& events){
    auto document=nb_object();document.object["version"]=nb_integer(1);document.object["activityId"]=json_text("act2autochess");document.object["metadataHash"]=json_text(metadata);document.object["gameAssemblyHash"]=json_text(assembly);document.object["runId"]=json_text(run);document.object["requests"]=nb_array();
    for(const auto& event:events)document.object["requests"].array.push_back(nbg_request(event));return cnjson::dump(document);
}
// This boundary owns immutable snapshots only. In particular, it never reads
// nb_state/nb_run_id, invokes IL2CPP, takes nb_data_lock, or writes diagnostics.
NBG_IOOutput nbg_execute_io(const NBG_IOInput& input){
    NBG_IOOutput output;output.generation=input.generation;output.run=input.run;const auto began=GetTickCount64();
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load()){output.error="private garrison IO cancelled during shutdown";return output;}
    try{
        output.journal=nbg_document(input.run,input.metadata,input.assembly,input.events);
        native_pending_write("garrison",input.run,output.journal);output.committed=true;
        const auto endpoint=L"/bridge/act2autochess/native-session/"+std::wstring(input.run.begin(),input.run.end())+L"/protocol";
        int32_t revision=input.revision;
        for(size_t index=0;index<input.events.size()&&index<8;++index){
            if(closing.load())throw std::runtime_error("private garrison IO cancelled during shutdown");
            const auto& event=input.events[index];const auto request=nbg_request(event);cnjson::Value reply;
            try{reply=nb_http(endpoint,&request);}catch(const std::exception&){if(closing.load())throw;reply=nb_http(endpoint,&request);}
            if(!nb_true(reply,"ok")){output.permanent=true;throw std::runtime_error("private garrison report rejected: "+nb_string(reply,"error"));}
            // The server's bond.add ACK intentionally has no native messages.
            // A different phase/model batch must never be played on a worker.
            output.permanent=true;const auto& state=nb_at(reply,"state");const auto& messages=nb_at(reply,"messages");
            if(nb_string(reply,"requestId")!=event.request||messages.kind!=cnjson::Value::Array||!messages.array.empty()||
                nb_string(state,"activityId")!="act2autochess"||nb_string(state,"runId")!=input.run||
                nb_string(state,"modeId")!=input.mode||nb_string(state,"stageId")!=input.stage||nb_string(state,"phase")!=input.phase||
                nb_number(state,"round")!=input.round||nb_number(state,"revision")<revision)
                throw std::runtime_error("private garrison acknowledgement identity/phase/revision mismatch");
            revision=nb_number(state,"revision");output.acknowledged.push_back(event.request);output.reply=std::move(reply);output.permanent=false;
        }
        // A bounded prefix can leave unsent requests in the original journal.
        // Replaying that whole document is safe because each ID is immutable.
        if(output.acknowledged.size()==input.events.size()){
            native_pending_erase("garrison",input.run,output.journal);output.erased=true;
        }
    }catch(const std::exception& ex){output.error=ex.what();if(!output.committed)output.permanent=true;}
    output.elapsed_ms=GetTickCount64()-began;return output;
}
void nbg_persist(){
    if(!nbg_journal_dirty)return;
    if(!cngarrison::journal_flush_allowed(nb_projection_depth!=0,nb_command_in_flight,nb_native_transition_depth!=0))throw std::runtime_error("private garrison journal requires a safe tick boundary");
    if(nbg_queue.empty()){
        if(!nbg_journal_bytes.empty())native_pending_erase("garrison",nb_run_id,nbg_journal_bytes);nbg_journal_bytes.clear();nbg_journal_dirty=false;return;
    }
    std::vector<cngarrison::Event> events(nbg_queue.events().begin(),nbg_queue.events().end());
    auto bytes=nbg_document(nb_run_id,CN_METADATA_HASH,native_pending_game_assembly_hash(),events);native_pending_write("garrison",nb_run_id,bytes);nbg_journal_bytes=std::move(bytes);nbg_journal_dirty=false;
}
void native_garrison_recover_prelaunch(cnjson::Value& response){
    // Scene teardown may have detached an IO job. Do not race its atomic
    // replacement/erase with a new launch's journal recovery.
    if(nbg_io.active()){
        NBG_IOOutput discarded;if(!nbg_io.try_take(discarded))throw std::runtime_error("private garrison background confirmation still finishing; retry launch");
        // The caller's GET may have raced the last accepted POST. Even if the
        // worker erased its fully ACKed journal, never reuse that older GET.
        const auto run=nb_string(nb_at(response,"state"),"runId");
        if(discarded.run!=run)throw std::runtime_error("private garrison detached job belongs to another run");
        auto fresh=nb_http(L"/bridge/act2autochess/native-session/"+std::wstring(run.begin(),run.end()));
        if(!nb_true(fresh,"ok")||nb_string(nb_at(fresh,"state"),"runId")!=run)throw std::runtime_error("private garrison detached recovery snapshot invalid");response=std::move(fresh);
    }
    const auto run=nb_string(nb_at(response,"state"),"runId");const auto bytes=native_pending_read("garrison",run);if(bytes.empty())return;
    const auto document=cnjson::parse(bytes);
    if(nb_number(document,"version")!=1||nb_string(document,"activityId")!="act2autochess"||nb_string(document,"metadataHash")!=CN_METADATA_HASH||nb_string(document,"gameAssemblyHash")!=native_pending_game_assembly_hash()||nb_string(document,"runId")!=run||document.object.size()!=6)throw std::runtime_error("private garrison journal profile mismatch");
    const auto& requests=nb_at(document,"requests");if(requests.kind!=cnjson::Value::Array||requests.array.empty()||requests.array.size()>4096)throw std::runtime_error("private garrison journal request count invalid");
    // Validate the entire immutable journal before the first loopback write.
    cngarrison::Queue validated;
    for(const auto& request:requests.array){
        if(request.object.size()!=5||nb_string(request,"actId")!="act2autochess"||nb_string(request,"type")!="AutoChessBattleSceneSelfBattleAddBondUp")throw std::runtime_error("private garrison journal request type rejected");
        const auto& payload=nb_at(request,"payload");cngarrison::Event event;event.run=run;event.request=nb_string(request,"requestId");event.round=nb_number(request,"round");event.source=nb_number(payload,"charInstId");event.count=nb_number(payload,"layerDelta");const auto& bonds=nb_at(payload,"bondIndexList");if(bonds.kind!=cnjson::Value::Array)throw std::runtime_error("private garrison journal targets missing");
        for(const auto& bond:bonds.array){if(bond.kind!=cnjson::Value::Integer||bond.integer<0||bond.integer>255)throw std::runtime_error("private garrison journal target invalid");event.bonds.push_back(int32_t(bond.integer));}validated.restore(event);
        if(cnjson::dump(payload)!=cnjson::dump(nbg_payload(event)))throw std::runtime_error("private garrison journal payload changed or contains unknown fields");
    }
    const auto endpoint=L"/bridge/act2autochess/native-session/"+std::wstring(run.begin(),run.end());
    for(const auto& request:requests.array){const auto reply=nb_http(endpoint+L"/protocol",&request);if(!nb_true(reply,"ok")||nb_string(nb_at(reply,"state"),"runId")!=run)throw std::runtime_error("private garrison persisted report rejected; journal retained");}
    auto fresh=nb_http(endpoint);if(!nb_true(fresh,"ok")||nb_string(nb_at(fresh,"state"),"runId")!=run)throw std::runtime_error("private garrison recovery snapshot invalid");
    native_pending_erase("garrison",run,bytes);response=std::move(fresh);report("private-native-garrison-persisted-reports-confirmed-before-combat-recovery");
}
int32_t nbg_identifier(void* key,TemporaryRoots& roots){
    void* activity[]={roots.retain(string_new("act2autochess"))};auto data=roots.retain(invoke(bound_methods.at(NBG_ACT_DATA.token),nullptr,activity));
    auto table=field_object(data,"bondInfoDict");if(!table)throw std::runtime_error("private garrison definitions unavailable");
    void* args[]={key};auto row=roots.retain(invoke(method(api.il2cpp_object_get_class(table),"get_Item",1),table,args));
    auto id=nb_native_int(row,"identifier");if(id<0||id>255||text(field_object(row,"bondId"))!=text(key))throw std::runtime_error("private garrison definition identifier/key mismatch");
    return id;
}
void nbg_add(void* self,int32_t source,void* bonds,int32_t count,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(self)){lease.release();nbg_old_add(self,source,bonds,count,method_info);return;}
    if(!cngarrison::capture_scope_with_transition(true,nb_projection_depth!=0,nb_command_in_flight,nb_native_transition_depth!=0,nbc_live_battle())){++nbg_echoes;return;}
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);PrivateDataScope private_scope(true);TemporaryRoots roots;
    if(!nbc_live_battle()||nbg_failed.load())throw std::runtime_error("private garrison outside active first battle or prior report failed");
    // Native _TriggerGarrisonStatusRefresh returns before AddBondCount unless
    // GameStatus.inSelfBattleState (currentState==4). Boss uses state 6.
    // A callback here during a boss is an invariant failure, not a delta to
    // drop silently after the native engine has already changed its counter.
    // Server PREP_FIN/REST_START effects never enter this combat-only bridge.
    if(nb_string(nb_state,"phase")=="boss")throw std::runtime_error("private boss combat garrison callback violates native self-battle gate; scene retained");
    // Source may be in the hand: native runes support character_target_inhand.
    // Validate owned character identity, never require battlefield deployment.
    bool owned=false;for(const auto& unit:nb_at(nb_at(nb_state,"player"),"charChess").array)if(nb_number(unit,"instId")==source){owned=true;break;}
    if(!owned)throw std::runtime_error("private garrison source is not an owned character");
    cngarrison::Event event;event.run=nb_run_id;event.round=nb_number(nb_state,"round");event.source=source;event.count=count;
    std::map<int32_t,std::string> keys;auto size=nbc_list_count(bonds,64);
    for(int32_t i=0;i<size;++i){auto key=nbc_list_item(bonds,i,roots);auto id=nbg_identifier(key,roots);event.bonds.push_back(id);keys.emplace(id,text(key));}
    nbg_queue.push(std::move(event),nbg_incarnation);nbg_touched.insert(keys.begin(),keys.end());nbg_journal_dirty=true;++nbg_captured;
}catch(const std::exception& ex){nbg_error("garrison-capture",ex);}}
void nbg_use_magic(void* self,int32_t inst_id,const void* method_info){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(self)){lease.release();nbg_old_magic(self,inst_id,method_info);return;}if(nb_suppress_outbound())return;nb_main_thread();
    if(nb_string(nb_state,"phase")!="preparation")throw std::runtime_error("private magic item requires preparation");
    auto positions=nbc_board_positions();auto payload=nb_object();payload.object["instId"]=nb_integer(inst_id);payload.object["unitPositionInfo"]=nb_array();auto deployed=nbc_native_deployment_count();
    for(const auto& position:positions){auto value=nb_object();value.object["instId"]=nb_integer(position.inst_id);value.object["position"]=nb_integer(position.position);value.object["chessType"]=nb_integer(position.chess_type);value.object["direction"]=nb_integer(position.direction);payload.object["unitPositionInfo"].array.push_back(value);}
    payload.object["deployCount"]=nb_integer(deployed);nb_send("AutoChessBattleSceneUseMagicUp",payload);
}catch(const std::exception& ex){nb_error("ReqUseMagic",ex);nb_recover_after_error();}}
void nbg_assert_authority(){
    if(!nbg_queue.empty())throw std::runtime_error("private garrison verification requires every pending acknowledgement");
    PrivateDataScope private_scope(true);TemporaryRoots roots;auto player=roots.retain(invoke(bound_methods.at(NBG_SELF_DATA.token),nb_centre()));
    if(!player)throw std::runtime_error("private garrison player model unavailable");
    for(const auto& pair:nbg_touched){
        int32_t authoritative=0;for(const auto& bond:nb_at(nb_at(nb_state,"player"),"bonds").array)if(nb_number(bond,"bondIdentifier")==pair.first){authoritative=nb_number(bond,"stackCnt");break;}
        void* args[]={roots.retain(string_new(pair.second.c_str()))};int32_t actual=0;
        if(!read(unbox(invoke(bound_methods.at(NBG_GET_STACK.token),player,args)),actual)||actual!=authoritative)
            throw std::runtime_error("private garrison native/confirmed layer mismatch for "+pair.second);
    }
    nbg_touched.clear();++nbg_verified;
}
bool native_garrison_pending(){return nbg_failed.load()||!nbg_queue.empty()||nbg_io.active();}
bool native_garrison_tick(){
    if(nbg_failed.load())return true;if(nbg_queue.empty()&&!nbg_journal_dirty&&!nbg_io.active())return false;
    if(!cngarrison::journal_flush_allowed(nb_projection_depth!=0,nb_command_in_flight,nb_native_transition_depth!=0)||closing.load())return true;
    try{nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        NBG_IOOutput completed;
        if(nbg_io.try_take(completed)){
            nbg_io_ms=completed.elapsed_ms;
            if(completed.generation!=nbg_generation||completed.run!=nb_run_id)throw std::runtime_error("private garrison detached acknowledgement ignored");
            if(completed.committed)nbg_journal_bytes=completed.erased?std::string{}:completed.journal;
            if(!completed.acknowledged.empty()){
                if(completed.acknowledged.size()>nbg_queue.size())throw std::runtime_error("private garrison acknowledgement exceeds pending queue");
                size_t index=0;for(const auto& request:completed.acknowledged)if(nbg_queue.events()[index++].request!=request)throw std::runtime_error("private garrison acknowledgement order changed");
                const auto& state=nb_at(completed.reply,"state");nb_check_state(state);
                if(!nbc_live_battle()||nb_string(state,"runId")!=nb_run_id||nb_string(state,"phase")!=nb_string(nb_state,"phase")||
                    nb_string(state,"modeId")!=nb_string(nb_state,"modeId")||nb_string(state,"stageId")!=nb_string(nb_state,"stageId")||
                    nb_number(state,"round")!=nb_number(nb_state,"round")||nb_number(state,"revision")<nb_number(nb_state,"revision"))
                    throw std::runtime_error("private garrison authority advanced outside the pending queue");
                // JSON authority only. The original native rune already
                // applied every optimistic delta, including inflight arrivals.
                nb_accept_reply(completed.reply);nb_commands+=unsigned(completed.acknowledged.size());nb_applied_batches+=unsigned(completed.acknowledged.size());
                for(const auto& request:completed.acknowledged){nbg_queue.acknowledge(request);++nbg_confirmed;}
            }
            nbg_journal_dirty=!nbg_queue.empty();
            if(!completed.error.empty()){
                if(completed.permanent||nbg_queue.empty())throw std::runtime_error(completed.error);
                auto& event=nbg_queue.front();event.last_attempt=GetTickCount64();++event.attempts;
                nbg_last_error="garrison-send: "+completed.error;++nbg_failures;
                if(event.attempts>=3)throw std::runtime_error(completed.error);
            }else nbg_last_error.clear();
        }
        // ACK updates authority JSON only: native AddBondCount already applied
        // every queued delta. Re-projecting an intermediate ACK would erase
        // later optimistic deltas and could restart the native self battle.
        if(nbg_queue.empty()&&!nbg_io.active()){nbg_journal_dirty=false;nbg_assert_authority();return false;}
        if(nbg_io.active())return true;
        auto& event=nbg_queue.front();
        if(!nbc_live_battle()||event.run!=nb_run_id||event.round!=nb_number(nb_state,"round"))throw std::runtime_error("private garrison pending event run/round changed");
        if(!cngarrison::retry_due(event,GetTickCount64()))return true;
        NBG_IOInput input;input.run=nb_run_id;input.mode=nb_string(nb_state,"modeId");input.stage=nb_string(nb_state,"stageId");input.phase=nb_string(nb_state,"phase");input.round=event.round;input.revision=nb_number(nb_state,"revision");input.metadata=CN_METADATA_HASH;input.assembly=native_pending_game_assembly_hash();input.generation=nbg_generation;input.events.assign(nbg_queue.events().begin(),nbg_queue.events().end());
        nbg_io.start(std::move(input),nbg_execute_io);++nbg_jobs;
    }catch(const std::exception& ex){
        // A transient uncertain send retries the identical request, but a
        // permanent rejection/three failed attempts retains the scene+queue.
        nbg_last_error=std::string("garrison-send: ")+ex.what();++nbg_failures;nb_error("garrison-send",ex);nbg_failed=true;
    }
    return native_garrison_pending();
}
cnjson::Value native_garrison_status(bool details){
    auto value=nb_object();value.object["calculatedLocally"]=nb_integer(nbg_captured.load());value.object["serverConfirmed"]=nb_integer(nbg_confirmed.load());value.object["layerVerifications"]=nb_integer(nbg_verified.load());value.object["failures"]=nb_integer(nbg_failures.load());value.object["projectionEchoesBlocked"]=nb_integer(nbg_echoes.load());value.object["failed"]=nb_boolean(nbg_failed.load());value.object["asyncJobs"]=nb_integer(nbg_jobs.load());value.object["lastIOMilliseconds"]=nb_integer(int64_t(nbg_io_ms.load()));
    if(details){value.object["pending"]=nb_integer(nbg_queue.size());value.object["journalDirty"]=nb_boolean(nbg_journal_dirty);value.object["lastError"]=json_text(nbg_last_error);if(!nbg_queue.empty())value.object["pendingRequestId"]=json_text(nbg_queue.front().request);}return value;
}
void native_garrison_setup(){
    bind_method(NBG_ACT_DATA);bind_method(NBG_SELF_DATA);bind_method(NBG_GET_STACK);nb_hook(0x60135ce,nbg_add,nbg_old_add);nb_hook(0x60135cb,nbg_use_magic,nbg_old_magic);
    // ReqSelfChoiceSelect has one owner: native_battle_data_setup installs
    // nb_self_choice for preparation choices (items and special offers alike).
    auto row=cls("Torappu.ActAutoChessData+ActAutoChessBondInfo");if(!row||api.il2cpp_field_get_offset(profile_field(row,"identifier"))!=84||!matches_type(api.il2cpp_field_get_type(profile_field(row,"identifier")),"System.Int32"))throw std::runtime_error("private garrison identifier layout mismatch");
    nbg_incarnation=std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64());
}
