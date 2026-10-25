// Private SINGLE save-and-leave. No settlement or official protocol fallback.
#pragma once
inline constexpr CN_Method CN_SOLO_EXIT_MARK={"Torappu.Battle.AutoChess.AutoChessGameManager","PauseSingleModeGame",0x601369a,0xb41910ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_SOLO_EXIT_LEAVE={"Torappu.Battle.AutoChess.AutoChessGameManager","QuitGame",0x6013698,0xb42020ULL,1,false,"System.Void",{"System.Boolean"}};
std::atomic<unsigned> nse_saved{0},nse_rejected{0};
thread_local unsigned nse_native_leave_depth=0;
std::string nse_request_id,nse_request_run;
int32_t nse_request_revision=-1,nse_request_round=-1;
struct NSE_LeaveScope{NSE_LeaveScope(){++nse_native_leave_depth;}~NSE_LeaveScope(){--nse_native_leave_depth;}};
enum class NSE_FinishKind{Saved,Terminal};
struct NSE_FinishProof{std::string run,request;int32_t revision=0;NSE_FinishKind kind=NSE_FinishKind::Saved;};
std::mutex nse_finish_proof_lock;
NSE_FinishProof nse_finish_proof;
void nse_remember_finish_proof(const std::string& run,const std::string& request,int32_t revision,NSE_FinishKind kind=NSE_FinishKind::Saved){
    if(run.size()!=32||request.empty()||request.size()>128||revision<1)throw std::runtime_error("private saved-exit proof is invalid");
    for(char c:run)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')))throw std::runtime_error("private saved-exit run identity is invalid");
    for(char c:request)if(!((c>='0'&&c<='9')||(c>='A'&&c<='Z')||(c>='a'&&c<='z')||c=='-'||c=='_'||c=='.'||c==':'))
        throw std::runtime_error("private saved-exit request identity is invalid");
    std::lock_guard<std::mutex> lock(nse_finish_proof_lock);nse_finish_proof={run,request,revision,kind};
}
void native_solo_terminal_finish_proof(const std::string& run,const std::string& request,int32_t revision){
    nse_remember_finish_proof(run,request,revision,NSE_FinishKind::Terminal);
}
std::string native_solo_exit_finish_url(){
    // Plain copied identifiers only: the native networking thread must never
    // retain or dereference a Unity/IL2CPP object or read mutable nb_state here.
    std::lock_guard<std::mutex> lock(nse_finish_proof_lock);
    if(nse_finish_proof.run.empty())return {};
    return private_backend_origin()+(nbb_participant_profile?"/bridge/act2autochess/multiplayer/solo-native-exit/":"/bridge/act2autochess/native-session/")+nse_finish_proof.run+
        (nse_finish_proof.kind==NSE_FinishKind::Terminal?"/terminal-exit/":"/saved-exit/")+nse_finish_proof.request+"?revision="+std::to_string(nse_finish_proof.revision);
}

void nb_solo_exit(void* self,const void* info,NB_Void0 original){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    // A reentrant native teardown is already owned by the acknowledged leave.
    if(nse_native_leave_depth)return;
    try{
        if(!nb_in_scope(self)){lease.release();original(self,info);return;}
        if(nb_suppress_outbound())return;
        nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        // nb_in_scope established exact private act/bridge ownership. Unsupported
        // private modes must fail closed; their original transport is not safe.
        if(!cncombat::mode_allowed(nb_string(nb_state,"modeId")))throw std::runtime_error("private save-and-leave requires a supported SINGLE mode");
        if(!nb_prepared.load()||!nb_started.load()||!nb_loaded.load())throw std::runtime_error("private save-and-leave requires a loaded session");
        if(nbc_pending.load()||nbc_journal_waiting.load()||nbc_cleanup_pending.load()||nbc_settle_pending.load())
            throw std::runtime_error("private save-and-leave awaits the already captured battle report");
        auto manager=nb_manager();auto centre=nb_centre();
        if(!manager||!centre||field_object(manager,"dataBridge")!=self||field_object(manager,"dataCenter")!=centre)
            throw std::runtime_error("private save-and-leave native owner changed");
        const auto run=nb_run_id;const auto round=nb_number(nb_state,"round");
        const auto revision=nb_number(nb_state,"revision");
        if(nse_request_id.empty()||nse_request_run!=run||nse_request_round!=round||nse_request_revision!=revision){
            nse_request_run=run;nse_request_round=round;nse_request_revision=revision;
            nse_request_id="native-suspend-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++nb_request_sequence);
        }
        auto payload=nb_object();payload.object["expectedRevision"]=nb_at(nb_state,"revision");
        // This private transaction retains preparation/decision state. An exact
        // unstarted battle may stage the existing manual-Ready replay checkpoint;
        // a progressed battle is explicitly rejected without changing its scene.
        // The response contains no PAUSE/SETTLE projection into the native GM.
        // Retain this identity across a double transport failure. A second user
        // click retries the same immutable transaction, including the exact
        // pre-replay revision, rather than inventing a new stale-revision write.
        if(!nb_send("StrongholdPrivateSuspendSession",payload,nse_request_id.c_str())){
            nse_request_id.clear();throw std::runtime_error("private save-and-leave rejected; current scene retained");
        }
        if(nb_run_id!=run||nb_number(nb_state,"round")!=round)throw std::runtime_error("private save-and-leave acknowledgement changed run or round");
        const auto phase=nb_string(nb_state,"phase");
        if(phase!="preparation"&&phase!="special"&&!(phase=="paused"&&nb_string(nb_state,"previousPhase")=="preparation"))
            throw std::runtime_error("private save-and-leave acknowledgement is not a resumable checkpoint");
        nse_remember_finish_proof(run,nse_request_id,nb_number(nb_state,"revision"));
        nse_request_id.clear();
        NSE_LeaveScope leaving;
        // The pinned native function sets isFromUserPause. GetActMeta carries
        // it into GetSceneParam so the original exit returns to the activity.
        // QuitGame(true) performs normal scene teardown; its boolean is local
        // isGiveUp, not a private result. It never calls QuitTrainingGame and
        // never submits a battle report, abandons the save or grants a reward.
        invoke(bound_methods.at(CN_SOLO_EXIT_MARK.token),manager);
        nbc_clock_active=false;
        uint8_t native_give_up=1;void* args[]={&native_give_up};
        invoke(bound_methods.at(CN_SOLO_EXIT_LEAVE.token),manager,args);
        ++nse_saved;report("private-single-session-saved-and-native-scene-left");
    }catch(const std::exception& error){++nse_rejected;nb_error("save-and-leave",error);}
}
void nb_pause(void* self,const void* info){nb_solo_exit(self,info,nb_old_pause);}
void nb_quit(void* self,const void* info){nb_solo_exit(self,info,nb_old_quit);}
void native_solo_exit_setup(){bind_method(CN_SOLO_EXIT_MARK);bind_method(CN_SOLO_EXIT_LEAVE);}
