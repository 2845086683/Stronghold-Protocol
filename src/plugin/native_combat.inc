// Included by native_battle_data.inc inside namespace cn.
// Four private SINGLE modes. Scheduling/combat stays in the Unity task graph.
#pragma once
std::atomic<unsigned> nbc_ready_commands{0},nbc_kills{0},nbc_escapes{0},nbc_infos{0},nbc_reports{0},nbc_board_unchanged{0},nbc_late_events{0};
std::atomic<bool> nbc_pending{false};
std::atomic<bool> nbc_journal_waiting{false},nbc_cleanup_pending{false};
std::atomic<bool> nbc_identity_failed{false};
// Preserve the first registration rejection independently of retryable final
// snapshot errors. Only values already observed in the callback are copied;
// error handling never queries Unity objects which may have been recycled.
struct NBC_SpawnTrace{
    std::string run,phase,actual_key,template_key,expected_meta_type,meta_key;
    int32_t round=0,issued_count=0,registered_count=0,inst_id=0,action_index=0;
    uint32_t uid=0;
    bool manager_matches=false,key_read=false,template_read=false,options_read=false;
    bool summon_read=false,summoned=false,meta_read=false,meta_present=false;
    bool meta_type_read=false,meta_type_matches=false,meta_identity_read=false,uid_read=false;
};
struct NBC_IdentityFailure{bool recorded=false;std::string step,error;NBC_SpawnTrace trace;};
NBC_IdentityFailure nbc_first_identity_failure;
void nbc_record_spawn_failure(const NBC_SpawnTrace& trace,const std::exception& ex) noexcept{
    try{std::lock_guard<std::recursive_mutex> lock(nb_data_lock);if(nbc_first_identity_failure.recorded)return;
        NBC_IdentityFailure failure;failure.step="combat-spawn-identity";failure.error=ex.what();failure.trace=trace;failure.recorded=true;nbc_first_identity_failure=std::move(failure);}catch(...){}
}
std::atomic<bool> nbc_boss_observed{false};
int32_t nbc_boss_hp=0;
void* nbc_boss_enemy=nullptr;
bool nbc_clock_active=false,nbc_clock_frozen=false,nbc_timeout=false,nbc_end_requested=false;
cntiming::Policy nbc_clock_timing;
int64_t nbc_clock_start=0;uint64_t nbc_clock_anchor=0;
int32_t nbc_clock_round=0,nbc_clock_duration=0,nbc_clock_elapsed=0,nbc_clock_hp_display=-1,nbc_clock_native_hp_loss=0,nbc_end_result=0;
void* nbc_clock_manager=nullptr;void* nbc_clock_centre=nullptr;
std::string nbc_clock_run,nbc_clock_mode,nbc_clock_stage;
std::atomic<bool> nbc_settle_pending{false};
std::string nbc_settle_run;
// Retained through scene teardown until the result-page consumer takes it.
cnjson::Value nbc_settle_snapshot;
std::atomic<bool> nbc_settle_summary_ready{false};
struct NBC_SettleProof{std::string run,request,uid;int32_t revision=0,round=0,attempt=0;};
NBC_SettleProof nbc_settle_proof;
bool nbc_settle_building=false;
extern std::string npb_model_run;
void native_combat_invalidate_settlement_snapshot(){
    std::lock_guard<std::recursive_mutex> lock(nb_data_lock);nbc_settle_summary_ready=false;
    nbc_settle_snapshot=cnjson::Value{};nbc_settle_proof={};
    if(!closing.load()){drop_cache("native-private-settlement-params");drop_cache("native-private-settlement");}
}
// A native finish-service request is a separate scene-unload ACK. Authorize it
// only from a genuine final-report response, never from a result-shaped state.
void native_solo_terminal_finish_proof(const std::string&,const std::string&,int32_t);
std::string nbc_terminal_ack_run,nbc_terminal_ack_request;
int32_t nbc_terminal_ack_revision=0,nbc_terminal_ack_round=0;
std::map<int32_t,void*> nbc_registered_enemies;
std::map<int32_t,uint32_t> nbc_registered_enemy_uids;
std::set<int32_t> nbc_respawned_enemies;
// LOCAL MockBossBattleManager does not populate selfBattleStatus. That object
// can still contain the previous normal round's kill/escape lists. Capture only
// the current boss round's real native callbacks, under issued object identity.
std::map<int32_t,cncombat::KillInfo> nbc_boss_killed;
std::set<int32_t> nbc_boss_escaped,nbc_boss_escaped_tokens;
// The current issued-instId whitelist owns protocol counts. Native extra
// entities (including skill targets) retain a separate object/UID namespace.
// Retain their exact observed object generation in a separate namespace.
struct NBC_UnindexedBossSummon{std::string key;uint32_t uid=0;};
std::map<void*,NBC_UnindexedBossSummon> nbc_unindexed_boss_summons;
struct NBC_RespawnContext{int32_t id=0,action=0,round=0;uint32_t old_uid=0;std::string key,run;void* previous=nullptr;void* meta=nullptr;void* spawned=nullptr;};
thread_local NBC_RespawnContext* nbc_respawn_context=nullptr;
void nbc_clear_registered_enemies(){std::lock_guard<std::recursive_mutex> lock(nb_data_lock);if(!closing.load()){for(const auto& row:nbc_registered_enemies)drop_cache("native-combat-issued-"+std::to_string(row.first));for(const auto& row:nbc_unindexed_boss_summons)drop_cache("native-combat-boss-summon-"+std::to_string(row.second.uid));}nbc_registered_enemies.clear();nbc_registered_enemy_uids.clear();nbc_respawned_enemies.clear();nbc_boss_killed.clear();nbc_boss_escaped.clear();nbc_boss_escaped_tokens.clear();nbc_unindexed_boss_summons.clear();nbc_first_identity_failure={};}
uint32_t nbc_enemy_uid(void* enemy){uint32_t uid=0;api.il2cpp_field_get_value(enemy,profile_field(api.il2cpp_object_get_class(enemy),"<instanceUid>k__BackingField"),&uid);return uid;}
// Original LOCAL auxiliary enemies have no protocol instId. Track their real
// native object identity, never invent an integer that could alias issued IDs.
std::map<void*,std::string> nbc_registered_local_enemies;
cnjson::Value nbc_pending_payload;
cnjson::Value nbc_last_board_submission;
std::string nbc_pending_run,nbc_pending_request,nbc_pending_mode,nbc_pending_stage,nbc_last_error,nbc_journal_bytes;
int32_t nbc_pending_round=0;
unsigned nbc_attempts=0;
uint64_t nbc_last_attempt=0;

