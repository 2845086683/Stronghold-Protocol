// Bounded diagnostics only. Callers retain their original rejection/callback
// behavior. No URL, credential, request body or arbitrary server text is saved.
#pragma once
#include <mutex>
namespace cnmp_trace {
inline const cnjson::Value& at(const cnjson::Value& v,const char* key){static const cnjson::Value nil;auto i=v.object.find(key);return v.kind==cnjson::Value::Object&&i!=v.object.end()?i->second:nil;}
inline std::string text(const cnjson::Value& v){return v.kind==cnjson::Value::String?v.string:std::string();}
inline int64_t number(const cnjson::Value& v,int64_t fallback=-1){return v.kind==cnjson::Value::Integer?v.integer:fallback;}
inline cnjson::Value object(){cnjson::Value v;v.kind=cnjson::Value::Object;return v;}
inline cnjson::Value string(const std::string& s){cnjson::Value v;v.kind=cnjson::Value::String;v.string=s;return v;}
inline cnjson::Value integer(int64_t n){cnjson::Value v;v.kind=cnjson::Value::Integer;v.integer=n;return v;}
inline bool one_of(const std::string& s,std::initializer_list<const char*> values){for(auto v:values)if(s==v)return true;return false;}
inline std::string operation(const std::string& s){return one_of(s,{"create","join","reconnect","leave","heartbeat","start","room.ready","room.return","room.dissolve","scene.quit","confirm","band.choose","protocol","battle.report","battle.ready","battle.info","battle.finish","battle.escape","help.finish","help.kill","help.escape","field.ready","field.failed","loading.ready","ready","board.move","board.equip","board.destroy","shop.buy","shop.refresh","shop.freeze","shop.upgrade","choice.pick","item.use","bond.add","pause","resume","boss.action","batch"})?s:"other";}
inline std::string protocol(const cnjson::Value& payload){auto value=text(at(at(payload,"payload"),"type"));const auto plus=value.rfind('+');if(plus!=std::string::npos)value=value.substr(plus+1);return one_of(value,{"AutoChessBattleSceneBroadcastUp","AutoChessBattleSceneLoadingReadyUp","AutoChessBattleScenePreparationReadyUp","AutoChessBattleSceneSelfBattleInfoUp","AutoChessBattleSceneSelfBattleFinishUp","AutoChessBattleSceneSelfBattleEnemyEscapeUp","AutoChessBattleSceneHelpBattleFinishUp","AutoChessBattleSceneHelpBattleKillEnemyUp","AutoChessBattleSceneHelpBattleEnemyEscapeUp","AutoChessBattleSceneActionUp","AutoChessBattleSceneShopBuyChessUp","AutoChessBattleSceneShopRefreshUp","AutoChessBattleSceneShopFrozeUp","AutoChessBattleSceneShopUpgradeUp","AutoChessBattleSceneUseEquipUp","AutoChessBattleSceneUseMagicUp","AutoChessBattleSceneDestroyChessUp","AutoChessBattleSceneChangePositionUp","AutoChessBattleSceneSelfBattleAddBondUp","AutoChessBattleSceneSpPreparationPickUp","AutoChessBattleSceneSelfChoicePickUp","AutoChessBattleScenePauseUp","AutoChessBattleSceneResumeUp","AutoChessBattleSceneQuitUp"})?value:value.empty()?"":"other";}
inline std::string route_kind(const std::string& route){
    const std::string prefix="/bridge/act2autochess/multiplayer/";
    if(route.rfind(prefix,0)!=0)return "outside";
    const auto rest=route.substr(prefix.size());
    if(rest=="participants")return "participants";
    if(rest=="rooms")return "rooms.create";
    if(rest=="rooms/join")return "rooms.join";if(rest.size()==17&&rest.rfind("rooms/",0)==0&&rest.substr(6,6).find_first_not_of("0123456789")==std::string::npos&&rest.substr(12)=="/join")return "rooms.join";
    if(rest.rfind("rooms/",0)==0){const auto tail=rest.substr(6);if(tail.size()>=32&&tail.substr(0,32).find_first_not_of("0123456789abcdef")==std::string::npos){const auto suffix=tail.substr(32);if(suffix=="/commands")return "room.commands";if(suffix=="/commands/batch")return "room.commands.batch";if(suffix.empty()||suffix.front()=='?')return "room.snapshot";}return "rooms.other";}
    if(rest.rfind("solo/",0)==0)return "solo";
    return "other";
}
inline std::string business_error(const cnjson::Value& response){const auto s=text(at(response,"error"));return one_of(s,{"protocol_adapter_pending","multiplayer_authentication_required","not_found","room_not_found","room_closed","room_expired","invalid_command","invalid_phase","stale_generation","stale_round","invalid_battle_report","invalid_help_report","member_not_active","not_room_member","operation_not_allowed","insufficient_coin","insufficient_gold","unknown_protocol"})?s:s.empty()?"":"other";}
inline std::string session(const cnjson::Value& view){const auto room=text(at(view,"roomId"));const auto seq=number(at(view,"matchSequence"),0);if(room.size()!=32||room.find_first_not_of("0123456789abcdef")!=std::string::npos||seq<0)return {};return room+":"+std::to_string(seq);}
inline cnjson::Value context(const cnjson::Value& view){auto row=object();for(const auto* key:{"round","generation","seat"})row.object[key]=integer(number(at(view,key)));const auto p=text(at(view,"phase"));row.object["phase"]=string(one_of(p,{"building","confirm","strategy","loading","preparation","battle_countdown","battle","help","boss","special","paused","result"})?p:"other");return row;}
class Ledger {
    mutable std::mutex mutex_;std::string session_;cnjson::Value first_http_,first_native_,last_http_,last_native_;unsigned http_count_=0,native_count_=0;
public:
    void record(const std::string& owner,bool native,cnjson::Value row){if(owner.empty())return;std::lock_guard<std::mutex> lock(mutex_);if(session_!=owner){session_=owner;first_http_={};first_native_={};last_http_={};last_native_={};http_count_=native_count_=0;}auto& count=native?native_count_:http_count_;auto& first=native?first_native_:first_http_;auto& last=native?last_native_:last_http_;++count;if(first.kind==cnjson::Value::Null)first=row;last=std::move(row);}
    cnjson::Value status()const{std::lock_guard<std::mutex> lock(mutex_);auto v=object();v.object["httpFailures"]=integer(http_count_);v.object["nativeExceptions"]=integer(native_count_);v.object["firstHttpFailure"]=first_http_;v.object["firstNativeException"]=first_native_;v.object["lastHttpFailure"]=last_http_;v.object["lastNativeException"]=last_native_;return v;}
};
inline Ledger ledger;
inline void http_failure(const std::string& route,const cnjson::Value* payload,const cnjson::Value& response,int64_t status,const cnjson::Value& view){auto row=context(view);row.object["source"]=string("private-http");row.object["route"]=string(route_kind(route));row.object["httpStatus"]=integer(status);row.object["businessError"]=string(business_error(response));row.object["businessResult"]=integer(number(at(response,"result")));row.object["operation"]=string(payload?operation(text(at(*payload,"op"))):"snapshot");row.object["protocol"]=string(payload?protocol(*payload):"");ledger.record(session(view),false,std::move(row));}
inline void native_exception(const cnjson::Value& view,uint64_t epoch,int32_t exception,int32_t parameter){auto row=context(view);row.object["source"]=string("AutoChessBattleServer._SetException");row.object["serviceEpoch"]=integer(int64_t(epoch));row.object["exception"]=integer(exception);row.object["parameter"]=integer(parameter);ledger.record(session(view),true,std::move(row));}
}
