#pragma once
#include "native_multiplayer_field_policy.h"
namespace cnrejoin {
// A cold join may be coalesced with the next Help envelope before Unity exists.
// Seed only its authenticated base scene. The original waiting state creates
// no battle; the untouched Help envelope is applied after the real preload.
inline cnjson::Value waiting_for_base(const cnjson::Value& raw){
    if(!cnfield::parse(raw).active)return raw;
    auto result=raw;
    auto& projection=result.object.at("dtoProjection");
    auto wait=[](cnjson::Value& scene){
        if(scene.kind!=cnjson::Value::Object)throw std::runtime_error("private bootstrap scene missing");
        auto& state=scene.object.at("state");state.kind=cnjson::Value::Integer;state.integer=100;
        auto& deadline=scene.object.at("forceEndTs");deadline.kind=cnjson::Value::Integer;deadline.integer=-1;
        for(auto key:{"selfBattleInfo","helpBattleInfo","bossBattleInfo"})scene.object.at(key)=cnjson::Value{};
    };
    wait(projection.object.at("allStateSync").object.at("payload").object.at("allStateSyncData").object.at("sceneDetail"));
    wait(projection.object.at("sceneStatus").object.at("payload").object.at("sceneStatus"));
    result.object["selfReconstruction"]=cnjson::parse("{\"presentation\":\"BATTLE_WAITING\",\"nativeBaseBootstrap\":true}");
    result.object["nativeField"]={};result.object["bossNative"]={};
    return result;
}
}