int32_t nbc_list_count(void* list,int32_t bound=128){
    if(!list)throw std::runtime_error("private native collection missing");int32_t count=0;
    if(!read(unbox(invoke(method(api.il2cpp_object_get_class(list),"get_Count",0),list)),count)||count<0||count>bound)
        throw std::runtime_error("private native collection bound invalid");return count;
}
void* nbc_list_item(void* list,int32_t index,TemporaryRoots& roots){
    void* args[]={&index};auto value=roots.retain(invoke(method(api.il2cpp_object_get_class(list),"get_Item",1),list,args));
    if(!value)throw std::runtime_error("private native collection item missing");return value;
}
std::set<int32_t> nbc_json_ids(const cnjson::Value& values){
    if(values.kind!=cnjson::Value::Array||values.array.size()>128)throw std::runtime_error("private combat identity list invalid");
    std::set<int32_t> ids;for(const auto& value:values.array){
        if(value.kind!=cnjson::Value::Integer||value.integer<=0||value.integer>INT32_MAX||!ids.insert(int32_t(value.integer)).second)
            throw std::runtime_error("private combat identity invalid or repeated");}return ids;
}
std::set<int32_t> nbc_owned_chess(bool include_equipment=false){
    std::set<int32_t> ids;const auto& player=nb_at(nb_state,"player");
    for(const auto* field:{"charChess","trapChess","equipChess"}){
        if(!include_equipment&&std::strcmp(field,"equipChess")==0)continue;
        const auto& list=nb_at(player,field);if(list.kind!=cnjson::Value::Array||list.array.size()>128)throw std::runtime_error("private owned chess list invalid");
        for(const auto& item:list.array){auto id=nb_number(item,"instId");if(id<=0||!ids.insert(id).second)throw std::runtime_error("private owned chess identity invalid");}
    }return ids;
}
std::set<int32_t> nbc_issued(const cnjson::Value& state){
    const auto& battle=nb_at(state,"battle");const auto& units=nb_at(battle,"units");
    if(nb_number(state,"nativeCombatVersion")!=2||nb_number(battle,"nativeCombatVersion")!=2||units.kind!=cnjson::Value::Array||units.array.empty()||units.array.size()>128)
        throw std::runtime_error("private native battle authority version or enemy count invalid");
    std::set<int32_t> ids;
    for(const auto& unit:units.array){auto id=nb_number(unit,"instId"),action=nb_number(unit,"actionIndex");auto enemy=nb_string(unit,"enemyId");
        if(id<=0||!ids.insert(id).second||action<0||action>4096||enemy.empty()||enemy.size()>128)throw std::runtime_error("private native enemy template or identity mismatch");
        // CN _GenerateActionFromEnemyInfo (0x601383e) creates a fresh Action
        // for each info, retaining the original route. At 0xb65034..0xb6504b
        // it appends to Dictionary<int,List<Action>> even when this action
        // index already exists. Strategy replacements can therefore share an
        // index with another key; registration still verifies the full issued
        // (instId, actionIndex, enemyId) triple against the actual native key.
    }
    return ids;
}
std::map<std::string,int32_t> nbc_local_enemy_contract(const cnjson::Value& state){
    const auto& battle=nb_at(state,"battle");
    auto actions=battle.object.find("nativeLocalEnemyActions"),keys=battle.object.find("nativeLocalEnemyKeys"),count=battle.object.find("nativeLocalEnemyCount");
    // Legacy introductory captures predate these fields. Absence grants no
    // local exception; a partial new declaration is always a contract error.
    if(actions==battle.object.end()&&keys==battle.object.end()&&count==battle.object.end())return {};
    if(actions==battle.object.end()||keys==battle.object.end()||count==battle.object.end()||
       actions->second.kind!=cnjson::Value::Array||actions->second.array.size()>128||keys->second.kind!=cnjson::Value::Array||keys->second.array.size()>128)
        throw std::runtime_error("private local enemy template declarations incomplete");
    std::map<std::string,int32_t> limits;std::set<std::string> declared;int32_t total=0;
    for(const auto& action:actions->second.array){auto key=nb_string(action,"enemyId");auto index=nb_number(action,"actionIndex"),amount=nb_number(action,"count");
        if(action.object.size()!=3||key.empty()||key.size()>128||index<0||index>4096||amount<1||amount>128||total>128-amount)
            throw std::runtime_error("private local enemy action bound invalid");
        total+=amount;limits[key]+=amount;
    }
    for(const auto& key:keys->second.array)if(key.kind!=cnjson::Value::String||!limits.count(key.string)||!declared.insert(key.string).second)
        throw std::runtime_error("private local enemy keys disagree with current template");
    if(declared.size()!=limits.size()||nb_number(battle,"nativeLocalEnemyCount")!=total)
        throw std::runtime_error("private local enemy count disagrees with current template");
    for(const auto& unit:nb_at(battle,"units").array)if(limits.count(nb_string(unit,"enemyId")))
        throw std::runtime_error("private local and issued enemy templates overlap");
    auto bosses=battle.object.find("nativeLocalBossKeys");if(bosses!=battle.object.end()){
        if(bosses->second.kind!=cnjson::Value::Array)throw std::runtime_error("private local boss template keys invalid");
        for(const auto& key:bosses->second.array)if(key.kind!=cnjson::Value::String||limits.count(key.string))
            throw std::runtime_error("private local auxiliary and boss templates overlap");
    }
    return limits;
}
cntiming::Policy nbc_timing_policy(const cnjson::Value& value){
    const auto& battle=nb_at(value,"battle");const auto mode=nb_string(value,"modeId");const auto round=nb_number(value,"round");
    const auto found=battle.object.find("battleTiming");
    if(found==battle.object.end())return cntiming::legacy(cncombat::is_boss_round(round,mode));
    const auto& snapshot=found->second;const auto expected=cntiming::configured(mode,round);
    if(snapshot.kind!=cnjson::Value::Object||snapshot.object.size()!=10||nb_string(snapshot,"clock")!="native-fixed-play-time"||
       nb_string(snapshot,"elapsedUnit")!="simulation-seconds"||nb_string(snapshot,"displayUnit")!="effective-real-seconds")
        throw std::runtime_error("private native battle timing units invalid");
    const std::pair<const char*,int32_t> fields[]={{"version",expected.version},{"speedNumerator",expected.speed_numerator},
        {"speedDenominator",expected.speed_denominator},{"displayDurationSeconds",expected.display_seconds},
        {"simulationDurationSeconds",expected.simulation_seconds},{"nativeBossOffsetSeconds",expected.boss_offset_seconds},
        {"nativeForceEndDurationSeconds",expected.force_end_seconds}};
    for(const auto& field:fields){const auto& saved=nb_at(snapshot,field.first);
        if(saved.kind!=cnjson::Value::Integer||saved.integer!=field.second)throw std::runtime_error("private native battle timing snapshot invalid");}
    return expected;
}
void native_combat_validate_state(const cnjson::Value& value){
    const auto phase=nb_string(value,"phase"),mode=nb_string(value,"modeId");
    if(!cncombat::mode_allowed(mode)||!cncombat::stage_allowed(nb_string(value,"stageId"),mode))throw std::runtime_error("private native battle mode/map unsupported");
    if(phase=="paused"&&nb_string(value,"previousPhase")!="preparation")throw std::runtime_error("private native mid-battle pause unsupported");
    if(phase!="battle"&&phase!="boss")return;
    const auto round=nb_number(value,"round");
    if(!cncombat::state_allowed(phase,round,mode))throw std::runtime_error("private native battle round unsupported");
    nbc_issued(value);nbc_local_enemy_contract(value);const auto& battle=nb_at(value,"battle");
    const auto timing=nbc_timing_policy(value);auto started=nb_at(battle,"startedAtTs"),deadline=nb_at(battle,"deadlineTs");
    if(started.kind!=cnjson::Value::Integer||deadline.kind!=cnjson::Value::Integer||started.integer<1||
       started.integer>INT64_MAX-timing.force_end_seconds||deadline.integer!=started.integer+timing.force_end_seconds)
        throw std::runtime_error("private native persisted battle deadline invalid");
    nbc_json_ids(nb_at(battle,"nativeOwnedChessIds"));
}
void native_combat_validate_resume(const cnjson::Value& value){
    if(nb_string(value,"phase")=="battle"||nb_string(value,"phase")=="boss")throw std::runtime_error("private native mid-battle rejoin unsupported; retain the active save");
}
void native_combat_reset(){
    native_battle_mode_runes_reset();
    native_battle_tutorial_reset();
    native_battle_buff_templates_reset();
    native_battle_resources_reset();
    // Detaching a Unity scene only releases in-memory state. A journal is erased
    // exclusively after an acknowledged immutable request, never on cancel.
    nbc_pending=false;nbc_journal_waiting=false;nbc_cleanup_pending=false;nbc_identity_failed=false;nbc_boss_observed=false;nbc_boss_hp=0;nbc_boss_enemy=nullptr;nbc_clear_registered_enemies();nbc_registered_local_enemies.clear();nbc_pending_payload=cnjson::Value{};nbc_pending_run.clear();nbc_pending_request.clear();nbc_pending_mode.clear();nbc_pending_stage.clear();nbc_pending_round=0;nbc_journal_bytes.clear();nbc_last_error.clear();nbc_attempts=0;nbc_last_attempt=0;
    nbc_settle_pending=false;nbc_settle_run.clear();
    nbc_terminal_ack_run.clear();nbc_terminal_ack_request.clear();nbc_terminal_ack_revision=nbc_terminal_ack_round=0;
    nbc_clock_active=false;nbc_clock_frozen=false;nbc_clock_timing={};nbc_timeout=false;nbc_end_requested=false;nbc_clock_manager=nbc_clock_centre=nullptr;nbc_clock_run.clear();nbc_clock_mode.clear();nbc_clock_stage.clear();nbc_clock_round=nbc_clock_duration=nbc_clock_elapsed=0;nbc_clock_hp_display=-1;nbc_clock_native_hp_loss=0;
}
void nbc_persist_captured(){
    auto previous=native_pending_read("combat",nbc_pending_run);
    if(!previous.empty()&&previous!=nbc_journal_bytes)throw std::runtime_error("private combat journal already contains a different captured report");
    if(previous.empty())native_pending_write("combat",nbc_pending_run,nbc_journal_bytes);
    nbc_journal_waiting=false;nbc_pending=true;
}
int32_t nbc_attempt_value(const cnjson::Value& value){
    if(value.kind!=cnjson::Value::Integer||value.integer<0||value.integer>INT32_MAX)
        throw std::runtime_error("private battle attempt identity invalid");
    return int32_t(value.integer);
}
int32_t nbc_state_attempt(const cnjson::Value& state,int32_t round){
    auto attempts=state.object.find("nativeBattleAttempts");if(attempts==state.object.end())return 0;
    if(attempts->second.kind!=cnjson::Value::Object)throw std::runtime_error("private battle attempt map invalid");
    auto attempt=attempts->second.object.find(std::to_string(round));return attempt==attempts->second.object.end()?0:nbc_attempt_value(attempt->second);
}
int32_t nbc_record_attempt(const cnjson::Value& record){
    if(record.kind!=cnjson::Value::Object)throw std::runtime_error("private battle attempt record invalid");
    auto attempt=record.object.find("battleAttempt");return attempt==record.object.end()?0:nbc_attempt_value(attempt->second);
}
int32_t nbc_current_attempt(){
    auto attempt=nbc_record_attempt(nb_at(nb_state,"battle"));
    if(attempt!=nbc_state_attempt(nb_state,nb_number(nb_state,"round")))throw std::runtime_error("private battle attempt differs from current session epoch");
    return attempt;
}
void nbc_verify_journal_authority(const cnpj::CombatRecord& record,const cnjson::Value& state){
    if(record.attempt!=nbc_state_attempt(state,record.round))throw std::runtime_error("private combat journal belongs to another battle attempt");
    const auto phase=nb_string(state,"phase");const auto round=nb_number(state,"round");
    if(record.run!=nb_string(state,"runId")||record.mode!=nb_string(state,"modeId")||record.stage!=nb_string(state,"stageId"))
        throw std::runtime_error("private combat journal run/mode/map differs from server state");
    if((phase=="battle"||phase=="boss")&&round==record.round){
        if(record.attempt!=nbc_record_attempt(nb_at(state,"battle"))||record.issued!=nbc_issued(state)||record.owned!=nbc_json_ids(nb_at(nb_at(state,"battle"),"nativeOwnedChessIds")))
            throw std::runtime_error("private combat journal disagrees with issued server identities");
        return;
    }
    if(round<record.round||(phase!="preparation"&&phase!="special"&&phase!="paused"&&phase!="result"))throw std::runtime_error("private combat journal outside acknowledged round checkpoint");
    const auto& saved=nb_at(nb_at(state,"completedReports"),std::to_string(record.round).c_str());
    if(record.attempt!=nbc_record_attempt(saved)||record.issued!=nbc_json_ids(nb_at(saved,"nativeIssuedEnemyIds"))||record.owned!=nbc_json_ids(nb_at(saved,"nativeOwnedChessIds"))||
       cnjson::dump(record.payload)!=cnjson::dump(nb_at(saved,cncombat::is_boss_round(record.round,record.mode)?"nativeBossReport":"nativeReport")))throw std::runtime_error("private combat journal differs from acknowledged server result");
}
void native_combat_recover_prelaunch(cnjson::Value& response){
    nb_main_thread();if(nb_started.load())throw std::runtime_error("private combat journal recovery requires prelaunch");
    const auto& state=nb_at(response,"state");nb_check_state(state);auto run=nb_string(state,"runId");
    auto raw=native_pending_read("combat",run);if(raw.empty())return;
    auto record=cnpj::decode(raw,run,CN_METADATA_HASH,native_pending_game_assembly_hash(),nb_string(state,"modeId"),nb_string(state,"stageId"));nbc_verify_journal_authority(record,state);
    // The native effect queue records causal events before the final report.
    // Its prelaunch recovery must complete first, including durable queue erase.
    if(!native_pending_read("garrison",run).empty()||native_garrison_pending())throw std::runtime_error("private native effect journal must recover before final combat report");
    auto request=nb_object();request.object["actId"]=json_text("act2autochess");request.object["requestId"]=json_text(record.request);
    request.object["type"]=json_text(cncombat::is_boss_round(record.round,record.mode)?"StrongholdPrivateBossBattleFinish":"AutoChessBattleSceneSelfBattleFinishUp");request.object["round"]=nb_integer(record.round);request.object["payload"]=record.payload;
    const auto endpoint=L"/bridge/act2autochess/native-session/"+std::wstring(run.begin(),run.end());
    cnjson::Value accepted;try{accepted=nb_http(endpoint+L"/protocol",&request);}catch(const std::exception&){accepted=nb_http(endpoint+L"/protocol",&request);}
    if(!nb_true(accepted,"ok"))throw std::runtime_error("private captured battle report recovery rejected: "+nb_string(accepted,"error"));
    // The idempotency reply may be an older round-two image. Always consume a
    // fresh full snapshot after successful replay, never launch that old image.
    auto fresh=nb_http(endpoint);const auto& latest=nb_at(fresh,"state");nb_check_state(latest);
    if(!nb_true(fresh,"ok")||nb_string(latest,"runId")!=run||nb_string(latest,"phase")=="battle"||nb_string(latest,"phase")=="boss")throw std::runtime_error("private captured battle recovery acknowledgement missing");
    nbc_verify_journal_authority(record,latest);native_pending_erase("combat",run,raw);response=fresh;
    report("private-native-captured-battle-journal-recovered-before-launch");
}
bool nbc_live_battle(){if(!nb_prepared.load()||!nb_started.load()||!nb_loaded.load())return false;const auto phase=nb_string(nb_state,"phase"),mode=nb_string(nb_state,"modeId");return (phase=="battle"||phase=="boss")&&cncombat::stage_allowed(nb_string(nb_state,"stageId"),mode)&&cncombat::state_allowed(phase,nb_number(nb_state,"round"),mode);}
void nbc_error(const char* step,const std::exception& ex){std::lock_guard<std::recursive_mutex> lock(nb_data_lock);nbc_last_error=std::string(step)+": "+ex.what();nb_error(step,ex);}
bool nbc_clock_owner(void* bridge=nullptr){
    if(!nbc_clock_active||!nbc_live_battle()||!nb_in_scope()||nbc_clock_run!=nb_run_id||nbc_clock_run!=nb_string(nb_state,"runId")||nb_string(nb_state,"activityId")!="act2autochess"||nbc_clock_round!=nb_number(nb_state,"round")||nbc_clock_mode!=nb_string(nb_state,"modeId")||nbc_clock_stage!=nb_string(nb_state,"stageId"))return false;
    auto manager=nb_manager(),centre=nb_centre();return manager&&manager==nbc_clock_manager&&centre&&centre==nbc_clock_centre&&field_object(manager,"dataCenter")==centre&&text(field_object(centre,"<actId>k__BackingField"))=="act2autochess"&&(!bridge||field_object(manager,"dataBridge")==bridge);
}
int64_t nbc_fixed_clock(){TemporaryRoots roots;int64_t value=0;auto fp=roots.retain(invoke(nb_method(0x600fbd5),nullptr));if(!fp||!read(unbox(fp),value)||value<0)throw std::runtime_error("private native logical clock unavailable");return value;}
int32_t nbc_elapsed_seconds(){
    if(nbc_clock_frozen)return nbc_clock_elapsed;
    TemporaryRoots roots;auto now=nbc_fixed_clock();if(now<nbc_clock_start)throw std::runtime_error("private native logical clock moved backwards");auto difference=now-nbc_clock_start;
    auto fp=roots.retain(value_box(cls("Torappu.FP"),&difference));int32_t seconds=0;
    if(!fp||!read(unbox(invoke(nb_method(0x6000324),instance_argument(fp))),seconds)||seconds<nbc_clock_elapsed||seconds>86400)throw std::runtime_error("private native logical elapsed time invalid");
    nbc_clock_elapsed=seconds;return seconds;
}
int32_t nbc_boss_player_hp(int32_t elapsed){
    auto status=field_object(nbc_clock_centre,"battleStatus"),round=field_object(status,"bossBattleStatus");if(!status||!round)throw std::runtime_error("private boss native HUD status missing");
    const auto initial_hp=nb_number(nb_at(nb_state,"player"),"hp"),native_hp=nb_native_int(round,"battleTeamHp"),previous=nbc_clock_hp_display<0?initial_hp:nbc_clock_hp_display;
    // At most 128 issued enemies can decrement the native field in a round;
    // simultaneous escapes can cross zero before the next observed UI tick.
    if(native_hp< -128||native_hp>initial_hp)throw std::runtime_error("private native boss player HP invalid");
    // Original LOCAL repeated escapes decrement this field independently of
    // the timer. Preserve that damage rather than restoring canonical HP.
    nbc_clock_native_hp_loss+=std::max<int32_t>(0,previous-std::max<int32_t>(0,native_hp));
    auto overtime=cntiming::overtime(nbc_clock_timing,elapsed),hp=std::max<int32_t>(0,initial_hp-nbc_clock_native_hp_loss-overtime);
    if(hp!=nbc_clock_hp_display||native_hp!=hp){auto progress=nb_native_int(round,"battleProcess");void* args[]={&hp,&progress};invoke(nb_method(0x60138f9),status,args);nbc_clock_hp_display=hp;}
    return hp;
}
using NBC_CurrentTime=uint64_t(*)(void*,const void*);NBC_CurrentTime nbc_old_current_time=nullptr;
uint64_t nbc_current_time(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return 0;
    if(!nbc_clock_active||nb_projection_depth)return nbc_old_current_time(self,m);bool owner=false;
    try{nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);owner=nbc_clock_owner(self);
        if(owner)return nbc_clock_anchor+cntiming::ui_ticks(nbc_clock_timing,nbc_elapsed_seconds());
    }catch(const std::exception& ex){if(owner){nbc_error("combat-logical-clock",ex);return nbc_clock_anchor+cntiming::ui_ticks(nbc_clock_timing,nbc_clock_elapsed);}}
    // Forward outside the observation try: foreign exceptions retain exactly
    // one original invocation and never trigger a second managed call.
    return nbc_old_current_time(self,m);
}
NB_Void0 nbc_old_start_battle=nullptr;
void nbc_start_battle(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    nbc_old_start_battle(self,m);
    try{if(!nbc_live_battle()||!nb_in_scope()||self!=nb_manager())return;nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);TemporaryRoots roots;
        if(nbc_clock_owner())return; // A duplicate native notification cannot reset this round's elapsed time.
        auto centre=nb_centre();if(!centre||field_object(self,"dataCenter")!=centre||text(field_object(centre,"<actId>k__BackingField"))!="act2autochess")throw std::runtime_error("private native logical clock owner invalid");
        const auto& battle=nb_at(nb_state,"battle");int64_t started=nb_at(battle,"startedAtTs").integer;void* args[]={&started};uint64_t anchor=0,end=0;
        auto time=roots.retain(invoke(nb_method(0x600016d),nullptr,args));auto status=field_object(centre,"battleStatus");
        if(!time||!read(unbox(time),anchor)||!status)throw std::runtime_error("private native logical clock anchor unavailable");
        api.il2cpp_field_get_value(status,profile_field(api.il2cpp_object_get_class(status),"battleNormalEndTime"),&end);
        const auto timing=nbc_timing_policy(nb_state);
        if(end<=anchor||end-anchor!=uint64_t(timing.display_seconds)*10000000ULL)throw std::runtime_error("private native battle duration invalid");
        nbc_clock_start=nbc_fixed_clock();nbc_clock_anchor=anchor;nbc_clock_timing=timing;nbc_clock_duration=timing.simulation_seconds;nbc_clock_round=nb_number(nb_state,"round");nbc_clock_run=nb_run_id;nbc_clock_mode=nb_string(nb_state,"modeId");nbc_clock_stage=nb_string(nb_state,"stageId");nbc_clock_manager=self;nbc_clock_centre=centre;nbc_clock_elapsed=0;nbc_clock_hp_display=-1;nbc_clock_native_hp_loss=0;nbc_clock_frozen=false;nbc_timeout=false;nbc_end_requested=false;
        set_field(status,profile_field(api.il2cpp_object_get_class(status),"battleStartTime"),&anchor);nbc_clock_active=true;report("private-native-round-logical-clock-started");
    }catch(const std::exception& ex){nbc_clock_active=false;nbc_error("combat-logical-clock-start",ex);}
}

