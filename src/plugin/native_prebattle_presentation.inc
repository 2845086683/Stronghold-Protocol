// Private preparation presentation only. LOCAL service/transport remains native.
#pragma once
inline constexpr CN_Method NPB_REGISTER={"Torappu.UI.AutoChess.AutoChessPrepareController","RegisterHandler",0x602b3ce,0x2721410ULL,1,false,"Torappu.UI.AutoChess.AutoChessPrepareModel",{"Torappu.UI.AutoChess.IAutoChessPrepareStateHandler"}};
inline constexpr CN_Method NPB_TEAM={"Torappu.UI.AutoChess.AutoChessPrepareModel","GetTeamInfo",0x602b4a3,0x275c8c0ULL,0,false,"Torappu.UI.AutoChess.Server.AutoChessServiceTeamInfo",{}};
inline constexpr CN_Method NPB_TRAINING_INPUT={"Torappu.UI.AutoChess.AutoChessStageInfoGroupViewModel+Input","CreateTrainingInput",0x602bbff,0x27de920ULL,1,true,"Torappu.UI.AutoChess.AutoChessStageInfoGroupViewModel+Input",{"System.String"}};
inline constexpr CN_Method NPB_NORMAL_INPUT={"Torappu.UI.AutoChess.AutoChessStageInfoGroupViewModel+Input",".ctor",0x602bbfe,0x27dea60ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.AutoChessPrepareModel"}};
inline constexpr CN_Method NPB_TEAM_CTOR={"Torappu.UI.AutoChess.Server.AutoChessServiceTeamInfo",".ctor",0x602bcf9,0x50fcc0ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPB_STAGE_ENTER={"Torappu.UI.AutoChess.AutoChessStageInfoState","OnStateEnter",0x602bb8d,0x27dc830ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPB_LEAVE_PREPARE={"Torappu.UI.AutoChess.AutoChessPrepareController","_LeavePreparePage",0x602b423,0x2726710ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPB_BANNED={"Torappu.UI.AutoChess.AutoChessUtil","CalculateBannedChessIdList",0x602b55b,0x2760390ULL,2,true,"System.Collections.Generic.List`1<System.String>",{"System.String","System.Collections.Generic.List`1<System.String>"}};
inline constexpr CN_Method NPB_CHESS_GROUP={"Torappu.UI.AutoChess.AutoChessStageInfoChessGroupModel","LoadData",0x602bbec,0x27d7c30ULL,5,false,"System.Void",{"Torappu.ActAutoChessData+ActAutoChessBondInfo","Torappu.ListDict`2<System.String,Torappu.ActAutoChessData+ActAutoChessCharShopChessData>","System.Collections.Generic.Dictionary`2<System.String,Torappu.ActAutoChessData+ActAutoChessCharChessData>","System.Collections.Generic.Dictionary`2<System.String,System.String>","System.Collections.Generic.List`1<System.String>"}};
inline constexpr CN_Method NPB_BOND_CTOR={"Torappu.ActAutoChessData+ActAutoChessBondInfo",".ctor",0x60076a8,0x23125f0ULL,0,false,"System.Void",{}};
inline constexpr CN_Method NPB_PLAYER_INFO={"Torappu.UI.AutoChess.Battle.AutoChessBattleUIPlayerInfoViewModel","Init",0x602c12c,0x28375b0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModel"}};
inline constexpr CN_Method NPB_HUD_BOND_INIT={"Torappu.UI.AutoChess.Battle.AutoChessBattleUIBondViewModel","Init",0x602c0fe,0x2817e90ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModel"}};
inline constexpr CN_Method NPB_HUD_BANNED_INIT={"Torappu.UI.AutoChess.Battle.AutoChessBattleUIBondViewModel","_InitBannedBond",0x602c100,0x28187a0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModel"}};
inline constexpr CN_Method NPB_HUD_BANNED_LOAD={"Torappu.UI.AutoChess.Battle.AutoChessBannedBondItemModel","LoadData",0x602c129,0x2825610ULL,3,false,"System.Void",{"System.String","Torappu.ActAutoChessData","System.Collections.Generic.List`1<System.String>"}};
inline constexpr CN_Method NPB_DISPLAY_CHESS={"Torappu.Battle.AutoChess.AutoChessBattleUtil","CheckIfDisplayChess",0x601353c,0xb23d90ULL,2,true,"System.Boolean",{"Torappu.ActAutoChessData","System.String"}};
using NPB_Register=void*(*)(void*,void*,const void*);
using NPB_GetTeam=void*(*)(void*,const void*);
using NPB_Input=void*(*)(void*,const void*);
using NPB_Enter=void(*)(void*,const void*);
using NPB_Banned=void*(*)(void*,void*,const void*);
using NPB_ChessGroup=void(*)(void*,void*,void*,void*,void*,void*,const void*);
using NPB_ViewInit=void(*)(void*,void*,const void*);
using NPB_BannedLoad=void(*)(void*,void*,void*,void*,const void*);
NPB_Register npb_original_register=nullptr;NPB_GetTeam npb_original_team=nullptr;NPB_Input npb_original_input=nullptr;NPB_Enter npb_original_stage_enter=nullptr;
NPB_Enter npb_original_leave_prepare=nullptr;
NPB_Banned npb_original_banned=nullptr;NPB_ChessGroup npb_original_chess_group=nullptr;
NPB_ViewInit npb_original_player_info=nullptr,npb_original_hud_bond_init=nullptr,npb_original_hud_banned_init=nullptr;
NPB_BannedLoad npb_original_hud_banned_load=nullptr;
thread_local void* npb_ui_prepare=nullptr;
thread_local void* npb_state_prepare=nullptr;
thread_local void* npb_battle_view=nullptr;
thread_local unsigned npb_ui_state_depth=0,npb_hud_depth=0;
struct NativePrebattleStateScope{
    void* previous;void* previous_ui;
    NativePrebattleStateScope():previous(npb_state_prepare),previous_ui(npb_ui_prepare){npb_state_prepare=nullptr;npb_ui_prepare=nullptr;++npb_ui_state_depth;}
    ~NativePrebattleStateScope(){npb_state_prepare=previous;npb_ui_prepare=previous_ui;--npb_ui_state_depth;}
};
struct NativePrebattleUiScope{
    void* previous;
    explicit NativePrebattleUiScope(void* prepare):previous(npb_ui_prepare){npb_ui_prepare=prepare;}
    ~NativePrebattleUiScope(){npb_ui_prepare=previous;}
};
std::atomic<unsigned> npb_models{0},npb_teams{0},npb_inputs{0},npb_selections{0},npb_failures{0},npb_banned_lists{0},npb_chess_groups{0},npb_hud_models{0},npb_hud_banned_counts{0};
std::string npb_model_run,npb_team_run,npb_last_error;
cnjson::Value native_battle_prebattle_snapshot();

bool npb_private(){
    return can_read()&&private_view.load()&&native_single_mode_captured&&cncombat::mode_allowed(nb_selected_mode)&&
        native_battle_launch_session_prepared.load()&&unity_ui_thread.load()==GetCurrentThreadId();
}
bool npb_ui_active(){
    if(npb_hud_depth)return false;
    auto prepare=nb_retained("native-private-prepare-model");
    return npb_private()&&prepare&&npb_model_run==nb_run_id&&((npb_ui_state_depth&&npb_state_prepare==prepare)||npb_ui_prepare==prepare)&&
        cnlaunch::target_activity(text(field_object(prepare,"<actId>k__BackingField")));
}
void* npb_field(void* object,const char* name,const char* type){
    if(!object)throw std::runtime_error("private prebattle field owner missing");
    auto field=profile_field(api.il2cpp_object_get_class(object),name);
    if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),type))
        throw std::runtime_error(std::string("private prebattle field contract: ")+name);
    return field;
}
cnjson::Value npb_snapshot(){
    auto value=native_battle_prebattle_snapshot();
    if(nb_number(value,"version")!=1||nb_string(value,"activityId")!="act2autochess"||nb_string(value,"runId")!=nb_run_id||
       nb_string(value,"modeId")!=nb_selected_mode||!cncombat::stage_allowed(nb_string(value,"stageId"),nb_selected_mode))
        throw std::runtime_error("private prebattle snapshot identity/mode/map mismatch");
    return value;
}
void npb_error(const char* where,const std::exception& error){
    ++npb_failures;npb_last_error=std::string(where)+": "+error.what();native_battle_launch_reject(npb_last_error.c_str());
}
bool native_multiplayer_social_result_close(void*,const void*);
void npb_leave_prepare(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(native_multiplayer_social_result_close(self,info))return;
    if(!npb_private()){lease.release();npb_original_leave_prepare(self,info);return;}
    try{
        if(!cnlaunch::target_activity(native_battle_prepare_activity(self,"m_actId"))){lease.release();npb_original_leave_prepare(self,info);return;}
        // This method is reached after the user's leave confirmation (or its
        // LOCAL LeaveRet), not when a confirmation dialog is merely opened.
        // A late callback from a replaced controller cannot revoke a new draft.
        auto prepare=nb_retained("native-private-prepare-model");
        if(!prepare||field_object(self,"m_model")!=prepare||npb_model_run!=nb_run_id)
            throw std::runtime_error("private briefing exit controller ownership changed");
        if(native_battle_discard_draft()){
            native_battle_launch_session_prepared.store(false);native_battle_launch_team_ready.store(false);
            native_battle_session_cancel();native_single_mode_captured=false;native_single_team_choices_ready=false;
            npb_model_run.clear();npb_team_run.clear();
        }
        // Native _LeavePreparePage closes the page and stops its LOCAL service.
        // An acknowledged formal strategy is deliberately retained for resume.
        npb_original_leave_prepare(self,info);
    }catch(const std::exception& error){npb_error("leave-briefing",error);}
}
void* npb_list(void* list_class,TemporaryRoots& roots){
    auto list=roots.retain(object_new(list_class));if(!list)throw std::runtime_error("private prebattle list allocation failed");
    invoke(method(list_class,".ctor",0),list);return list;
}
void npb_add(void* list,void* object){void* args[]={object};invoke(method(api.il2cpp_object_get_class(list),"Add",1),list,args);}
void* npb_strings(const cnjson::Value& values,void* owner,const char* field_name,TemporaryRoots& roots){
    if(values.kind!=cnjson::Value::Array||values.array.size()>512)throw std::runtime_error("private prebattle string list invalid");
    auto field=npb_field(owner,field_name,"System.Collections.Generic.List`1<System.String>");
    auto list=npb_list(api.il2cpp_class_from_type(api.il2cpp_field_get_type(field)),roots);
    for(const auto& entry:values.array){
        if(entry.kind!=cnjson::Value::String||entry.string.empty()||entry.string.size()>256)throw std::runtime_error("private prebattle string value invalid");
        npb_add(list,roots.retain(string_new(entry.string.c_str())));
    }return list;
}
bool npb_contains(const cnjson::Value& values,const std::string& key){
    if(values.kind!=cnjson::Value::Array||values.array.size()>512)throw std::runtime_error("private prebattle pool/list invalid");
    for(const auto& entry:values.array){if(entry.kind!=cnjson::Value::String)throw std::runtime_error("private prebattle pool key invalid");if(entry.string==key)return true;}return false;
}
void* npb_boxed(void* object,const char* field_name,const char* type,TemporaryRoots& roots){
    npb_field(object,field_name,type);auto value=native_battle_boxed_field(object,field_name,roots);
    if(!value||!api.il2cpp_class_is_valuetype(api.il2cpp_object_get_class(value)))throw std::runtime_error("private prebattle status value missing");
    return value;
}
void native_prebattle_team_initialize(void* team,void* activity){
    if(!npb_private()||!cnlaunch::target_activity(text(activity)))throw std::runtime_error("private solo team initialization scope missing");
    auto preview=npb_snapshot();TemporaryRoots roots;roots.retain(team);
    auto self_field=npb_field(team,"m_selfStatus","Torappu.UI.AutoChess.Server.MsgAutoChessPlayerStatus");
    (void)self_field;auto self=roots.retain(field_object(team,"m_selfStatus"));
    if(!self)throw std::runtime_error("private solo team native self missing");
    auto status=npb_boxed(team,"m_teamStatus","Torappu.UI.AutoChess.Server.AutoChessTeamStatus",roots);
    auto players_field=npb_field(status,"players","System.Collections.Generic.List`1<Torappu.UI.AutoChess.Server.MsgAutoChessPlayerStatus>");
    auto players=npb_list(api.il2cpp_class_from_type(api.il2cpp_field_get_type(players_field)),roots);npb_add(players,self);set_object_field(status,players_field,players);
    auto npc_field=npb_field(team,"m_npcStatusList","System.Collections.Generic.List`1<Torappu.UI.AutoChess.Server.MsgAutoChessPlayerStatus>");
    auto empty_npc=npb_list(api.il2cpp_class_from_type(api.il2cpp_field_get_type(npc_field)),roots);set_object_field(team,npc_field,empty_npc);
    auto brief=npb_boxed(status,"strategyDecisionBrief","Torappu.UI.AutoChess.Server.StrategyDecisionBrief",roots);
    auto choices_field=npb_field(brief,"choices","System.Collections.Generic.List`1<Torappu.UI.AutoChess.Server.MsgStrategyChoice>");
    auto choices=npb_list(api.il2cpp_class_from_type(api.il2cpp_field_get_type(choices_field)),roots);set_object_field(brief,choices_field,choices);
    set_field(status,npb_field(status,"strategyDecisionBrief","Torappu.UI.AutoChess.Server.StrategyDecisionBrief"),unbox(brief));
    set_field(team,npb_field(team,"m_teamStatus","Torappu.UI.AutoChess.Server.AutoChessTeamStatus"),unbox(status));
    protect("native-private-training-team",team);npb_team_run=nb_string(preview,"runId");
}
void* npb_register(void* self,void* handler,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()){lease.release();return npb_original_register(self,handler,info);}
    auto result=npb_original_register(self,handler,info);
    if(npb_ui_state_depth)npb_state_prepare=nullptr;
    if(!npb_private())return result;
    try{
        if(!cnlaunch::target_activity(native_battle_prepare_activity(self,"m_actId")))return result;
        if(api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessPrepareController")||!result||
           api.il2cpp_object_get_class(result)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||field_object(self,"m_model")!=result||
           !cnlaunch::target_activity(native_battle_prepare_activity(result,"<actId>k__BackingField")))
            throw std::runtime_error("private prebattle PrepareModel ownership mismatch");
        auto preview=npb_snapshot();protect("native-private-prepare-model",result);npb_model_run=nb_string(preview,"runId");if(npb_ui_state_depth)npb_state_prepare=result;++npb_models;
    }catch(const std::exception& error){npb_error("register-prepare",error);return nullptr;}
    return result;
}
void* npb_team(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()){lease.release();return npb_original_team(self,info);}
    auto original=npb_original_team(self,info);
    if(!npb_private()||nb_retained("native-private-prepare-model")!=self||npb_model_run!=nb_run_id||
       (npb_ui_prepare!=self&&(!npb_ui_state_depth||npb_state_prepare!=self)))return original;
    try{
        if(!cnlaunch::target_activity(native_battle_prepare_activity(self,"<actId>k__BackingField")))return original;
        auto preview=npb_snapshot();TemporaryRoots roots;roots.retain(original);
        if(!original||api.il2cpp_object_get_class(original)!=cls("Torappu.UI.AutoChess.Server.AutoChessServiceTeamInfo"))throw std::runtime_error("private prebattle native team unavailable");
        auto copy=roots.retain(object_new(cls("Torappu.UI.AutoChess.Server.AutoChessServiceTeamInfo")));invoke(bound_methods.at(NPB_TEAM_CTOR.token),copy);
        auto status=npb_boxed(original,"teamStatus","Torappu.UI.AutoChess.Server.AutoChessTeamStatus",roots);
        const auto& input=nb_at(preview,"stageInfoInput");
        if(nb_string(input,"actId")!="act2autochess"||nb_string(input,"modeId")!=nb_string(preview,"modeId")||nb_at(input,"trainingBondList").kind!=cnjson::Value::Null)
            throw std::runtime_error("private prebattle stage input identity/tutorial range mismatch");
        set_object_field(status,npb_field(status,"modeId","System.String"),roots.retain(string_new(nb_string(preview,"modeId").c_str())));
        set_object_field(status,npb_field(status,"bossId","System.String"),roots.retain(string_new(nb_string(input,"bossId").c_str())));
        set_object_field(status,npb_field(status,"bannedBond","System.Collections.Generic.List`1<System.String>"),npb_strings(nb_at(input,"bannedBondList"),status,"bannedBond",roots));
        set_object_field(status,npb_field(status,"monsterKeys","System.Collections.Generic.List`1<System.String>"),npb_strings(nb_at(input,"enemyTypes"),status,"monsterKeys",roots));
        set_field(copy,npb_field(copy,"teamStatus","Torappu.UI.AutoChess.Server.AutoChessTeamStatus"),unbox(status));
        set_object_field(copy,npb_field(copy,"<teamID>k__BackingField","System.String"),field_object(original,"<teamID>k__BackingField"));
        ++npb_teams;return copy;
    }catch(const std::exception& error){npb_error("present-team",error);return nullptr;}
}
void* npb_input(void* activity,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()){lease.release();return npb_original_input(activity,info);}
    if(!npb_private()||!activity||!cnlaunch::target_activity(text(activity))){lease.release();return npb_original_input(activity,info);}
    try{
        auto preview=npb_snapshot();auto prepare=nb_retained("native-private-prepare-model");
        if(!prepare||npb_model_run!=nb_string(preview,"runId")||api.il2cpp_object_get_class(prepare)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel")||
           !cnlaunch::target_activity(native_battle_prepare_activity(prepare,"<actId>k__BackingField")))throw std::runtime_error("private prebattle input has no current PrepareModel");
        TemporaryRoots roots;roots.retain(prepare);auto value=roots.retain(object_new(cls("Torappu.UI.AutoChess.AutoChessStageInfoGroupViewModel+Input")));
        void* args[]={prepare};{PrivateDataScope private_scope(true);NativePrebattleUiScope presentation(prepare);invoke(bound_methods.at(NPB_NORMAL_INPUT.token),value,args);}
        // The ordinary constructor leaves this null. A training range would
        // hide the normal mode's full/banned bond list in _LoadBondData.
        if(field_object(value,"trainingBondList"))throw std::runtime_error("private prebattle input retained a tutorial bond range");
        ++npb_inputs;return value;
    }catch(const std::exception& error){npb_error("present-input",error);return nullptr;}
}
void native_prebattle_select_band(const char* band_id){
    auto preview=npb_snapshot();auto team=nb_retained("native-private-training-team");
    if(!team||npb_team_run!=nb_string(preview,"runId")||nb_string(preview,"bandId")!=band_id)throw std::runtime_error("private prebattle selected band acknowledgement mismatch");
    TemporaryRoots roots;roots.retain(team);auto self=roots.retain(field_object(team,"m_selfStatus"));auto uid=roots.retain(field_object(team,"m_selfUID"));
    if(!self||!uid)throw std::runtime_error("private prebattle selected self identity missing");
    auto band=roots.retain(string_new(band_id));set_object_field(self,npb_field(self,"strategy","System.String"),band);
    auto status=npb_boxed(team,"m_teamStatus","Torappu.UI.AutoChess.Server.AutoChessTeamStatus",roots);
    auto brief=npb_boxed(status,"strategyDecisionBrief","Torappu.UI.AutoChess.Server.StrategyDecisionBrief",roots);
    auto field=npb_field(brief,"choices","System.Collections.Generic.List`1<Torappu.UI.AutoChess.Server.MsgStrategyChoice>");
    auto choices=npb_list(api.il2cpp_class_from_type(api.il2cpp_field_get_type(field)),roots);
    auto choice=roots.retain(object_new(cls("Torappu.UI.AutoChess.Server.MsgStrategyChoice")));
    invoke(method(api.il2cpp_object_get_class(choice),".ctor",0),choice);
    set_object_field(choice,npb_field(choice,"uid","System.String"),uid);set_object_field(choice,npb_field(choice,"strategy","System.String"),band);npb_add(choices,choice);
    set_object_field(brief,field,choices);set_field(status,npb_field(status,"strategyDecisionBrief","Torappu.UI.AutoChess.Server.StrategyDecisionBrief"),unbox(brief));
    // Replace TeamData's list. The old service snapshot must retain its empty
    // list until original _EventOnChooseBand passes its duplicate-choice guard.
    set_field(team,npb_field(team,"m_teamStatus","Torappu.UI.AutoChess.Server.AutoChessTeamStatus"),unbox(status));++npb_selections;
}
void npb_stage_enter(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()||!npb_private()){lease.release();npb_original_stage_enter(self,info);return;}
    NativePrebattleStateScope presentation;npb_original_stage_enter(self,info);
}
bool npb_hud_owner(void* view){
    if(!npb_private()||!nb_prepared.load()||!native_battle_manager_root||!view||
       api.il2cpp_object_get_class(view)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModel"))return false;
    auto manager=hold_get(native_battle_manager_root),dc=nb_centre();
    if(!manager||manager!=nb_manager()||!dc||field_object(manager,"dataCenter")!=dc||
       field_object(manager,"dataBridge")!=nb_retained("native-battle-local-player")||!nb_retained("native-battle-local-player"))return false;
    if(!cnlaunch::target_activity(text(field_object(dc,"<actId>k__BackingField")))||
       !cnlaunch::target_activity(text(field_object(view,"<actId>k__BackingField")))||
       text(field_object(dc,"<modeId>k__BackingField"))!=nb_selected_mode||text(field_object(view,"<modeId>k__BackingField"))!=nb_selected_mode)return false;
    auto data=field_object(dc,"<actData>k__BackingField"),bonds=field_object(dc,"<bannedBonds>k__BackingField");
    // InitData writes these two references before HUD.Init. Squad/player-model
    // references are assigned only by the later _ApplyData, so they cannot gate
    // first-frame HUD initialization (2805f53 -> 2806267 -> 28071ba).
    return data&&bonds&&field_object(view,"<autoChessActData>k__BackingField")==data&&
        field_object(view,"<bannedBonds>k__BackingField")==bonds;
}
struct NativePrebattleHudScope{
    void* previous;
    explicit NativePrebattleHudScope(void* view):previous(npb_battle_view){npb_battle_view=view;++npb_hud_depth;}
    ~NativePrebattleHudScope(){npb_battle_view=previous;--npb_hud_depth;}
};
bool npb_hud_active(){return npb_battle_view&&npb_hud_owner(npb_battle_view);}
void npb_hud_init(void* self,void* view,const void* info,const char* field,NPB_ViewInit original){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()){lease.release();original(self,view,info);return;}
    bool owner=false;
    try{owner=npb_hud_owner(view);}catch(const std::exception& error){npb_error("present-hud-owner",error);return;}
    // A foreign native callback retains its own exception semantics, and a
    // nested foreign HUD cannot borrow the outer private HUD/prepare context.
    if(!owner){lease.release();NativePrebattleHudScope unrelated(nullptr);original(self,view,info);return;}
    try{
        auto hud=field_object(view,"<hudModel>k__BackingField");
        if(!hud||field_object(hud,field)!=self)throw std::runtime_error("private HUD child ownership mismatch");
        npb_snapshot();NativePrebattleHudScope presentation(view);original(self,view,info);++npb_hud_models;
    }catch(const std::exception& error){npb_error("present-hud-init",error);}
}
void npb_player_info(void* self,void* view,const void* info){npb_hud_init(self,view,info,"playerInfoModel",npb_original_player_info);}
void npb_hud_bond_init(void* self,void* view,const void* info){npb_hud_init(self,view,info,"bondModel",npb_original_hud_bond_init);}
void npb_hud_banned_init(void* self,void* view,const void* info){npb_hud_init(self,view,info,"bondModel",npb_original_hud_banned_init);}
void npb_hud_banned_load(void* self,void* bond,void* data,void* banned,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()){lease.release();npb_original_hud_banned_load(self,bond,data,banned,info);return;}
    npb_original_hud_banned_load(self,bond,data,banned,info);
    if(!npb_hud_active())return;
    try{
        if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBannedBondItemModel")||
           data!=field_object(npb_battle_view,"<autoChessActData>k__BackingField"))throw std::runtime_error("private HUD banned bond data mismatch");
        auto preview=npb_snapshot();auto bond_id=text(bond);const auto& squad=nb_at(preview,"nativeSquad");
        if(bond_id.empty()||squad.kind!=cnjson::Value::Array||squad.array.size()>256)throw std::runtime_error("private HUD banned bond/squad invalid");
        TemporaryRoots roots;std::map<std::string,bool> seen;int32_t count=0;
        for(const auto& row:squad.array){auto key=nb_string(row,"chessId");
            if(!npb_contains(nb_at(preview,"chessPool"),key)||!npb_contains(nb_at(preview,"bannedChessIds"),key)||!npb_contains(nb_at(row,"bondList"),bond_id))continue;
            if(!seen.emplace(key,true).second)throw std::runtime_error("private HUD duplicate configured chess");
            void* args[]={data,roots.retain(string_new(key.c_str()))};uint8_t show=0;
            if(!read(unbox(invoke(bound_methods.at(NPB_DISPLAY_CHESS.token),nullptr,args)),show))throw std::runtime_error("private HUD display classification failed");
            if(show)++count;
        }
        set_field(self,npb_field(self,"absentCnt","System.Int32"),&count);++npb_hud_banned_counts;
    }catch(const std::exception& error){npb_error("present-hud-banned-count",error);}
}
void* npb_banned(void* activity,void* bonds,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()||!activity||!cnlaunch::target_activity(text(activity))){lease.release();return npb_original_banned(activity,bonds,info);}
    bool battle=false,owned=false;
    try{battle=npb_hud_active();owned=battle||npb_ui_active();}catch(const std::exception& error){npb_error("present-banned-owner",error);return nullptr;}
    if(!owned){lease.release();return npb_original_banned(activity,bonds,info);}
    try{
        auto preview=npb_snapshot();TemporaryRoots roots;
        void* owner=npb_battle_view;const char* field="<bannedBonds>k__BackingField";
        if(!battle){auto prepare=nb_retained("native-private-prepare-model"),team=roots.retain(npb_original_team(prepare,bound_methods.at(NPB_TEAM.token)));owner=npb_boxed(team,"teamStatus","Torappu.UI.AutoChess.Server.AutoChessTeamStatus",roots);field="bannedBond";}
        // The native consumer checks the elite key, then renders its ordinary
        // shop card. The server projects both equivalent keys for this run.
        auto result=npb_strings(nb_at(preview,"bannedChessDisplayIds"),owner,field,roots);++npb_banned_lists;return result;
    }catch(const std::exception& error){npb_error("present-banned-chess",error);return nullptr;}
}
void npb_chess_group(void* self,void* bond,void* shops,void* chars,void* normal_ids,void* banned,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load()||!npb_ui_active()){lease.release();npb_original_chess_group(self,bond,shops,chars,normal_ids,banned,info);return;}
    try{
        auto preview=npb_snapshot();TemporaryRoots roots;roots.retain(self);roots.retain(bond);
        if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessStageInfoChessGroupModel")||!bond||
           api.il2cpp_object_get_class(bond)!=cls("Torappu.ActAutoChessData+ActAutoChessBondInfo"))throw std::runtime_error("private prebattle chess group type mismatch");
        auto bond_id=text(field_object(bond,"bondId"));if(bond_id.empty())throw std::runtime_error("private prebattle chess group bond missing");
        const auto& squad=nb_at(preview,"nativeSquad");const auto& pool=nb_at(preview,"chessPool");const auto& banned_ids=nb_at(preview,"bannedChessIds");
        if(squad.kind!=cnjson::Value::Array||squad.array.size()>256)throw std::runtime_error("private prebattle squad bound");
        std::map<std::string,const cnjson::Value*> selected;
        cnjson::Value variants;variants.kind=cnjson::Value::Array;
        for(const auto& row:squad.array){
            auto key=nb_string(row,"chessId");if(!npb_contains(pool,key)||!npb_contains(banned_ids,key)||!npb_contains(nb_at(row,"bondList"),bond_id))continue;
            if(!selected.emplace(key,&row).second)throw std::runtime_error("private prebattle duplicate configured chess");
            auto id=roots.retain(string_new(key.c_str()));auto shop=roots.retain(dictionary_find(shops,id));
            if(!shop)throw std::runtime_error("private prebattle configured shop chess missing: "+key);
            auto golden=text(field_object(shop,"goldenChessId"));if(golden.empty())throw std::runtime_error("private prebattle golden chess missing: "+key);
            cnjson::Value entry;entry.kind=cnjson::Value::String;entry.string=golden;variants.array.push_back(entry);
        }
        // Only the temporary bond's chessIdList is consumed by the native
        // loader (27d7d78/27d7da0). No shared ActData/bond dictionary is changed.
        auto temporary=roots.retain(object_new(cls("Torappu.ActAutoChessData+ActAutoChessBondInfo")));invoke(bound_methods.at(NPB_BOND_CTOR.token),temporary);
        set_object_field(temporary,npb_field(temporary,"chessIdList","System.Collections.Generic.List`1<System.String>"),npb_strings(variants,temporary,"chessIdList",roots));
        npb_original_chess_group(self,temporary,shops,chars,normal_ids,banned,info);
        auto list=roots.retain(field_object(self,"chessList"));if(!list)throw std::runtime_error("private prebattle native chess list missing");
        int32_t count=0;if(!read(unbox(invoke(method(api.il2cpp_object_get_class(list),"get_Count",0),list)),count)||count<0||size_t(count)!=selected.size())throw std::runtime_error("private prebattle native chess list mismatch");
        for(int32_t i=0;i<count;++i){void* args[]={&i};auto item=roots.retain(invoke(method(api.il2cpp_object_get_class(list),"get_Item",1),list,args));
            auto key=text(field_object(item,"chessId"));auto row=selected.find(key);if(row==selected.end())throw std::runtime_error("private prebattle unexpected native chess");
            auto actual=nb_string(*row->second,"charId");if(actual.empty())throw std::runtime_error("private prebattle actual operator missing");
            if(text(field_object(item,"charId"))!=actual){set_object_field(item,npb_field(item,"charId","System.String"),roots.retain(string_new(actual.c_str())));set_object_field(item,npb_field(item,"tmplId","System.String"),nullptr);}
        }++npb_chess_groups;
    }catch(const std::exception& error){
        npb_error("present-chess-group",error);
        if(self&&api.il2cpp_object_get_class(self)==cls("Torappu.UI.AutoChess.AutoChessStageInfoChessGroupModel")){
            auto field=npb_field(self,"chessList","System.Collections.Generic.List`1<Torappu.UI.AutoChess.AutoChessStageInfoChessViewModel>");TemporaryRoots roots;
            set_object_field(self,field,npb_list(api.il2cpp_class_from_type(api.il2cpp_field_get_type(field)),roots));
        }
    }
}
void native_prebattle_presentation_setup(){
    hook(NPB_REGISTER,npb_register,npb_original_register);hook(NPB_TEAM,npb_team,npb_original_team);hook(NPB_TRAINING_INPUT,npb_input,npb_original_input);
    hook(NPB_STAGE_ENTER,npb_stage_enter,npb_original_stage_enter);
    hook(NPB_LEAVE_PREPARE,npb_leave_prepare,npb_original_leave_prepare);
    hook(NPB_BANNED,npb_banned,npb_original_banned);hook(NPB_CHESS_GROUP,npb_chess_group,npb_original_chess_group);
    hook(NPB_PLAYER_INFO,npb_player_info,npb_original_player_info);hook(NPB_HUD_BOND_INIT,npb_hud_bond_init,npb_original_hud_bond_init);
    hook(NPB_HUD_BANNED_INIT,npb_hud_banned_init,npb_original_hud_banned_init);hook(NPB_HUD_BANNED_LOAD,npb_hud_banned_load,npb_original_hud_banned_load);
    bind_method(NPB_NORMAL_INPUT);bind_method(NPB_TEAM_CTOR);bind_method(NPB_BOND_CTOR);bind_method(NPB_DISPLAY_CHESS);
}
