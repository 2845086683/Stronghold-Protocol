#pragma once
#include "native_json.h"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace cnfield {
inline const cnjson::Value& at(const cnjson::Value& v,const char* key){auto p=v.object.find(key);if(v.kind!=cnjson::Value::Object||p==v.object.end())throw std::runtime_error(std::string("private native field missing ")+key);return p->second;}
inline std::string text(const cnjson::Value& v,const char* key){auto& p=at(v,key);if(p.kind!=cnjson::Value::String)throw std::runtime_error(std::string("private native field string ")+key);return p.string;}
inline int64_t number(const cnjson::Value& v,const char* key){auto& p=at(v,key);if(p.kind!=cnjson::Value::Integer)throw std::runtime_error(std::string("private native field integer ")+key);return p.integer;}
inline bool sha256(const std::string& value){return value.size()==64&&std::all_of(value.begin(),value.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
inline bool stage(const std::string& value){static const std::set<std::string> stages={"act1autochess_m01","act1autochess_m02","act1autochess_m03","act1autochess_m04","act2autochess_m01","act2autochess_m02","act2autochess_m03","act2autochess_m04"};return stages.count(value)!=0;}
struct Source {std::string stageId,assetKey,graphicHash,mapHash,predefinesHash;};
inline Source source(const cnjson::Value& row){Source p;p.stageId=text(row,"stageId");p.mapHash=text(row,"mapDataSha256");p.predefinesHash=text(row,"predefinesSha256");const auto& graphic=at(row,"graphic");p.assetKey=text(graphic,"assetKey");p.graphicHash=text(graphic,"sha256");if(!stage(p.stageId)||text(row,"levelTemplateKey")!="level_"+p.stageId||!sha256(p.mapHash)||!sha256(p.predefinesHash)||!sha256(p.graphicHash)||p.assetKey!="private/autochess/mixed-map-v1/"+p.stageId+"-graphic.prefab")throw std::runtime_error("private native field source differs from fixed catalog shape");return p;}
struct Segment {int32_t seat=-1;std::string side;Source source;};
struct Plan {
    bool active=false,single=false,running=false,authorized=false,aborted=false;int32_t self=-1,attempt=0;int64_t round=0,generation=0,match=0,envelopeGeneration=0;
    std::string room,run,hash,catalog,route;std::vector<Segment> segments;Source restore;
};
inline Plan parse(const cnjson::Value& view){
    Plan p;if(view.kind!=cnjson::Value::Object)return p;
    // A rebuilt client waits out its lost round. The room can still be in
    // Help for other members; it must not start/retain a composite or emit a
    // loading ACK for this recipient after its native DTO became WAITING.
    const auto reconstruction=view.object.find("selfReconstruction");
    if(reconstruction!=view.object.end()&&reconstruction->second.kind==cnjson::Value::Object)return p;
    if(text(view,"phase")!="help")return p;
    auto fieldIt=view.object.find("nativeField");if(fieldIt==view.object.end()||fieldIt->second.kind==cnjson::Value::Null)throw std::runtime_error("private help has no native field loading contract");
    const auto& field=fieldIt->second;const auto& plan=at(field,"plan");p.active=true;
    const auto self=number(view,"seat");if(self<0||self>3)throw std::runtime_error("private native field actor outside roster");
    p.room=text(view,"roomId");p.self=int32_t(self);p.envelopeGeneration=number(view,"generation");p.round=number(view,"round");p.match=number(view,"matchSequence");p.run=text(at(view,"selfState"),"runId");p.hash=text(field,"planHash");p.catalog=text(plan,"catalogHash");p.generation=number(plan,"compositionGeneration");p.route=text(plan,"routeLevelPath");
    const auto kind=text(plan,"kind");p.single=kind=="single-owner";const auto status=text(field,"status");p.running=status=="running";p.aborted=status=="aborted";
    if(number(field,"version")!=1||number(plan,"version")!=1||p.room.empty()||p.run.empty()||p.self<0||p.self>3||p.round<1||p.round>15||p.match<1||p.generation<1||p.envelopeGeneration<1||!sha256(p.hash)||!sha256(p.catalog)||number(plan,"round")!=p.round||number(plan,"matchSequence")!=p.match||text(plan,"roomId")!=p.room||(!p.single&&kind!="serial-two-owner")||(!p.running&&!p.aborted&&status!="loading"))throw std::runtime_error("private native field identity/phase mismatch");
    if(p.route!=(p.single?"Activities/ACT1AUTOCHESS/level_act1autochess_escaped_single":"Activities/ACT1AUTOCHESS/level_act1autochess_escaped_multi"))throw std::runtime_error("private native field escaped route identity changed");
    auto& growth=at(plan,"allowBondLayerGrowth");if(growth.kind!=cnjson::Value::Boolean||growth.boolean)throw std::runtime_error("private native help must not grow bond layers");
    auto& segments=at(plan,"segments");auto& order=at(plan,"nativePlayerOrder");if(segments.kind!=cnjson::Value::Array||segments.array.size()!=(p.single?1U:2U)||order.kind!=cnjson::Value::Array||order.array.size()!=segments.array.size())throw std::runtime_error("private native field defender count mismatch");
    std::set<int32_t> seats;for(const auto& row:segments.array){Segment s;const auto actor=number(row,"seat");if(actor<0||actor>3)throw std::runtime_error("private native field defender outside roster");s.seat=int32_t(actor);s.side=text(row,"side");s.source=source(at(row,"source"));if(!seats.insert(s.seat).second||(s.side!="left"&&s.side!="right"))throw std::runtime_error("private native field segment owner invalid");p.segments.push_back(s);}
    if(!p.single){std::sort(p.segments.begin(),p.segments.end(),[](const Segment& a,const Segment& b){return a.side<b.side;});if(p.segments[0].side!="left"||p.segments[1].side!="right")throw std::runtime_error("private native field needs one original left and one right");}
    for(size_t i=0;i<order.array.size();++i)if(order.array[i].kind!=cnjson::Value::Integer||order.array[i].integer!=p.segments[i].seat)throw std::runtime_error("private native field player order differs from side ownership");
    if(number(plan,"rightSeat")!=p.segments.back().seat)throw std::runtime_error("private native field encounter owner mismatch");
    const auto selfKey=std::to_string(p.self);const auto& restore=at(at(plan,"restoreBySeat"),selfKey.c_str());if(text(restore,"runId")!=p.run)throw std::runtime_error("private native field restore belongs to another run");p.restore=source(at(restore,"source"));
    const auto& attempt=at(at(field,"loadAttemptBySeat"),selfKey.c_str());if(attempt.kind!=cnjson::Value::Integer||attempt.integer<0||attempt.integer>INT32_MAX)throw std::runtime_error("private native field load attempt invalid");p.attempt=int32_t(attempt.integer);
    auto contains=[&](const char* key){const auto& list=at(field,key);if(list.kind!=cnjson::Value::Array||list.array.size()>4)throw std::runtime_error("private native field readiness roster invalid");std::set<int64_t> values;for(const auto& value:list.array){if(value.kind!=cnjson::Value::Integer||value.integer<0||value.integer>3||!values.insert(value.integer).second)throw std::runtime_error("private native field readiness seat invalid");}return values.count(p.self)!=0;};
    const bool ready=contains("readySeats"),recovering=contains("recoveringSeats"),worldLost=contains("worldLostSeats");p.authorized=p.running&&ready&&!recovering&&!worldLost;
    return p;
}
inline bool same_load(const Plan& a,const Plan& b){return a.active&&b.active&&a.room==b.room&&a.run==b.run&&a.round==b.round&&a.generation==b.generation&&a.match==b.match&&a.hash==b.hash&&a.attempt==b.attempt;}
}