void* nbc_serializer(TemporaryRoots& roots){
    // These two methods are pure native projections. Their bodies never read
    // instance fields or send requests. Do not construct/start its network bridge.
    auto object=roots.retain(object_new(cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer")));
    if(!object)throw std::runtime_error("private native projection helper allocation failed");return object;
}
std::vector<cncombat::Position> nbc_board_positions(){
    NB_ProjectionScope projection_scope;PrivateDataScope private_scope(true);TemporaryRoots roots;
    auto dc=nb_centre();auto data=field_object(dc,"battleData");auto map_info=field_object(data,"chessMapInfo");auto map=field_object(map_info,"chessInstMap");
    if(!map)throw std::runtime_error("private native live chess map missing");
    auto values=roots.retain(invoke(method(api.il2cpp_object_get_class(map),"get_Values",0),map));
    auto iterator=values?roots.retain(invoke(method(api.il2cpp_object_get_class(values),"GetEnumerator",0),values)):nullptr;
    if(!iterator)throw std::runtime_error("private native live chess map iterator unavailable");
    auto c=api.il2cpp_object_get_class(iterator);auto move=method(c,"MoveNext",0),current=method(c,"get_Current",0);
    auto serializer=nbc_serializer(roots);auto owned=nbc_owned_chess(),all_owned=nbc_owned_chess(true);
    std::set<int32_t> character_ids,observed_characters,token_owners;
    for(const auto& character:nb_at(nb_at(nb_state,"player"),"charChess").array)character_ids.insert(nb_number(character,"instId"));
    std::vector<cncombat::Position> positions;
    for(unsigned index=0;;++index){uint8_t next=0;if(!read(unbox(invoke(move,instance_argument(iterator))),next))throw std::runtime_error("private native chess iterator result missing");if(!next)break;
        if(index>=128)throw std::runtime_error("private native chess map exceeds bound");auto chess=roots.retain(invoke(current,instance_argument(iterator)));
        if(!chess||nb_native_int(chess,"playerIndex")!=0)throw std::runtime_error("private native chess map owner invalid");
        uint8_t is_token=0;
        if(!read(unbox(invoke(nb_method(0x60138e3),chess)),is_token))throw std::runtime_error("private native token classification unavailable");
        if(is_token){
            // Native TOKEN chess shares its owner's positive instId; its map
            // key is the tile position. The server keeps these derived rows
            // separate from owned inventory and echoes the native layout.
            auto owner=nb_native_int(chess,"instId");uint8_t supported=0;void* token_args[]={&owner};
            if(!character_ids.count(owner)||!read(unbox(invoke(nb_method(0x601351a),nullptr,token_args)),supported)||!supported)
                throw std::runtime_error("private native preparation token owner unsupported");
            token_owners.insert(owner);
        }
        void* args[]={chess};auto value=roots.retain(invoke(nb_method(0x601361c),serializer,args));
        cncombat::Position p{nb_native_int(value,"instId"),nb_native_int(value,"position"),nb_native_int(value,"chessType"),nb_native_int(value,"direction")};
        if(is_token){if(p.chess_type!=3)throw std::runtime_error("private native token projection mismatch");positions.push_back(p);continue;}
        if(!all_owned.count(p.inst_id)||(p.position>0&&!owned.count(p.inst_id)))throw std::runtime_error("private native map contains an unsupported chess identity");
        if(p.chess_type==0&&character_ids.count(p.inst_id))observed_characters.insert(p.inst_id);positions.push_back(p);
    }
    if(auto dispose=method(c,"Dispose",0))invoke(dispose,instance_argument(iterator));
    for(auto owner:token_owners)if(!observed_characters.count(owner))throw std::runtime_error("private native preparation token has no character on the map");
    cncombat::layout_signature(positions);return positions;
}
std::vector<cncombat::Position> nbc_authoritative_positions(){
    const auto& positions=nb_at(nb_at(nb_state,"player"),"positions");if(positions.kind!=cnjson::Value::Array||positions.array.size()>128)throw std::runtime_error("private authoritative positions invalid");
    std::vector<cncombat::Position> result;for(const auto& value:positions.array){const auto& pos=nb_at(value,"position");result.push_back({nb_number(value,"instId"),cncombat::encode_position(nb_number(pos,"x"),nb_number(pos,"y")),0,nb_number(value,"direction")});}
    const auto& hand=nb_at(nb_at(nb_state,"player"),"handPositions");
    if(hand.kind!=cnjson::Value::Null){
        if(hand.kind!=cnjson::Value::Array||hand.array.size()>128)throw std::runtime_error("private authoritative hand positions invalid");
        for(const auto& value:hand.array){auto position=nb_number(value,"position");if(position>0)throw std::runtime_error("private authoritative hand position is deployed");
            result.push_back({nb_number(value,"instId"),position,nb_number(value,"chessType"),nb_number(value,"direction")});}
    }
    auto tokens=nb_state.object.find("nativeTokenPositions");
    if(tokens!=nb_state.object.end()){
        if(tokens->second.kind!=cnjson::Value::Array||tokens->second.array.size()>128)throw std::runtime_error("private authoritative token positions invalid");
        for(const auto& token:tokens->second.array){auto id=nb_number(token,"instId");bool current_owner=false;
            for(const auto& owner:nb_at(nb_at(nb_state,"player"),"charChess").array){
                if(nb_number(owner,"instId")!=id)continue;
                for(const auto& p:result)if(p.inst_id==id&&p.chess_type==0&&p.position==nb_number(token,"ownerPosition"))current_owner=true;
            }
            if(current_owner)result.push_back({id,nb_number(token,"position"),3,nb_number(token,"direction")});
        }
    }return result;
}
int32_t nbc_native_deployment_count(){
    // Match the original MultiPlayer ReqMoveChess/ReqUseMagic projection.
    // DummyManager has already summed each real Character's native volume;
    // TOKEN prefabs can consume zero or one slot independently of chess type.
    TemporaryRoots roots;auto controller=roots.retain(invoke(method(cls("Torappu.Battle.BattleController"),"get_instance",0),nullptr));
    if(!controller)throw std::runtime_error("private native deployment controller missing");
    auto type=api.il2cpp_object_get_class(controller);auto manager=roots.retain(invoke(method(type,"get_unitManager",0),controller));int32_t side=0,count=-1;
    if(!manager||!read(unbox(roots.retain(invoke(method(type,"get_playerSide",0),controller))),side))
        throw std::runtime_error("private native deployment manager or player side missing");
    void* args[]={&side};
    if(!read(unbox(roots.retain(invoke(method(api.il2cpp_object_get_class(manager),"GetPlayerCharacterCnt",1),manager,args))),count)||count<0||count>128)
        throw std::runtime_error("private native deployment volume invalid");
    return count;
}
bool native_combat_sync_board(){
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);if(nb_suppress_outbound())return false;
    if(!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||!cncombat::preparation_resume(nb_string(nb_state,"phase"),nb_number(nb_state,"round"),nb_string(nb_state,"modeId")))
        throw std::runtime_error("private board submission requires supported preparation");
    native_combat_validate_state(nb_state);
    auto positions=nbc_board_positions();
    if(cncombat::layout_signature(positions)==cncombat::layout_signature(nbc_authoritative_positions())){++nbc_board_unchanged;return true;}
    // PREP_FIN has already been committed for this exact recovery checkpoint.
    // Do not silently replace its deployment with a partial native map, or
    // accept a second board transaction while the prepared battle is frozen.
    if(nb_state.object.find("nativeReplayPending")!=nb_state.object.end())
        throw std::runtime_error("private restored preparation layout differs from its saved checkpoint");
    auto units=nb_array();auto deployed=nbc_native_deployment_count();
    for(const auto& p:positions){auto item=nb_object();item.object["instId"]=nb_integer(p.inst_id);item.object["position"]=nb_integer(p.position);item.object["chessType"]=nb_integer(p.chess_type);item.object["direction"]=nb_integer(p.direction);units.array.push_back(item);}
    auto payload=nb_object();payload.object["unitPositionInfo"]=units;payload.object["deployCount"]=nb_integer(deployed);
    nbc_last_board_submission=nb_object();nbc_last_board_submission.object["runId"]=json_text(nb_run_id);nbc_last_board_submission.object["round"]=nb_at(nb_state,"round");nbc_last_board_submission.object["payload"]=payload;
    return nb_send("AutoChessBattleSceneChangePositionUp",payload);
}

using NBC_Escape=void(*)(void*,int32_t,bool,const void*);NBC_Escape nbc_old_escape=nullptr;
using NBC_Killed=void(*)(void*,void*,const void*);NBC_Killed nbc_old_killed=nullptr;
using NBC_Finish=void(*)(void*,int32_t,const void*);NBC_Finish nbc_old_finish=nullptr;
NB_Void0 nbc_old_info=nullptr;
using NBC_Registered=void(*)(void*,void*,const void*);NBC_Registered nbc_old_registered=nullptr;
using NBC_BossHP=void(*)(void*,void*,int32_t,const void*);NBC_BossHP nbc_old_boss_hp=nullptr;
NBC_Registered nbc_old_boss_killed=nullptr,nbc_old_boss_escape=nullptr;
std::string nbc_enemy_template_key(void* enemy,const std::string& actual_key){
    // EnemyDatabase.GetComputedEnemyData retains the requested level key in
    // EnemyData.alias and resolves EnemyData.key to its real prefab. Hidden
    // bosses therefore have e.g. alias *_2 but Entity.id is the base prefab.
    // Validate that native relationship; never infer aliases from suffixes.
    if(actual_key.empty()||actual_key.size()>128)throw std::runtime_error("private native enemy key invalid");
    auto data=field_object(enemy,"<data>k__BackingField");if(!data)return actual_key;
    if(api.il2cpp_object_get_class(data)!=cls("Torappu.LevelData+EnemyData"))throw std::runtime_error("private native enemy data type invalid");
    const auto key=text(field_object(data,"key")),alias=text(field_object(data,"alias"));
    if(key!=actual_key)throw std::runtime_error("private native enemy prefab and data key differ");
    return alias.empty()?key:alias;
}
void nbc_boss_state(void* self,void* enemy,int32_t hp,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    nbc_old_boss_hp(self,enemy,hp,m);
    try{if(!nb_in_scope()||!nbc_live_battle()||nb_string(nb_state,"phase")!="boss")return;nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        if(hp<0)throw std::runtime_error("private native boss HP negative");
        bool registered=nbc_boss_enemy==enemy;
        if(!registered)throw std::runtime_error("private native boss HP from unissued enemy");
        nbc_boss_hp=hp;nbc_boss_enemy=enemy;nbc_boss_observed=true;
        native_broadcast_boss_progress(self);
    }catch(const std::exception& ex){nbc_identity_failed=true;nbc_error("combat-boss-hp",ex);}}
void nbc_registered(void* self,void* enemy,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    nbc_old_registered(self,enemy,m); // Preserve the original Unity event even when observation rejects it.
    NBC_SpawnTrace trace;bool authoritative=false;int32_t observed_id=0;
    try{if(!nb_in_scope()||!nbc_live_battle()||self!=nb_manager())return;trace.manager_matches=true;nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);PrivateDataScope private_scope(true);TemporaryRoots roots;
        trace.run=nb_run_id;trace.round=nb_number(nb_state,"round");trace.phase=nb_string(nb_state,"phase");trace.registered_count=int32_t(nbc_registered_enemies.size());
        const auto& declared_units=nb_at(nb_at(nb_state,"battle"),"units");if(declared_units.kind==cnjson::Value::Array)trace.issued_count=int32_t(declared_units.array.size());
        if(!enemy)throw std::runtime_error("private native registration has no entity");
        // A pooled pointer belongs to its old issued object only while its
        // native generation still matches. A fresh native generation is extra
        // unless it explicitly claims a current issued instId again.
        for(const auto& row:nbc_registered_enemies)if(row.second==enemy){trace.uid=nbc_enemy_uid(enemy);trace.uid_read=true;
            auto generation=nbc_registered_enemy_uids.find(row.first);if(generation!=nbc_registered_enemy_uids.end()&&generation->second&&generation->second==trace.uid){authoritative=true;observed_id=row.first;break;}}
        if(trace.phase=="boss"&&nbc_boss_enemy==enemy)authoritative=true;
        auto options=roots.retain(invoke(nb_method(0x6012a4d),enemy));trace.options_read=true;auto meta=field_object(options,"actionExtraMeta");trace.meta_read=true;trace.meta_present=meta!=nullptr;
        const auto meta_type=trace.phase=="boss"?"Torappu.Battle.AutoChess.BossBattleEnemyInfo":"Torappu.Battle.AutoChess.SelfEnemyInfo";
        trace.expected_meta_type=meta_type;if(meta){trace.meta_type_matches=api.il2cpp_object_get_class(meta)==cls(meta_type);trace.meta_type_read=true;}
        if(trace.meta_type_matches){trace.inst_id=nb_native_int(meta,"instId");
            for(const auto& unit:declared_units.array)if(nb_number(unit,"instId")==trace.inst_id)authoritative=true;
            trace.action_index=nb_native_int(meta,"actionIndex");trace.meta_key=text(field_object(meta,"enemyId"));trace.meta_identity_read=true;}
        trace.actual_key=text(invoke(method(api.il2cpp_object_get_class(enemy),"get_id",0),enemy));trace.key_read=true;const auto& actual_key=trace.actual_key;
        trace.template_key=nbc_enemy_template_key(enemy,actual_key);trace.template_read=true;const auto& template_key=trace.template_key;
        if(observed_id&&(!trace.meta_type_matches||trace.inst_id!=observed_id))throw std::runtime_error("private registered issued object changed authoritative instId");
        if(nb_string(nb_state,"phase")=="boss"){
            void* args[]={roots.retain(string_new(template_key.c_str()))};uint8_t is_boss=0;
            if(!read(unbox(invoke(nb_method(0x601353a),nullptr,args)),is_boss))throw std::runtime_error("private boss classification unavailable");
            const auto& keys=nb_at(nb_at(nb_state,"battle"),"nativeLocalBossKeys");bool expected=false;
            if(keys.kind!=cnjson::Value::Array)throw std::runtime_error("private native boss template missing");
            for(const auto& key:keys.array)if(key.kind==cnjson::Value::String&&key.string==template_key)expected=true;
            if(expected)authoritative=true;
            if(is_boss||expected){
                if(!expected||(nbc_boss_enemy&&nbc_boss_enemy!=enemy))throw std::runtime_error("private native boss differs from current level");
                if(!is_boss)throw std::runtime_error("private expected native boss classification differs");
                // The original local boss action has no server-issued metadata.
                nbc_boss_enemy=enemy;report("private-native-local-boss-observed-in-unity");return;
            }
        }
        if(!meta){
            if(authoritative)throw std::runtime_error("private registered issued enemy lost authoritative action metadata");
            uint8_t summoned=0;api.il2cpp_field_get_value(options,profile_field(api.il2cpp_object_get_class(options),"isSummon"),&summoned);trace.summon_read=true;trace.summoned=summoned!=0;
            auto context=nbc_respawn_context;
            if(!summoned&&context&&nb_string(nb_state,"phase")=="boss"&&nbc_clock_owner()&&context->run==nb_run_id&&context->round==nb_number(nb_state,"round")&&context->key==template_key){
                authoritative=true;
                if(context->spawned&&context->spawned!=enemy)throw std::runtime_error("private native escaped-enemy respawn produced multiple objects");
                context->spawned=enemy;report("private-native-boss-respawn-awaiting-original-metadata");return;
            }
            // This native namespace indexes Entity.id, as the original
            // OnSelfBattleEnemyReachExit path does; it is not an issued alias.
            if(summoned){auto indexer=roots.retain(invoke(nb_method(0x60134e8),nullptr));void* args[]={roots.retain(string_new(actual_key.c_str()))};int32_t identifier=-1;
                if(indexer&&read(unbox(invoke(nb_method(0x60134aa),indexer,args)),identifier)&&identifier>=0){nbc_last_error="combat-extra-enemy-observed: native summoned identifier namespace";return;}
                if(nb_string(nb_state,"phase")=="boss"||nb_string(nb_state,"phase")=="battle"){
                    // The index is only the optional escaped-token protocol
                    // namespace. A genuine native summon can lack it. Require
                    // native EnemyData and its exact observed generation;
                    // issued or scheduled objects cannot use this exception.
                    if(self!=nb_manager())throw std::runtime_error("private unindexed boss summon from a foreign manager");
                    auto data=roots.retain(field_object(enemy,"<data>k__BackingField"));
                    if(!data||api.il2cpp_object_get_class(data)!=cls("Torappu.LevelData+EnemyData"))throw std::runtime_error("private unindexed boss summon native data missing");
                    const auto uid=nbc_enemy_uid(enemy);trace.uid=uid;trace.uid_read=true;if(!uid)throw std::runtime_error("private unindexed boss summon generation missing");
                    for(const auto& row:nbc_registered_enemies)if(row.second==enemy&&nbc_registered_enemy_uids.at(row.first)==uid){authoritative=true;throw std::runtime_error("private issued enemy cannot become an unindexed boss summon");}
                    if(nbc_registered_local_enemies.count(enemy))throw std::runtime_error("private local enemy cannot become an unindexed boss summon");
                    auto found=nbc_unindexed_boss_summons.find(enemy);
                    if(found!=nbc_unindexed_boss_summons.end()){
                        if(found->second.key!=template_key&&found->second.uid==uid)throw std::runtime_error("private unindexed boss summon changed native template");
                        // A new original registration from this manager is the
                        // native pool's generation boundary. It also covers
                        // silent native despawns, which need not emit a kill.
                        if(found->second.uid!=uid){drop_cache("native-combat-boss-summon-"+std::to_string(found->second.uid));found->second={template_key,uid};}
                    }else nbc_unindexed_boss_summons.emplace(enemy,NBC_UnindexedBossSummon{template_key,uid});
                    protect("native-combat-boss-summon-"+std::to_string(uid),enemy);
                    nbc_last_error="combat-extra-enemy-observed: native object and generation namespace";return;
                }}
            else{
                const auto limits=nbc_local_enemy_contract(nb_state);auto expected=limits.find(template_key);
                if(expected!=limits.end()){
                    void* args[]={roots.retain(string_new(template_key.c_str()))};uint8_t boss=0;
                    if(!read(unbox(invoke(nb_method(0x601353a),nullptr,args)),boss)||boss)
                        throw std::runtime_error("private local auxiliary enemy is a native boss");
                    auto previous=nbc_registered_local_enemies.find(enemy);
                    if(previous!=nbc_registered_local_enemies.end()){
                        if(previous->second!=template_key)throw std::runtime_error("private local enemy changed its template identity");
                        return;
                    }
                    const auto observed=std::count_if(nbc_registered_local_enemies.begin(),nbc_registered_local_enemies.end(),[&](const auto& row){return row.second==template_key;});
                    if(observed>=expected->second)throw std::runtime_error("private local enemy exceeds current template count");
                    nbc_registered_local_enemies.emplace(enemy,template_key);
                    // Native Mock.OnBossBattleEnemyKilled/ReachExit returns
                    // on null actionExtraMeta for these non-boss objects.
                    // They never enter protocol killed/escaped integer lists.
                    nbc_last_error="combat-extra-enemy-observed: local auxiliary namespace";return;
                }
            }
        }
        if(!meta||!trace.meta_type_matches)throw std::runtime_error("private spawned enemy lost authoritative action metadata");
        auto id=trace.inst_id,action=trace.action_index;const auto& key=trace.meta_key;
        bool matched=false;
        for(const auto& unit:nb_at(nb_at(nb_state,"battle"),"units").array)
            if(nb_number(unit,"instId")==id&&nb_number(unit,"actionIndex")==action&&nb_string(unit,"enemyId")==key&&template_key==key){matched=true;break;}
        if(!matched)throw std::runtime_error("private spawned enemy identity/template differs from the issued contract");
        const auto uid=nbc_enemy_uid(enemy);trace.uid=uid;trace.uid_read=true;if(!uid)throw std::runtime_error("private native registered enemy generation missing");
        auto found=nbc_registered_enemies.find(id);if(found!=nbc_registered_enemies.end()&&(found->second!=enemy||nbc_registered_enemy_uids.at(id)!=uid))throw std::runtime_error("private enemy identity spawned twice");
        nbc_registered_enemies[id]=enemy;nbc_registered_enemy_uids[id]=uid;protect("native-combat-issued-"+std::to_string(id),enemy);report("private-native-issued-enemy-observed-in-unity");
    }catch(const std::exception& ex){if(authoritative){nbc_identity_failed=true;nbc_record_spawn_failure(trace,ex);nbc_error("combat-spawn-identity",ex);}
        else{try{std::lock_guard<std::recursive_mutex> lock(nb_data_lock);nbc_last_error=std::string("combat-extra-enemy-ignored: ")+ex.what();}catch(...){}}}}
