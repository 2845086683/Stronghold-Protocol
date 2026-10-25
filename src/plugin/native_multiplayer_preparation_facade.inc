// Research integration facade. Include after cn_native_adapter.inc. These
// functions are not referenced by the frozen 0.1.58 production translation unit.
#include "native_multiplayer_departure_policy.h"
#include "native_multiplayer_reconstruction_bootstrap.h"
namespace preparation_view_native {
Native* adapter=nullptr;preparation_view::Coordinator* coordinator=nullptr;
bool serverWriteAllowed=false,serverReturnBarrier=false;
Plan presentedPlan;
std::mutex diagnosticMutex;std::string firstFailure,lastFailureIdentity;unsigned failureCount=0;
bool transfer_blocked();
std::string digest_json(const cnjson::Value& value){auto json=cnjson::dump(value);return nl_sha256(std::vector<uint8_t>(json.begin(),json.end()));}
void bind_facade(){
 if(adapter||coordinator)throw std::runtime_error("native preparation view setup repeated");
 auto native=new Native;native->bind();adapter=native;
 preparation_view::Host host;
 host.mainThread=nb_main_thread;host.world=[](){return adapter->world();};
 host.retainWorld=[](const World& value){return std::static_pointer_cast<void>(std::make_shared<Roots>(value));};
 host.seed=[](const Plan& plan){adapter->seed(plan);};host.reload=[](const Plan& plan){adapter->reload(plan);};host.poll=[](const Plan& plan){return adapter->poll(plan);};host.rebuild=[](const Plan& plan,const World& value){return adapter->rebuild(plan,value);};
 host.presented=[](const Plan& plan){std::lock_guard<std::mutex> lock(nmp_mutex);return !transfer_blocked()&&nmp_state.room==plan.room&&preparation_view::same_presentation(presentedPlan,plan)&&presentedPlan.generation==plan.generation;};
 host.enqueue=[](const std::string& op,const cnjson::Value& payload){return native_multiplayer_enqueue(op,payload);};
 // A first controller exists before the original resource preload completes.
 // Its partial map/bridge is not evidence that another scene must be loaded.
 host.externalReload=[](){return !nmh_scope()||!nmo_controller()||nff_restore_pending||nff_retry_reload||(native_battle_launch_in_flight.load()&&!native_battle_launch_loading_complete.load());};
 host.stillValid=[](const Plan& plan,const Proof& proof){return adapter->valid(plan,proof);};
 host.acceptedLoad=[](){return adapter->loader_pending()?adapter->loader_plan():Plan{};};
 host.canRebuild=[](const Plan& plan,const World& value){return adapter->can_rebuild(plan,value);};
 coordinator=new preparation_view::Coordinator(std::move(host));
}
cnjson::Value effective_view(const cnjson::Value& raw){
 nb_main_thread();if(!native_multiplayer_entry_active()||!coordinator)return raw;
 if(adapter&&!native_battle_launch_in_flight.load()&&!adapter->world().controller&&cnfield::parse(raw).active)
     return cnrejoin::waiting_for_base(raw);
 auto write=raw.object.find("preparationWriteAllowed");serverWriteAllowed=write!=raw.object.end()&&write->second.kind==cnjson::Value::Boolean&&write->second.boolean;
 auto barrier=raw.object.find("returnOwnBarrier");serverReturnBarrier=barrier!=raw.object.end()&&barrier->second.kind!=cnjson::Value::Null;
 coordinator->confirm_battle_start(raw);
 coordinator->confirm_observed_battle_start(raw);
 auto plan=preparation_view::parse(raw,digest_json,[](const std::string& stage){return native_multiplayer_stage_path(stage,CN_PRIVATE_LEVELS);});
 if(plan.active){
  auto source=cnfield::source(cnfield::at(plan.rawPlan,"source"));const CN_Asset* asset=nullptr;for(const auto& row:CN_ASSETS)if(source.assetKey==row.alias){asset=&row;break;}
  if(!asset||asset->scene||source.assetKey!=asset->asset)throw std::runtime_error("native preparation source is outside the fixed resource profile");
  const CN_Bundle* bundle=nullptr;for(const auto& row:CN_BUNDLES)if(std::strcmp(asset->bundle,row.name)==0){bundle=&row;break;}
  if(!bundle||source.graphicHash!=bundle->hash)throw std::runtime_error("native preparation source hash differs from compiled resources");
 }
 coordinator->accept(std::move(plan));
 const auto& staged=coordinator->staged();if(!staged.active)return raw;
 if(cnfield::text(raw,"roomId")!=staged.room||cnfield::text(cnfield::at(raw,"selfState"),"runId")!=staged.run)throw std::runtime_error("native preparation view cannot stage another room/run");
 // Preserve the newest transport generation/event cursor and authenticated
 // actor fields. Only native DTO presentation is leased by the in-flight load.
 auto effective=raw;effective.object["dtoProjection"]=staged.projection;adapter->context(staged);return effective;
}
bool owns_presentation(const cnjson::Value& effective){
 if(!native_multiplayer_entry_active()||!coordinator)return false;
 const auto& p=coordinator->staged();if(!p.active)return false;
 if(cnfield::text(effective,"roomId")!=p.room||cnfield::text(cnfield::at(effective,"selfState"),"runId")!=p.run||cnfield::number(effective,"seat")!=p.seat||cnjson::dump(cnfield::at(effective,"dtoProjection"))!=cnjson::dump(p.projection))throw std::runtime_error("native preparation staged DTO lost its verified owner");
 adapter->context(p);return true;
}
void tick(){
 if(!coordinator||!native_multiplayer_entry_active())return;nb_main_thread();auto now=native_multiplayer_transport_time();if(now>0)coordinator->tick(int64_t(now));
 if(!coordinator->failure().empty()){
  const auto& p=coordinator->current();const auto identity=p.hash+":"+std::to_string(p.attempt);
  std::lock_guard<std::mutex> lock(diagnosticMutex);if(identity!=lastFailureIdentity){lastFailureIdentity=identity;++failureCount;if(firstFailure.empty())firstFailure=coordinator->failure();}
 }
}
cnjson::Value status(){std::lock_guard<std::mutex> lock(diagnosticMutex);auto value=nb_object();value.object["firstFailure"]=json_text(firstFailure);value.object["failures"]=nb_integer(failureCount);value.object["fullRebuilds"]=nb_integer(adapter?adapter->full_rebuilds():0);value.object["incrementalUpdates"]=nb_integer(adapter?adapter->incremental_updates():0);value.object["naturalPrepares"]=nb_integer(adapter?adapter->natural_prepares():0);value.object["specialSelectionReceipts"]=nb_integer(adapter?adapter->special_selections():0);return value;}
bool may_start_own_battle(){return !native_multiplayer_entry_active()||!coordinator||coordinator->can_begin_own_battle();}
bool owns_scene_transition(){
 if(!adapter||!coordinator||!adapter->loader_pending())return false;
 const auto& p=adapter->loader_plan();return p.active&&coordinator->scene_loading();
}
void notify_presented(const cnjson::Value& raw){nb_main_thread();presentedPlan=preparation_view::parse(raw,digest_json,[](const std::string& stage){return native_multiplayer_stage_path(stage,CN_PRIVATE_LEVELS);});}
bool may_write_board(){return native_multiplayer_entry_active()&&coordinator&&serverWriteAllowed&&coordinator->current().active&&preparation_view::runtime_state(coordinator->current())==3&&!coordinator->current().observerOnly&&coordinator->current().target==coordinator->current().seat&&coordinator->can_commit(coordinator->current());}
bool may_choose(){if(!native_multiplayer_entry_active()||!coordinator||serverReturnBarrier)return false;const auto& p=coordinator->current();return p.active&&!p.observerOnly&&p.target==p.seat&&p.kind=="return-own"&&(p.phase=="preparation"||p.phase=="special")&&coordinator->can_commit(p);}
}
bool native_multiplayer_preparation_view_may_start_observed_battle(const cnjson::Value& view){return native_multiplayer_entry_active()&&preparation_view_native::coordinator&&preparation_view_native::coordinator->can_begin_observed_battle(view);}
void native_multiplayer_preparation_view_notify_presented(const cnjson::Value& raw){preparation_view_native::notify_presented(raw);}

