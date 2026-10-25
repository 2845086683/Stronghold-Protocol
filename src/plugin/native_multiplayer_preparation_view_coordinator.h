#pragma once
#include "native_multiplayer_preparation_view_policy.h"
#include <memory>
namespace preparation_view {
struct World {void* controller=nullptr,*map=nullptr,*level=nullptr,*centre=nullptr,*manager=nullptr,*bridge=nullptr;std::string stage,levelId;uint64_t fieldEpoch=0;};
// In-scene imports retain these Unity objects but finish their predefined
// units and replace the tiles. Their old preparation receipt cannot be reused.
inline bool same_world(const World& a,const World& b){return a.controller==b.controller&&a.map==b.map&&a.level==b.level&&a.centre==b.centre&&a.manager==b.manager&&a.bridge==b.bridge&&a.fieldEpoch==b.fieldEpoch;}
struct Proof {World world;std::string planHash,terrain,projection;int32_t attempt=0,target=-1;int64_t revision=0;bool prepared=false,inOb=false,selectionReady=false;};
// SP_PREPARE presents the choice model; the original full board preparation
// runs only after PREPARE begins. A choice receipt is never a deployment proof.
inline bool proof_matches_phase(const Plan& p,const Proof& proof){return runtime_state(p)==2?proof.selectionReady&&!proof.prepared:proof.prepared&&!proof.selectionReady;}
struct Host {
 std::function<void()> mainThread;
 std::function<World()> world;
 std::function<std::shared_ptr<void>(const World&)> retainWorld;
 std::function<void(const Plan&)> seed;
 std::function<void(const Plan&)> reload;
 std::function<bool(const Plan&)> poll;
 std::function<Proof(const Plan&,const World&)> rebuild;
 std::function<bool(const Plan&)> presented;
 std::function<bool(const std::string&,const Json&)> enqueue;
 std::function<bool()> externalReload;
 std::function<bool(const Plan&,const Proof&)> stillValid;
 std::function<Plan()> acceptedLoad={};
 std::function<bool(const Plan&,const World&)> canRebuild={};
};
class Coordinator {
 Host host;Plan plan,inflight;World before,bound;Proof receipt;std::shared_ptr<void> beforeRoot,worldRoot;
 World lastOwnWorld;std::shared_ptr<void> lastOwnRoot;bool needsOwnProof=true;
 World observerWorld;std::shared_ptr<void> observerRoot;Plan observerPlan;
 void clear_observer(){observerRoot.reset();observerWorld={};observerPlan={};}
 bool seeded=false,reloading=false,finished=false,readyQueued=false,failureQueued=false,failed=false,freshRequired=false;std::string error;
 void fail(const std::string& text){failed=true;finished=false;error=text;}
 static bool exact_for(const World& world,const Plan& p){return world.controller&&world.map&&world.level&&world.centre&&world.manager&&world.bridge&&world.stage==p.stage&&world.levelId==p.level;}
 bool exact(const World& world)const{return exact_for(world,plan);}
 void publish(){
  if(!plan.active||!host.presented(plan))return;
  if(failed&&!failureQueued){Json payload;payload.kind=Json::Object;payload.object["planHash"]=string(plan.hash);payload.object["attempt"]=integer(plan.attempt);payload.object["reason"]=string("runtime");if(host.enqueue("view.failed",payload))failureQueued=true;}
  else if(finished&&!readyQueued&&!plan.committed){if(host.enqueue("view.ready",ready(plan)))readyQueued=true;}
 }
public:
 explicit Coordinator(Host h):host(std::move(h)){}
 const Plan& current()const{return plan;}bool pending()const{return plan.active&&!finished;}bool locally_ready()const{return finished&&!failed;}
 bool scene_loading()const{return reloading&&inflight.active;}
 void retire(){host.mainThread();clear_observer();plan={};finished=readyQueued=failureQueued=false;needsOwnProof=true;}
 void release_retired(){host.mainThread();if(reloading)throw std::runtime_error("cannot reset a live preparation loader");clear_observer();plan={};inflight={};beforeRoot.reset();worldRoot.reset();lastOwnRoot.reset();before={};bound={};lastOwnWorld={};receipt={};seeded=finished=readyQueued=failureQueued=failed=freshRequired=false;needsOwnProof=true;error.clear();}
 void release_at_vm_shutdown(bool drained,bool joined){if(!drained||!joined)throw std::runtime_error("preparation shutdown requires the final runtime barrier");reloading=false;clear_observer();plan={};inflight={};beforeRoot.reset();worldRoot.reset();lastOwnRoot.reset();before={};bound={};lastOwnWorld={};receipt={};}
 const std::string& failure()const{return error;}
 void confirm_observed_battle_start(const Json& view){
  host.mainThread();if(!cndead_battle::valid(view,true))return;
  if(!plan.active){if(!can_begin_observed_battle(view))throw std::runtime_error("observed battle has no retained prepared world");return;}
  const auto& e=cnfield::at(view,"nativeDeadBattleObservation"),&proof=cnfield::at(e,"proof");
  if(!plan.observerOnly||plan.kind!="observe"||runtime_state(plan)!=3||
     cnfield::text(e,"roomId")!=plan.room||cnfield::number(e,"matchSequence")!=plan.match||cnfield::number(e,"round")!=plan.round||
     cnfield::number(e,"seat")!=plan.seat||cnfield::text(e,"runId")!=plan.run||cnfield::number(e,"targetSeat")!=plan.target||
     cnfield::text(e,"targetRunId")!=plan.targetRun||cnfield::text(e,"stageId")!=plan.stage||
     cnfield::text(e,"planHash")!=plan.hash||cnfield::number(e,"attempt")!=plan.attempt||
     cnfield::text(proof,"roomId")!=plan.room||cnfield::number(proof,"matchSequence")!=plan.match||cnfield::number(proof,"round")!=plan.round||
     cnfield::number(proof,"seat")!=plan.seat||cnfield::text(proof,"runId")!=plan.run||cnfield::number(proof,"targetSeat")!=plan.target||
     cnfield::text(proof,"stageId")!=plan.stage||cnfield::text(proof,"planHash")!=plan.hash||cnfield::number(proof,"attempt")!=plan.attempt||
     cnfield::number(proof,"targetRevision")!=plan.revision||cnfield::text(proof,"terrainSha256")!=plan.terrain||
     (!readyQueued&&!plan.committed)||!finished||failed||!same_world(host.world(),bound)||!host.stillValid(plan,receipt))
   throw std::runtime_error("observed battle receipt differs from the completed target world");
  observerPlan=plan;observerWorld=bound;observerRoot=worldRoot;
 }
 bool can_begin_observed_battle(const Json& view)const{
  if(!observerRoot||!observerPlan.active||!cndead_battle::valid(view,true)||!same_world(host.world(),observerWorld))return false;
  const auto& e=cnfield::at(view,"nativeDeadBattleObservation");
  return cnfield::text(e,"roomId")==observerPlan.room&&cnfield::number(e,"matchSequence")==observerPlan.match&&
   cnfield::number(e,"round")==observerPlan.round&&cnfield::number(e,"seat")==observerPlan.seat&&cnfield::text(e,"runId")==observerPlan.run&&
   cnfield::number(e,"targetSeat")==observerPlan.target&&cnfield::text(e,"targetRunId")==observerPlan.targetRun&&
   cnfield::text(e,"stageId")==observerPlan.stage&&cnfield::text(e,"planHash")==observerPlan.hash&&cnfield::number(e,"attempt")==observerPlan.attempt;
 }
 void confirm_battle_start(const Json& view){
  host.mainThread();auto found=view.object.find("preparationStartReceipt");
  if(found==view.object.end()||found->second.kind==Json::Null||!plan.active)return;
  const auto& value=found->second;const auto phase=cnfield::text(view,"phase");
  if((phase!="battle"&&phase!="boss")||plan.observerOnly||plan.kind!="return-own"||plan.target!=plan.seat||
     cnfield::number(value,"version")!=1||cnfield::text(value,"phase")!="preparation"||
     cnfield::text(view,"roomId")!=plan.room||cnfield::number(view,"matchSequence")!=plan.match||
     cnfield::number(view,"round")!=plan.round||cnfield::number(view,"seat")!=plan.seat||
     cnfield::number(view,"generation")<=plan.generation||
     cnfield::text(cnfield::at(view,"selfState"),"runId")!=plan.run||
     cnfield::text(cnfield::at(view,"selfState"),"stageId")!=plan.stage||
     cnfield::text(value,"roomId")!=plan.room||cnfield::number(value,"matchSequence")!=plan.match||
     cnfield::number(value,"round")!=plan.round||cnfield::number(value,"seat")!=plan.seat||
     cnfield::text(value,"runId")!=plan.run||cnfield::number(value,"targetSeat")!=plan.seat||
     cnfield::text(value,"stageId")!=plan.stage||cnfield::text(value,"planHash")!=plan.hash||
     cnfield::number(value,"attempt")!=plan.attempt||cnfield::number(value,"targetRevision")!=plan.revision||
     cnfield::text(value,"terrainSha256")!=plan.terrain)
   throw std::runtime_error("native battle start receipt differs from the prepared world");
  if((!readyQueued&&!plan.committed)||!finished||failed||!same_world(host.world(),bound)||!host.stillValid(plan,receipt))
   throw std::runtime_error("native battle start receipt has no completed local world proof");
  // Only an exact server ACK can commit the existing local receipt. No seed,
  // rebuild, scene load or native StartBattle is invoked by this handoff.
  plan.committed=true;
 }
 void accept(Plan next){
  host.mainThread();if(!next.active){
   if(can_begin_own_battle()&&plan.active){needsOwnProof=false;lastOwnWorld=bound;lastOwnRoot=worldRoot;}
   if(reloading){plan=std::move(next);return;}plan=std::move(next);beforeRoot.reset();worldRoot.reset();before={};bound={};receipt={};seeded=reloading=finished=readyQueued=failureQueued=failed=freshRequired=false;error.clear();return;
  }
  if(plan.active&&next.room==plan.room&&next.match==plan.match&&next.round==plan.round&&next.viewGeneration<plan.viewGeneration)return;
  clear_observer();
  const bool ownerChanged=!same_owner(plan,next),presentationChanged=!same_presentation(plan,next);
  if(ownerChanged){freshRequired=failed;if(!reloading){before=host.world();beforeRoot=host.retainWorld(before);}worldRoot.reset();bound={};receipt={};seeded=finished=readyQueued=failureQueued=failed=false;error.clear();}
  else if(presentationChanged){finished=false;readyQueued=false;seeded=false;receipt={};}
  plan=std::move(next);needsOwnProof=true;if(plan.failed)fail("server rejected native preparation view");
 }
 const Plan& staged()const{return reloading?inflight:plan;}
 // Called after the immutable HTTP envelope was accepted. Normal async work and
 // native failures do not throw back into dispatch or prevent presentation.
 void tick(int64_t now){
  host.mainThread();if(!plan.active&&!reloading)return;
  try{
   // Unity's battle_loader owns its input and bridge lease until completion.
   // A cancel/new plan may supersede intent, but cannot overwrite that input
   // or start a concurrent scene. The old completion never ACKs the new plan.
   if(reloading){
    if(plan.active&&!plan.committed&&now>=plan.deadline)fail("native preparation view timed out while its old loader is draining");
    if(!host.poll(inflight)){publish();return;}auto loaded=host.world();
    if(!exact_for(loaded,inflight)||loaded.controller==before.controller)throw std::runtime_error("native preparation view loader returned without a fresh exact world");
    const bool desired=same_owner(inflight,plan);reloading=false;inflight={};seeded=false;freshRequired=false;
    if(!desired){before=loaded;beforeRoot=host.retainWorld(before);}
    if(!plan.active){worldRoot.reset();beforeRoot.reset();return;}
   }
   if(!failed&&!finished){
    if(now>=plan.deadline&&!plan.committed)fail("native preparation view timed out");
    else if(!host.externalReload()){
     if(!seeded){host.seed(plan);seeded=true;}
     auto world=host.world();
     // A new authenticated return/watch attempt can keep an exact live world.
     // Only an actual failed execution requires replacing that same controller.
     const bool forceFresh=freshRequired&&before.controller&&world.controller==before.controller;
     if(!exact(world)||forceFresh){try{host.reload(plan);world=host.world();if(exact(world)){freshRequired=false;}else{inflight=plan;reloading=true;return;}}catch(...){if(host.acceptedLoad){auto accepted=host.acceptedLoad();if(same_owner(accepted,plan)){inflight=std::move(accepted);reloading=true;}}throw;}}
     else if(reloading&&world.controller==before.controller)return;
     if(!exact(world))return;
     // Filling service data only schedules the original Unity state change.
     // Let its normal tick enter PREPARE before invoking prepare executors.
     if(host.canRebuild&&!host.canRebuild(plan,world))return;
     bound=world;worldRoot=host.retainWorld(bound);auto got=host.rebuild(plan,bound);
     if(!same_world(host.world(),bound)||!same_world(got.world,bound)||!proof_matches_phase(plan,got)||got.planHash!=plan.hash||got.attempt!=plan.attempt||got.revision!=plan.revision||got.target!=plan.target||got.inOb!=(plan.target!=plan.seat)||got.terrain!=plan.terrain||got.projection!=plan.projectionHash)throw std::runtime_error("native preparation view receipt does not belong to this target/world");
     receipt=std::move(got);finished=true;beforeRoot.reset();
    }
   }
   if(finished&&(!same_world(host.world(),bound)||!host.stillValid(plan,receipt)))fail("native preparation view world/view/terrain changed after completion");
  }catch(const std::exception& ex){fail(ex.what());}
  catch(...){fail("native preparation view host failed");}
  try{publish();}catch(const std::exception& ex){fail(ex.what());}catch(...){fail("native preparation view queue failed");}
 }
 bool can_commit(const Plan& latest)const{return latest.committed&&same_presentation(plan,latest)&&finished&&!failed&&same_world(host.world(),bound)&&host.stillValid(plan,receipt);}
 bool can_begin_own_battle()const{return !plan.observerOnly&&(!plan.active?(!needsOwnProof&&(!lastOwnWorld.controller||same_world(host.world(),lastOwnWorld))):(runtime_state(plan)==3&&plan.kind=="return-own"&&plan.target==plan.seat&&can_commit(plan)));}
};
}
