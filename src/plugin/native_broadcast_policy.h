#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>
#include "native_json.h"

// Display delivery policy only. The server owns economic/elite events; the
// original Unity boss HUD owns real-time boss progress. Neither is inferred
// from an outgoing request or a synthetic PlayerData model.
namespace cnbroadcast {
struct Entry {std::string id;int32_t type=0;double threshold=0;};
struct Event {std::string key,run,id;int32_t round=0,index=0;cnjson::Value payload;};
inline const cnjson::Value& at(const cnjson::Value& v,const char* k){
    auto i=v.object.find(k);if(v.kind!=cnjson::Value::Object||i==v.object.end())throw std::runtime_error("broadcast field missing");return i->second;
}
inline std::string string(const cnjson::Value& v,const char* k,size_t limit=192){
    const auto& x=at(v,k);if(x.kind!=cnjson::Value::String||x.string.empty()||x.string.size()>limit||x.string.find('\0')!=std::string::npos)throw std::runtime_error("broadcast string invalid");return x.string;
}
inline int32_t number(const cnjson::Value& v,const char* k,int32_t minimum,int32_t maximum){
    const auto& x=at(v,k);if(x.kind!=cnjson::Value::Integer||x.integer<minimum||x.integer>maximum)throw std::runtime_error("broadcast integer invalid");return int32_t(x.integer);
}
inline void validate_catalog(const std::vector<Entry>& rows){
    std::set<std::string> ids;std::vector<double> thresholds;
    for(const auto& row:rows){if(row.id.empty()||row.id.size()>96||!ids.insert(row.id).second||row.type<1||row.type>6)throw std::runtime_error("broadcast catalog invalid");
        if(row.type==3){if(!std::isfinite(row.threshold)||row.threshold<=0||row.threshold>=1)throw std::runtime_error("boss broadcast threshold invalid");thresholds.push_back(row.threshold);}}
    if(thresholds.size()!=3||!std::is_sorted(thresholds.begin(),thresholds.end())||thresholds[0]==thresholds[1]||thresholds[1]==thresholds[2])throw std::runtime_error("boss broadcast threshold order invalid");
}
inline Event parse(const cnjson::Value& value,const std::vector<Entry>& catalog,const std::string& run,int32_t first_round,int32_t last_round){
    Event e;e.key=string(value,"eventId");e.run=string(value,"runId",128);e.id=string(value,"broadcastId",96);e.round=number(value,"round",0,15);e.index=number(value,"uIdx",0,3);
    if(e.run!=run||first_round<0||last_round<first_round||last_round>15||e.round<first_round||e.round>last_round||e.key.rfind(run+":",0)!=0)throw std::runtime_error("broadcast run/round receipt mismatch");
    const auto found=std::find_if(catalog.begin(),catalog.end(),[&](const Entry& row){return row.id==e.id;});
    if(found==catalog.end()||found->type==4)throw std::runtime_error("broadcast parser unsupported");
    const auto& strings=at(value,"strParams"),&ints=at(value,"intParams");
    if(strings.kind!=cnjson::Value::Array||ints.kind!=cnjson::Value::Array||strings.array.size()>4||ints.array.size()>4)throw std::runtime_error("broadcast argument list invalid");
    for(const auto& item:strings.array)if(item.kind!=cnjson::Value::String||item.string.size()>256||item.string.find('\0')!=std::string::npos)throw std::runtime_error("broadcast string argument invalid");
    for(const auto& item:ints.array)if(item.kind!=cnjson::Value::Integer||item.integer<0||item.integer>INT32_MAX)throw std::runtime_error("broadcast integer argument invalid");
    if(found->type==1&&(ints.array.size()!=1||!strings.array.empty()))throw std::runtime_error("elite broadcast requires a chess identifier");
    if(found->type==2&&(!ints.array.empty()||!strings.array.empty()))throw std::runtime_error("shop broadcast parameters are native catalog values");
    if(found->type==3&&(!ints.array.empty()||strings.array.size()>1))throw std::runtime_error("boss broadcast parameters are native catalog values");
    if(found->type==5&&(strings.array.size()!=1||strings.array[0].string.empty()||ints.array.size()!=1||ints.array[0].integer>3))throw std::runtime_error("gift broadcast recipient/sender invalid");
    e.payload.kind=cnjson::Value::Object;for(const char* key:{"uIdx","broadcastId","strParams","intParams"})e.payload.object[key]=at(value,key);return e;
}
inline unsigned crossed(const std::vector<Entry>& catalog,double remaining,unsigned shown){
    if(!std::isfinite(remaining)||remaining<0||remaining>1||shown>7)throw std::runtime_error("boss broadcast progress invalid");
    unsigned result=0,bit=1;const double damage=1.0-remaining;
    for(const auto& entry:catalog)if(entry.type==3){if(!(shown&bit)&&damage+0.0000001>=entry.threshold)result|=bit;bit<<=1;}
    return result;
}
struct Delivery {
    std::string run;int32_t high_revision=0;std::set<std::string> seen;std::map<int32_t,unsigned> boss_shown;
    void session(const std::string& value,int32_t revision){if(value!=run){run=value;seen.clear();boss_shown.clear();high_revision=0;}high_revision=std::max(high_revision,revision);}
    bool unseen(const Event& e)const{return e.run==run&&!seen.count(e.key);}
    void accepted(const Event& e){if(e.run!=run||seen.size()>=8192)throw std::runtime_error("broadcast delivery journal bound");seen.insert(e.key);}
    unsigned mask(int32_t round)const{auto i=boss_shown.find(round);return i==boss_shown.end()?0:i->second;}
    void boss_accepted(int32_t round,unsigned bit){if(round<1||round>15||!bit||bit>4||(bit&(bit-1)))throw std::runtime_error("boss broadcast bit invalid");boss_shown[round]|=bit;}
};
}
