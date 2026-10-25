// Explicit MULTI/TEAM alternative to the established private LOCAL launch.
// The original utility, controller, multiplayer bridge and game mode retain
// ownership of their resource loading, map composition and Unity battle.
#pragma once
#include "native_multiplayer_phase_policy.h"
bool native_multiplayer_launch_util(void* raw_param,const void* info){
    if(!native_multiplayer_entry_active())return false;
    const bool was_in_flight=native_battle_launch_in_flight.load();cnlaunch::SceneTransition transition;bool began=false;try{
    native_battle_launch_require_ui();cnlaunch::RawStartBattleParam input{};
    if(!read(raw_param,input))throw std::runtime_error("private multiplayer launch input unreadable");
    auto snapshot=native_multiplayer_transport_snapshot();
    if(!native_multiplayer_phase_can_launch(cnmp::text(snapshot,"phase"))||text(reinterpret_cast<void*>(uintptr_t(input.act_id)))!="act2autochess"||input.mode_type!=2||input.multi_mode_sub_type!=2||
       text(reinterpret_cast<void*>(uintptr_t(input.mode_id)))!=cnmp::text(snapshot,"modeId")||
       text(reinterpret_cast<void*>(uintptr_t(input.scene_id)))!=cnmp::text(snapshot,"roomId")||!native_multiplayer_dispatch_ready())
        throw std::runtime_error("private multiplayer launch identity or native scene not ready");
    const auto& self=cnmp::at(snapshot,"selfState");const auto stage=text(reinterpret_cast<void*>(uintptr_t(input.stage_id)));
    if(stage!=cnmp::text(self,"stageId")&&!native_multiplayer_preparation_view_allows_initial_stage(snapshot,stage))throw std::runtime_error("private multiplayer launch map differs from its authenticated own/preparation view");
    if(native_battle_launch_in_flight.load()){native_battle_launch_reject("a private multiplayer native battle launch is already active");return true;}
    const bool resources_were_enabled=private_battle_resources.exchange(true);
    try{ensure_bundles();}catch(...){if(!resources_were_enabled)private_battle_resources.store(false);throw;}
    transition=native_battle_scene_begin();began=true;
    ++native_battle_launch_attempts;native_battle_launch_in_flight.store(true);native_battle_launch_loading_complete.store(false);
    PrivateDataScope private_scope(true);NativeBattleDepth depth(native_battle_utility_depth);
    native_battle_old_util_start(raw_param,info);native_battle_scene_accept(transition);began=false;report("private-native-multiplayer-battle-dispatched");return true;
    }catch(const std::exception& error){if(began)native_battle_scene_rollback(transition);native_battle_launch_in_flight.store(was_in_flight);native_battle_launch_reject(error.what());return true;}
}
bool native_multiplayer_launch_controller(void* raw_param,void* plugin,const void* info,void*& handler){
    if(!native_multiplayer_entry_active())return false;
    const bool was_in_flight=native_battle_launch_in_flight.load();handler=nullptr;try{
    native_battle_launch_require_ui();
    if(!native_battle_utility_depth||!native_battle_launch_in_flight.load()||!native_multiplayer_dispatch_ready())throw std::runtime_error("private multiplayer controller outside validated MULTI/TEAM launch");
    PrivateDataScope private_scope(true);handler=native_battle_old_controller_start(raw_param,plugin,info);return true;
    }catch(const std::exception& error){native_battle_launch_in_flight.store(was_in_flight);native_battle_launch_reject(error.what());return true;}
}
bool native_multiplayer_level_scope(){
    if(!native_multiplayer_entry_active()||!native_multiplayer_dispatch_ready()||!native_battle_launch_in_flight.load()||!can_read()||unity_ui_thread.load()!=GetCurrentThreadId())return false;
    if(native_battle_utility_depth)return true;
    try{
        if(native_multiplayer_pending_level_scope())return true;
        return native_battle_current_activity()=="act2autochess";
    }catch(...){return false;}
}
