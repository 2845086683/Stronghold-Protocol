#pragma once
#include "native_json.h"
#include "native_combat_policy.h"
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

// A journal is evidence for one already captured private result, never a stored
// endpoint, path or arbitrary command. The server remains the final authority.
namespace cnpj {
inline constexpr std::size_t max_bytes=1024*1024;
inline bool run_id(const std::string& value){
    return value.size()==32&&value.find_first_not_of("0123456789abcdef")==std::string::npos;
}
inline bool kind(const std::string& value){return value=="combat"||value=="garrison";}
inline const cnjson::Value& at(const cnjson::Value& value,const char* key){
    auto it=value.object.find(key);if(value.kind!=cnjson::Value::Object||it==value.object.end())throw std::runtime_error("private journal field missing");return it->second;
}
inline std::string string(const cnjson::Value& value,const char* key){
    const auto& item=at(value,key);if(item.kind!=cnjson::Value::String)throw std::runtime_error("private journal string invalid");return item.string;
}
inline std::int32_t integer(const cnjson::Value& value,const char* key){
    const auto& item=at(value,key);if(item.kind!=cnjson::Value::Integer||item.integer<INT32_MIN||item.integer>INT32_MAX)throw std::runtime_error("private journal integer invalid");return std::int32_t(item.integer);
}
inline cnjson::Value object(){cnjson::Value v;v.kind=cnjson::Value::Object;return v;}
inline cnjson::Value array(){cnjson::Value v;v.kind=cnjson::Value::Array;return v;}
inline cnjson::Value text(const std::string& s){cnjson::Value v;v.kind=cnjson::Value::String;v.string=s;return v;}
inline cnjson::Value number(std::int32_t n){cnjson::Value v;v.kind=cnjson::Value::Integer;v.integer=n;return v;}
inline std::set<std::int32_t> ids(const cnjson::Value& values){
    if(values.kind!=cnjson::Value::Array||values.array.size()>128)throw std::runtime_error("private journal identity list invalid");
    std::set<std::int32_t> result;for(const auto& value:values.array){
        if(value.kind!=cnjson::Value::Integer||value.integer<1||value.integer>INT32_MAX||!result.insert(std::int32_t(value.integer)).second)
            throw std::runtime_error("private journal identity invalid");
    }return result;
}
inline cnjson::Value id_array(const std::set<std::int32_t>& ids){auto v=array();for(auto id:ids)v.array.push_back(number(id));return v;}
inline std::string request_id(const std::string& run,std::int32_t round=1,
        const std::string& mode="mode_single_funny",const std::string& stage="act1autochess_m01",std::int32_t attempt=0){
    if(!run_id(run)||!cncombat::mode_allowed(mode)||!cncombat::stage_allowed(stage,mode)||
       !cncombat::state_allowed(cncombat::is_boss_round(round,mode)?"boss":"battle",round,mode)||attempt<0)
        throw std::runtime_error("private journal run/round/mode/stage invalid");
    return std::string(cncombat::is_boss_round(round,mode)?"native-boss-finish-":"native-self-finish-")+run+"-"+std::to_string(round)+(attempt?"-r"+std::to_string(attempt):"");
}
struct CombatRecord {
    std::string run,request,raw,mode,stage;
    std::int32_t round=0,attempt=0;
    std::set<std::int32_t> issued,owned;
    cnjson::Value payload;
};
inline CombatRecord decode(const std::string& raw,const std::string& run,const char* metadata,const char* assembly,
        const std::string& mode="mode_single_funny",const std::string& stage="act1autochess_m01"){
    if(!run_id(run)||raw.empty()||raw.size()>max_bytes)throw std::runtime_error("private combat journal size or run invalid");
    const auto value=cnjson::parse(raw);
    const auto round=integer(value,"round"),version=integer(value,"version");
    const auto attempt=version==3?integer(value,"battleAttempt"):0;
    if((version!=2&&version!=3)||(version==2&&value.object.size()!=13)||(version==3&&(value.object.size()!=14||attempt<=0))||!cncombat::mode_allowed(mode)||!cncombat::stage_allowed(stage,mode)||
       !cncombat::state_allowed(cncombat::is_boss_round(round,mode)?"boss":"battle",round,mode)||
       string(value,"activityId")!="act2autochess"||string(value,"modeId")!=mode||string(value,"stageId")!=stage||
       string(value,"scope")!="CN-native-intro-run"||string(value,"metadataSha256")!=metadata||string(value,"gameAssemblySha256")!=assembly||
       string(value,"runId")!=run||string(value,"requestId")!=request_id(run,round,mode,stage,attempt))throw std::runtime_error("private combat journal binding mismatch");
    CombatRecord record;record.run=run;record.round=round;record.attempt=attempt;record.request=request_id(run,round,mode,stage,attempt);record.raw=raw;record.mode=mode;record.stage=stage;
    record.issued=ids(at(value,"issuedEnemyIds"));record.owned=ids(at(value,"ownedChessIds"));record.payload=at(value,"payload");
    const auto& p=record.payload;
    const bool has_elapsed=p.kind==cnjson::Value::Object&&p.object.count("elapsedSeconds");
    const bool has_player_hp=cncombat::is_boss_round(round,mode)&&p.kind==cnjson::Value::Object&&p.object.count("playerHp");
    const bool has_broadcast_mask=cncombat::is_boss_round(round,mode)&&p.kind==cnjson::Value::Object&&p.object.count("bossBroadcastMask");
    const bool has_attempt=p.kind==cnjson::Value::Object&&p.object.count("battleAttempt");
    if((attempt>0&&(!has_attempt||integer(p,"battleAttempt")!=attempt))||(attempt==0&&has_attempt))
        throw std::runtime_error("private combat journal attempt mismatch");
    if(has_elapsed&&(integer(p,"elapsedSeconds")<0||integer(p,"elapsedSeconds")>86400))
        throw std::runtime_error("private combat journal elapsed time invalid");
    if(has_player_hp&&integer(p,"playerHp")<0)throw std::runtime_error("private boss journal player HP invalid");
    if(has_broadcast_mask&&(integer(p,"bossBroadcastMask")<0||integer(p,"bossBroadcastMask")>7))throw std::runtime_error("private boss journal broadcast mask invalid");
    const auto report_fields=p.object.size()-(has_elapsed?1:0)-(has_player_hp?1:0)-(has_broadcast_mask?1:0)-(has_attempt?1:0);
    if(cncombat::is_boss_round(round,mode)){
        if(p.kind!=cnjson::Value::Object||(report_fields!=4&&report_fields!=7)||integer(p,"round")!=round||integer(p,"bossHp")<0)
            throw std::runtime_error("private boss journal report header invalid");
        const auto result=string(p,"result");if(result!="victory"&&result!="defeat"&&result!="interrupt")throw std::runtime_error("private boss journal result invalid");
        if(result=="victory"&&integer(p,"bossHp")>1)throw std::runtime_error("private boss victory without defeated native boss");
        const auto& chars=at(p,"charBattleStatusList");if(chars.kind!=cnjson::Value::Array||chars.array.size()>128)throw std::runtime_error("private boss journal character list invalid");
        std::set<std::int32_t> seen;for(const auto& c:chars.array){const auto id=integer(c,"instId"),hp=integer(c,"hp"),tech=integer(c,"tech");
            if(c.object.size()!=3||!record.owned.count(id)||!seen.insert(id).second||hp<0||hp>100||tech<0||tech>100)throw std::runtime_error("private boss journal character status invalid");}
        if(report_fields==7){
            const auto& tokens=at(p,"escapedTokenInstIds");const auto& escapes=at(p,"escapedInstIds");const auto& kills=at(p,"killedInsts");
            if(tokens.kind!=cnjson::Value::Array||tokens.array.size()>4096||escapes.kind!=cnjson::Value::Array||escapes.array.size()>128||kills.kind!=cnjson::Value::Array||kills.array.size()>128)
                throw std::runtime_error("private boss journal side-enemy lists invalid");
            std::set<int32_t> token_ids;for(const auto& id:tokens.array)if(id.kind!=cnjson::Value::Integer||id.integer<0||id.integer>INT32_MAX||!token_ids.insert(int32_t(id.integer)).second)throw std::runtime_error("private boss journal summoned escape invalid");
            std::vector<int32_t> escaped;for(auto id:ids(escapes))escaped.push_back(id);
            std::vector<cncombat::KillInfo> killed;std::vector<cncombat::CharacterStatus> unused;
            for(const auto& v:kills.array){if(v.object.size()!=3)throw std::runtime_error("private boss journal kill shape invalid");killed.push_back({integer(v,"enemyInstId"),integer(v,"attackerInstId"),integer(v,"damageSrc")});}
            cncombat::normalize_report(record.issued,record.owned,escaped,killed,unused,false);
        }
        return record;
    }
    if(p.kind!=cnjson::Value::Object||report_fields!=6||integer(p,"round")!=round||at(p,"fromRejoin").kind!=cnjson::Value::Boolean||at(p,"fromRejoin").boolean)
        throw std::runtime_error("private combat journal report header invalid");
    const auto& token=at(p,"escapedTokenInstIds");const auto& escape=at(p,"escapedInstIds");const auto& kills=at(p,"killedInsts");const auto& chars=at(p,"charBattleStatusList");
    if(token.kind!=cnjson::Value::Array||token.array.size()>4096||escape.kind!=cnjson::Value::Array||kills.kind!=cnjson::Value::Array||chars.kind!=cnjson::Value::Array||
       escape.array.size()>128||kills.array.size()>128||chars.array.size()>128)throw std::runtime_error("private combat journal report collections invalid");
    std::set<std::int32_t> token_ids;for(const auto& id:token.array){if(id.kind!=cnjson::Value::Integer||id.integer<0||id.integer>INT32_MAX||!token_ids.insert(int32_t(id.integer)).second)throw std::runtime_error("private combat journal token identity invalid");}
    std::vector<std::int32_t> escaped;for(auto id:ids(escape))escaped.push_back(id);
    std::vector<cncombat::KillInfo> killed;for(const auto& v:kills.array){if(v.object.size()!=3)throw std::runtime_error("private combat journal kill shape invalid");killed.push_back({integer(v,"enemyInstId"),integer(v,"attackerInstId"),integer(v,"damageSrc")});}
    std::vector<cncombat::CharacterStatus> characters;for(const auto& v:chars.array){if(v.object.size()!=3)throw std::runtime_error("private combat journal character shape invalid");characters.push_back({integer(v,"instId"),integer(v,"hp"),integer(v,"tech")});}
    cncombat::normalize_report(record.issued,record.owned,escaped,killed,characters);
    return record;
}
inline std::string encode(const std::string& run,const char* metadata,const char* assembly,const std::set<std::int32_t>& issued,const std::set<std::int32_t>& owned,const cnjson::Value& payload,
        const std::string& mode="mode_single_funny",const std::string& stage="act1autochess_m01",std::int32_t attempt=0){
    const auto round=integer(payload,"round");
    if(attempt<0)throw std::runtime_error("private combat journal attempt invalid");
    auto value=object();value.object["version"]=number(attempt?3:2);value.object["round"]=number(round);value.object["activityId"]=text("act2autochess");value.object["modeId"]=text(mode);value.object["stageId"]=text(stage);
    if(attempt)value.object["battleAttempt"]=number(attempt);
    // Keep the v2 scope label and request spelling for previously captured
    // introductory journals. Actual authority is explicitly bound to the
    // allowlisted mode/stage above and rechecked against live server state.
    value.object["scope"]=text("CN-native-intro-run");value.object["metadataSha256"]=text(metadata);value.object["gameAssemblySha256"]=text(assembly);value.object["runId"]=text(run);value.object["requestId"]=text(request_id(run,round,mode,stage,attempt));
    value.object["issuedEnemyIds"]=id_array(issued);value.object["ownedChessIds"]=id_array(owned);value.object["payload"]=payload;
    auto raw=cnjson::dump(value);decode(raw,run,metadata,assembly,mode,stage);return raw;
}
// Retry indefinitely at 5, 10, 20, 40, then 60 seconds; attempts cannot wrap.
inline std::uint64_t retry_delay(unsigned attempts){return attempts==0?0:attempts<5?(std::uint64_t(5000)<<(attempts-1)):60000;}
inline void attempted(unsigned& attempts){if(attempts<std::numeric_limits<unsigned>::max())++attempts;}
}
