#pragma once
#include <stdexcept>
#include <string>
#include "native_level_compat.h"

// Resolve through the compiled signed-byte adapter table. A display stage id
// is not a LevelUtils input path and must never bypass that adapter.
template<class Rows> std::string native_multiplayer_stage_path(const std::string& stage,const Rows& rows){
    if(stage.size()!=17||stage.compare(0,14,"act1autochess_")!=0){
        if(stage.size()!=17||stage.compare(0,14,"act2autochess_")!=0)throw std::runtime_error("private multiplayer stage is outside the base-map catalog");
    }
    if(stage[14]!='m'||stage[15]!='0'||stage[16]<'1'||stage[16]>'4')throw std::runtime_error("private multiplayer stage is outside the base-map catalog");
    const auto asset="level_"+stage;std::string result;
    for(const auto& row:rows)if(asset==row.assetName){if(!result.empty())throw std::runtime_error("private multiplayer stage has duplicate signed paths");result=row.levelId;}
    if(result.empty())throw std::runtime_error("private multiplayer stage has no signed native adapter path");
    return result;
}

// LevelUtils.LoadLevel keeps the caller's authored path in LevelData.levelId.
// The original stage table uses e.g. Activities/ACT1AUTOCHESS, while the signed
// adapter indexes lowercase resource keys. Match through that same resource
// key normalization, then return only the exact signed path for this stage.
template<class Rows> std::string native_multiplayer_resolve_stage_path(const std::string& stage,const std::string& loaded,const Rows& rows){
    const auto expected=native_multiplayer_stage_path(stage,rows);
    return cnlevels::key(loaded)==expected?expected:std::string{};
}
