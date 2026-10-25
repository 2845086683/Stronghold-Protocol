// Private MULTI observation and authored terrain execution. Included after
// native_multiplayer_help.inc. Original map/blackboard executors remain owners.
#pragma once
#include "native_battle_launch_policy.h"
cnlaunch::SceneTransition native_battle_scene_begin();
void native_battle_scene_accept(cnlaunch::SceneTransition&);
void native_battle_scene_rollback(cnlaunch::SceneTransition&);
bool native_battle_scene_ready(void*,void*);
bool native_multiplayer_world_field_info(void*,void*,std::string&,std::string&);
uint64_t native_multiplayer_world_field_epoch();
void native_multiplayer_world_field_switch(const cnjson::Value&,int32_t,bool);
void native_multiplayer_world_field_reset();
static const CN_Method NMO_METHODS[]={
{"Torappu.Battle.AutoChess.AutoChessBattleDataModel","_ApplyChangeMapEffect",0x60138bb,0xba1d80ULL,2,false,"System.Void",{"System.String","System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessMapAreaManager","IsLeftMap",0x601385b,0xb669e0ULL,1,false,"System.Boolean",{"Torappu.GridPosition"}},
{"Torappu.Battle.AutoChess.AutoChessMapAreaManager","IsRightMap",0x601385c,0xb66d00ULL,1,false,"System.Boolean",{"Torappu.GridPosition"}},
{"Torappu.Battle.AutoChess.AutoChessMapAreaManager","UpdateTileIndexerFromOb",0x6013867,0xb67430ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessGameStatus","get_inBattleState",0x60136eb,0xb4b580ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.AutoChess.AutoChessGameStatus","get_inMainPrepareState",0x60136f3,0xb4bb30ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","get_inObMode",0x601343c,0xb13ed0ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","get_playerViewIndex",0x601343f,0xb14700ULL,0,false,"System.Int32",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","SetCurrentPlayerView",0x6013476,0xb11650ULL,1,false,"System.Boolean",{"System.Int32"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdatePlayerBattleData",0x601347c,0xb12590ULL,1,false,"System.Void",{"System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","PrepareForPrepareState",0x60136b4,0xb41d30ULL,0,false,"System.Void",{}},
{"Torappu.Battle.LevelUtils","LoadLevel",0x60131d7,0xafee90ULL,1,true,"Torappu.LevelData",{"System.String"}},
{"Torappu.Battle.BattleInOut","get_input",0x600f5cc,0x5d9990ULL,0,false,"Torappu.Battle.BattleInOut+InParams",{}},
{"Torappu.Battle.BattleInOut","SetInParams",0x600f5cf,0x5d9530ULL,1,false,"System.Void",{"Torappu.Battle.BattleInOut+InParams"}},
{"Torappu.Battle.BattleController","get_levelData",0x600fbea,0x642ed0ULL,0,false,"Torappu.LevelData",{}},
{"Torappu.Battle.BattleController","get_map",0x600fbe8,0x6431d0ULL,0,false,"Torappu.Battle.Map",{}},
{"Torappu.Battle.BattleController","get_mapData",0x600fbe9,0x643150ULL,0,false,"Torappu.MapData",{}},
{"Torappu.GameFlowController","StartScene",0x6005809,0x1ba10c0ULL,2,true,"System.Boolean",{"System.String","Torappu.GameFlowController+Options"}},
};
using NMOChangeMap=void(*)(void*,void*,int32_t,const void*);
NMOChangeMap nmo_original_change_map=nullptr;
cnjson::Value nmo_snapshot;
std::string nmo_terrain_fingerprint,nmo_room,nmo_stage,nmo_error;
std::string nmo_own_stage,nmo_phase;
int32_t nmo_observed=-1,nmo_self=-1,nmo_applied=-1;
bool nmo_ready=false,nmo_failed=false;
std::atomic<unsigned> nmo_switches{0},nmo_rebuilds{0},nmo_terrain_calls{0},nmo_terrain_skips{0},nmo_failures{0};
uint32_t nmo_reload_controller_root=0;
std::string nmo_reload_stage,nmo_reload_level;
uint64_t nmo_reload_at=0;
thread_local unsigned nmo_start_reload_depth=0;
std::atomic<unsigned> nmo_reloads{0},nmo_reload_completions{0},nmo_reload_deferrals{0};
bool native_multiplayer_observation_reloading(){return nmo_reload_controller_root!=0&&!nmo_failed;}
bool native_multiplayer_observation_starting_reload(){return nmo_start_reload_depth!=0;}
bool nmo_initial_stage();

void* nmo_method(uint32_t token){return bound_methods.at(token);}
void nmo_fail(const std::exception& ex){nmo_failed=true;nmo_error=ex.what();++nmo_failures;nb_error("multiplayer-observation",ex);}
int32_t nmo_count(void* list){auto count=nmh_result<int32_t>(invoke(method(api.il2cpp_object_get_class(list),"get_Count",0),list));if(count<0||count>16384)throw std::runtime_error("multiplayer observation list bound");return count;}
void* nmo_item(void* list,int32_t index){void* args[]={&index};return invoke(method(api.il2cpp_object_get_class(list),"get_Item",1),list,args);}
void* nmo_position(void* predefine,TemporaryRoots& roots){
    auto field=profile_field(api.il2cpp_object_get_class(predefine),"position");
    if(!matches_type(api.il2cpp_field_get_type(field),"Torappu.GridPosition"))throw std::runtime_error("multiplayer terrain position type mismatch");
    auto type=api.il2cpp_class_from_type(api.il2cpp_field_get_type(field));uint32_t align=0;
    auto size=api.il2cpp_class_value_size(type,&align);if(size!=8)throw std::runtime_error("multiplayer terrain GridPosition ABI mismatch");
    std::array<uint8_t,8> bytes{};api.il2cpp_field_get_value(predefine,field,bytes.data());return roots.retain(value_box(type,bytes.data()));
}
void* nmo_filtered(void* list,void* area,const std::set<bool>& sides,TemporaryRoots& roots){
    if(!list)throw std::runtime_error("multiplayer terrain predefine list missing");
    auto type=api.il2cpp_object_get_class(list);auto filtered=roots.retain(object_new(type));
    invoke(method(type,".ctor",0),filtered);auto add=method(type,"Add",1);const auto count=nmo_count(list);
    for(int32_t index=0;index<count;++index){
        auto row=roots.retain(nmo_item(list,index));if(!row)throw std::runtime_error("multiplayer terrain predefine null");
        auto position=nmo_position(row,roots);void* args[]={unbox(position)};bool include=false;
        for(const auto right:sides)if(nmh_result<uint8_t>(invoke(nmo_method(right?0x601385c:0x601385b),area,args))){include=true;break;}
        if(include){void* add_args[]={row};invoke(add,filtered,add_args);}
    }return filtered;
}
std::set<int32_t> nmo_players(bool battle){
    auto dc=nb_centre();std::set<int32_t> result;
    if(!battle){result.insert(nmh_result<int32_t>(invoke(nmo_method(0x601343f),dc)));return result;}
    auto players=field_object(dc,"m_battlePlayerIndex");if(!players)throw std::runtime_error("multiplayer terrain active players missing");
    const auto count=nmo_count(players);if(count<1||count>2)throw std::runtime_error("multiplayer terrain active player count invalid");
    for(int32_t index=0;index<count;++index)result.insert(nmh_result<int32_t>(nmo_item(players,index)));return result;
}
bool nmo_right(int32_t player,bool battle){
    if(!battle)return false;
    auto right=field_object(nb_centre(),"m_rightMapPlayerIndex");if(!right)throw std::runtime_error("multiplayer terrain right map owners missing");
    void* args[]={&player};return nmh_result<uint8_t>(invoke(method(api.il2cpp_object_get_class(right),"Contains",1),right,args))!=0;
}
std::set<int32_t> nmo_effect_owners(const std::string& effect,int32_t player,const std::set<int32_t>& active){
    if(player>=0)return active.count(player)?std::set<int32_t>{player}:std::set<int32_t>{};
    const auto& all=nb_at(nb_at(nb_at(nb_at(nmo_snapshot,"dtoProjection"),"allStateSync"),"payload"),"allStateSyncData");
    const auto& scene=nb_at(all,"sceneDetail");std::set<int32_t> roster;
    for(const auto& row:nb_at(all,"staticPlayers").array)roster.insert(int32_t(nb_number(row,"index")));
    std::set<int32_t> owners;bool found=false;
    for(const auto& row:nb_at(scene,"specialEffects").array){if(nb_string(row,"effectId")!=effect)continue;found=true;
        const auto& targets=nb_at(row,"affectedUidIndex");if(targets.kind!=cnjson::Value::Array)throw std::runtime_error("multiplayer terrain targets not a declared list");
        for(const auto& target:targets.array){if(target.kind!=cnjson::Value::Integer)throw std::runtime_error("multiplayer terrain target not integer");
            const auto owner=target.integer;if(owner<0||owner>3||!roster.count(int32_t(owner)))throw std::runtime_error("multiplayer terrain target outside authenticated seats");
            if(active.count(int32_t(owner)))owners.insert(int32_t(owner));}
    }
    if(!found)throw std::runtime_error("multiplayer terrain global effect has no private target declaration");
    // Empty is deliberately inert, including expired conditional cards. Global
    // application means explicitly naming every active seat, never an empty list.
    return owners;
}
struct NMOPredefineScope{
    void* model;void* token_field;void* char_field;void* tokens;void* chars;bool installed=false;
    NMOPredefineScope(void* value,void* area,const std::set<bool>& sides,TemporaryRoots& roots):model(value){
        auto type=api.il2cpp_object_get_class(model);token_field=profile_field(type,"m_predefinedTokens");char_field=profile_field(type,"m_predefinedChars");
        tokens=roots.retain(field_object(model,"m_predefinedTokens"));chars=roots.retain(field_object(model,"m_predefinedChars"));
        auto filtered_tokens=nmo_filtered(tokens,area,sides,roots),filtered_chars=nmo_filtered(chars,area,sides,roots);
        set_object_field(model,token_field,filtered_tokens);installed=true;
        try{set_object_field(model,char_field,filtered_chars);}catch(...){set_object_field(model,token_field,tokens);installed=false;throw;}
    }
    ~NMOPredefineScope(){if(installed){set_object_field(model,char_field,chars);set_object_field(model,token_field,tokens);}}
};
void nmo_change_map(void* self,void* effect,int32_t player,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!nmh_scope()){lease.release();nmo_original_change_map(self,effect,player,info);return;}
    try{
        nb_main_thread();if(!nmo_ready)throw std::runtime_error("multiplayer terrain private snapshot not ready");
        auto manager=nb_manager(),dc=nb_centre();if(field_object(dc,"battleData")!=self)throw std::runtime_error("multiplayer terrain model owner mismatch");
        auto status=field_object(manager,"gameStatus"),area=field_object(manager,"mapAreaManager");if(!status||!area)throw std::runtime_error("multiplayer terrain native map state missing");
        const bool battle=nmh_result<uint8_t>(invoke(nmo_method(0x60136eb),status))!=0;
        const auto owners=nmo_effect_owners(text(effect),player,nmo_players(battle));std::set<bool> sides;
        for(const auto owner:owners)sides.insert(nmo_right(owner,battle));
        if(sides.empty()){++nmo_terrain_skips;return;}
        // Both overloads enumerate synchronously (ba1f20..ba1f89 and
        // ba2104..ba24d4). The original mutable row references are preserved;
        // only the temporary enumeration lists are scoped to authored map areas.
        TemporaryRoots roots;roots.retain(self);roots.retain(effect);roots.retain(area);
        NMOPredefineScope scoped(self,area,sides,roots);nmo_original_change_map(self,effect,player,info);++nmo_terrain_calls;
    }catch(const std::exception& ex){nmo_fail(ex);}
}
void native_multiplayer_observation_prepare(const cnjson::Value& view){
    nb_main_thread();const auto& projection=nb_at(view,"dtoProjection");const auto& policy=nb_at(projection,"view");
    if(nb_at(projection,"allStateSync").kind!=cnjson::Value::Object)return;
    for(const auto* field:{"recipientSeat","observedSeat","writeSeat"})if(nb_at(policy,field).kind!=cnjson::Value::Integer)throw std::runtime_error("multiplayer observation seat policy missing");
    if(nb_at(policy,"observationGrantsWriteAccess").kind!=cnjson::Value::Boolean)throw std::runtime_error("multiplayer observation access policy missing");
    const auto self=nb_number(view,"seat"),observed=nb_number(policy,"observedSeat");
    if(self<0||self>3||observed<0||observed>3||nb_number(policy,"recipientSeat")!=self||nb_number(policy,"writeSeat")!=self||nb_true(policy,"observationGrantsWriteAccess"))throw std::runtime_error("multiplayer observation ownership policy invalid");
    auto room=nb_string(view,"roomId");if(room.empty())throw std::runtime_error("multiplayer observation room missing");
    if(nmo_room!=room){nmo_room=room;nmo_stage.clear();nmo_observed=nmo_applied=-1;nmo_terrain_fingerprint.clear();nmo_failed=false;nmo_error.clear();}
    const auto phase=nb_string(view,"phase"),own_stage=nb_string(nb_at(view,"selfState"),"stageId");
    const auto& all=nb_at(nb_at(nb_at(projection,"allStateSync"),"payload"),"allStateSyncData");
    const auto stage=nb_string(all,"stageId");
    if(own_stage.empty())throw std::runtime_error("multiplayer observation own base stage absent");
    if((phase=="preparation"||phase=="special")&&stage!=own_stage&&!native_multiplayer_preparation_view_owns_presentation(view))throw std::runtime_error("cross-base-stage observation is unavailable");
    // Runs before Fill/OnBattleStatusChanged: never start native own combat on
    // an observed stage or another seat's enemy/deployment projection.
    if((phase=="battle"||phase=="battle_countdown")&&!(nb_true(view,"observerOnly")&&(native_multiplayer_preparation_view_owns_presentation(view)||cndead_battle::valid(view,true)))){
        if(observed!=self||stage!=own_stage)throw std::runtime_error("multiplayer own battle projection is not the authenticated seat");
        if(nmo_reload_controller_root)throw std::runtime_error("multiplayer own battle cannot start during an observation scene load");
        if(nmh_scope()&&(!nmo_initial_stage()||nmo_stage!=own_stage))throw std::runtime_error("multiplayer own battle base stage is not loaded");
    }
    nmo_self=int32_t(self);nmo_observed=int32_t(observed);nmo_snapshot=view;nmo_own_stage=own_stage;nmo_phase=phase;nmo_ready=true;
}
bool nmo_can_reload(const cnjson::Value& view){
    const auto phase=nb_string(view,"phase");if(phase!="preparation"&&phase!="special")return false;
    const auto deadline=nb_number(view,"deadlineTs"),now=nb_number(view,"serverTime");
    // Private policy: do not begin an expensive Unity scene transition in the
    // final ten seconds. Server phase deadlines continue to run unchanged.
    return nb_at(view,"deadlineTs").kind==cnjson::Value::Null||deadline<0||(now>0&&deadline-now>=10);
}
void nmo_release_reload(){if(nmo_reload_controller_root)hold_free(nmo_reload_controller_root);nmo_reload_controller_root=0;nmo_reload_at=0;nmo_reload_level.clear();nmo_reload_stage.clear();}
void* nmo_controller(){return invoke(method(cls("Torappu.Battle.BattleController"),"get_instance",0),nullptr);}
void nmo_start_reload(const std::string& stage){
    TemporaryRoots roots;auto controller=roots.retain(nmo_controller());auto inout=roots.retain(invoke(method(cls("Torappu.Battle.BattleInOut"),"get_instance",0),nullptr));
    auto original=roots.retain(invoke(nmo_method(0x600f5cc),inout));if(!controller||!inout||!original)throw std::runtime_error("multiplayer observation original scene input missing");
    auto input_class=api.il2cpp_object_get_class(original);uint32_t align=0;auto size=api.il2cpp_class_value_size(input_class,&align);
    if(size<1||size>4096)throw std::runtime_error("multiplayer observation scene input size invalid");
    std::vector<uint8_t> bytes(size_t(size),uint8_t{});SIZE_T copied=0;
    if(!ReadProcessMemory(GetCurrentProcess(),unbox(original),bytes.data(),bytes.size(),&copied)||copied!=bytes.size())throw std::runtime_error("multiplayer observation scene input unreadable");
    auto input=roots.retain(value_box(input_class,bytes.data()));auto stage_id=roots.retain(string_new(stage.c_str()));
    auto load_id=roots.retain(string_new(native_multiplayer_stage_path(stage,CN_PRIVATE_LEVELS).c_str()));void* load_args[]={load_id};
    auto level=roots.retain(invoke(nmo_method(0x60131d7),nullptr,load_args));if(!level)throw std::runtime_error("multiplayer observation authored stage unavailable");
    const auto level_id=text(field_object(level,"levelId"));if(level_id.empty())throw std::runtime_error("multiplayer observation authored stage identity absent");
    set_object_field(input,profile_field(input_class,"levelData"),level);
    // stageInfo is a boxed value; retain all display, activity, game-mode,
    // authenticated player and callback fields in the original input copy.
    auto stage_info=roots.retain(native_battle_boxed_field(input,"stageInfo",roots));if(!stage_info)throw std::runtime_error("multiplayer observation stage info missing");
    auto native_level_id=roots.retain(string_new(level_id.c_str()));auto info_class=api.il2cpp_object_get_class(stage_info);
    set_object_field(stage_info,profile_field(info_class,"stageId"),stage_id);set_object_field(stage_info,profile_field(info_class,"levelId"),native_level_id);
    set_field(input,profile_field(input_class,"stageInfo"),unbox(stage_info));
    auto scene=roots.retain(string_new("battle_loader"));auto opts=nb_object();opts.object["stopMusic"]=nb_boolean(true);
    auto options=build_value(api.il2cpp_method_get_param(nmo_method(0x6005809),1),opts,roots);
    nmo_reload_controller_root=hold_new(controller,false);if(!nmo_reload_controller_root)throw std::runtime_error("multiplayer observation controller retention failed");
    nmo_reload_stage=stage;nmo_reload_level=level_id;nmo_reload_at=GetTickCount64();
    cnlaunch::SceneTransition transition;bool began=false;
    try{
        transition=native_battle_scene_begin();began=true;
        void* input_args[]={unbox(input)};invoke(nmo_method(0x600f5cf),inout,input_args);
        // Exact native RestartGame transition: battle_loader, Options with
        // stopMusic=true. Calling StartScene directly retains its acceptance bit.
        void* scene_args[]={scene,options.argument};
        // StartScene may synchronously dispose the old private manager before
        // the outer preparation adapter can publish its accepted loader lease.
        // Keep this exact call's lease even when recovering a terrain failure.
        struct Starting{Starting(){++nmo_start_reload_depth;}~Starting(){--nmo_start_reload_depth;}} starting;
        if(!nmh_result<uint8_t>(invoke(nmo_method(0x6005809),nullptr,scene_args)))throw std::runtime_error("multiplayer observation native loader transition declined");
        native_battle_scene_accept(transition);++nmo_reloads;
    }catch(...){if(began)native_battle_scene_rollback(transition);void* restore_args[]={unbox(original)};invoke(nmo_method(0x600f5cf),inout,restore_args);nmo_release_reload();throw;}
}
bool nmo_poll_reload(){
    if(!nmo_reload_controller_root)return true;
    if(GetTickCount64()-nmo_reload_at>30000){nmo_release_reload();throw std::runtime_error("multiplayer observation native loader timed out");}
    if(!nmh_scope())return false;auto controller=nmo_controller();
    if(!controller||controller==hold_get(nmo_reload_controller_root))return false;
    TemporaryRoots roots;roots.retain(controller);auto level=roots.retain(invoke(nmo_method(0x600fbea),controller)),map=roots.retain(invoke(nmo_method(0x600fbe8),controller)),mapData=roots.retain(invoke(nmo_method(0x600fbe9),controller));
    auto inout=roots.retain(invoke(method(cls("Torappu.Battle.BattleInOut"),"get_instance",0),nullptr));auto input=roots.retain(invoke(nmo_method(0x600f5cc),inout));auto inputLevel=roots.retain(field_object(input,"levelData"));
    if(!level||!map||!mapData||field_object(map,"m_data")!=mapData||!inputLevel)return false;
    auto info=roots.retain(native_battle_boxed_field(input,"stageInfo",roots));const auto stage=text(field_object(info,"stageId"));
    if(stage!=nmo_reload_stage||native_multiplayer_resolve_stage_path(stage,text(field_object(inputLevel,"levelId")),CN_PRIVATE_LEVELS)!=nmo_reload_level)return false;
    if(!native_battle_scene_ready(controller,nb_manager()))return false;
    nmo_stage=nmo_reload_stage;nmo_applied=-1;nmo_terrain_fingerprint.clear();nmo_release_reload();++nmo_reload_completions;return true;
}
bool nmo_initial_stage(){
    TemporaryRoots roots;auto controller=roots.retain(nmo_controller());auto inout=roots.retain(invoke(method(cls("Torappu.Battle.BattleInOut"),"get_instance",0),nullptr));
    auto input=roots.retain(invoke(nmo_method(0x600f5cc),inout));if(!controller||!input)return false;
    auto level=roots.retain(invoke(nmo_method(0x600fbea),controller));auto map=roots.retain(invoke(nmo_method(0x600fbe8),controller));auto map_data=roots.retain(invoke(nmo_method(0x600fbe9),controller));auto input_level=roots.retain(field_object(input,"levelData"));
    if(!level||!map||!map_data||!input_level||field_object(map,"m_data")!=map_data)return false;
    std::string imported_stage,imported_level;
    if(native_multiplayer_world_field_info(controller,map,imported_stage,imported_level)){
        if(native_multiplayer_stage_path(imported_stage,CN_PRIVATE_LEVELS)!=imported_level)return false;
        nmo_stage=imported_stage;return true;
    }
    // Native round preparation/combat may replace the logical LevelData with
    // its enemy template. The immutable loader input identifies the physical
    // base map; a valid template change is not a lost or unloaded battlefield.
    const auto expected=text(field_object(input_level,"levelId"));
    auto info=roots.retain(native_battle_boxed_field(input,"stageInfo",roots));const auto stage=text(field_object(info,"stageId"));
    if(stage.empty())throw std::runtime_error("multiplayer initial native stage identity absent");
    if(expected.empty()||native_multiplayer_resolve_stage_path(stage,expected,CN_PRIVATE_LEVELS).empty()||(!nmo_stage.empty()&&nmo_stage!=stage))return false;
    nmo_stage=stage;return true;
}
void native_multiplayer_observation_apply(const cnjson::Value& view){
    nb_main_thread();if(native_multiplayer_preparation_view_owns_presentation(view))return;try{if(!nmo_poll_reload())return;}catch(const std::exception& ex){nmo_fail(ex);throw;}
    if(!nmh_scope()||!nmo_ready)return;
    auto dc=nb_centre(),manager=nb_manager();auto status=field_object(manager,"gameStatus");
    if(!status)throw std::runtime_error("multiplayer observation native state missing");
    if(nmh_int(dc,"<selfPlayerIndex>k__BackingField")!=nmo_self)throw std::runtime_error("multiplayer observation cannot replace native actor identity");
    const bool battle=nmh_result<uint8_t>(invoke(nmo_method(0x60136eb),status))!=0;
    if(battle)return; // Original battle observers are driven by Help/Step runtime.
    if(!nmo_initial_stage())return;
    const auto& all=nb_at(nb_at(nb_at(nb_at(view,"dtoProjection"),"allStateSync"),"payload"),"allStateSyncData");
    auto stage=nb_string(all,"stageId");
    // A separate loader transition must replace scene/mapData for another stage;
    // SetCurrentPlayerView and FullFill alone never establish that acceptance.
    if(!nmo_stage.empty()&&stage!=nmo_stage)throw std::runtime_error("multiplayer observation cross-field switch requires its authenticated preparation plan");
    auto desired=nmo_observed;const auto previous=nmh_result<int32_t>(invoke(nmo_method(0x601343f),dc));
    const bool view_changed=previous!=desired||(nmo_applied>=0&&nmo_applied!=desired);
    if(previous!=desired){void* args[]={&desired};if(!nmh_result<uint8_t>(invoke(nmo_method(0x6013476),dc,args)))throw std::runtime_error("multiplayer observation native view declined");++nmo_switches;}
    auto area=field_object(manager,"mapAreaManager");if(!area)throw std::runtime_error("multiplayer observation map indexer missing");
    if(view_changed){invoke(nmo_method(0x6013867),area);void* args[]={&desired};invoke(nmo_method(0x601347c),dc,args);}
    const auto terrain=cnjson::dump(nb_at(nb_at(all,"sceneDetail"),"specialEffects"));
    const bool changed=!nmo_terrain_fingerprint.empty()&&terrain!=nmo_terrain_fingerprint;
    if((view_changed||changed)&&nmh_result<uint8_t>(invoke(nmo_method(0x60136f3),status))){
        invoke(nmo_method(0x60136b4),manager);if(nmo_failed)throw std::runtime_error(nmo_error);++nmo_rebuilds;
        // Native DataCenter.PrepareForPrepareState tail-calls ResetPlayerView
        // (b1149f). Reapply the real observation chain after this native reset.
        if(nmh_result<int32_t>(invoke(nmo_method(0x601343f),dc))!=desired){void* args[]={&desired};if(!nmh_result<uint8_t>(invoke(nmo_method(0x6013476),dc,args)))throw std::runtime_error("multiplayer observation native post-prepare view declined");}
        invoke(nmo_method(0x6013867),area);void* args[]={&desired};invoke(nmo_method(0x601347c),dc,args);
    }
    const auto in_ob=nmh_result<uint8_t>(invoke(nmo_method(0x601343c),dc))!=0;
    if(nmh_result<int32_t>(invoke(nmo_method(0x601343f),dc))!=desired||in_ob!=(desired!=nmo_self))throw std::runtime_error("multiplayer observation final read-only view mismatch");
    nmo_stage=stage;nmo_applied=desired;nmo_terrain_fingerprint=terrain;
}
void native_multiplayer_observation_reset(){nb_main_thread();nmo_release_reload();native_multiplayer_world_field_reset();nmo_snapshot={};nmo_room.clear();nmo_stage.clear();nmo_own_stage.clear();nmo_phase.clear();nmo_terrain_fingerprint.clear();nmo_observed=nmo_applied=nmo_self=-1;nmo_ready=nmo_failed=false;nmo_error.clear();}
void native_multiplayer_observation_require_ready(){nb_main_thread();if(nmo_reload_controller_root)throw std::runtime_error("multiplayer native observation scene is still loading");if(nmo_failed)throw std::runtime_error("multiplayer terrain execution failed: "+nmo_error);if(nmo_phase=="battle"&&(!nmo_ready||nmo_observed!=nmo_self||nmo_stage!=nmo_own_stage))throw std::runtime_error("multiplayer own battle stage/view is not ready for a terminal report");}
void native_multiplayer_observation_setup(){for(const auto& spec:NMO_METHODS){if(spec.token==0x60138bb)hook(spec,nmo_change_map,nmo_original_change_map);else bind_method(spec);}}
cnjson::Value native_multiplayer_observation_status(){auto result=nb_object();result.object["ready"]=nb_boolean(nmo_ready);result.object["failed"]=nb_boolean(nmo_failed);result.object["viewSwitches"]=nb_integer(nmo_switches.load());result.object["preparationRebuilds"]=nb_integer(nmo_rebuilds.load());result.object["terrainCalls"]=nb_integer(nmo_terrain_calls.load());result.object["terrainSkips"]=nb_integer(nmo_terrain_skips.load());result.object["failures"]=nb_integer(nmo_failures.load());result.object["reloadPending"]=nb_boolean(nmo_reload_controller_root!=0);result.object["reloadRequests"]=nb_integer(nmo_reloads.load());result.object["reloadCompletions"]=nb_integer(nmo_reload_completions.load());result.object["reloadDeferrals"]=nb_integer(nmo_reload_deferrals.load());result.object["confirmedStage"]=json_text(nmo_stage);result.object["lastError"]=json_text(nmo_error);result.object["nativeValidated"]=nb_boolean(false);return result;}
