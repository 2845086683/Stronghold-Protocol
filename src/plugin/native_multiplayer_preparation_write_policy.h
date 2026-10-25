#pragma once
#include <array>
#include <string>
#include <stdexcept>

namespace cnpreparation_write {
struct Denied : std::runtime_error {
    explicit Denied(const std::string& type):std::runtime_error("multiplayer preparation write requires current owned view: "+type){}
};
enum class Kind { Other, Board, Choice };
inline constexpr std::array<const char*,8> board_types={{
    "AutoChessBattleSceneShopBuyChessUp",
    "AutoChessBattleSceneShopRefreshUp",
    "AutoChessBattleSceneShopFrozeUp",
    "AutoChessBattleSceneShopUpgradeUp",
    "AutoChessBattleSceneChangePositionUp",
    "AutoChessBattleSceneDestroyChessUp",
    "AutoChessBattleSceneUseEquipUp",
    "AutoChessBattleSceneUseMagicUp",
}};
inline constexpr std::array<const char*,2> choice_types={{
    "AutoChessBattleSceneSpPreparationPickUp",
    "AutoChessBattleSceneSelfChoicePickUp",
}};
inline Kind classify(const std::string& short_type){
    for(auto name:board_types)if(short_type==name)return Kind::Board;
    for(auto name:choice_types)if(short_type==name)return Kind::Choice;
    return Kind::Other;
}
inline bool allowed(const std::string& short_type,bool board_allowed,bool choice_own_proof){
    switch(classify(short_type)){
    case Kind::Board:return board_allowed;
    case Kind::Choice:return choice_own_proof;
    case Kind::Other:return true;
    }
    return false;
}
}
