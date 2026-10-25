#pragma once
#include <string>
inline bool native_multiplayer_phase_can_launch(const std::string& phase){
    return phase=="loading"||phase=="special"||phase=="preparation"||phase=="battle_countdown"||phase=="battle"||phase=="help"||phase=="boss";
}
