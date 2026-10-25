#pragma once
#include "native_json.h"
#include "native_combat_policy.h"
#include <cstdint>
#include <stdexcept>
#include <string>

namespace cnliveness {
inline constexpr std::uint64_t LEASE_MS=8000;
struct Identity {
    std::string run,stage,mode;
    std::int64_t revision=0;
    bool terminal=false;
    std::string terminal_proof={};
    bool same_session(const Identity& other)const{return run==other.run&&stage==other.stage&&mode==other.mode;}
};
inline const cnjson::Value& member(const cnjson::Value& object,const char* key){
    if(object.kind!=cnjson::Value::Object)throw std::runtime_error("private liveness object required");
    auto found=object.object.find(key);if(found==object.object.end())throw std::runtime_error(std::string("private liveness field missing: ")+key);return found->second;
}
inline std::string string(const cnjson::Value& object,const char* key){const auto& v=member(object,key);if(v.kind!=cnjson::Value::String)throw std::runtime_error("private liveness string required");return v.string;}
inline std::int64_t number(const cnjson::Value& object,const char* key){const auto& v=member(object,key);if(v.kind!=cnjson::Value::Integer)throw std::runtime_error("private liveness integer required");return v.integer;}
inline void boolean(const cnjson::Value& object,const char* key,bool expected){const auto& v=member(object,key);if(v.kind!=cnjson::Value::Boolean||v.boolean!=expected)throw std::runtime_error("private liveness transport identity rejected");}
inline Identity identity(const cnjson::Value& state){
    if(string(state,"activityId")!="act2autochess")throw std::runtime_error("private liveness wrong activity");
    Identity result{string(state,"runId"),string(state,"stageId"),string(state,"modeId"),number(state,"revision")};
    if(result.run.size()!=32||result.run.find_first_not_of("0123456789abcdef")!=std::string::npos||!cncombat::stage_allowed(result.stage,result.mode)||result.revision<0||number(state,"round")<0)
        throw std::runtime_error("private liveness session identity rejected");
    const auto phase=string(state,"phase");
    if(!cncombat::state_allowed(phase,int(number(state,"round")),result.mode))throw std::runtime_error("private liveness session phase rejected");
    result.terminal=phase=="result";
    return result;
}
inline Identity terminal_identity(const cnjson::Value& state,const cnjson::Value& summary){
    auto result=identity(state);if(!result.terminal)return result;
    const auto& outcome=member(state,"result");const auto& success=member(outcome,"success");
    if(success.kind!=cnjson::Value::Boolean)throw std::runtime_error("private liveness terminal outcome missing");
    auto passed=number(outcome,"roundsPassed");
    if(passed<0||passed>number(state,"round")||passed>15||string(summary,"modeId")!=result.mode||string(summary,"bandId")!=string(state,"bandId"))
        throw std::runtime_error("private liveness terminal summary identity mismatch");
    const auto& teams=member(summary,"teamInfo");
    if(teams.kind!=cnjson::Value::Array||teams.array.size()!=1)throw std::runtime_error("private liveness terminal self team missing");
    const auto& team=teams.array[0];
    // Pinned CN AutoChessSettleGamePlayerStatus: INTERRUPT=0, DEAD=1, PASS=2.
    auto abandoned=outcome.object.find("abandoned");bool interrupted=false;
    if(abandoned!=outcome.object.end()){if(abandoned->second.kind!=cnjson::Value::Boolean)throw std::runtime_error("private liveness terminal abandoned flag invalid");interrupted=abandoned->second.boolean;}
    if(string(team,"uid")!="local-"+result.run||number(team,"uidIndex")!=0||number(team,"passRound")!=passed||number(team,"gameCode")!=(success.boolean?2:interrupted?0:1))
        throw std::runtime_error("private liveness terminal self outcome mismatch");
    boolean(summary,"gameFinished",!interrupted);boolean(summary,"isViolation",false);
    if(number(summary,"startTs")<=0||number(summary,"endTs")<number(summary,"startTs"))throw std::runtime_error("private liveness terminal timestamps invalid");
    for(const auto* key:{"onStageChars","onStageBond","bossRecord"}){const auto& rows=member(summary,key);if(rows.kind!=cnjson::Value::Array||rows.array.size()>128)throw std::runtime_error("private liveness terminal details invalid");}
    result.terminal_proof=cnjson::dump(outcome)+"\n"+cnjson::dump(summary);return result;
}
inline bool can_advance(const Identity& expected,const Identity& observed){
    return observed.same_session(expected)&&observed.revision>=expected.revision&&
        (!expected.terminal||(observed.terminal&&!expected.terminal_proof.empty()&&observed.terminal_proof==expected.terminal_proof));
}
inline Identity validate_reply(const cnjson::Value& reply,const Identity& expected){
    boolean(reply,"ok",true);boolean(reply,"officialFallback",false);boolean(reply,"officialWireCompatible",false);
    if(string(reply,"activityId")!="act2autochess"||string(reply,"runId")!=expected.run)throw std::runtime_error("private liveness envelope identity mismatch");
    auto result=identity(member(reply,"state"));
    if(result.terminal){if(string(reply,"nativeSettleGameInfoType")!="Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo")throw std::runtime_error("private liveness terminal DTO type mismatch");result=terminal_identity(member(reply,"state"),member(reply,"nativeSettleGameInfo"));}
    if(!can_advance(expected,result)||number(reply,"revision")!=result.revision)throw std::runtime_error("private liveness state identity/revision mismatch");
    return result;
}
struct Lease {
    std::uint64_t expires=0;
    void confirm(std::uint64_t now){expires=now+LEASE_MS;}
    void revoke(){expires=0;}
    bool valid(std::uint64_t now)const{return expires&&now<expires;}
};
inline bool scope(bool prepared,bool in_flight,bool scene_entered,const std::string& ui_activity,const std::string& service_activity){
    return prepared&&in_flight&&scene_entered&&ui_activity=="act2autochess"&&service_activity=="act2autochess";
}
}