struct NBC_BossOutcomeIdentity{int32_t issued=0,token=-1;};
NBC_BossOutcomeIdentity nbc_boss_outcome_identity(void* enemy,TemporaryRoots& roots){
    bool authoritative=false;try{
    if(!enemy)throw std::runtime_error("private native boss outcome enemy missing");
    if(enemy==nbc_boss_enemy)return {};
    for(const auto& row:nbc_registered_enemies)if(row.second==enemy){auto generation=nbc_registered_enemy_uids.find(row.first);
        if(generation!=nbc_registered_enemy_uids.end()&&generation->second&&generation->second==nbc_enemy_uid(enemy)){authoritative=true;break;}}
    auto options=roots.retain(invoke(nb_method(0x6012a4d),enemy));auto meta=field_object(options,"actionExtraMeta");
    if(meta&&api.il2cpp_object_get_class(meta)==cls("Torappu.Battle.AutoChess.BossBattleEnemyInfo")){const auto id=nb_native_int(meta,"instId");
        for(const auto& row:nb_at(nb_at(nb_state,"battle"),"units").array)if(nb_number(row,"instId")==id)authoritative=true;}
    auto actual_key=text(invoke(method(api.il2cpp_object_get_class(enemy),"get_id",0),enemy));
    auto summon=nbc_unindexed_boss_summons.find(enemy);
    if(!authoritative&&summon!=nbc_unindexed_boss_summons.end()){
        uint8_t summoned=0;api.il2cpp_field_get_value(options,profile_field(api.il2cpp_object_get_class(options),"isSummon"),&summoned);
        if(meta||!summoned||!summon->second.uid||nbc_enemy_uid(enemy)!=summon->second.uid||nbc_enemy_template_key(enemy,actual_key)!=summon->second.key)
            throw std::runtime_error("private unindexed boss summon outcome generation invalid");
        // Preserve a real native escaped-token identifier if one is available;
        // otherwise the original LOCAL callback consumes this non-protocol
        // unit without inventing a killed/escaped instId.
        auto indexer=roots.retain(invoke(nb_method(0x60134e8),nullptr));void* args[]={roots.retain(string_new(actual_key.c_str()))};int32_t id=-1;
        if(indexer&&read(unbox(invoke(nb_method(0x60134aa),indexer,args)),id)&&id>=0)return {0,id};
        return {};
    }
    if(!authoritative&&nbc_registered_local_enemies.count(enemy)){
        if(nbc_registered_local_enemies.at(enemy)!=nbc_enemy_template_key(enemy,actual_key))
            throw std::runtime_error("private local boss outcome changed template");
        return {};
    }
    if(!meta){
        if(authoritative)throw std::runtime_error("private native boss outcome lacks issued metadata");
        uint8_t summoned=0;api.il2cpp_field_get_value(options,profile_field(api.il2cpp_object_get_class(options),"isSummon"),&summoned);
        if(summoned){auto indexer=roots.retain(invoke(nb_method(0x60134e8),nullptr));void* args[]={roots.retain(string_new(actual_key.c_str()))};int32_t id=-1;
            if(indexer&&read(unbox(invoke(nb_method(0x60134aa),indexer,args)),id)&&id>=0)return {0,id};}
        throw std::runtime_error("private native boss outcome lacks issued metadata");
    }
    if(api.il2cpp_object_get_class(meta)!=cls("Torappu.Battle.AutoChess.BossBattleEnemyInfo"))
        throw std::runtime_error("private native boss outcome metadata type invalid");
    auto id=nb_native_int(meta,"instId"),action=nb_native_int(meta,"actionIndex");auto key=text(field_object(meta,"enemyId"));
    auto observed=nbc_registered_enemies.find(id);auto generation=nbc_registered_enemy_uids.find(id);
    if(observed==nbc_registered_enemies.end()||observed->second!=enemy||generation==nbc_registered_enemy_uids.end()||
            !generation->second||nbc_enemy_uid(enemy)!=generation->second||nbc_enemy_template_key(enemy,actual_key)!=key)
        throw std::runtime_error("private native boss outcome object generation invalid");
    bool issued=false;for(const auto& row:nb_at(nb_at(nb_state,"battle"),"units").array)
        if(nb_number(row,"instId")==id&&nb_number(row,"actionIndex")==action&&nb_string(row,"enemyId")==key)issued=true;
    if(!issued)throw std::runtime_error("private native boss outcome differs from current issued template");
    return {id,-1};
    }catch(const std::exception& ex){if(authoritative)throw;try{nbc_last_error=std::string("combat-extra-outcome-ignored: ")+ex.what();}catch(...){}return {};}
}
void nbc_boss_killed_event(void* self,void* enemy,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    bool target=false;
    try{if(nb_in_scope()&&self==nb_manager()&&nbc_live_battle()&&nb_string(nb_state,"phase")=="boss"&&!nb_suppress_outbound()){
        target=true;nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);PrivateDataScope private_scope(true);TemporaryRoots roots;
        if(nbc_end_requested||nbc_pending.load()||nbc_journal_waiting.load())++nbc_late_events;
        else{auto identity=nbc_boss_outcome_identity(enemy,roots);if(identity.issued){
            void* args[]={&identity.issued};cncombat::KillInfo info;
            auto snapshot=roots.retain(invoke(nb_method(0x6013513),nullptr,args));
            if(!snapshot||!read(unbox(snapshot),info)||info.enemy_id!=identity.issued)
                throw std::runtime_error("private native boss killed snapshot identity invalid");
            auto issued=nbc_issued(nb_state),owned=nbc_json_ids(nb_at(nb_at(nb_state,"battle"),"nativeOwnedChessIds"));
            std::vector<int32_t> escaped;std::vector<cncombat::KillInfo> killed{info};std::vector<cncombat::CharacterStatus> characters;
            cncombat::normalize_report(issued,owned,escaped,killed,characters,false);
            auto previous=nbc_boss_killed.find(info.enemy_id);
            if(previous!=nbc_boss_killed.end()&&(previous->second.attacker_id!=info.attacker_id||previous->second.damage_src!=info.damage_src))
                throw std::runtime_error("private native boss duplicate kill attribution conflicts");
            nbc_boss_killed[info.enemy_id]=info;nbc_boss_escaped.erase(info.enemy_id);++nbc_kills;
        }}
    }}catch(const std::exception& ex){if(target){nbc_identity_failed=true;nbc_error("combat-boss-killed",ex);}}
    // Preserve exactly one native event outside observation and its catch.
    nbc_old_boss_killed(self,enemy,m);
}
void nbc_boss_escape_event(void* self,void* enemy,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    bool target=false;
    try{if(nb_in_scope()&&self==nb_manager()&&nbc_live_battle()&&nb_string(nb_state,"phase")=="boss"&&!nb_suppress_outbound()){
        target=true;nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);PrivateDataScope private_scope(true);TemporaryRoots roots;
        if(nbc_end_requested||nbc_pending.load()||nbc_journal_waiting.load())++nbc_late_events;
        else{auto identity=nbc_boss_outcome_identity(enemy,roots);
            if(identity.issued){if(nbc_boss_killed.count(identity.issued))throw std::runtime_error("private native boss killed enemy cannot escape");nbc_boss_escaped.insert(identity.issued);++nbc_escapes;}
            else if(identity.token>=0){nbc_boss_escaped_tokens.insert(identity.token);++nbc_escapes;}
        }
    }}catch(const std::exception& ex){if(target){nbc_identity_failed=true;nbc_error("combat-boss-escape",ex);}}
    // The original may synchronously respawn and replace this enemy generation.
    // Capture the genuine exit before forwarding, without changing its HP loss.
    nbc_old_boss_escape(self,enemy,m);
}
using NBC_Respawn=bool(*)(void*,void*,const void*);NBC_Respawn nbc_old_respawn=nullptr;
bool nbc_respawn(void* self,void* info,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;
    bool target=false;try{nb_main_thread();target=nbc_clock_owner()&&nb_string(nb_state,"phase")=="boss"&&self==field_object(nbc_clock_manager,"levelEnemyManager");}catch(...){}
    if(!target)return nbc_old_respawn(self,info,m);
    try{std::lock_guard<std::recursive_mutex> lock(nb_data_lock);PrivateDataScope private_scope(true);TemporaryRoots roots;roots.retain(info);
        if(nbc_identity_failed.load()||nbc_respawn_context||!info||api.il2cpp_object_get_class(info)!=cls("Torappu.Battle.AutoChess.BossBattleEnemyInfo"))throw std::runtime_error("private native boss respawn context invalid");
        NBC_RespawnContext context;context.id=nb_native_int(info,"instId");context.action=nb_native_int(info,"actionIndex");context.key=text(field_object(info,"enemyId"));context.run=nb_run_id;context.round=nb_number(nb_state,"round");context.meta=info;
        bool issued=false;for(const auto& row:nb_at(nb_at(nb_state,"battle"),"units").array)if(nb_number(row,"instId")==context.id&&nb_number(row,"actionIndex")==context.action&&nb_string(row,"enemyId")==context.key)issued=true;
        auto previous=nbc_registered_enemies.find(context.id);
        if(!issued||previous==nbc_registered_enemies.end()||nbc_respawned_enemies.count(context.id))throw std::runtime_error("private native boss respawn lacks a unique issued predecessor");
        context.previous=previous->second;context.old_uid=nbc_registered_enemy_uids.at(context.id);auto current_uid=nbc_enemy_uid(context.previous);
        auto options=roots.retain(invoke(nb_method(0x6012a4d),context.previous));
        if((current_uid&&current_uid!=context.old_uid)||nb_native_int(context.previous,"<finishReason>k__BackingField")!=1||field_object(options,"actionExtraMeta")!=info)throw std::runtime_error("private native boss respawn predecessor is not the escaped issued generation");
        struct Scope{NBC_RespawnContext* previous;explicit Scope(NBC_RespawnContext& value):previous(nbc_respawn_context){nbc_respawn_context=&value;}~Scope(){nbc_respawn_context=previous;}} scope(context);
        const auto spawned=nbc_old_respawn(self,info,m);
        if(!spawned){if(context.spawned)throw std::runtime_error("private native boss respawn failed after registering an object");return false;}
        if(!nbc_clock_owner()||context.run!=nb_run_id||context.round!=nb_number(nb_state,"round")||!context.spawned||nbc_identity_failed.load())throw std::runtime_error("private native boss respawn did not preserve its exact round");
        auto replacement=roots.retain(context.spawned);auto replacement_options=roots.retain(invoke(nb_method(0x6012a4d),replacement));auto uid=nbc_enemy_uid(replacement);
        auto actual_key=text(invoke(method(api.il2cpp_object_get_class(replacement),"get_id",0),replacement));
        if(!uid||uid==context.old_uid||field_object(replacement_options,"actionExtraMeta")!=info||nbc_enemy_template_key(replacement,actual_key)!=context.key||nb_native_int(info,"instId")!=context.id||nb_native_int(info,"actionIndex")!=context.action||text(field_object(info,"enemyId"))!=context.key)throw std::runtime_error("private native boss respawn original metadata or generation mismatch");
        nbc_registered_enemies[context.id]=replacement;nbc_registered_enemy_uids[context.id]=uid;nbc_respawned_enemies.insert(context.id);protect("native-combat-issued-"+std::to_string(context.id),replacement);report("private-native-boss-escaped-enemy-respawn-verified");return true;
    }catch(const std::exception& ex){nbc_identity_failed=true;nbc_error("combat-boss-respawn",ex);return false;}
}
void nbc_ready(void* self,bool current_ready,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(self)){lease.release();nb_old_prepare(self,current_ready,m);return;}if(nb_suppress_outbound())return;nb_main_thread();
    if(!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||!cncombat::preparation_resume(nb_string(nb_state,"phase"),nb_number(nb_state,"round"),nb_string(nb_state,"modeId")))throw std::runtime_error("private ready requires supported SINGLE preparation");
    native_combat_validate_state(nb_state);
    if(nbc_pending.load()||nbc_journal_waiting.load()||nbc_cleanup_pending.load()||native_garrison_pending())throw std::runtime_error("private prior battle reports must be acknowledged before ready");
    if(current_ready)throw std::runtime_error("private solo cancel-ready is not applicable");
    native_preparation_tokens_preflight();
    native_battle_tutorial_preflight();
    native_battle_buff_templates_preflight();
    native_battle_resources_preflight();
    native_battle_mode_runes_preflight();
    if(!native_combat_sync_board())return;nbc_clear_registered_enemies();nbc_registered_local_enemies.clear();nbc_identity_failed=false;nbc_boss_observed=false;nbc_boss_hp=0;nbc_boss_enemy=nullptr;nbc_clock_active=false;nbc_clock_frozen=false;nbc_timeout=false;nbc_end_requested=false;
    if(cncombat::is_boss_round(nb_number(nb_state,"round"),nb_string(nb_state,"modeId"))){
        PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;
        auto mock=field_object(self,"m_mockBossBattleManager");if(!mock)throw std::runtime_error("private native boss manager missing");
        // Only reset the existing native manager. _StartBattle also randomizes
        // enemy identities and is not part of this authoritative bridge.
        invoke(nb_method(0x60135fd),mock);
    }
    auto payload=nb_object();payload.object["readyType"]=nb_integer(1);
    if(nb_send("AutoChessBattleScenePreparationReadyUp",payload))++nbc_ready_commands;
    }catch(const std::exception& ex){nbc_error("combat-ready",ex);nb_recover_after_error();}}
