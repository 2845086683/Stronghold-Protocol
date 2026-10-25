#pragma once
#include "native_json.h"
#include <set>
#include <string>

namespace cnhelp_outcome {
inline const cnjson::Value& at(const cnjson::Value& v,const char* key){
    const auto i=v.object.find(key);if(v.kind!=cnjson::Value::Object||i==v.object.end())throw std::runtime_error(std::string("Help outcome field absent: ")+key);return i->second;
}
inline int64_t number(const cnjson::Value& v,const char* key){const auto& n=at(v,key);if(n.kind!=cnjson::Value::Integer)throw std::runtime_error("Help outcome integer required");return n.integer;}
inline bool running(const cnjson::Value& view){
    const auto& field=at(at(view,"jointDefense"),"fieldLoad");const auto& status=at(field,"status");
    return status.kind==cnjson::Value::String&&status.string=="running";
}
inline bool expired(const cnjson::Value& view,double now){
    if(!running(view))return false;
    const auto& plan=at(view,"jointDefense");const auto start=number(plan,"startedAtTs"),end=number(plan,"deadlineTs");
    return end>start&&now>=double(end);
}
inline int64_t authority(const cnjson::Value& view){
    const auto& plan=at(view,"jointDefense");const auto& defenders=at(plan,"defenderSeats");
    if(defenders.kind!=cnjson::Value::Array||defenders.array.empty())throw std::runtime_error("Help outcome defender absent");
    const auto found=plan.object.find("authoritySeat");const auto& owner=found==plan.object.end()?defenders.array.front():found->second;
    if(owner.kind!=cnjson::Value::Integer||owner.integer<0||owner.integer>3)throw std::runtime_error("Help outcome authority invalid");
    for(const auto& defender:defenders.array)if(defender.kind==cnjson::Value::Integer&&defender.integer==owner.integer)return owner.integer;
    throw std::runtime_error("Help outcome authority is not selected defender");
}
inline bool display_only(const cnjson::Value& view,const std::string& name){
    if(name!="AutoChessBattleSceneHelpBattleKillEnemyUp"&&name!="AutoChessBattleSceneHelpBattleEnemyEscapeUp"&&name!="AutoChessBattleSceneHelpBattleFinishUp")return false;
    if(view.kind!=cnjson::Value::Object)return false;const auto phase=view.object.find("phase");
    if(phase==view.object.end()||phase->second.kind!=cnjson::Value::String||phase->second.string!="help")return false;
    return number(view,"seat")!=authority(view);
}
inline void normalize_finish(cnjson::Value& dto){
    // CN _ReqHelpBattleFinish (6013626/b34f63) tests killedByPlayer > 0
    // when exporting escapes, although uidIndex zero is a valid killer. The
    // real native kill journal exports every killer, including zero. Remove
    // only exact full-identity overlaps already proved by that native journal.
    const auto& kills=at(dto,"killInfos");auto& escapes=dto.object.at("escapedEnemies");
    if(kills.kind!=cnjson::Value::Array||escapes.kind!=cnjson::Value::Array)throw std::runtime_error("Help outcome terminal lists invalid");
    std::set<std::string> confirmed;
    for(const auto& kill:kills.array)confirmed.insert(cnjson::dump(at(kill,"killedEnemyInfo")));
    std::vector<cnjson::Value> pending;
    for(const auto& escape:escapes.array)if(!confirmed.count(cnjson::dump(escape)))pending.push_back(escape);
    escapes.array=std::move(pending);
}
inline bool journal_complete(const cnjson::Value& view){
    const auto& plan=at(view,"jointDefense");const auto& enemies=at(plan,"enemies");
    const auto& kills=at(plan,"kills");const auto& escapes=at(plan,"escapes");
    if(enemies.kind!=cnjson::Value::Array||enemies.array.empty()||kills.kind!=cnjson::Value::Object||escapes.kind!=cnjson::Value::Object)return false;
    // Entries in these maps were individually identity/attacker validated by
    // the server from this authority's native Kill/Exit Up events. Counts from
    // the HUD, character deaths and a spectator's local world are not proof.
    std::set<std::string> terminal;
    for(const auto& row:kills.object)terminal.insert(row.first);
    for(const auto& row:escapes.object)if(!terminal.insert(row.first).second)return false;
    return terminal.size()==enemies.array.size();
}
inline bool premature_finish(const cnjson::Value& view,const std::string& name,double now){
    if(name!="AutoChessBattleSceneHelpBattleFinishUp"||view.kind!=cnjson::Value::Object)return false;
    const auto phase=view.object.find("phase");
    if(phase==view.object.end()||phase->second.kind!=cnjson::Value::String||phase->second.string!="help")return false;
    // ReqLoadReady/_ReqHelpBattleFinish also exports every unresolved enemy
    // as an escape. A syntactically complete report is not combat completion.
    return !running(view)||(!journal_complete(view)&&!expired(view,now));
}
}
