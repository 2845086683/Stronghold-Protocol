#pragma once
#include "native_json.h"
#include <map>
#include <set>
#include <string>
#include <tuple>

namespace cnmulti_help_counter {
// Pure local presentation clock. The server still owns actual HP/settlement.
struct BossPreview {
    std::string room;int64_t seat=-1,round=0,deadline=0;
    int32_t lost(double now,int32_t total)const{
        if(room.empty()||deadline<=0||!std::isfinite(now)||now<=double(deadline)||total<=0)return 0;
        const auto elapsed=now-double(deadline);
        return elapsed>=total?total:static_cast<int32_t>(elapsed);
    }
};
inline BossPreview boss_preview(const cnjson::Value& view){
    auto phase=view.object.find("phase");if(phase==view.object.end()||phase->second.string!="boss")return {};
    auto reconstruction=view.object.find("selfReconstruction");
    if(reconstruction!=view.object.end()&&reconstruction->second.kind==cnjson::Value::Object)return {};
    BossPreview value;
    for(const auto* key:{"roomId","seat","round","deadlineTs"})if(!view.object.count(key))return {};
    value.room=view.object.at("roomId").string;value.seat=view.object.at("seat").integer;
    value.round=view.object.at("round").integer;value.deadline=view.object.at("deadlineTs").integer;
    if(view.object.at("roomId").kind!=cnjson::Value::String||view.object.at("seat").kind!=cnjson::Value::Integer||
       view.object.at("round").kind!=cnjson::Value::Integer||view.object.at("deadlineTs").kind!=cnjson::Value::Integer||
       value.seat<0||value.seat>3||value.round<1||value.round>15||value.deadline<1)return {};
    return value;
}
inline const cnjson::Value& at(const cnjson::Value& object,const char* name){
    auto found=object.object.find(name);
    if(object.kind!=cnjson::Value::Object||found==object.object.end())throw std::runtime_error(std::string("help counter field missing: ")+name);
    return found->second;
}
inline int64_t number(const cnjson::Value& object,const char* name,int64_t low,int64_t high){
    const auto& value=at(object,name);if(value.kind!=cnjson::Value::Integer||value.integer<low||value.integer>high)throw std::runtime_error(std::string("help counter integer invalid: ")+name);return value.integer;
}
inline std::string string(const cnjson::Value& object,const char* name,bool allow_empty=false){const auto& value=at(object,name);if(value.kind!=cnjson::Value::String||(!allow_empty&&value.string.empty())||value.string.size()>256)throw std::runtime_error("help counter string invalid");return value.string;}
using Key=std::tuple<int32_t,bool,int32_t>;
struct Enemy {Key key;std::string id;};
inline Enemy enemy(const cnjson::Value& row){
    const auto& token=at(row,"isToken");if(token.kind!=cnjson::Value::Boolean)throw std::runtime_error("help counter enemy kind invalid");
    // Indexed escaped summons keep an empty wire enemyId; the original Help
    // generator resolves their packed enemyInstId. Ordinary enemies need IDs.
    return {{int32_t(number(row,"ownerPlayerIndex",0,3)),token.boolean,int32_t(number(row,"enemyInstId",0,2147483647))},string(row,"enemyId",token.boolean)};
}
inline std::string key_text(const Key& key){return std::to_string(std::get<0>(key))+":"+std::to_string(int(std::get<1>(key)))+":"+std::to_string(std::get<2>(key));}
inline std::map<int32_t,int32_t> counts(const cnjson::Value& object){
    if(object.kind!=cnjson::Value::Object||object.object.size()>4)throw std::runtime_error("help counter owner map invalid");
    std::map<int32_t,int32_t> result;
    for(const auto& row:object.object){if(row.first.size()!=1||row.first[0]<'0'||row.first[0]>'3'||row.second.kind!=cnjson::Value::Integer||row.second.integer<0||row.second.integer>16384)throw std::runtime_error("help counter owner count invalid");result.emplace(row.first[0]-'0',int32_t(row.second.integer));}return result;
}
struct Snapshot {
    std::string room;
    int32_t seat=-1,round=0,source=0,killed=0,escaped=0,remaining=0,unvanquished=0;
    int64_t generation=0,event_seq=0;
    bool active=false;
};
inline Snapshot parse(const cnjson::Value& view){
    Snapshot result;result.room=string(view,"roomId");result.seat=int32_t(number(view,"seat",0,3));
    result.generation=number(view,"generation",1,INT64_MAX);result.event_seq=number(view,"eventSeq",0,INT64_MAX);
    if(string(view,"phase")!="help")return result;
    const auto reconstruction=view.object.find("selfReconstruction");
    if(reconstruction!=view.object.end()&&reconstruction->second.kind==cnjson::Value::Object)return result;
    result.round=int32_t(number(view,"round",1,15));const auto& plan=at(view,"jointDefense");
    if(number(plan,"version",1,1)!=1||number(plan,"round",1,15)!=result.round||number(plan,"generation",1,INT64_MAX)!=result.generation||string(plan,"status")!="battle")throw std::runtime_error("help counter plan identity mismatch");
    const auto source=counts(at(plan,"sourceCounts"));auto actual=source;
    for(auto& row:actual)row.second=0;
    const auto& issued=at(plan,"enemies");if(issued.kind!=cnjson::Value::Array||issued.array.size()>16384)throw std::runtime_error("help counter issued list invalid");
    std::map<Key,std::string> identities;
    for(const auto& row:issued.array){auto normalized=enemy(row);const auto owner=std::get<0>(normalized.key);auto count=actual.find(owner);
        if(count==actual.end()||!identities.emplace(normalized.key,normalized.id).second)throw std::runtime_error("help counter issued enemy owner/identity invalid");++count->second;}
    if(actual!=source)throw std::runtime_error("help counter source inventory differs from issued enemies");
    auto unvanquished=source;std::map<int32_t,int32_t> killed,escaped;std::set<Key> terminal;
    for(const auto* field:{"kills","escapes"}){const auto& entries=at(plan,field);if(entries.kind!=cnjson::Value::Object||entries.object.size()>identities.size())throw std::runtime_error("help counter terminal ledger invalid");
        for(const auto& row:entries.object){const bool is_kill=std::string(field)=="kills";auto normalized=enemy(is_kill?at(row.second,"killedEnemyInfo"):row.second);auto known=identities.find(normalized.key);
            if(known==identities.end()||known->second!=normalized.id||row.first!=key_text(normalized.key)||!terminal.insert(normalized.key).second)throw std::runtime_error("help counter terminal identity/partition invalid");
            auto owner=std::get<0>(normalized.key);if(is_kill){++killed[owner];--unvanquished.at(owner);}else ++escaped[owner];}}
    if(counts(at(view,"jointDefenseRemainingByOwner"))!=unvanquished)throw std::runtime_error("help counter unvanquished settlement ledger mismatch");
    const auto own=source.find(result.seat);result.source=own==source.end()?0:own->second;
    result.killed=killed[result.seat];result.escaped=escaped[result.seat];result.unvanquished=result.source-result.killed;
    // Original OnHelpBattleEnemyKilled AND OnHelpBattleEnemyReachExit decrement
    // the HUD remaining count. Escapes stay unvanquished for server settlement.
    result.remaining=result.unvanquished-result.escaped;if(result.remaining<0)throw std::runtime_error("help counter negative remainder");result.active=true;return result;
}
inline bool stale(const Snapshot& old,const Snapshot& next){return old.room==next.room&&(next.generation<old.generation||(next.generation==old.generation&&next.event_seq<old.event_seq));}
inline bool display_changed(const Snapshot& old,const Snapshot& next){return old.active!=next.active||old.room!=next.room||old.seat!=next.seat||old.round!=next.round||old.generation!=next.generation||old.remaining!=next.remaining||old.escaped!=next.escaped;}
}