void nbc_killed(void* self,void* raw,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(self)){lease.release();nbc_old_killed(self,raw,m);return;}if(nb_suppress_outbound())return;nb_main_thread();if(!nbc_live_battle()){++nbc_late_events;return;}
    cncombat::KillInfo killed;if(!read(raw,killed))throw std::runtime_error("private native killed-info pointer unavailable");auto issued=nbc_issued(nb_state),owned=nbc_json_ids(nb_at(nb_at(nb_state,"battle"),"nativeOwnedChessIds"));
    if(!issued.count(killed.enemy_id)||(killed.attacker_id!=-1&&!owned.count(killed.attacker_id))||killed.damage_src<0||killed.damage_src>3)throw std::runtime_error("private native killed identity invalid");++nbc_kills;
    }catch(const std::exception& ex){nbc_error("combat-killed",ex);}}
void nbc_escape(void* self,int32_t enemy_id,bool token,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(self)){lease.release();nbc_old_escape(self,enemy_id,token,m);return;}if(nb_suppress_outbound())return;nb_main_thread();if(!nbc_live_battle()){++nbc_late_events;return;}
    if((token&&enemy_id<0)||(!token&&!nbc_issued(nb_state).count(enemy_id)))throw std::runtime_error("private native escaped identity invalid");++nbc_escapes;
    }catch(const std::exception& ex){nbc_error("combat-escape",ex);}}
