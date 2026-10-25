#pragma once
#include "native_multiplayer_preparation_view_coordinator.h"
#include <algorithm>
#include <deque>
#include <set>
namespace preparation_view {
struct Session {
 std::string room,run;int64_t match=0;bool party=false;
 bool operator==(const Session& b)const{return room==b.room&&run==b.run&&match==b.match&&party==b.party;}
 bool valid()const{return !room.empty()&&match>0&&(!run.empty()||party);}
};
inline Session session(const Json& raw){const auto phase=cnfield::text(raw,"phase");const auto& state=cnfield::at(raw,"selfState");const bool party=phase=="building"&&cnfield::number(raw,"round")==0&&state.kind==Json::Null;return {cnfield::text(raw,"roomId"),state.kind==Json::Null?std::string{}:cnfield::text(state,"runId"),cnfield::number(raw,"matchSequence"),party};}
enum class Transfer { Preparation,Field,Native,Exit };
struct TransferHost {
 std::function<void()> mainThread,retirePreparation,drainPreparation,releasePreparation,fieldTick;
 std::function<bool()> preparationBusy,fieldLoadBusy,fieldRetryBusy,fieldRestoreBusy,fieldActive,otherLoaderBusy;
 std::function<void()> retireField;
 std::function<std::shared_ptr<void>()> retainCurrentWorld;
 std::function<void(const Json&)> validateFieldBase,applyNative,presentRaw;
 std::function<bool(const Json&)> fieldOwns,fieldAccepted;
 std::function<void(bool,bool)> shutdownRoots;
 std::function<bool(const Session&,uint64_t)> bootstrapCurrent;
 std::function<bool()> bootstrapLaunchIdle;
 std::function<bool()> fieldBasePending;
};
class Handoff {
 TransferHost host;Session held,desiredOwner;Transfer desired=Transfer::Native;Json raw;
 struct Continuation{uint64_t id=0;Session owner;std::function<void()> before;std::function<void(const Json&)> after;std::function<void(const std::string&)> rejected;bool beforeDone=false;};
 std::deque<Continuation> continuations;uint64_t nextContinuation=0;
 struct ExitContinuation{std::string key;std::function<void()> done;};std::deque<ExitContinuation> exits;std::set<std::string> exitKeys;Session exitOwner;
 std::shared_ptr<void> worldRoot;Session completedExitOwner;
 bool waiting=false,prepRetired=false,fieldRetired=false,forwarded=false,terminal=false,executingExit=false,exitFailed=false,finalizationWindow=false;Session exiting;
 uint64_t serial=0,forwardSerial=0;std::string error;
 void retire_once(){if(!prepRetired){host.retirePreparation();prepRetired=true;}}
 static bool compatible(const Session& origin,const Session& latest){return origin==latest||(origin.party&&!latest.party&&origin.room==latest.room&&origin.match==latest.match&&!latest.run.empty());}
 void record_error(const std::string& reason){if(error.empty())error=reason;}
 void reject(Continuation value,const std::string& reason){try{if(value.rejected)value.rejected(reason);}catch(const std::exception& ex){record_error(ex.what());}catch(...){record_error("stale native continuation failed");}}
 bool completed_exit_identity(const Session& origin)const{return origin.valid()&&origin==completedExitOwner&&!held.valid()&&!desiredOwner.valid()&&desired==Transfer::Exit;}
 bool completed_exit_idle()const{return !waiting&&!forwarded&&!worldRoot&&continuations.empty()&&exits.empty()&&raw.kind==Json::Null&&!host.preparationBusy()&&!host.fieldLoadBusy()&&!host.fieldRetryBusy()&&!host.fieldRestoreBusy()&&!host.fieldActive()&&!host.otherLoaderBusy();}
 void run_exits(bool alreadyRetired=false){
  executingExit=true;
  try{while(!exits.empty()){
   // A callback can synchronously accept a new party/run. No remaining
   // acknowledgement for the old, already retired world may enter that owner.
   if(alreadyRetired&&!completed_exit_identity(exiting)){exits.clear();break;}
   auto row=std::move(exits.front());exits.pop_front();if(row.done)row.done();
  }}catch(const std::exception& ex){executingExit=false;exitFailed=true;record_error(ex.what());throw;}
  catch(...){executingExit=false;exitFailed=true;record_error("native exit completion failed");throw;}
  executingExit=false;
 }
 void retain_callbacks_for(const Session& latest){std::deque<Continuation> stale,kept;while(!continuations.empty()){auto row=std::move(continuations.front());continuations.pop_front();(compatible(row.owner,latest)?kept:stale).push_back(std::move(row));}continuations.swap(kept);for(auto& row:stale)reject(std::move(row),"native scene continuation owner changed");}
 bool run_before(const Session& applyingOwner,uint64_t applyingSerial){
  for(;;){auto found=std::find_if(continuations.begin(),continuations.end(),[&](const Continuation& row){return !row.beforeDone&&compatible(row.owner,applyingOwner);});if(found==continuations.end())return true;
   found->beforeDone=true;auto callback=found->before;finalizationWindow=true;
   try{if(callback)callback();}catch(...){finalizationWindow=false;exitFailed=true;throw;}finalizationWindow=false;
   if(serial!=applyingSerial)return false;
  }
 }
 void run_after(const Json& applying,const Session& applyingOwner,uint64_t applyingSerial){
  std::vector<uint64_t> ids;for(const auto& row:continuations)if(row.beforeDone&&compatible(row.owner,applyingOwner))ids.push_back(row.id);
  for(auto id:ids){auto found=std::find_if(continuations.begin(),continuations.end(),[&](const Continuation& row){return row.id==id;});if(found==continuations.end())continue;auto row=std::move(*found);continuations.erase(found);
   try{if(row.after)row.after(applying);}catch(const std::exception& ex){reject(std::move(row),ex.what());}catch(...){reject(std::move(row),"native completion callback failed");}
   if(serial!=applyingSerial)return;
  }
 }
public:
 explicit Handoff(TransferHost value):host(std::move(value)){}
 bool blocked()const{return waiting||terminal;}bool vm_terminal()const{return terminal;}
 const Session& owner()const{return held;}const std::string& failure()const{return error;}
 Transfer intent()const{return desired;}
 bool finalizing()const{return finalizationWindow||executingExit;}
 size_t pending_callbacks()const{return continuations.size();}
 bool may_report_preparation()const{return !blocked();}
 bool may_report_field(const Json& value)const{return !terminal&&forwarded&&forwardSerial==serial&&desired==Transfer::Field&&session(value)==desiredOwner&&host.fieldAccepted(value);}
 void observe_preparation(const Json& value){host.mainThread();if(blocked())throw std::runtime_error("preparation cannot acquire a retiring scene");held=session(value);completedExitOwner={};}
 void submit(const Json& value,Transfer kind,std::function<void()> before={},std::function<void(const Json&)> after={},std::function<void(const std::string&)> rejected={},bool retireScene=true){
  host.mainThread();if(terminal)return;
  auto incoming=session(value);if(!incoming.valid())throw std::runtime_error("scene transfer owner is invalid");
  completedExitOwner={};raw=value;desiredOwner=incoming;desired=kind;++serial;forwarded=false;
  retain_callbacks_for(incoming);
  if(before||after||rejected){if(continuations.size()>=256){if(rejected)rejected("native scene continuation bound");else throw std::runtime_error("native scene continuation bound");}else continuations.push_back({++nextContinuation,incoming,std::move(before),std::move(after),std::move(rejected),false});}
  // The newest HTTP envelope may be presented without touching shared native
  // BattleInfo/NMO/NFF inputs. A native side effect is always deferred below.
  host.presentRaw(value);waiting=true;if(retireScene)retire_once();
 }