// Candidate: append after the preparation facade, before the launch helper.
// The ordinary own-stage comparison remains the first accepted branch.
namespace preparation_view_native {
bool allows_initial_stage(const cnjson::Value& raw,const std::string& stage){
 nb_main_thread();
 if(!native_multiplayer_entry_active()||native_battle_launch_in_flight.load()||!coordinator||!adapter||adapter->loader_pending())return false;
 const auto phase=cnfield::text(raw,"phase");const bool prepare=phase=="preparation"||phase=="special";if(!prepare&&phase!="battle"&&phase!="battle_countdown")return false;
 auto p=preparation_view::parse(raw,digest_json,[](const std::string& value){return native_multiplayer_stage_path(value,CN_PRIVATE_LEVELS);});
 if(!p.active||p.failed||stage!=p.stage)return false;
 if(!prepare){
  // A fresh eliminated recipient still needs its one initial Unity world.
  // Native BattleInfo.Fill/StartMultiBattle uses the authenticated target
  // stage. Permit only the signed read-only BATTLE_WAITING presentation, never
  // an alive player's peer battle or the eliminated actor's historical fight.
  if(!p.observerOnly||p.kind!="observe"||p.target==p.seat||p.targetRun==p.run||cn_departure::scene_departed(raw))return false;
  auto presence=raw.object.find("selfScenePresence");if(presence==raw.object.end()||presence->second.kind!=cnjson::Value::Object)return false;
  const auto& self=cnfield::at(raw,"selfState");const auto& role=cnfield::at(raw,"observerRollover");
  const auto& readOnly=cnfield::at(role,"readOnly"),&mayStart=cnfield::at(role,"mayStartSelfBattle");
  if(cnfield::number(cnfield::at(self,"player"),"hp")!=0||cnfield::number(role,"version")!=1||readOnly.kind!=cnjson::Value::Boolean||!readOnly.boolean||mayStart.kind!=cnjson::Value::Boolean||mayStart.boolean||cnfield::number(role,"currentRoomRound")!=p.round||cnfield::number(role,"personalRound")!=cnfield::number(self,"round"))return false;
  const auto& roster=cnfield::at(raw,"roster");if(roster.kind!=cnjson::Value::Array)return false;
  unsigned actor=0,target=0;
  for(const auto& row:roster.array){
   const auto seat=cnfield::number(row,"seat");const auto& left=cnfield::at(row,"left");
   if(seat!=p.seat&&seat!=p.target)continue;
   if(left.kind!=cnjson::Value::Boolean||left.boolean)return false;
   if(seat==p.seat){++actor;if(cnfield::number(row,"hp")!=0||cnfield::text(row,"stageId")!=cnfield::text(self,"stageId"))return false;}
   if(seat==p.target){++target;if(cnfield::number(row,"hp")<=0||cnfield::text(row,"stageId")!=p.stage)return false;}
  }
  if(actor!=1||target!=1)return false;
 }
 const auto& current=coordinator->current();const auto& staged=coordinator->staged();
 // Joining executes inside the quiescent finalizing scope before global apply.
 // A blanket transfer_blocked test here would reject the initial PREPARE load.
 // Both identities must nevertheless be the exact verified DTO just primed.
 return preparation_view::same_presentation(p,current)&&preparation_view::same_presentation(p,staged)&&p.generation==current.generation&&p.generation==staged.generation;
}
}
bool native_multiplayer_preparation_view_allows_initial_stage(const cnjson::Value& raw,const std::string& stage){return preparation_view_native::allows_initial_stage(raw,stage);}