void nbc_info(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(self)){lease.release();nbc_old_info(self,m);return;}if(nb_suppress_outbound())return;nb_main_thread();if(nbc_live_battle())++nbc_infos;
    }catch(const std::exception& ex){nbc_error("combat-info",ex);}}

cnjson::Value nbc_final_snapshot(){
    NB_ProjectionScope projection_scope;PrivateDataScope private_scope(true);TemporaryRoots roots;
    auto issued=nbc_issued(nb_state);
    if(nbc_identity_failed.load()||(!nbc_timeout&&nbc_registered_enemies.size()!=issued.size()))throw std::runtime_error("private final report requires every issued enemy observed in Unity");
    for(auto id:issued)if(!nbc_timeout&&!nbc_registered_enemies.count(id))throw std::runtime_error("private final report enemy was not observed in Unity");
    auto status=field_object(field_object(nb_centre(),"battleStatus"),"selfBattleStatus");if(!status)throw std::runtime_error("private native final battle status missing");
    auto tokens=field_object(status,"enemyEscapedTokenInstIds");std::set<int32_t> token_ids;
    for(int32_t i=0,count=nbc_list_count(tokens,4096);i<count;++i){int32_t id=0;if(!read(unbox(nbc_list_item(tokens,i,roots)),id)||id<0)throw std::runtime_error("private native token result identity invalid");token_ids.insert(id);}
    std::vector<int32_t> escaped;auto escaped_list=field_object(status,"enemyEscapedInstIds");auto count=nbc_list_count(escaped_list);
    for(int32_t i=0;i<count;++i){int32_t id=0;if(!read(unbox(nbc_list_item(escaped_list,i,roots)),id))throw std::runtime_error("private native escaped result unreadable");escaped.push_back(id);}
    std::vector<cncombat::KillInfo> killed;auto killed_list=field_object(status,"enemyKilledInfos");count=nbc_list_count(killed_list);
    for(int32_t i=0;i<count;++i){cncombat::KillInfo info;if(!read(unbox(nbc_list_item(killed_list,i,roots)),info))throw std::runtime_error("private native killed result unreadable");killed.push_back(info);}
    auto method_info=nb_method(0x6013624);auto empty=build_value(api.il2cpp_method_get_param(method_info,0),nb_array(),roots);void* args[]={empty.argument};invoke(method_info,nbc_serializer(roots),args);
    std::vector<cncombat::CharacterStatus> characters;count=nbc_list_count(empty.object);
    for(int32_t i=0;i<count;++i){auto value=nbc_list_item(empty.object,i,roots);characters.push_back({nb_native_int(value,"instId"),nb_native_int(value,"hp"),nb_native_int(value,"tech")});}
    if(nbc_timeout){
        if(!nbc_clock_owner()||nbc_elapsed_seconds()<nbc_clock_duration)throw std::runtime_error("private timeout lacks an elapsed native round clock");
        cncombat::normalize_report(issued,nbc_json_ids(nb_at(nb_at(nb_state,"battle"),"nativeOwnedChessIds")),escaped,killed,characters,false);
        std::set<int32_t> resolved;
        for(auto id:escaped){if(!nbc_registered_enemies.count(id))throw std::runtime_error("private native timed battle escaped enemy was not observed in Unity");resolved.insert(id);}
        for(const auto& k:killed){if(!nbc_registered_enemies.count(k.enemy_id))throw std::runtime_error("private native timed battle killed enemy was not observed in Unity");resolved.insert(k.enemy_id);}
        // Time expiry resolves every remaining authoritative enemy, including
        // scheduler actions that have not spawned; it invents no kill credit.
        for(auto id:issued)if(!resolved.count(id))escaped.push_back(id);
    }
    cncombat::normalize_report(issued,nbc_json_ids(nb_at(nb_at(nb_state,"battle"),"nativeOwnedChessIds")),escaped,killed,characters);
    auto payload=nb_object();payload.object["round"]=nb_at(nb_state,"round");payload.object["fromRejoin"]=nb_boolean(false);payload.object["escapedInstIds"]=nb_array();payload.object["escapedTokenInstIds"]=nb_array();payload.object["killedInsts"]=nb_array();payload.object["charBattleStatusList"]=nb_array();
    for(auto id:token_ids)payload.object["escapedTokenInstIds"].array.push_back(nb_integer(id));
    for(auto id:escaped)payload.object["escapedInstIds"].array.push_back(nb_integer(id));
    for(const auto& k:killed){auto item=nb_object();item.object["enemyInstId"]=nb_integer(k.enemy_id);item.object["attackerInstId"]=nb_integer(k.attacker_id);item.object["damageSrc"]=nb_integer(k.damage_src);payload.object["killedInsts"].array.push_back(item);}
    for(const auto& c:characters){auto item=nb_object();item.object["instId"]=nb_integer(c.inst_id);item.object["hp"]=nb_integer(c.hp);item.object["tech"]=nb_integer(c.tech);payload.object["charBattleStatusList"].array.push_back(item);}return payload;
}
cnjson::Value nbc_boss_snapshot(int32_t result){
    // A lethal native hit may cross all milestones and synchronously enter
    // this capture before the next frame. Queue the native panel messages
    // while the same boss run/round still owns the scene.
    native_broadcast_tick();
    NB_ProjectionScope projection_scope;PrivateDataScope private_scope(true);TemporaryRoots roots;
    if(nbc_identity_failed.load()||!nbc_boss_enemy)throw std::runtime_error("private boss result requires an observed native boss");
    // A defeat without any damage still has a real native entity HP. Invoke
    // the boxed FP method through IL2CPP; no fixed-point ABI is guessed.
    if(!nbc_boss_observed.load()){
        auto hp=roots.retain(invoke(nb_method(0x6010383),nbc_boss_enemy));
        if(!hp||!read(unbox(invoke(nb_method(0x6000324),instance_argument(hp))),nbc_boss_hp)||nbc_boss_hp<0)
            throw std::runtime_error("private native final boss HP unavailable");
    }
    if(result!=1&&result!=2)throw std::runtime_error("private native final boss game result invalid");
    if(result==1&&nbc_boss_hp>1)throw std::runtime_error("private native boss victory requires defeated HP");
    auto m=nb_method(0x6013624);auto list=build_value(api.il2cpp_method_get_param(m,0),nb_array(),roots);void* args[]={list.argument};invoke(m,nbc_serializer(roots),args);
    auto payload=nb_object();payload.object["round"]=nb_at(nb_state,"round");payload.object["result"]=json_text(result==1?"victory":"defeat");payload.object["bossHp"]=nb_integer(nbc_boss_hp);payload.object["bossBroadcastMask"]=nb_integer(native_broadcast_boss_mask());payload.object["charBattleStatusList"]=nb_array();
    if(nbc_clock_owner())payload.object["playerHp"]=nb_integer(nbc_clock_hp_display);
    auto owned=nbc_json_ids(nb_at(nb_at(nb_state,"battle"),"nativeOwnedChessIds"));std::set<int32_t> seen;
    for(int32_t i=0,count=nbc_list_count(list.object);i<count;++i){auto value=nbc_list_item(list.object,i,roots);auto id=nb_native_int(value,"instId"),hp=nb_native_int(value,"hp"),tech=nb_native_int(value,"tech");
        if(!owned.count(id)||!seen.insert(id).second||hp<0||hp>100||tech<0||tech>100)throw std::runtime_error("private native boss character status invalid");
        auto item=nb_object();item.object["instId"]=nb_integer(id);item.object["hp"]=nb_integer(hp);item.object["tech"]=nb_integer(tech);payload.object["charBattleStatusList"].array.push_back(item);}
    std::sort(payload.object["charBattleStatusList"].array.begin(),payload.object["charBattleStatusList"].array.end(),[](const cnjson::Value& a,const cnjson::Value& b){return nb_number(a,"instId")<nb_number(b,"instId");});
    // Native boss death can finish before all side enemies spawn or die.
    // Preserve actual issued side-enemy outcomes for server item rewards;
    // the local boss and native summoned enemies have separate identities.
    auto issued=nbc_issued(nb_state);std::vector<int32_t> escaped;std::vector<cncombat::KillInfo> killed;std::vector<cncombat::CharacterStatus> unused;
    for(auto id:nbc_boss_escaped)escaped.push_back(id);
    for(const auto& row:nbc_boss_killed)killed.push_back(row.second);
    cncombat::normalize_report(issued,owned,escaped,killed,unused,false);
    for(auto id:escaped)if(!nbc_registered_enemies.count(id))throw std::runtime_error("private native boss escaped enemy was not observed in Unity");
    for(const auto& k:killed)if(!nbc_registered_enemies.count(k.enemy_id))throw std::runtime_error("private native boss killed enemy was not observed in Unity");
    payload.object["escapedInstIds"]=nb_array();payload.object["escapedTokenInstIds"]=nb_array();payload.object["killedInsts"]=nb_array();
    for(auto id:nbc_boss_escaped_tokens)payload.object["escapedTokenInstIds"].array.push_back(nb_integer(id));
    for(auto id:escaped)payload.object["escapedInstIds"].array.push_back(nb_integer(id));
    for(const auto& k:killed){auto item=nb_object();item.object["enemyInstId"]=nb_integer(k.enemy_id);item.object["attackerInstId"]=nb_integer(k.attacker_id);item.object["damageSrc"]=nb_integer(k.damage_src);payload.object["killedInsts"].array.push_back(item);}
    return payload;
}
void nbc_finish(void* self,int32_t result,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(self)){lease.release();nbc_old_finish(self,result,m);return;}if(nb_suppress_outbound())return;nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    if(nbc_pending.load()||nbc_journal_waiting.load())return;if(!nbc_live_battle()){++nbc_late_events;return;}
    if(nbc_clock_owner()){
        const bool was_frozen=nbc_clock_frozen;const auto elapsed=nbc_elapsed_seconds();nbc_clock_frozen=true;
        // Unity has already ended. Freeze UI, overtime and the report timestamp
        // before any projection, journal or HTTP boundary can fail and retry.
        if(!nbc_end_requested){nbc_end_requested=true;nbc_end_result=result;}
        if(nb_string(nb_state,"phase")=="boss"){if(!was_frozen||nbc_clock_hp_display<0)nbc_boss_player_hp(elapsed);}
        else if(elapsed>=nbc_clock_duration)nbc_timeout=true;
    }
    native_battle_mode_runes_require_applied();
    auto payload=nb_string(nb_state,"phase")=="boss"?nbc_boss_snapshot(result):nbc_final_snapshot();
    const auto attempt=nbc_current_attempt();if(attempt)payload.object["battleAttempt"]=nb_integer(attempt);
    if(nbc_clock_owner())payload.object["elapsedSeconds"]=nb_integer(nbc_elapsed_seconds());
    auto mode=nb_string(nb_state,"modeId"),stage=nb_string(nb_state,"stageId");
    auto bytes=cnpj::encode(nb_run_id,CN_METADATA_HASH,native_pending_game_assembly_hash(),nbc_issued(nb_state),nbc_json_ids(nb_at(nb_at(nb_state,"battle"),"nativeOwnedChessIds")),payload,mode,stage,attempt);
    nbc_pending_payload=payload;nbc_pending_run=nb_run_id;nbc_pending_mode=mode;nbc_pending_stage=stage;nbc_pending_round=nb_number(payload,"round");nbc_pending_request=cnpj::request_id(nb_run_id,nbc_pending_round,mode,stage,attempt);nbc_journal_bytes=bytes;nbc_attempts=0;nbc_last_attempt=0;nbc_journal_waiting=true;
    nbc_persist_captured();
    report("private-native-battle-report-captured-awaiting-ui-tick");
    }catch(const std::exception& ex){nbc_error("combat-finish-snapshot",ex);}}
