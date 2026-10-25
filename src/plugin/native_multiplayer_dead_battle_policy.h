#pragma once
#include "native_multiplayer_field_policy.h"

// A dead observer executes a target's frozen native input, never its historical
// own battle. This grants presentation only; transport authentication and the
// completed physical preparation receipt are checked by the calling host.
namespace cndead_battle {
inline bool yes(const cnjson::Value& v,const char* k,bool expected){auto i=v.object.find(k);return i!=v.object.end()&&i->second.kind==cnjson::Value::Boolean&&i->second.boolean==expected;}
inline bool valid(const cnjson::Value& v,bool runningOnly=false){
 try{
  using namespace cnfield;
  if(text(v,"phase")!="battle"||!yes(v,"observerOnly",true)||!yes(v,"ok",true)||!yes(v,"officialFallback",false)||text(v,"activityId")!="act2autochess")return false;
  auto r=v.object.find("selfReconstruction");if(r!=v.object.end()&&r->second.kind!=cnjson::Value::Null)return false;
  const auto& self=at(v,"selfState"),&e=at(v,"nativeDeadBattleObservation");
  const auto seat=number(v,"seat"),target=number(e,"targetSeat");const auto status=text(e,"status");
  if(number(at(self,"player"),"hp")!=0||seat<0||seat>3||target<0||target>3||seat==target||number(e,"version")!=1||!yes(e,"readOnly",true)||!yes(e,"mayReportCombat",false)||
     (status!="running"&&(runningOnly||status!="preparing"))||text(e,"roomId")!=text(v,"roomId")||text(e,"roomId").empty()||
     number(e,"matchSequence")!=number(v,"matchSequence")||number(e,"round")!=number(v,"round")||number(e,"seat")!=seat||
     text(e,"runId")!=text(self,"runId")||text(e,"runId").empty()||text(e,"targetRunId").empty()||text(e,"targetRunId")==text(e,"runId")||
     !sha256(text(e,"planHash"))||number(e,"attempt")<1||number(e,"deadlineTs")!=number(v,"deadlineTs"))return false;
  const auto& p=at(v,"dtoProjection"),&policy=at(p,"view"),&all=at(at(at(p,"allStateSync"),"payload"),"allStateSyncData"),&scene=at(all,"sceneDetail");
  if(number(policy,"recipientSeat")!=seat||number(policy,"writeSeat")!=seat||number(policy,"observedSeat")!=target||!yes(policy,"observationGrantsWriteAccess",false)||
     number(all,"uidIndex")!=seat||text(all,"stageId")!=text(e,"stageId")||number(scene,"obUidIndex")!=target||
     number(at(scene,"preparationStatus"),"uidIndex")!=target||number(scene,"state")!=(status=="running"?4:3))return false;
  unsigned actors=0,targets=0;for(const auto& row:at(v,"roster").array){
   auto index=number(row,"seat");if(index!=seat&&index!=target)continue;
   if(yes(row,"quit",true)||yes(row,"teamLeft",true)||!yes(row,"left",false))return false;
   if(index==seat){++actors;if(number(row,"hp")!=0)return false;}
   else {++targets;if(number(row,"hp")<=0||text(row,"stageId")!=text(e,"stageId"))return false;}
  }
  return actors==1&&targets==1;
 }catch(...){return false;}
}
inline std::string execution_run(const cnjson::Value& v){
 const auto run=cnfield::text(cnfield::at(v,"selfState"),"runId");
 if(!valid(v))return run;
 const auto& e=cnfield::at(v,"nativeDeadBattleObservation");
 return run+":observe:"+cnfield::text(e,"targetRunId")+":"+cnfield::text(e,"planHash")+":"+std::to_string(cnfield::number(e,"attempt"));
}
}
