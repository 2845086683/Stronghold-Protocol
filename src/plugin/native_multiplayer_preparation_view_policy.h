#pragma once
#include "native_json.h"
#include "native_multiplayer_dead_battle_policy.h"
#include "native_multiplayer_field_policy.h"
#include <functional>
#include <limits>
#include <string>
namespace preparation_view {
using Json=cnjson::Value;
struct Plan{
 bool active=false,committed=false,failed=false,observerOnly=false,observedBattlePreparing=false;int32_t seat=-1,target=-1,attempt=0;int64_t match=0,round=0,generation=0,viewGeneration=0,revision=0,deadline=0;
 std::string room,run,targetRun,kind,phase,stage,level,hash,terrain,runtimeTerrain,projectionHash;Json rawPlan,projection,effective;
};
inline bool same_owner(const Plan& a,const Plan& b){return a.active&&b.active&&a.room==b.room&&a.run==b.run&&a.match==b.match&&a.round==b.round&&a.hash==b.hash&&a.attempt==b.attempt&&a.seat==b.seat;}
inline bool same_presentation(const Plan& a,const Plan& b){return same_owner(a,b)&&a.revision==b.revision&&a.projectionHash==b.projectionHash;}
// A transport attempt/revision is not a Unity scene or prepare lifetime. The
// countdown still presents PREPARE; normal shop/ready deltas keep that world's
// original completed terrain/prepare work and update its native data models.
inline int runtime_state(const Plan& p){return !p.observedBattlePreparing&&p.observerOnly&&(p.phase=="battle_countdown"||p.phase=="battle"||p.phase=="boss")?100:p.phase=="special"?2:3;}
inline bool same_prepared_scene(const Plan& a,const Plan& b){return a.active&&b.active&&a.room==b.room&&a.run==b.run&&a.match==b.match&&a.round==b.round&&a.seat==b.seat&&a.stage==b.stage&&a.level==b.level&&(a.runtimeTerrain.empty()?a.terrain:a.runtimeTerrain)==(b.runtimeTerrain.empty()?b.terrain:b.runtimeTerrain)&&a.observerOnly==b.observerOnly&&runtime_state(a)==runtime_state(b);}
inline bool same_runtime(const Plan& a,const Plan& b){return same_prepared_scene(a,b)&&a.target==b.target&&a.targetRun==b.targetRun;}
inline Json string(const std::string& value){Json p;p.kind=Json::String;p.string=value;return p;}
inline Json integer(int64_t value){Json p;p.kind=Json::Integer;p.integer=value;return p;}
inline Plan parse(const Json& view,const std::function<std::string(const Json&)>& digest,const std::function<std::string(const std::string&)>& stagePath){
 Plan out;if(view.kind!=Json::Object)return out;
 auto observer=view.object.find("observerOnly");if(observer!=view.object.end()){if(observer->second.kind!=Json::Boolean)throw std::runtime_error("preparation view observer role invalid");out.observerOnly=observer->second.boolean;}
 auto found=view.object.find("preparationView");if(found==view.object.end()||found->second.kind==Json::Null)return out;
 const auto& envelope=found->second;const auto& p=cnfield::at(envelope,"plan");
 const auto& observerPlan=cnfield::at(p,"observerOnly");if(observerPlan.kind!=Json::Boolean||observerPlan.boolean!=out.observerOnly)throw std::runtime_error("preparation view observer role differs from immutable plan");
 out.observedBattlePreparing=cndead_battle::valid(view)&&cnfield::text(cnfield::at(view,"nativeDeadBattleObservation"),"status")=="preparing";
 out.phase=cnfield::text(view,"phase");if(out.phase!="preparation"&&out.phase!="special"&&out.phase!="battle_countdown"&&!(out.observerOnly&&(out.phase=="battle"||out.phase=="boss")))throw std::runtime_error("preparation view outside native prepare/return barrier");
 const auto seat=cnfield::number(view,"seat"),target=cnfield::number(p,"targetSeat"),attempt=cnfield::number(envelope,"attempt");
 if(seat<0||seat>3||target<0||target>3||attempt<1||attempt>INT32_MAX)throw std::runtime_error("preparation view seat/attempt outside bound");
 out.active=true;out.seat=int32_t(seat);out.target=int32_t(target);out.attempt=int32_t(attempt);
 out.room=cnfield::text(view,"roomId");out.match=cnfield::number(view,"matchSequence");out.round=cnfield::number(view,"round");out.generation=cnfield::number(view,"generation");out.run=cnfield::text(cnfield::at(view,"selfState"),"runId");
 out.kind=cnfield::text(p,"kind");out.targetRun=cnfield::text(p,"targetRunId");out.stage=cnfield::text(p,"targetStageId");out.level=cnfield::text(p,"targetLevelId");out.viewGeneration=cnfield::number(p,"viewGeneration");out.terrain=cnfield::text(p,"terrainSha256");out.hash=cnfield::text(envelope,"planHash");out.projectionHash=cnfield::text(envelope,"targetProjectionSha256");out.revision=cnfield::number(envelope,"targetRevision");out.deadline=cnfield::number(envelope,"deadlineTs");
 const auto status=cnfield::text(envelope,"status");out.committed=status=="committed";out.failed=status=="failed";
 if(cnfield::number(envelope,"version")!=1||cnfield::number(p,"version")!=1||out.room.empty()||out.run.empty()||out.targetRun.empty()||out.match<1||out.round<1||out.round>15||out.generation<1||out.viewGeneration<1||out.revision<0||out.deadline<1||(!out.committed&&!out.failed&&status!="loading")||(out.kind!="observe"&&out.kind!="return-own")||
   cnfield::text(p,"roomId")!=out.room||cnfield::number(p,"matchSequence")!=out.match||cnfield::number(p,"round")!=out.round||cnfield::number(p,"seat")!=out.seat||cnfield::text(p,"runId")!=out.run||((!out.committed||out.phase!="battle_countdown")&&!(out.observerOnly&&(out.phase=="battle_countdown"||out.phase=="battle"||out.phase=="boss"))&&cnfield::text(p,"phase")!=out.phase)||!cnfield::stage(out.stage)||stagePath(out.stage)!=out.level||!cnfield::sha256(out.hash)||!cnfield::sha256(out.terrain)||!cnfield::sha256(out.projectionHash)||digest(p)!=out.hash)throw std::runtime_error("preparation view immutable identity/hash differs");
 if(out.kind=="return-own"&&(out.target!=out.seat||out.targetRun!=out.run||out.stage!=cnfield::text(cnfield::at(view,"selfState"),"stageId")))throw std::runtime_error("preparation view return-own is not the authenticated battlefield");
 if(out.phase=="battle_countdown"&&((!out.committed&&!out.observerOnly)||(!out.observerOnly&&out.kind!="return-own")))throw std::runtime_error("preparation view countdown preceded own-field commitment");
 const auto& source=cnfield::at(p,"source");if(cnfield::source(source).stageId!=out.stage)throw std::runtime_error("preparation view fixed source differs from target");
 out.projection=cnfield::at(envelope,"targetProjection");if(out.projection.kind!=Json::Object||digest(out.projection)!=out.projectionHash)throw std::runtime_error("preparation view target DTO digest differs");
 const auto& policy=cnfield::at(out.projection,"view");const auto& all=cnfield::at(cnfield::at(cnfield::at(out.projection,"allStateSync"),"payload"),"allStateSyncData");const auto& scene=cnfield::at(all,"sceneDetail");const auto& grants=cnfield::at(policy,"observationGrantsWriteAccess");
 if(cnfield::number(policy,"recipientSeat")!=out.seat||cnfield::number(policy,"writeSeat")!=out.seat||cnfield::number(policy,"observedSeat")!=out.target||grants.kind!=Json::Boolean||grants.boolean||cnfield::number(all,"uidIndex")!=out.seat||cnfield::number(scene,"obUidIndex")!=out.target||cnfield::number(cnfield::at(scene,"preparationStatus"),"uidIndex")!=out.target||cnfield::text(all,"stageId")!=out.stage||digest(cnfield::at(scene,"specialEffects"))!=out.terrain)throw std::runtime_error("preparation view target DTO ownership/terrain differs");
 // The official specialEffects collection belongs to the whole scene. Keep
 // its full signed hash above, but unrelated players' targets cannot force a
 // local terrain/board rebuild. Empty targets remain inert, as in the native
 // Contains(actualPlayerIndex) consumer. A new target's actual effects still
 // need their own prepare/terrain execution before reusing a receipt.
 Json relevant;relevant.kind=Json::Array;
 const auto& effects=cnfield::at(scene,"specialEffects");if(effects.kind!=Json::Array)throw std::runtime_error("preparation view effects list invalid");
 for(const auto& effect:effects.array){const auto& targets=cnfield::at(effect,"affectedUidIndex");if(targets.kind!=Json::Array)throw std::runtime_error("preparation view effect targets invalid");bool applies=false;for(const auto& target:targets.array){if(target.kind!=Json::Integer||target.integer<0||target.integer>3)throw std::runtime_error("preparation view effect target outside room seats");if(target.integer==out.target)applies=true;}if(applies)relevant.array.push_back(effect);}
 out.runtimeTerrain=digest(relevant);
 if(out.observerOnly){
  auto rollover=view.object.find("observerRollover");if(cnfield::number(cnfield::at(cnfield::at(view,"selfState"),"player"),"hp")>0)throw std::runtime_error("preparation view observer actor is alive");
  if(out.phase=="battle"||out.phase=="boss"||out.phase=="battle_countdown"){
   if(rollover==view.object.end())throw std::runtime_error("preparation view observer rollover authorization absent");const auto& role=rollover->second;const auto& readOnly=cnfield::at(role,"readOnly"),&mayStart=cnfield::at(role,"mayStartSelfBattle");if(readOnly.kind!=Json::Boolean||!readOnly.boolean||mayStart.kind!=Json::Boolean||mayStart.boolean)throw std::runtime_error("preparation view observer rollover permits own combat");
   for(const auto* key:{"selfBattleInfo","helpBattleInfo","bossBattleInfo"}){auto value=scene.object.find(key);if(value!=scene.object.end()&&value->second.kind!=Json::Null)throw std::runtime_error("preparation view observer rollover retained a historical battle ledger");}
  }
 }
 // Recovered CN metadata enum constant, not declaration-order inference.
 const auto expectedState=runtime_state(out);
 if(cnfield::number(scene,"state")!=expectedState)throw std::runtime_error("preparation view target DTO carries a combat state instead of its authorized prepare/wait state");
 // selfState remains the authenticated actor, including a historical eliminated
 // actor's round. Only native presentation uses target DTOs.
 out.rawPlan=p;out.effective=view;out.effective.object["dtoProjection"]=out.projection;return out;
}
inline Json ready(const Plan& p){Json out;out.kind=Json::Object;out.object["planHash"]=string(p.hash);out.object["attempt"]=integer(p.attempt);out.object["targetRevision"]=integer(p.revision);out.object["targetSeat"]=integer(p.target);out.object["stageId"]=string(p.stage);out.object["terrainSha256"]=string(p.terrain);return out;}
}