 bool claim_initial_owner(const Session& origin,uint64_t epoch){
  host.mainThread();
  if(terminal||exitFailed||!finalizationWindow||executingExit||!waiting||forwarded||held.valid()||!origin.valid()||!compatible(origin,desiredOwner))return false;
  if(!host.bootstrapCurrent||!host.bootstrapLaunchIdle||!host.bootstrapCurrent(origin,epoch)||!host.bootstrapLaunchIdle())return false;
  if(host.preparationBusy()||host.fieldLoadBusy()||host.fieldRetryBusy()||host.fieldRestoreBusy()||host.fieldActive()||host.otherLoaderBusy())return false;
  // Lifecycle owner only. No native DTO, world receipt, ready ACK or battle lease.
  held=origin;completedExitOwner={};return true;
 }
 bool claim_rejoin_owner(const Session& origin,uint64_t epoch){
  host.mainThread();
  // A late envelope can retain only the logical session after Service.Dispose.
  // The caller must prove that disposal completed and no native roots remain.
  // Never transfer a different party/run or bypass a loader/exit barrier.
  if(!(held==origin)||!(desiredOwner==origin)||!origin.valid()||terminal||exitFailed||executingExit||!finalizationWindow||!waiting||forwarded)return false;
  if(!host.bootstrapCurrent||!host.bootstrapLaunchIdle)return false;
  if(!host.bootstrapCurrent(origin,epoch)||!host.bootstrapLaunchIdle())return false;
  if(host.preparationBusy()||host.fieldLoadBusy()||host.fieldRetryBusy()||
     host.fieldRestoreBusy()||host.fieldActive()||host.otherLoaderBusy())return false;
  completedExitOwner={};return true;
 }
 bool request_exit(const Session& origin,std::function<void()> done,const std::string& key="default"){
  host.mainThread();if(terminal||exitFailed)return false;
  const bool live=held.valid()&&origin==held;const bool completed=completed_exit_identity(origin);
  if(!live&&!completed)return false;
  if(key.empty()||key.size()>256)throw std::runtime_error("native retirement completion key invalid");
  // A consumed receipt is idempotent even if unrelated native work became
  // busy. A new receipt requires the exact already-drained owner to be idle.
  if(exitOwner==origin&&exitKeys.count(key))return true;
  if(!live&&!(executingExit&&exiting==origin)&&!completed_exit_idle())return false;
  if(!(exitOwner==origin)){exitOwner=origin;exitKeys.clear();}
  if(exits.size()>=32||exitKeys.size()>=32)throw std::runtime_error("native retirement completion bound");exitKeys.insert(key);exits.push_back({key,std::move(done)});if(executingExit)return true;
  if(completed){
   // Quit already drained and released this world. Its later authenticated
   // Leave/Return only finishes native UI/service cleanup; do not retain or
   // retire the destroyed world again. Reentrant completion remains finalizing.
   exiting=origin;try{run_exits(true);}catch(...){exiting={};throw;}exiting={};return true;
  }
  const bool replacement=waiting&&desiredOwner.valid()&&!compatible(held,desiredOwner);
  if(!replacement){desired=Transfer::Exit;desiredOwner={};raw={};retain_callbacks_for({});}++serial;forwarded=false;waiting=true;retire_once();return true;
 }
 void tick(){
  host.mainThread();if(terminal||exitFailed||!waiting)return;
  try{
   // Neither a timeout nor disappearance of NMO's observer root cancels the
   // original task. The independent adapter lease is the first authority.
   if(host.preparationBusy()){host.drainPreparation();if(host.preparationBusy())return;}
   if(host.fieldLoadBusy())return;
   if(host.fieldRetryBusy()){host.fieldTick();if(host.fieldRetryBusy())return;}
   const bool replacingField=desired!=Transfer::Field||!(held==desiredOwner);
   if(replacingField&&host.fieldActive()&&!fieldRetired){host.retireField();fieldRetired=true;}
   if(host.fieldRestoreBusy()){host.fieldTick();if(host.fieldRestoreBusy())return;}
   if(host.otherLoaderBusy())return;
   // First rejoin must create its original base world before Help composition
   // can validate or retain it. This is a wait, never a ready/map proof.
   if(desired==Transfer::Field&&host.fieldBasePending&&host.fieldBasePending())return;
   if(!worldRoot)worldRoot=host.retainCurrentWorld();
   if(!forwarded){
    if(prepRetired)host.releasePreparation();
    if(!exits.empty()){const auto retired=held;exiting=held;run_exits();held={};exiting={};if(desired==Transfer::Exit&&!desiredOwner.valid())completedExitOwner=retired;}
    if(desired==Transfer::Exit){worldRoot.reset();waiting=prepRetired=fieldRetired=false;return;}
    if(desired==Transfer::Field)host.validateFieldBase(raw);
    auto applying=raw;const auto applyingOwner=desiredOwner;const auto applyingSerial=serial;
    if(!run_before(applyingOwner,applyingSerial))return;
    host.applyNative(applying);held=applyingOwner;if(serial!=applyingSerial)return;forwarded=true;forwardSerial=serial;run_after(applying,applyingOwner,applyingSerial);if(serial!=applyingSerial)return;
   }
   if(desired==Transfer::Field&&!host.fieldOwns(raw))return;
   worldRoot.reset();waiting=prepRetired=fieldRetired=false;
  }catch(const std::exception& ex){record_error(ex.what());}
  catch(...){record_error("native scene transfer failed");}
 }
 void shutdown_after_drain(bool gateDrained,bool workerJoined){
  // This is the actual final VM shutdown edge, not a room reset. No managed
  // getters/reset methods/Unity waits are legal after the gate has drained.
  if(!gateDrained||!workerJoined)throw std::runtime_error("scene lease shutdown requires the final runtime barrier");
  terminal=true;host.shutdownRoots(gateDrained,workerJoined);worldRoot.reset();exits.clear();exitKeys.clear();continuations.clear();raw={};held={};desiredOwner={};exiting={};exitOwner={};completedExitOwner={};executingExit=exitFailed=waiting=finalizationWindow=false;
 }
};
}