NB_Void0 nbc_old_settle_task=nullptr;
using NBC_SettleData=void*(*)(void*,void*,const void*);NBC_SettleData nbc_old_settle_data=nullptr;
// Optional home-page result projection. It owns a separate exact Params
// identity and never restores a battle or changes this round's report state.
using NBC_RestoreResult=void*(*)(void*,void*);NBC_RestoreResult nbc_restore_result=nullptr;
using NBC_ResultPresented=void(*)(const std::string&,int32_t,int32_t);NBC_ResultPresented nbc_result_presented=nullptr;
void nbc_capture_settlement_snapshot(){
    // The prepare model already owns a cached native UID. Copy it while this
    // exact acknowledged run still owns the scene, before LOCAL Finish clears
    // its bridge and before the original training loader loses its team data.
    TemporaryRoots roots;auto prepare=nb_retained("native-private-prepare-model");
    if(!prepare||npb_model_run!=nbc_terminal_ack_run||api.il2cpp_object_get_class(prepare)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||
       text(field_object(prepare,"<actId>k__BackingField"))!="act2autochess")
        throw std::runtime_error("private settlement acknowledged prepare identity unavailable");
    auto uid=text(roots.retain(invoke(nb_method(0x602b490),prepare)));
    if(uid.empty()||uid.size()>128)throw std::runtime_error("private settlement cached self UID missing");
    auto summary=nb_settle_game_info;auto& rows=summary.object.at("teamInfo");
    if(rows.kind!=cnjson::Value::Array||rows.array.size()!=1||rows.array[0].kind!=cnjson::Value::Object)
        throw std::runtime_error("private settlement acknowledged team count invalid");
    rows.array[0].object["uid"]=json_text(uid);
    const auto attempt=nbc_state_attempt(nb_state,nbc_terminal_ack_round);
    if(attempt!=nbc_record_attempt(nbc_pending_payload))throw std::runtime_error("private settlement acknowledged battle attempt changed");
    NBC_SettleProof proof{nbc_terminal_ack_run,nbc_terminal_ack_request,uid,nbc_terminal_ack_revision,nbc_terminal_ack_round,attempt};
    nbc_settle_snapshot=std::move(summary);nbc_settle_proof=std::move(proof);nbc_settle_summary_ready=true;
}
void* nbc_settle_data(void* self,void* activity,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;
    if(nbc_restore_result)try{if(auto restored=nbc_restore_result(self,activity))return restored;}
        catch(const std::exception& ex){nbc_error("combat-durable-result-summary",ex);return nullptr;}
    if(text(activity)!="act2autochess"||(!nbc_settle_summary_ready.load()&&nbc_settle_proof.run.empty())){lease.release();return nbc_old_settle_data(self,activity,m);}
    try{nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);PrivateDataScope private_scope(true);TemporaryRoots roots;
        if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessSettleGameState")||
           nbc_settle_proof.run.empty()||nbc_settle_proof.request.empty()||nbc_settle_proof.revision<1||
           nbc_settle_proof.round<1||nbc_settle_proof.uid.empty())throw std::runtime_error("private settlement acknowledged snapshot unavailable");
        auto page=roots.retain(invoke(nb_method(0x601adc8),self));
        if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage"))throw std::runtime_error("private settlement native page unavailable");
        auto args=roots.retain(invoke(nb_method(0x601ac23),page));
        // The pinned LOCAL recovery-page maker preserves native TRAINING=0
        // and derives source=2. The independent server run is still SINGLE;
        // the separate GET-backed result entry owns its explicit 1/2 Params.
        if(!args||api.il2cpp_object_get_class(args)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage+Params")||
           text(field_object(args,"actId"))!="act2autochess"||nb_native_int(args,"modeType")!=0||nb_native_int(args,"fromBattleSource")!=2)
            throw std::runtime_error("private settlement native page does not own the acknowledged handoff");
        auto controller=field_object(page,"_controller"),model=field_object(controller,"m_model");
        if(!controller||!model||api.il2cpp_object_get_class(model)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||
           text(roots.retain(invoke(nb_method(0x602b490),model)))!=nbc_settle_proof.uid)
            throw std::runtime_error("private settlement native page self UID changed");
        auto consumed_args=nb_retained("native-private-settlement-params");
        if(consumed_args){
            auto consumed=nb_retained("native-private-settlement");
            if(consumed_args!=args||!consumed)throw std::runtime_error("private settlement snapshot belongs to another native page");
            return consumed;
        }
        if(!nbc_settle_summary_ready.load()||nbc_settle_building)throw std::runtime_error("private settlement snapshot conversion already in progress");
        struct Building{Building(){nbc_settle_building=true;}~Building(){nbc_settle_building=false;}}building;
        const auto proof=nbc_settle_proof;
        auto summary=nbc_settle_snapshot;auto& rows=summary.object.at("teamInfo");
        if(rows.kind!=cnjson::Value::Array||rows.array.size()!=1||nb_string(rows.array[0],"uid")!=nbc_settle_proof.uid)
            throw std::runtime_error("private settlement immutable team identity changed");
        native_broadcast_project_settlement(summary,proof.uid);
        auto type=api.il2cpp_method_get_return_type(nb_method(0x602b79f));auto result=build_value(type,summary,roots).object;
        if(!result)throw std::runtime_error("private settlement DTO conversion returned no object");
        if(nbc_settle_proof.run!=proof.run||nbc_settle_proof.request!=proof.request||nbc_settle_proof.revision!=proof.revision||
           nbc_settle_proof.round!=proof.round||nbc_settle_proof.attempt!=proof.attempt||nbc_settle_proof.uid!=proof.uid)
            throw std::runtime_error("private settlement handoff changed during native DTO conversion");
        protect("native-private-settlement",result);protect("native-private-settlement-params",args);nbc_settle_summary_ready=false;
        if(nbc_result_presented)nbc_result_presented(proof.run,proof.revision,proof.attempt);
        report("private-native-run-settlement-summary-projected");return result;
    }catch(const std::exception& ex){nbc_error("combat-settlement-summary",ex);return nullptr;}
}
void nbc_settle_task(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(native_multiplayer_entry_active()){lease.release();nbc_old_settle_task(self,m);return;}
    if(!nb_in_scope()){lease.release();nbc_old_settle_task(self,m);return;}nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    if(!nb_prepared.load()||!nb_started.load()||nb_string(nb_state,"phase")!="result")throw std::runtime_error("private settlement requires confirmed terminal state");
    nb_at(nb_state,"result");nbc_settle_run=nb_run_id;nbc_settle_pending=true;
    // Native training GameSettle hardcodes a win. Defer the real native exit
    // until the immutable report is acknowledged and its journal is cleared.
    report("private-native-settlement-deferred-until-report-ack");
    }catch(const std::exception& ex){nbc_error("combat-settlement",ex);}}
