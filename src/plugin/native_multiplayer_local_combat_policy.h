#pragma once
#include "native_json.h"
#include <string>

namespace cnlocal_combat {
// These native notifications are telemetry. The original FinishUp already
// contains the complete disjoint kill/escape partition and terminal operators.
// Keep native optimistic Rune/layer execution; layer writes retain ordered
// durable server receipts. Shared Help and Boss protocols are never suppressed.
inline bool local_only(const cnjson::Value& view,const std::string& name,uint64_t cookie){
    if(cookie||view.kind!=cnjson::Value::Object)return false;
    const auto phase=view.object.find("phase");
    if(phase==view.object.end()||phase->second.kind!=cnjson::Value::String||phase->second.string!="battle")return false;
    const auto role=view.object.find("observerOnly");
    if(role!=view.object.end()&&role->second.kind==cnjson::Value::Boolean&&role->second.boolean&&
       (name=="AutoChessBattleSceneSelfBattleFinishUp"||name=="AutoChessBattleSceneSelfBattleAddBondUp"))return true;
    return name=="AutoChessBattleSceneSelfBattleInfoUp"||name=="AutoChessBattleSceneSelfBattleKillEnemyUp"||name=="AutoChessBattleSceneSelfBattleEnemyEscapeUp";
}
}
