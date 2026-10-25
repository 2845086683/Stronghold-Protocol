#pragma once
#include "native_json.h"
#include <map>
#include <string>
#include <vector>

namespace cnmulti_mode {
inline const cnjson::Value& at(const cnjson::Value& v,const char* k){auto p=v.object.find(k);if(v.kind!=cnjson::Value::Object||p==v.object.end())throw std::runtime_error(std::string("multiplayer mode field missing: ")+k);return p->second;}
inline std::string text(const cnjson::Value& v,const char* k){const auto& p=at(v,k);if(p.kind!=cnjson::Value::String)throw std::runtime_error("multiplayer mode string required");return p.string;}
inline int64_t number(const cnjson::Value& v,const char* k){const auto& p=at(v,k);if(p.kind!=cnjson::Value::Integer)throw std::runtime_error("multiplayer mode integer required");return p.integer;}
inline bool allowed(const std::string& id){return id=="aceffect_enemy_1"||id=="aceffect_enemy_2"||id=="aceffect_enemy_2_2"||id=="aceffect_enemy_3"||id=="aceffect_enemy_4";}
inline std::vector<std::string> ids(const cnjson::Value& v){
    if(v.kind!=cnjson::Value::Array||v.array.size()>16)throw std::runtime_error("multiplayer mode Rune list outside bound");
    std::vector<std::string> out;for(const auto& x:v.array){if(x.kind!=cnjson::Value::String||!allowed(x.string))throw std::runtime_error("multiplayer mode Rune outside CN templates");out.push_back(x.string);}return out;
}
struct Plan {
    bool active=false;std::string room,run,mode,stage,cycle;int64_t match=0,round=0,seat=-1;std::vector<std::string> effects;
};
inline std::vector<std::string> strength_ids(const cnjson::Value& strength){
    const auto& complete=at(strength,"nativeProjectionComplete");
    if(number(strength,"version")!=1||complete.kind!=cnjson::Value::Boolean||!complete.boolean)throw std::runtime_error("multiplayer COMMON projection incomplete");
    return ids(at(strength,"nativeEffectIds"));
}
inline void own_ids(const cnjson::Value& self,Plan& p){
    if(number(self,"modeStrengthVersion")!=1||number(self,"multiplayerPolicyVersion")!=1)throw std::runtime_error("multiplayer COMMON mode version unsupported");
    const auto& outer=at(self,"multiplayerNativeProjectionComplete");if(outer.kind!=cnjson::Value::Boolean||!outer.boolean)throw std::runtime_error("multiplayer COMMON projection incomplete");
    auto own=ids(at(self,"nativeModeEffectIds"));
    if(own!=strength_ids(at(self,"multiplayerModeStrength"))||own!=p.effects)throw std::runtime_error("multiplayer COMMON policy/list mismatch");
    auto battle=self.object.find("battle");if(battle!=self.object.end()&&battle->second.kind==cnjson::Value::Object&&own!=ids(at(battle->second,"nativeModeEffectIds")))throw std::runtime_error("multiplayer COMMON battle/list mismatch");
}
inline Plan parse(const cnjson::Value& view){
    Plan p;if(view.kind!=cnjson::Value::Object||text(view,"phase")=="result")return p;const auto& self=at(view,"selfState");if(self.kind==cnjson::Value::Null||number(view,"round")==0)return p;
    p.active=true;p.room=text(view,"roomId");p.run=text(self,"runId");p.mode=text(view,"modeId");p.stage=text(self,"stageId");p.match=number(view,"matchSequence");p.round=number(view,"round");p.seat=number(view,"seat");
    if(text(view,"activityId")!="act2autochess"||text(self,"activityId")!="act2autochess"||text(self,"modeId")!=p.mode||p.room.empty()||p.run.empty()||p.stage.empty()||p.match<1||p.round<1||p.round>15||p.seat<0||p.seat>3||
       (p.mode!="mode_multi_funny"&&p.mode!="mode_multi_normal"&&p.mode!="mode_multi_hard"&&p.mode!="mode_multi_abyss"))throw std::runtime_error("multiplayer mode room/run/mode/round identity mismatch");
    const auto own_round=number(self,"round");auto envelope=view.object.find("nativeCommonMode");
    if(envelope!=view.object.end()&&envelope->second.kind!=cnjson::Value::Null){
        const auto& common=envelope->second;const auto& observer=at(common,"observer");
        if(number(common,"version")!=1||text(common,"roomId")!=p.room||text(common,"runId")!=p.run||text(common,"modeId")!=p.mode||number(common,"matchSequence")!=p.match||number(common,"round")!=p.round||number(common,"seat")!=p.seat||observer.kind!=cnjson::Value::Boolean)throw std::runtime_error("multiplayer COMMON envelope owner mismatch");
        p.effects=ids(at(common,"nativeEffectIds"));if(p.effects!=strength_ids(at(common,"nativeModeStrength")))throw std::runtime_error("multiplayer COMMON envelope/list mismatch");
        if(observer.boolean){
            if(number(at(self,"player"),"hp")>0||own_round<1||own_round>=p.round||number(self,"multiplayerPolicyVersion")!=1)throw std::runtime_error("multiplayer COMMON observer is not an eliminated prior-round seat");
        }else{
            if(own_round!=p.round)throw std::runtime_error("multiplayer COMMON active round mismatch");own_ids(self,p);
        }
    }else{
        if(own_round!=p.round)throw std::runtime_error("multiplayer COMMON legacy round mismatch");
        p.effects=ids(at(self,"nativeModeEffectIds"));own_ids(self,p);
    }
    const auto phase=text(view,"phase");p.cycle=phase=="help"?"help":"normal";
    if(phase=="help")p.cycle+=":"+text(at(view,"nativeField"),"planHash");
    return p;
}
inline bool same(const Plan& a,const Plan& b){return a.active&&b.active&&a.room==b.room&&a.run==b.run&&a.mode==b.mode&&a.stage==b.stage&&a.cycle==b.cycle&&a.match==b.match&&a.round==b.round&&a.seat==b.seat&&a.effects==b.effects;}
inline std::map<std::string,size_t> counts(const std::vector<std::string>& ids){std::map<std::string,size_t> out;for(const auto& id:ids)++out[id];return out;}
}
