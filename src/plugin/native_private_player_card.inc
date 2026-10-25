// Private result cards display cached local identity only. They must not send
// official name-card, friend or report requests for private scene identities.
#pragma once
inline constexpr CN_Method NPC_CHECK_CARD={"Torappu.UI.AutoChess.AutoChessPrepareController","_EventOnCheckNameCard",0x602b408,0x2722160ULL,1,false,"System.Void",{"System.String"}};
inline constexpr CN_Method NPC_ADD_FRIEND={"Torappu.UI.AutoChess.AutoChessPrepareController","_EventOnAddFriend",0x602b409,0x2721f00ULL,1,false,"System.Void",{"System.String"}};
inline constexpr CN_Method NPC_TRY_REPORT={"Torappu.UI.AutoChess.AutoChessSettleGameState","_OnTryReportPlayerBtnClick",0x602b792,0x278cf60ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPC_MESSAGE={"Torappu.UI.AutoChess.AutoChessSettleGameState","OnMessage",0x602b790,0x278bbd0ULL,2,false,"System.Void",{"System.Int32","Torappu.ValueBundle"}};
inline constexpr CN_Method NPC_GET_PAGE={"Torappu.UI.State","GetPage",0x601adc8,0x141ec30ULL,0,false,"Torappu.UI.UIPage",{}};
inline constexpr CN_Method NPC_GET_ARGS={"Torappu.UI.UIPage","GetArguments",0x601ac23,0x140ee70ULL,0,false,"System.Object",{}};
using NPC_StringAction=void(*)(void*,void*,const void*);
using NPC_Action=void(*)(void*,const void*);
using NPC_Message=void(*)(void*,int32_t,void*,const void*);
NPC_StringAction npc_old_check_card=nullptr,npc_old_add_friend=nullptr;
NPC_Action npc_old_try_report=nullptr;NPC_Message npc_old_message=nullptr;
std::atomic<unsigned> npc_blocked_actions{0},npc_guard_failures{0};

bool npc_owned_result_page(void* page){
    if(!page||api.il2cpp_object_get_class(page)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage"))return false;
    TemporaryRoots roots;auto args=roots.retain(invoke(bound_methods.at(NPC_GET_ARGS.token),page));
    const auto live=nb_retained("native-private-settlement-params"),restored=nb_retained("private-liveness-result-params");
    if(!args||(args!=live&&args!=restored))return false;
    if(api.il2cpp_object_get_class(args)!=cls("Torappu.UI.AutoChess.AutoChessPreparePage+Params")||
       text(field_object(args,"actId"))!="act2autochess"||nb_native_int(args,"fromBattleSource")!=2||
       nb_native_int(args,"modeType")!=(args==live?0:1))throw std::runtime_error("private display-only result page identity changed");
    return true;
}
bool npc_owned_controller(void* self){
    if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessPrepareController")||text(field_object(self,"m_actId"))!="act2autochess")return false;
    nb_main_thread();auto page=field_object(self,"m_page");
    if(!page||field_object(page,"_controller")!=self)return false;
    return npc_owned_result_page(page);
}
bool npc_owned_state(void* self){
    if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessSettleGameState"))return false;
    nb_main_thread();TemporaryRoots roots;
    return npc_owned_result_page(roots.retain(invoke(bound_methods.at(NPC_GET_PAGE.token),self)));
}
void npc_check_card(void* self,void* uid,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{if(native_multiplayer_social_controller(self,uid)||npc_owned_controller(self)){++npc_blocked_actions;return;}}
    catch(const std::exception&){++npc_guard_failures;report("private-name-card-action-guard-failed");return;}
    lease.release();npc_old_check_card(self,uid,info);
}
void npc_add_friend(void* self,void* uid,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{if(native_multiplayer_social_controller(self,uid)||npc_owned_controller(self)){++npc_blocked_actions;return;}}
    catch(const std::exception&){++npc_guard_failures;report("private-friend-action-guard-failed");return;}
    lease.release();npc_old_add_friend(self,uid,info);
}
void npc_try_report(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{if(native_multiplayer_social_state(self)||npc_owned_state(self)){++npc_blocked_actions;return;}}
    catch(const std::exception&){++npc_guard_failures;report("private-report-action-guard-failed");return;}
    lease.release();npc_old_try_report(self,info);
}
void npc_message(void* self,int32_t key,void* bundle,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    // Pinned native OnMessage key 2 opens report selection. Key 1 is navigation
    // and key 3 is a report-success notification; keep their native semantics.
    try{if(key==2&&(native_multiplayer_social_state(self)||npc_owned_state(self))){++npc_blocked_actions;return;}}
    catch(const std::exception&){++npc_guard_failures;report("private-report-message-guard-failed");return;}
    lease.release();npc_old_message(self,key,bundle,info);
}
void native_private_player_card_setup(){
    bind_method(NPC_GET_PAGE);bind_method(NPC_GET_ARGS);
    for(const auto& row:std::initializer_list<std::tuple<const char*,const char*,const char*,size_t>>{
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_actId","System.String",64},
        {"Torappu.UI.AutoChess.AutoChessPrepareController","m_page","Torappu.UI.AutoChess.AutoChessPreparePage",80},
        {"Torappu.UI.AutoChess.AutoChessPreparePage","_controller","Torappu.UI.AutoChess.AutoChessPrepareController",248},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","actId","System.String",16},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","modeType","Torappu.ActAutoChessModeType",24},
        {"Torappu.UI.AutoChess.AutoChessPreparePage+Params","fromBattleSource","Torappu.UI.AutoChess.FromBattleSource",32}
    })nbb_field(cls(std::get<0>(row)),std::get<1>(row),std::get<2>(row),std::get<3>(row));
    hook(NPC_CHECK_CARD,npc_check_card,npc_old_check_card);hook(NPC_ADD_FRIEND,npc_add_friend,npc_old_add_friend);
    hook(NPC_TRY_REPORT,npc_try_report,npc_old_try_report);hook(NPC_MESSAGE,npc_message,npc_old_message);
}
