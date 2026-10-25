// Research-only. Called once by the private nmd_send after DTO normalization.
#pragma once
#include "native_multiplayer_preparation_write_policy.h"

bool native_multiplayer_preparation_view_may_write_board();
// This is an authenticated current own-view proof, independent of normal
// preparationWriteAllowed. Its facade implementation belongs to root/scene.
bool native_multiplayer_preparation_view_may_choose();

void native_multiplayer_preparation_view_require_request(const std::string& short_type){
    const auto kind=cnpreparation_write::classify(short_type);
    if(kind==cnpreparation_write::Kind::Other)return;
    const bool board=kind==cnpreparation_write::Kind::Board&&native_multiplayer_preparation_view_may_write_board();
    const bool choice=kind==cnpreparation_write::Kind::Choice&&native_multiplayer_preparation_view_may_choose();
    if(!cnpreparation_write::allowed(short_type,board,choice))
        throw cnpreparation_write::Denied(short_type);
}