void nbc_tick_round_end(){
    if(!nbc_clock_active||closing.load()||nb_projection_depth||nb_command_in_flight||nbc_pending.load()||nbc_journal_waiting.load()||nbc_cleanup_pending.load())return;
    try{nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);if(!nbc_clock_owner()||nbc_identity_failed.load())return;
        const auto elapsed=nbc_elapsed_seconds();const bool boss=nb_string(nb_state,"phase")=="boss";int32_t result=0;
        if(!nbc_end_requested){if(boss){
            // The primary native boss decides victory. Unborn or living side
            // enemies do not postpone it or become fabricated killed units.
            if(nbc_boss_enemy&&nbc_boss_observed.load()&&nbc_boss_hp<=1)result=1;
            auto hp=nbc_boss_player_hp(elapsed);
            if(!result&&hp==0)result=2;
        }else if(elapsed>=nbc_clock_duration){nbc_timeout=true;result=2;}}
        if(!result&&!nbc_end_requested)return;
        native_battle_mode_runes_require_applied();
        if(!nbc_end_requested){nbc_clock_frozen=true;nbc_end_requested=true;nbc_end_result=result;void* args[]={&result};invoke(nb_method(0x601369c),nbc_clock_manager,args);}
        // The original AutoChess mode reaches ReqBattleFinish synchronously.
        // A recoverable projection/journal failure can retry the immutable
        // capture without dispatching OnBattleFinish or finish animation twice.
        if(!nbc_pending.load()&&!nbc_journal_waiting.load())nbc_finish(field_object(nbc_clock_manager,"dataBridge"),nbc_end_result,nullptr);
    }catch(const std::exception& ex){nbc_error("combat-native-round-end",ex);}
}
void native_combat_tick(){
    native_broadcast_tick();
    nbc_tick_round_end();
    if((!nbc_pending.load()&&!nbc_journal_waiting.load()&&!nbc_cleanup_pending.load()&&!nbc_settle_pending.load())||native_garrison_pending()||nb_projection_depth||nb_command_in_flight||closing.load())return;
    try{nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        if(nbc_settle_pending.load()&&!nbc_pending.load()&&!nbc_journal_waiting.load()&&!nbc_cleanup_pending.load()){
            if(nbc_settle_run!=nb_run_id||nb_string(nb_state,"phase")!="result")throw std::runtime_error("private pending settlement run changed");
            const auto& result=nb_at(nb_state,"result");const auto& success=nb_at(result,"success");
            if(success.kind!=cnjson::Value::Boolean)throw std::runtime_error("private settlement success missing");
            uint8_t win=success.boolean;auto manager=nb_manager();if(!manager)throw std::runtime_error("private settlement native manager missing");
            if(nb_settle_game_info.kind!=cnjson::Value::Object)throw std::runtime_error("private run settlement summary missing");
            if(nbc_terminal_ack_run!=nb_run_id||nbc_terminal_ack_request.empty()||
               nbc_terminal_ack_revision!=nb_number(nb_state,"revision")||nbc_terminal_ack_round!=nb_number(nb_state,"round"))
                throw std::runtime_error("private terminal scene exit requires the exact acknowledged final report");
            nbc_capture_settlement_snapshot();
            native_solo_terminal_finish_proof(nbc_terminal_ack_run,nbc_terminal_ack_request,nbc_terminal_ack_revision);
            void* args[]={&win};nbc_settle_pending=false;invoke(nb_method(0x6013699),manager,args);return;
        }
        if(!nb_prepared.load()||!nb_started.load()||nbc_pending_run!=nb_run_id||nbc_pending_mode!=nb_string(nb_state,"modeId")||nbc_pending_stage!=nb_string(nb_state,"stageId"))throw std::runtime_error("private pending battle report run/mode/map changed");
        if(nbc_last_attempt&&GetTickCount64()-nbc_last_attempt<cnpj::retry_delay(nbc_attempts))return;
        nbc_last_attempt=GetTickCount64();cnpj::attempted(nbc_attempts);
        if(nbc_cleanup_pending.load()){native_pending_erase("combat",nbc_pending_run,nbc_journal_bytes);nbc_cleanup_pending=false;return;}
        if(nbc_journal_waiting.load())nbc_persist_captured();
        if(nb_send(cncombat::is_boss_round(nbc_pending_round,nbc_pending_mode)?"StrongholdPrivateBossBattleFinish":"AutoChessBattleSceneSelfBattleFinishUp",nbc_pending_payload,nbc_pending_request.c_str(),nbc_pending_round)){
            if(nb_string(nb_state,"phase")=="result"){
                if(nb_run_id!=nbc_pending_run||nb_string(nb_state,"runId")!=nbc_pending_run||nb_number(nb_state,"round")!=nbc_pending_round)
                    throw std::runtime_error("private terminal acknowledgement changed final report identity");
                nbc_terminal_ack_run=nbc_pending_run;nbc_terminal_ack_request=nbc_pending_request;
                nbc_terminal_ack_revision=nb_number(nb_state,"revision");nbc_terminal_ack_round=nbc_pending_round;
            }
            nbc_pending=false;++nbc_reports;nbc_cleanup_pending=true;
            report("private-native-battle-report-accepted-next-phase-unverified");
            native_pending_erase("combat",nbc_pending_run,nbc_journal_bytes);nbc_cleanup_pending=false;
        }
        else throw std::runtime_error("private battle report rejected; captured report retained");
    }catch(const std::exception& ex){nbc_error("combat-finish-send",ex);}
}
cnjson::Value native_combat_status(bool details){
    auto value=nb_object();value.object["firstNormalOnly"]=nb_boolean(false);value.object["introductoryRun"]=nb_boolean(false);value.object["singleModes"]=nb_integer(4);value.object["pendingReport"]=nb_boolean(nbc_pending.load());value.object["readyCommands"]=nb_integer(nbc_ready_commands.load());value.object["killCallbacks"]=nb_integer(nbc_kills.load());value.object["escapeCallbacks"]=nb_integer(nbc_escapes.load());value.object["infoCallbacks"]=nb_integer(nbc_infos.load());value.object["acceptedReports"]=nb_integer(nbc_reports.load());value.object["unchangedBoards"]=nb_integer(nbc_board_unchanged.load());value.object["lateCallbacks"]=nb_integer(nbc_late_events.load());
    value.object["spawnIdentityFailed"]=nb_boolean(nbc_identity_failed.load());
    value.object["firstIdentityFailure"]=cnjson::Value{};
    if(details&&nbc_first_identity_failure.recorded){const auto& failure=nbc_first_identity_failure;const auto& trace=failure.trace;auto first=nb_object();
        first.object["step"]=json_text(failure.step);first.object["error"]=json_text(failure.error);first.object["runId"]=json_text(trace.run);first.object["round"]=nb_integer(trace.round);first.object["phase"]=json_text(trace.phase);
        first.object["managerMatches"]=nb_boolean(trace.manager_matches);first.object["issuedCount"]=nb_integer(trace.issued_count);first.object["registeredCount"]=nb_integer(trace.registered_count);
        first.object["actualKeyRead"]=nb_boolean(trace.key_read);first.object["actualKey"]=json_text(trace.actual_key);first.object["templateKeyRead"]=nb_boolean(trace.template_read);first.object["templateKey"]=json_text(trace.template_key);
        first.object["optionsRead"]=nb_boolean(trace.options_read);first.object["isSummonRead"]=nb_boolean(trace.summon_read);first.object["isSummon"]=nb_boolean(trace.summoned);first.object["metadataRead"]=nb_boolean(trace.meta_read);first.object["metadataPresent"]=nb_boolean(trace.meta_present);
        first.object["expectedMetadataType"]=json_text(trace.expected_meta_type);first.object["metadataTypeRead"]=nb_boolean(trace.meta_type_read);first.object["metadataTypeMatches"]=nb_boolean(trace.meta_type_matches);first.object["metadataIdentityRead"]=nb_boolean(trace.meta_identity_read);
        first.object["instId"]=nb_integer(trace.inst_id);first.object["actionIndex"]=nb_integer(trace.action_index);first.object["metadataEnemyId"]=json_text(trace.meta_key);first.object["instanceUidRead"]=nb_boolean(trace.uid_read);first.object["instanceUid"]=nb_integer(trace.uid);value.object["firstIdentityFailure"]=std::move(first);
    }
    value.object["logicalClockActive"]=nb_boolean(nbc_clock_active);value.object["elapsedSeconds"]=nb_integer(nbc_clock_elapsed);value.object["normalDurationSeconds"]=nb_integer(nbc_clock_duration);value.object["nativeRoundEndRequested"]=nb_boolean(nbc_end_requested);
    if(details){value.object["registeredLocalAuxiliaries"]=nb_integer(nbc_registered_local_enemies.size());value.object["verifiedBossRespawns"]=nb_integer(nbc_respawned_enemies.size());value.object["nativeBossRegistered"]=nb_boolean(nbc_boss_enemy!=nullptr);value.object["nativeBossHp"]=nb_integer(nbc_boss_observed.load()?nbc_boss_hp:-1);value.object["bossOutcomeKills"]=nb_integer(nbc_boss_killed.size());value.object["bossOutcomeEscapes"]=nb_integer(nbc_boss_escaped.size());value.object["bossOutcomeSource"]=json_text("current-round-native-boss-events");}
    value.object["resourcePreflight"]=native_battle_resources_status(details);
    value.object["buffTemplates"]=native_battle_buff_templates_status(details);
    value.object["tutorial"]=native_battle_tutorial_status(details);
    value.object["modeRunes"]=native_battle_mode_runes_status(details);
    value.object["journalWritePending"]=nb_boolean(nbc_journal_waiting.load());value.object["journalCleanupPending"]=nb_boolean(nbc_cleanup_pending.load());
    if(details){value.object["registeredIssuedIds"]=nb_array();for(const auto& item:nbc_registered_enemies)value.object["registeredIssuedIds"].array.push_back(nb_integer(item.first));value.object["reportAttempts"]=nb_integer(nbc_attempts);value.object["lastError"]=json_text(nbc_last_error);value.object["lastBoardSubmission"]=nbc_last_board_submission;}return value;
}
void native_combat_setup(){
    nb_hook(0x60135cd,nbc_ready,nb_old_prepare);nb_hook(0x60135d0,nbc_info,nbc_old_info);nb_hook(0x60135d1,nbc_escape,nbc_old_escape);nb_hook(0x60135d2,nbc_killed,nbc_old_killed);nb_hook(0x60135d9,nbc_finish,nbc_old_finish);
    nb_hook(0x60136a1,nbc_registered,nbc_old_registered);
    nb_hook(0x60136a5,nbc_boss_killed_event,nbc_old_boss_killed);
    nb_hook(0x60136a6,nbc_boss_escape_event,nbc_old_boss_escape);
    nb_hook(0x60138f8,nbc_boss_state,nbc_old_boss_hp);
    nb_hook(0x60136bc,nbc_start_battle,nbc_old_start_battle);
    nb_hook(0x60135c1,nbc_current_time,nbc_old_current_time);
    nb_hook(0x6013844,nbc_respawn,nbc_old_respawn);
    nb_hook(0x6013d51,nbc_settle_task,nbc_old_settle_task);
    nb_hook(0x602b79f,nbc_settle_data,nbc_old_settle_data);
    auto result_enum=cls("Torappu.Battle.BattleController+GameResult");
    for(const auto& value:std::initializer_list<std::pair<const char*,int32_t>>{{"NOT_YET",0},{"WIN",1},{"LOSE",2}}){int32_t actual=-1;api.il2cpp_field_static_get_value(profile_field(result_enum,value.first),&actual);if(actual!=value.second)throw std::runtime_error("private native GameResult enum mismatch");}
    int32_t reach_exit=-1;api.il2cpp_field_static_get_value(profile_field(cls("Torappu.Battle.Entity+FinishReason"),"REACH_EXIT"),&reach_exit);if(reach_exit!=1)throw std::runtime_error("private native escaped-enemy finish reason ABI mismatch");
    auto killed=cls("Torappu.Battle.AutoChess.BattleEnemyKilledInfo");uint32_t alignment=0;
    if(!killed||!api.il2cpp_class_is_valuetype(killed)||api.il2cpp_class_value_size(killed,&alignment)!=12)throw std::runtime_error("private native killed-info ABI mismatch");
    auto header=api.il2cpp_object_header_size();
    for(const auto& field:std::initializer_list<std::pair<const char*,size_t>>{{"enemyInstId",0},{"attackerInstId",4},{"damageSrc",8}})
        if(api.il2cpp_field_get_offset(profile_field(killed,field.first))!=header+field.second)throw std::runtime_error("private native killed-info field layout mismatch");
    auto options=cls("Torappu.Battle.Enemy+Options");uint32_t options_alignment=0;
    if(!options||!api.il2cpp_class_is_valuetype(options)||api.il2cpp_class_value_size(options,&options_alignment)!=48||api.il2cpp_field_get_offset(profile_field(options,"actionExtraMeta"))!=header+24||api.il2cpp_field_get_offset(profile_field(options,"isSummon"))!=header+8)
        throw std::runtime_error("private native enemy options metadata layout mismatch");
    for(const char* name:{"Torappu.FP","System.DateTime"}){auto type=cls(name);uint32_t align=0;if(!type||!api.il2cpp_class_is_valuetype(type)||api.il2cpp_class_value_size(type,&align)!=8)throw std::runtime_error("private native logical-clock value ABI mismatch");}
    if(api.il2cpp_field_get_offset(profile_field(cls("System.DateTime"),"_dateData"))!=header)throw std::runtime_error("private native DateTime ticks layout mismatch");
}
