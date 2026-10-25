#pragma once
#include <algorithm>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace cncombat {
struct KillInfo {std::int32_t enemy_id=0,attacker_id=-1,damage_src=0;};
struct CharacterStatus {std::int32_t inst_id=0,hp=0,tech=0;};
struct Position {std::int32_t inst_id=0,position=0,chess_type=0,direction=0;};
static_assert(sizeof(KillInfo)==12,"CN Win64 BattleEnemyKilledInfo is an indirect 12-byte value");

// CN StageDatasDict positive-weight maps and four SINGLE difficulty curves.
inline bool mode_allowed(const std::string& mode){
    return mode=="mode_single_funny"||mode=="mode_single_normal"||mode=="mode_single_hard"||mode=="mode_single_abyss";
}
inline bool stage_allowed(const std::string& stage,const std::string& mode=""){
    const bool first=stage=="act1autochess_m01";
    const bool other=stage=="act1autochess_m02"||stage=="act1autochess_m03"||stage=="act1autochess_m04"||
        stage=="act2autochess_m01"||stage=="act2autochess_m02"||stage=="act2autochess_m03"||stage=="act2autochess_m04";
    if(mode.empty())return first||other;
    if(!mode_allowed(mode))return false;
    if(mode=="mode_single_funny")return first;
    return other||(first&&mode=="mode_single_normal");
}
inline std::int32_t final_round(const std::string& mode){return !mode_allowed(mode)?0:mode=="mode_single_funny"?9:14;}
inline bool is_boss_round(std::int32_t round,const std::string& mode){
    auto last=final_round(mode);return last&&(round==last||(last==14&&round==15));
}
inline bool state_allowed(const std::string& phase,std::int32_t round,const std::string& mode="mode_single_funny"){
    auto last=final_round(mode);if(!last)return false;
    auto maximum=last==14?15:9;
    return (phase=="loading"&&round==0)||
        ((phase=="preparation"||phase=="special"||phase=="paused"||phase=="result")&&round>=1&&round<=maximum)||
        (phase=="battle"&&round>=1&&round<last)||(phase=="boss"&&is_boss_round(round,mode));
}
inline bool preparation_resume(const std::string& phase,std::int32_t round,const std::string& mode="mode_single_funny"){
    return phase=="preparation"&&state_allowed(phase,round,mode);
}
inline std::int32_t encode_position(std::int32_t x,std::int32_t y){
    if(x<0||x>65535||y<0||y>32767)throw std::runtime_error("private board coordinate outside native encoding");
    auto value=std::uint32_t(y)*65536u+std::uint32_t(x);
    if(!value)throw std::runtime_error("private deployed position encodes a hand slot");
    return static_cast<std::int32_t>(value);
}
inline std::string deployed_signature(const std::vector<Position>& positions){
    if(positions.size()>128)throw std::runtime_error("private board exceeds native bound");
    std::map<std::int32_t,Position> ordered;std::set<std::int32_t> tiles;
    for(const auto& p:positions){
        if(p.inst_id<=0||p.direction<0||p.direction>3||!ordered.emplace(p.inst_id,p).second)
            throw std::runtime_error("private native board identity or direction invalid");
        if(p.position>0&&!tiles.insert(p.position).second)throw std::runtime_error("private native board tile collision");
    }
    std::string result;
    for(const auto& pair:ordered){const auto& p=pair.second;if(p.position>0)
        result+=std::to_string(p.inst_id)+":"+std::to_string(p.position)+":"+std::to_string(p.direction)+";";}
    return result;
}
inline std::string layout_signature(const std::vector<Position>& positions){
    // Native serializers use 0,-1,-2,... for the shared character/item hand.
    // A hand-only drag is authoritative layout, even with no battlefield change.
    std::vector<Position> owned;std::map<std::tuple<int32_t,int32_t,int32_t>,Position> tokens;
    for(const auto& p:positions)if(p.chess_type==3){
        if(p.inst_id<=0||p.direction<0||p.direction>3||!tokens.emplace(std::make_tuple(p.inst_id,p.position,p.direction),p).second)
            throw std::runtime_error("private native token identity or direction invalid");
    }else owned.push_back(p);
    deployed_signature(owned);
    std::map<std::int32_t,Position> ordered;std::set<std::int32_t> hand;
    std::set<std::int32_t> tiles;
    for(const auto& p:owned){
        if(p.position<=0&&!hand.insert(p.position).second)throw std::runtime_error("private native hand slot collision");
        if(p.position>0)tiles.insert(p.position);
        ordered.emplace(p.inst_id,p);
    }
    std::string result;
    for(const auto& pair:ordered){const auto& p=pair.second;
        result+=std::to_string(p.inst_id)+":"+std::to_string(p.position)+":"+std::to_string(p.direction)+";";}
    for(const auto& pair:tokens){const auto& p=pair.second;auto owner=ordered.find(p.inst_id);
        if(owner==ordered.end()||owner->second.chess_type!=0)throw std::runtime_error("private native token owner missing");
        if(!(p.position>0?tiles:hand).insert(p.position).second)throw std::runtime_error("private native token tile collision");
        result+="T:"+std::to_string(p.inst_id)+":"+std::to_string(p.position)+":"+std::to_string(p.direction)+";";
    }
    return result;
}
inline void normalize_report(const std::set<std::int32_t>& issued,const std::set<std::int32_t>& owned,
        std::vector<std::int32_t>& escaped,std::vector<KillInfo>& killed,std::vector<CharacterStatus>& characters,bool complete=true){
    if(issued.empty()||issued.size()>128)throw std::runtime_error("private battle enemy identity bound invalid");
    std::set<std::int32_t> escaped_set;
    for(auto id:escaped)if(!issued.count(id))throw std::runtime_error("private escaped enemy identity unknown");else escaped_set.insert(id);
    std::map<std::int32_t,KillInfo> killed_map;
    for(const auto& k:killed){
        if(!issued.count(k.enemy_id)||escaped_set.count(k.enemy_id)||k.damage_src<0||k.damage_src>3||
           (k.attacker_id!=-1&&!owned.count(k.attacker_id)))throw std::runtime_error("private kill identity, attacker or damage source invalid");
        auto it=killed_map.find(k.enemy_id);
        if(it!=killed_map.end()&&(it->second.attacker_id!=k.attacker_id||it->second.damage_src!=k.damage_src))
            throw std::runtime_error("private duplicate kill records conflict");
        killed_map[k.enemy_id]=k;
    }
    if(complete&&escaped_set.size()+killed_map.size()!=issued.size())throw std::runtime_error("private final enemy partition incomplete");
    std::map<std::int32_t,CharacterStatus> character_map;
    for(const auto& c:characters){
        if(!owned.count(c.inst_id)||c.hp<0||c.hp>100||c.tech<0||c.tech>100)throw std::runtime_error("private character battle status invalid");
        auto it=character_map.find(c.inst_id);
        if(it!=character_map.end()&&(it->second.hp!=c.hp||it->second.tech!=c.tech))throw std::runtime_error("private duplicate character status conflicts");
        character_map[c.inst_id]=c;
    }
    escaped.assign(escaped_set.begin(),escaped_set.end());killed.clear();characters.clear();
    for(const auto& pair:killed_map)killed.push_back(pair.second);
    for(const auto& pair:character_map)characters.push_back(pair.second);
}
}
