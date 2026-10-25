// Included inside namespace cn after bind_method/hook helpers, before setup().
// Pinned CN DTO consumer bridge. Local act2 requests terminate at 127.0.0.1.
// Static method preflight is separate from native loading/shop acceptance.
#pragma once

static const CN_Method NB_DATA_METHODS[]={
{"Torappu.Battle.AutoChess.AutoChessBattleDataModel","_LoadBattleRunes",0x60138c0,0xba37e0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleDataModel","_GetPackedRuneData",0x60138ca,0xba2b60ULL,1,false,"Torappu.RuneTable+PackedRuneData",{"System.String"}},
{"Torappu.Battle.AutoChess.AutoChessBattleDataModel","_AddPackedRuneData",0x60138cd,0xba05f0ULL,3,false,"System.Void",{"Torappu.RuneTable+PackedRuneData","Torappu.PlayerSide","System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","GetBattlePlayerSide",0x601353e,0xb24d30ULL,1,true,"Torappu.PlayerSide",{"System.Boolean"}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","get_isTrainingMode",0x6013680,0xb47740ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.AutoChess.AutoChessGameStatus","get_inMainPrepareState",0x60136f3,0xb4bb30ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.AutoChess.AutoChessTutorialManager","Start",0x601387f,0xb69f00ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.AutoChessTutorialManager+AutoChessTutorialManagerConfig"}},
{"Torappu.Battle.AutoChess.AutoChessTutorialManager+BlockSubStateTask","TryBlockSubStateOrNot",0x60138a3,0xbacbc0ULL,1,false,"System.Void",{"System.Boolean"}},
{"Torappu.Battle.BuffDBUtil","TryGetTemplate",0x60131c3,0xaf41d0ULL,2,true,"System.Boolean",{"System.String","Torappu.Battle.BuffTemplate&"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqLoadReady",0x60135c3,0xb2bcb0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","Start",0x60135c4,0xb2d020ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","Finish",0x60135c5,0xb2a5b0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqBuyChess",0x60135c6,0xb2b680ULL,3,false,"System.Void",{"System.Int32","System.Boolean","System.Action"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqShopFrozen",0x60135c7,0xb2c910ULL,1,false,"System.Void",{"System.Boolean"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqShopUpgrade",0x60135c8,0xb2cc10ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqShopRefresh",0x60135c9,0xb2ca70ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqMoveChess",0x60135ca,0xb2bd10ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqUseMagic",0x60135cb,0xb2cfc0ULL,1,false,"System.Void",{"System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqEquipItem",0x60135cc,0xb2ba30ULL,3,false,"System.Void",{"System.Int32","System.Int32","System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqPrepareReady",0x60135cd,0xb2c1a0ULL,1,false,"System.Void",{"System.Boolean"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqAddBondStackCount",0x60135ce,0xb2b240ULL,3,false,"System.Void",{"System.Int32","System.Collections.Generic.List`1<System.String>","System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqHelpBattleEnemyEscape",0x60135cf,0xb2bbb0ULL,2,false,"System.Void",{"System.Int32","System.Boolean"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSelfBattleInfoUp",0x60135d0,0xb2c540ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSelfBattleEnemyEscape",0x60135d1,0xb2c280ULL,2,false,"System.Void",{"System.Int32","System.Boolean"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSelfBattleEnemyKilled",0x60135d2,0xb2c4c0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.BattleEnemyKilledInfo"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqHelpBattleEnemyKilled",0x60135d3,0xb2bc30ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.HelpBattleEnemyKilledInfo"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqObOtherPlayer",0x60135d4,0xb2c0e0ULL,1,false,"System.Void",{"System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqCancelOb",0x60135d5,0xb2b970ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqDeadAutoOb",0x60135d6,0xb2b9d0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSpPrepareSelect",0x60135d7,0xb2ccf0ULL,1,false,"System.Void",{"System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSellOrDestroy",0x60135d8,0xb2c600ULL,1,false,"System.Void",{"Torappu.GridPosition"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqBattleFinish",0x60135d9,0xb2b2d0ULL,1,false,"System.Void",{"Torappu.Battle.BattleController+GameResult"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSelfChoiceSelect",0x60135da,0xb2c5a0ULL,1,false,"System.Void",{"System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSendEmoji",0x60135db,0xb2c890ULL,2,false,"System.Void",{"System.String","System.String"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqSendBroadcast",0x60135dc,0xb2c800ULL,3,false,"System.Void",{"System.Int32","System.String","System.Collections.Generic.IList`1<System.String>"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqGiveUp",0x60135dd,0xb2bb00ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqBattleSceneActionUp",0x60135de,0xb2b600ULL,2,false,"System.Void",{"System.Int32","System.Collections.Generic.List`1<Torappu.AutoChessBattleStepActionData>"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqPauseSingleMode",0x60135df,0xb2c140ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","ReqResumeSingleMode",0x60135e0,0xb2c220ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","GenerateLocalSave",0x60135e5,0xb2a6b0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","OnMessage",0x60135f6,0xb2af80ULL,0,false,"System.Void",{}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData","FillAllStateSync",0x602b5f4,0x27690d0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessBattleAllStateSyncData"}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData","FillWithSceneStatus",0x602b5f5,0x2769880ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessBattleSceneStatus"}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData","FillWithPreparationStatus",0x602b5f6,0x27695d0ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessBattlePreparationStatus"}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData","FillWithPlayerStatus",0x602b5f7,0x2769550ULL,1,false,"System.Void",{"System.Collections.Generic.List`1<Torappu.UI.AutoChess.Server.AutoChessBattlePlayerRuntimeInfo>"}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData","FillWithBoardStatus",0x602b5f8,0x2769440ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoChessBattleBoardStatus"}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData","FillSpPreparationUpdateInfo",0x602b5f9,0x2769320ULL,1,false,"System.Void",{"Torappu.UI.AutoChess.Server.AutoAutoChessBattleSpPreparationUpdateInfo"}},
{"Torappu.UI.AutoChess.AutoChessBattleSceneData",".ctor",0x602b5fa,0x276a0a0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","get_instanceOrNull",0x601342c,0xb13fe0ULL,0,true,"Torappu.Battle.DataCenter.AutoChessDataCenter",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","get_selfPlayerBattleData",0x6013461,0xb14970ULL,0,false,"Torappu.Battle.AutoChess.PlayerBattleData",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","InitData",0x6013475,0xb0ffc0ULL,2,false,"System.Void",{"Torappu.Battle.AutoChess.SceneGameData","Torappu.Battle.AutoChess.ScenePlayerStaticData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","SetCurrentPlayerView",0x6013476,0xb11650ULL,1,false,"System.Boolean",{"System.Int32"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x6013477,0xb11bf0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.SceneStateData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x6013478,0xb11fe0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.SelfBattleData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x6013479,0xb122b0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.HelpBattleData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x601347a,0xb11f20ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.BossBattleData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x601347b,0xb12420ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.ScenePlayerRunTimeData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdatePlayerBattleData",0x601347c,0xb12590ULL,1,false,"System.Void",{"System.Int32"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x601347e,0xb12370ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.PrepareStateData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x601347f,0xb11e90ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.SpPrepareStateData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x6013480,0xb121d0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.SettleData"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","UpdateData",0x6013481,0xb12090ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.BossRoundInfo"}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter","OnTick",0x6013494,0xb11080ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","UpdateGameState",0x60136a8,0xb452f0ULL,2,false,"System.Void",{"Torappu.AutoChessGameStateType","System.Boolean"}},
{"Torappu.Battle.AutoChess.AutoChessLevelEnemyManager","Init",0x601382f,0xb636f0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.SceneRoundEnemyData"}},
{"Torappu.Battle.AutoChess.AutoChessLevelEnemyManager","UpdateRoundEnemies",0x6013832,0xb639e0ULL,1,false,"System.Void",{"Torappu.Battle.AutoChess.SceneRoundEnemyData"}},
{"Torappu.Battle.AutoChess.AutoChessLevelEnemyManager","_GenerateAllBattleLevelIds",0x6013833,0xb65170ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","GetChessInstByPos",0x60134fe,0xb269e0ULL,1,true,"Torappu.Battle.AutoChess.ChessInst",{"Torappu.GridPosition"}},
{"Torappu.Battle.AutoChess.ChessInst","get_isToken",0x60138e3,0xbad780ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","IsChessIdHasRelatedTokenInPrepare",0x601351a,0xb28010ULL,1,true,"System.Boolean",{"System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer","_ConvertChessInst",0x601361c,0xb344f0ULL,1,false,"Torappu.UI.AutoChess.Server.AutoChessBattleChessPosUnitInfo",{"Torappu.Battle.AutoChess.ChessInst"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer","_TakeCharBattleStatus",0x6013624,0xb35df0ULL,1,false,"System.Void",{"System.Collections.Generic.List`1<Torappu.UI.AutoChess.Server.AutoChessBattleCharBattleStatus>"}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","OnEnemyRegistered",0x60136a1,0xb40dd0ULL,1,false,"System.Void",{"Torappu.Battle.Enemy"}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","_OnBossBattleEnemyKilled",0x60136a5,0xb46220ULL,1,false,"System.Void",{"Torappu.Battle.Enemy"}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","_OnBossBattleEnemyReachExit",0x60136a6,0xb46360ULL,1,false,"System.Void",{"Torappu.Battle.Enemy"}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","TakeKilledInfoSnapShot",0x6013513,0xb28690ULL,1,true,"Torappu.Battle.AutoChess.BattleEnemyKilledInfo",{"System.Int32"}},
{"Torappu.Battle.Enemy","get_options",0x6012a4d,0xa7f560ULL,0,false,"Torappu.Battle.Enemy+Options",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","OnBossStateChanged",0x60138f8,0xba5c30ULL,2,false,"System.Void",{"Torappu.Battle.Enemy","System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer+MockBossBattleManager","Start",0x60135fd,0xb3c1c0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.Entity","get_hp",0x6010383,0x704d10ULL,0,false,"Torappu.FP",{}},
{"Torappu.FP","AsInt",0x6000324,0x66fae80ULL,0,false,"System.Int32",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","IsBossEnemy",0x601353a,0xb27d40ULL,1,true,"System.Boolean",{"System.String"}},
{"Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+GameSettle","OnTaskStart",0x6013d51,0xbfb4d0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","QuitTrainingGame",0x6013699,0xb42120ULL,1,false,"System.Void",{"System.Boolean"}},
{"Torappu.UI.AutoChess.AutoChessSettleGameState","_LoadSettleDataFromTrainingBattle",0x602b79f,0x278c590ULL,1,false,"Torappu.UI.AutoChess.AutoChessSeasonSettleGameInfo",{"System.String"}},
{"Torappu.Battle.AutoChess.AutoChessBattleUtil","get_indexer",0x60134e8,0xb296f0ULL,0,true,"Torappu.Battle.DataCenter.AutoChessDataIndexer",{}},
{"Torappu.Battle.DataCenter.AutoChessDataIndexer","GetSummonedEnemyIdentifierById",0x60134aa,0xb16260ULL,1,false,"System.Int32",{"System.String"}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","StartBattle",0x60136bc,0xb44670ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessDataBridgeLocalPlayer","get_currentTime",0x60135c1,0xb31be0ULL,0,false,"System.DateTime",{}},
{"Torappu.Battle.BattleController","get_fixedPlayTime",0x600fbd5,0x6416e0ULL,0,true,"Torappu.FP",{}},
{"Torappu.DateTimeUtil","TimeStampToDateTime",0x600016d,0x66e0340ULL,1,true,"System.DateTime",{"System.Int64"}},
{"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","OnBossTeamStateChanged",0x60138f9,0xba5f60ULL,2,false,"System.Void",{"System.Int32","System.Int32"}},
{"Torappu.Battle.AutoChess.AutoChessLevelEnemyManager","BossRoundSpawnEscapedEnemy",0x6013844,0xb621c0ULL,1,false,"System.Boolean",{"Torappu.Battle.AutoChess.BossBattleEnemyInfo"}},
};

std::recursive_mutex nb_data_lock;
std::atomic<bool> nb_setup_ready{false},nb_prepared{false},nb_started{false},nb_loaded{false};
std::atomic<unsigned> nb_initializations{0},nb_applied_batches{0},nb_commands{0},nb_failures{0},nb_blocked{0};
cnjson::Value nb_state,nb_initial_reply;
cnjson::Value nb_settle_game_info;
std::string nb_run_id,nb_last_error,nb_initial_scene_json,nb_last_band_selection;
// Only the saved-session reconnect stack may require a specific existing run.
// This is never inferred from a page flag and cannot allocate a successor.
thread_local std::string nb_resume_expected_run;
uint64_t nb_request_sequence=0,nb_last_ready_attempt=0;
unsigned nb_ready_attempts=0;
std::atomic<bool> nb_ready_sent{false},nb_phase_applied{false};
std::atomic<bool> nb_initial_player_view_ready{false};
thread_local unsigned nb_projection_depth=0;
thread_local bool nb_command_in_flight=false;
thread_local unsigned nb_native_transition_depth=0;
std::atomic<unsigned> nb_projection_echoes{0},nb_reentrant_requests{0};
// BattleDataModel.UpdateData can synchronously request a board move after
// arranging the hand map. Authoritative projection must stay receive-only.
struct NB_ProjectionScope{
    NB_ProjectionScope(){++nb_projection_depth;}
    ~NB_ProjectionScope(){--nb_projection_depth;}
    NB_ProjectionScope(const NB_ProjectionScope&)=delete;
    NB_ProjectionScope& operator=(const NB_ProjectionScope&)=delete;
};
struct NB_CommandScope{
    NB_CommandScope(){if(nb_command_in_flight)throw std::runtime_error("private battle command already in flight");nb_command_in_flight=true;}
    ~NB_CommandScope(){nb_command_in_flight=false;}
    NB_CommandScope(const NB_CommandScope&)=delete;
    NB_CommandScope& operator=(const NB_CommandScope&)=delete;
};
bool native_battle_data_projecting(){return nb_projection_depth!=0;}
bool nb_suppress_outbound(){
    if(nb_projection_depth){++nb_projection_echoes;return true;}
    if(nb_command_in_flight){++nb_reentrant_requests;return true;}
    return false;
}
const char* NB_UI_TYPE="Torappu.UI.AutoChess.AutoChessBattleSceneData";
const char* NB_CENTRE_TYPE="Torappu.Battle.DataCenter.AutoChessDataCenter";
void native_combat_validate_state(const cnjson::Value& value);
void native_combat_validate_resume(const cnjson::Value& value);
void native_combat_recover_prelaunch(cnjson::Value& response);
void native_combat_reset();
void native_combat_invalidate_settlement_snapshot();
void native_combat_tick();
void native_garrison_reset();
bool native_garrison_tick();
bool native_garrison_pending();
void native_garrison_recover_prelaunch(cnjson::Value& response);
bool native_combat_sync_board();
void native_preparation_tokens_preflight();
void native_preparation_tokens_reset();
void native_broadcast_session(const std::string& run);
void native_broadcast_detach();
void native_broadcast_project_identity(cnjson::Value& all);
void native_broadcast_receive(const cnjson::Value& response);

const CN_Method& nb_spec(uint32_t token){for(const auto& spec:NB_DATA_METHODS)if(spec.token==token)return spec;throw std::runtime_error("private battle method token missing");}
void* nb_method(uint32_t token){return bound_methods.at(token);}
cnjson::Value nb_object(){cnjson::Value v;v.kind=cnjson::Value::Object;return v;}
cnjson::Value nb_array(){cnjson::Value v;v.kind=cnjson::Value::Array;return v;}
cnjson::Value nb_integer(int64_t n){cnjson::Value v;v.kind=cnjson::Value::Integer;v.integer=n;return v;}
cnjson::Value nb_boolean(bool n){cnjson::Value v;v.kind=cnjson::Value::Boolean;v.boolean=n;return v;}
const cnjson::Value& nb_at(const cnjson::Value& object,const char* name){auto it=object.object.find(name);if(object.kind!=cnjson::Value::Object||it==object.object.end())throw std::runtime_error(std::string("private battle JSON field missing: ")+name);return it->second;}
std::string nb_string(const cnjson::Value& object,const char* name){const auto& v=nb_at(object,name);if(v.kind!=cnjson::Value::String)throw std::runtime_error(std::string("private battle string missing: ")+name);return v.string;}
int32_t nb_number(const cnjson::Value& object,const char* name){const auto& v=nb_at(object,name);if(v.kind!=cnjson::Value::Integer||v.integer<INT32_MIN||v.integer>INT32_MAX)throw std::runtime_error(std::string("private battle integer missing: ")+name);return int32_t(v.integer);}
bool nb_true(const cnjson::Value& object,const char* name){const auto& v=nb_at(object,name);return v.kind==cnjson::Value::Boolean&&v.boolean;}
void nb_error(const char* step,const std::exception& ex){++nb_failures;{std::lock_guard<std::recursive_mutex> lock(nb_data_lock);nb_last_error=std::string(step)+": "+ex.what();}report((std::string("private-native-battle-failed: ")+step+": "+ex.what()).c_str());}
void nb_main_thread(){if(!can_read()||unity_ui_thread.load()!=GetCurrentThreadId())throw std::runtime_error("private battle call outside observed Unity main thread");}
void* nb_manager(){auto c=cls("Torappu.Battle.AutoChess.AutoChessGameManager");return invoke(method(c,"get_instanceOrNull",0),nullptr);}
// CN 601342c dereferences the manager even though its name says OrNull.
// A room already has a service scene container before Unity creates a manager.
// Keep the original getter (including native hot updates) once its owner exists.
void* nb_centre(){return nb_manager()?invoke(nb_method(0x601342c),nullptr):nullptr;}
void* nb_retained(const char* key){auto it=held.find(key);return it==held.end()?nullptr:hold_get(it->second);}
void* nb_ui(){auto object=nb_retained("native-battle-scene-data");if(!object)throw std::runtime_error("private native scene container missing");return object;}
void* nb_container_model(void* container,const char* module){auto wrapper=field_object(container,module);auto model=field_object(wrapper,"data");if(!model)throw std::runtime_error(std::string("private battle native model missing: ")+module);return model;}
void* nb_model(const char* module){return nb_container_model(nb_ui(),module);}
void nb_fill(void* container,uint32_t token,const cnjson::Value& value);
void nb_check_container(void* container){
    for(const char* module:{"sceneGameData","playerStaticData","sceneStateData","prepareStateData","spPrepareStateData","selfBattleData","helpBattleData","bossRoundInfo","bossBattleData","playerRunTimeData","settleData","roundEnemyData"})nb_container_model(container,module);
    for(const auto& field:std::initializer_list<std::pair<const char*,const char*>>{{"sceneGameData","modeId"},{"sceneGameData","bannedBonds"},{"playerStaticData","data"},{"playerRunTimeData","data"},{"prepareStateData","shopData"}})
        if(!field_object(nb_container_model(container,field.first),field.second))throw std::runtime_error(std::string("private initial native field missing: ")+field.first+"."+field.second);
}
bool nb_in_scope(void* self=nullptr){
    auto dc=nb_centre();if(dc){auto id=field_object(dc,"<actId>k__BackingField");if(id)return act(id);}
    auto owner=nb_retained("native-battle-local-player");return nb_prepared.load()&&(!self||!owner||self==owner);
}

#include "native_broadcast.inc"

// Own HTTP boundary accepts creation (201) and preserves local error DTO bodies.
// Uses the configured independent origin, explicit bearer header, no redirects.
cnjson::Value nb_http(const std::wstring& route,const cnjson::Value* request=nullptr){
    if(route.rfind(L"/bridge/act2autochess/native-session",0)!=0)throw std::runtime_error("private battle route outside scope");
    if(nbb_participant_profile)return native_multiplayer_solo_http(route.c_str(),request);
    std::string body=request?cnjson::dump(*request):std::string{};
    HINTERNET session=WinHttpOpen(L"StrongholdCNPrivateBattle/1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);
    if(!session)throw std::runtime_error("private battle HTTP session unavailable");
    WinHttpSetTimeouts(session,500,500,900,1200);
    HINTERNET connection=private_backend_connect(session);
    HINTERNET handle=connection?WinHttpOpenRequest(connection,request?L"POST":L"GET",route.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,private_backend_request_flags()):nullptr;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,status=0,size=sizeof(status);
    bool ok=handle&&body.size()<=1048576&&WinHttpSetOption(handle,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect))&&private_backend_headers(handle)&&
        WinHttpSendRequest(handle,request?L"Content-Type: application/json\r\n":WINHTTP_NO_ADDITIONAL_HEADERS,request?DWORD(-1):0,
            request?body.data():WINHTTP_NO_REQUEST_DATA,DWORD(body.size()),DWORD(body.size()),0)&&WinHttpReceiveResponse(handle,nullptr)&&
        WinHttpQueryHeaders(handle,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);
    std::string response;auto began=GetTickCount64();
    while(ok){DWORD available=0;if(GetTickCount64()-began>3500||!WinHttpQueryDataAvailable(handle,&available)){ok=false;break;}if(!available)break;
        if(response.size()+available>8*1024*1024){ok=false;break;}auto previous=response.size();response.resize(previous+available);DWORD used=0;
        if(!WinHttpReadData(handle,response.data()+previous,available,&used)){ok=false;break;}response.resize(previous+used);}
    if(handle)WinHttpCloseHandle(handle);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
    if(!ok||response.empty())throw std::runtime_error("private battle loopback backend unavailable");
    auto parsed=cnjson::parse(response);if(status!=200&&status!=201&&status!=400)throw std::runtime_error("private battle HTTP "+std::to_string(status));return parsed;
}

void nb_check_state(const cnjson::Value& value){
    if(nb_string(value,"activityId")!="act2autochess"||!cncombat::stage_allowed(nb_string(value,"stageId"),nb_string(value,"modeId")))
        throw std::runtime_error("private native activity/mode/map checkpoint rejected");
    auto phase=nb_string(value,"phase");auto round=nb_number(value,"round");
    if(!cncombat::state_allowed(phase,round,nb_string(value,"modeId")))throw std::runtime_error("private native single-run checkpoint rejects this phase");
    const auto confirmed=value.object.find("nativeBandConfirmed");
    if(confirmed!=value.object.end()&&(confirmed->second.kind!=cnjson::Value::Boolean||
       (!confirmed->second.boolean&&(phase!="loading"||round!=0))))
        throw std::runtime_error("private strategy draft checkpoint invalid");
    native_combat_validate_state(value);
    auto id=nb_string(value,"runId");if(id.empty()||id.size()>128||id.find_first_not_of("0123456789abcdef")!=std::string::npos)throw std::runtime_error("private run identity invalid");
}
void nb_accept_reply(const cnjson::Value& response){
    auto current=nb_at(response,"state");nb_check_state(current);auto run=nb_string(current,"runId");if(!nb_run_id.empty()&&run!=nb_run_id)throw std::runtime_error("private battle reply run mismatch");
    nb_state=current;nb_run_id=run;
    auto summary=response.object.find("nativeSettleGameInfo");nb_settle_game_info=summary==response.object.end()?cnjson::Value{}:summary->second;
}
bool native_battle_session_active(){return nb_prepared.load()&&!closing.load();}
bool native_battle_data_ready(){return nb_setup_ready.load()&&!closing.load();}
const cnjson::Value& native_battle_session_state(){return nb_state;}
cnjson::Value native_battle_prebattle_snapshot(){
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    if(!nb_prepared.load())throw std::runtime_error("private prebattle session is not prepared");
    auto snapshot=nb_at(nb_initial_reply,"prebattle");
    if(nb_number(snapshot,"version")!=1||nb_string(snapshot,"activityId")!="act2autochess"||
       nb_string(snapshot,"runId")!=nb_run_id||nb_string(snapshot,"modeId")!=nb_string(nb_state,"modeId")||
       nb_string(snapshot,"stageId")!=nb_string(nb_state,"stageId")||nb_string(snapshot,"bandId")!=nb_string(nb_state,"bandId")||
       nb_number(snapshot,"revision")>nb_number(nb_state,"revision"))
        throw std::runtime_error("private prebattle snapshot does not match the authoritative run");
    return snapshot;
}
std::string native_battle_session_id(){std::lock_guard<std::recursive_mutex> lock(nb_data_lock);return nb_run_id;}
void native_battle_session_cancel(){
    native_fault_capture_scope(false);
    native_broadcast_detach();
    std::lock_guard<std::recursive_mutex> lock(nb_data_lock);nb_prepared.store(false);nb_started.store(false);nb_loaded.store(false);nb_ready_sent=false;nb_phase_applied=false;nb_initial_player_view_ready=false;
    if(!closing.load()){drop_cache("native-battle-local-player");drop_cache("native-battle-scene-data");}
    nb_run_id.clear();nb_state=cnjson::Value{};nb_initial_reply=cnjson::Value{};nb_initial_scene_json.clear();
    native_combat_reset();native_garrison_reset();native_preparation_tokens_reset();
}
std::string nb_draft_discard_run,nb_draft_discard_request;
int32_t nb_draft_discard_revision=-1;
bool nb_fresh_entry_requested=false; // Only explicit UI entry clicks set this.
bool nb_discard_draft_snapshot(const cnjson::Value& draft){
    // Only a confirmed page exit may revoke a preview. Generic detach/error
    // cleanup deliberately keeps its old save-retaining semantics.
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    const auto marker=draft.object.find("nativeBandConfirmed");
    if(marker==draft.object.end())return false; // Advanced legacy saves are formal.
    if(marker->second.kind!=cnjson::Value::Boolean)
        throw std::runtime_error("private briefing confirmation marker unavailable");
    if(marker->second.boolean)return false;
    nb_check_state(draft);
    if(nb_started.load()||nb_string(draft,"phase")!="loading"||nb_number(draft,"round")!=0)
        throw std::runtime_error("private briefing exit is not an unstarted draft");
    const auto run=nb_string(draft,"runId");const auto revision=nb_number(draft,"revision");
    if(nb_draft_discard_request.empty()||nb_draft_discard_run!=run||nb_draft_discard_revision!=revision){
        nb_draft_discard_run=run;nb_draft_discard_revision=revision;
        nb_draft_discard_request="native-discard-draft-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++nb_request_sequence);
    }
    auto request=nb_object(),payload=nb_object();request.object["actId"]=json_text("act2autochess");
    request.object["requestId"]=json_text(nb_draft_discard_request);request.object["type"]=json_text("StrongholdPrivateDiscardDraft");
    request.object["round"]=nb_integer(0);payload.object["expectedRevision"]=nb_integer(revision);request.object["payload"]=payload;
    const auto route=L"/bridge/act2autochess/native-session/"+std::wstring(run.begin(),run.end())+L"/protocol";
    cnjson::Value ack;try{ack=nb_http(route,&request);}catch(const std::exception&){ack=nb_http(route,&request);}
    if(!nb_true(ack,"ok"))throw std::runtime_error("private briefing exit rejected: "+nb_string(ack,"error"));
    const auto& state=nb_at(ack,"state");const auto& details=nb_at(ack,"details");
    if(nb_string(state,"runId")!=run||nb_string(state,"activityId")!="act2autochess"||
       nb_string(state,"phase")!="loading"||nb_number(state,"round")!=0||nb_number(state,"revision")!=revision+1||
       nb_at(state,"nativeBandConfirmed").kind!=cnjson::Value::Boolean||nb_true(state,"nativeBandConfirmed")||
       nb_at(state,"nativeDraftDiscardedAtTs").kind!=cnjson::Value::Integer||nb_number(state,"nativeDraftDiscardedAtTs")<0||
       nb_number(state,"nativeDraftDiscardedFromRevision")!=revision||!nb_true(details,"draftDiscarded")||nb_true(details,"matchStarted")||
       nb_at(ack,"messages").kind!=cnjson::Value::Array||!nb_at(ack,"messages").array.empty())
        throw std::runtime_error("private briefing discard acknowledgement mismatch");
    nb_draft_discard_request.clear();nb_draft_discard_run.clear();nb_draft_discard_revision=-1;
    report("private-unconfirmed-briefing-draft-discarded");return true;
}
bool native_battle_discard_draft(){
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    if(!nb_prepared.load()||nb_run_id.empty()||nb_string(nb_state,"runId")!=nb_run_id)
        throw std::runtime_error("private briefing exit session ownership missing");
    return nb_discard_draft_snapshot(nb_state);
}
void nb_discard_prior_entry_draft(cnjson::Value& response){
    // A new entry click also retires a preview left by an older plugin/crash.
    // Consume once before transport: uncertain creation ACKs and same-page
    // preparation retries must reuse the accepted draft rather than reroll it.
    if(!nb_fresh_entry_requested||!nb_resume_expected_run.empty())return;nb_fresh_entry_requested=false;
    auto state=response.object.find("state");if(state==response.object.end())return;
    const auto marker=state->second.object.find("nativeBandConfirmed");
    if(marker==state->second.object.end()||marker->second.kind!=cnjson::Value::Boolean||marker->second.boolean)return;
    if(!nb_discard_draft_snapshot(state->second))throw std::runtime_error("private prior briefing discard was not acknowledged");
    response=nb_http(L"/bridge/act2autochess/native-session");
    if(!nb_true(response,"ok")||response.object.count("state"))
        throw std::runtime_error("private new entry changed while its prior draft was discarded");
}
std::string nb_selected_mode="mode_single_funny";
bool nb_requires_loading_ready(const cnjson::Value& state){
    const auto phase=nb_string(state,"phase");auto pending=state.object.find("nativeReplayPending");
    const auto confirmed=state.object.find("nativeBandConfirmed");
    if(confirmed!=state.object.end()){
        if(confirmed->second.kind!=cnjson::Value::Boolean)throw std::runtime_error("private strategy confirmation marker invalid");
        if(!confirmed->second.boolean){
            if(phase!="loading"||nb_number(state,"round")!=0)throw std::runtime_error("private unconfirmed strategy outside its draft preview");
            // A preview supplies the original briefing and strategy UI. It is
            // not a launched game and may not send the loading handshake.
            return false;
        }
    }
    if(pending==state.object.end())return phase=="loading";
    if(phase!="preparation"||pending->second.kind!=cnjson::Value::Object||
       nb_number(pending->second,"version")!=1||nb_number(pending->second,"round")!=nb_number(state,"round"))
        throw std::runtime_error("private battle replay checkpoint marker invalid");
    const auto source=nb_string(pending->second,"sourcePhase");
    if(source!="battle"&&source!="boss")throw std::runtime_error("private battle replay source phase invalid");
    const auto source_revision=nb_number(pending->second,"sourceRevision");
    if(source_revision<=0||source_revision>=nb_number(state,"revision"))
        throw std::runtime_error("private battle replay source revision invalid");
    // A restored checkpoint is an untimed preparation. Only the user's
    // normal Ready command may start it, after all native battle preflights.
    return false;
}
std::string nb_loading_ready_request_id(const cnjson::Value& state){
    if(state.object.find("nativeReplayPending")!=state.object.end())
        throw std::runtime_error("private restored preparation requires the user's Ready command");
    return "native-loading-ready";
}
void nb_replay_unstarted_battle(cnjson::Value& response){
    const auto& old=nb_at(response,"state");const auto phase=nb_string(old,"phase");
    if(phase!="battle"&&phase!="boss")return;
    const auto run=nb_string(old,"runId"),mode=nb_string(old,"modeId"),stage=nb_string(old,"stageId");
    const auto round=nb_number(old,"round"),revision=nb_number(old,"revision");
    auto request=nb_object(),payload=nb_object();request.object["actId"]=json_text("act2autochess");
    request.object["requestId"]=json_text("native-battle-entry-replay-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++nb_request_sequence));
    request.object["type"]=json_text("StrongholdPrivateReplayUnstartedBattle");request.object["round"]=nb_integer(round);
    payload.object["expectedRevision"]=nb_integer(revision);request.object["payload"]=payload;
    const auto route=L"/bridge/act2autochess/native-session/"+std::wstring(run.begin(),run.end());
    cnjson::Value ack;try{ack=nb_http(route+L"/protocol",&request);}catch(const std::exception&){ack=nb_http(route+L"/protocol",&request);}
    if(!nb_true(ack,"ok"))throw std::runtime_error("private battle entry replay rejected: "+nb_string(ack,"error"));
    auto fresh=nb_http(route);const auto& state=nb_at(fresh,"state");
    if(!nb_true(fresh,"ok")||nb_string(state,"runId")!=run||nb_string(state,"modeId")!=mode||nb_string(state,"stageId")!=stage||
       nb_number(state,"round")!=round||nb_number(state,"revision")<=revision||nb_string(state,"phase")!="preparation"||
       state.object.find("nativeReplayPending")==state.object.end()||nb_requires_loading_ready(state))
        throw std::runtime_error("private battle entry replay checkpoint response mismatch");
    const auto& pending=nb_at(state,"nativeReplayPending");
    if(nb_string(pending,"sourcePhase")!=phase||nb_number(pending,"sourceRevision")!=revision)
        throw std::runtime_error("private battle entry replay source checkpoint mismatch");
    response=std::move(fresh);report("private-native-unstarted-battle-checkpoint-prepared");
}
void native_battle_session_prepare(const std::string& mode,const std::string& band){
    nb_main_thread();if(!native_battle_data_ready())throw std::runtime_error("private native battle method preflight not ready");
    std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
    if(nb_started.load())throw std::runtime_error("private native battle already started");
    auto response=nb_http(L"/bridge/act2autochess/native-session");
    nb_discard_prior_entry_draft(response);
    auto found=response.object.find("state");
    if(!nb_resume_expected_run.empty()&&(found==response.object.end()||
       nb_string(found->second,"runId")!=nb_resume_expected_run||nb_string(found->second,"phase")=="result"||
       nb_string(found->second,"modeId")!=mode||
       (found->second.object.count("nativeBandConfirmed")&&!nb_true(found->second,"nativeBandConfirmed"))))
        throw std::runtime_error("private reconnect no longer has its exact active run");
    // Finish replay belongs to the completed run, before allocating its successor.
    if(found!=response.object.end()&&nb_string(found->second,"phase")=="result"){
        native_garrison_recover_prelaunch(response);native_combat_recover_prelaunch(response);
        found=response.object.find("state");
    }
    bool restart_legacy=false;
    if(found!=response.object.end()&&nb_string(found->second,"phase")!="result"){
        for(const char* key:{"poolVersion","setupVersion","modeStrengthVersion"}){
            const auto version=found->second.object.find(key);
            if(version==found->second.object.end()||(version->second.kind==cnjson::Value::Integer&&version->second.integer==0))restart_legacy=true;
        }
    }
    const bool change_mode=found!=response.object.end()&&nb_string(found->second,"phase")!="result"&&
        cncombat::mode_allowed(mode)&&nb_string(found->second,"modeId")!=mode;
    if(!nb_resume_expected_run.empty()&&(change_mode||restart_legacy))
        throw std::runtime_error("private reconnect cannot replace its saved run");
    if(change_mode||restart_legacy){native_garrison_recover_prelaunch(response);native_combat_recover_prelaunch(response);found=response.object.find("state");}
    if(found==response.object.end()||nb_string(found->second,"phase")=="result"||change_mode||restart_legacy){
        auto request=nb_object();request.object["actId"]=json_text("act2autochess");request.object["requestId"]=json_text("native-start-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(GetTickCount64()));
        request.object["modeId"]=json_text(mode.empty()?"mode_single_funny":mode);request.object["bandId"]=json_text(band.empty()?"band_bldsk":band);
        request.object["deferBandSelection"]=nb_boolean(true);
        if(change_mode||restart_legacy)request.object["replaceActiveRunId"]=json_text(nb_string(found->second,"runId"));
        if(restart_legacy)request.object["restartLegacyRun"]=nb_boolean(true);
        // A lost first creation ACK must reuse the same draft and seed. Keep
        // this bounded to one retry, like the strategy/ordinary command path.
        try{response=nb_http(L"/bridge/act2autochess/native-session",&request);}
        catch(const std::exception&){response=nb_http(L"/bridge/act2autochess/native-session",&request);}
    }
    if(!nb_true(response,"ok"))throw std::runtime_error("private session rejected: "+nb_string(response,"error"));
    native_garrison_recover_prelaunch(response);
    native_combat_recover_prelaunch(response);
    nb_replay_unstarted_battle(response);
    const auto& state=nb_at(response,"state");nb_check_state(state);native_combat_validate_resume(state);
    const auto& messages=nb_at(response,"messages");if(messages.kind!=cnjson::Value::Array||messages.array.size()!=1||nb_string(messages.array[0],"type")!="Torappu.UI.AutoChess.Server.AutoChessBattleProtocol+AutoChessBattleSceneAllStateSyncDn")throw std::runtime_error("private initial AllStateSync DTO missing");
    auto initial=nb_at(nb_at(messages.array[0],"payload"),"allStateSyncData");native_broadcast_project_identity(initial);auto initial_json=cnjson::dump(initial);
    // This standalone native DTO consumer does not access a battle singleton.
    // Fail here, before Unity scene launch, if an AOT collection or converter is unavailable.
    if(initial_json!=nb_initial_scene_json||!nb_retained("native-battle-scene-data")){
        PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;TemporaryRoots roots;auto container=roots.retain(object_new(cls(NB_UI_TYPE)));invoke(nb_method(0x602b5fa),container);
        nb_fill(container,0x602b5f4,initial);nb_check_container(container);protect("native-battle-scene-data",container);nb_initial_scene_json=initial_json;
    }else nb_check_container(nb_ui());
    native_combat_invalidate_settlement_snapshot();
    nb_run_id.clear();nb_accept_reply(response);nb_initial_reply=response;nb_prepared.store(true);nb_started.store(false);nb_loaded.store(false);nb_ready_sent=!nb_requires_loading_ready(nb_state);nb_phase_applied=false;nb_initial_player_view_ready=false;nb_ready_attempts=0;nb_last_ready_attempt=0;
    native_broadcast_session(nb_run_id);
    native_combat_reset();native_garrison_reset();native_preparation_tokens_reset();
    report("private-native-session-typed-preflight-ready-runtime-unverified");
}
bool native_battle_prepare_for_stage(const char* stage){
    try{if(stage&&*stage&&!cncombat::stage_allowed(stage))throw std::runtime_error("private native stage whitelist rejected");native_battle_session_prepare(nb_selected_mode,"band_bldsk");return true;}
    catch(const std::exception& ex){nb_error("prepare-session",ex);return false;}
}
bool native_battle_select_band(const char* band_id){
    try{
        nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        if(nb_suppress_outbound())return false;
        if(!band_id||!*band_id||std::strlen(band_id)>128)throw std::runtime_error("private band identity missing or out of bounds");
        if(!nb_prepared.load()||nb_started.load())throw std::runtime_error("private band selection requires a prepared unstarted session");
        // An uncertain ACK or typed projection may be retried only for this
        // selected run, never by allocating or replacing another session.
        struct ExactSelectedRun{
            std::string previous;
            explicit ExactSelectedRun(const std::string& run):previous(nb_resume_expected_run){
                if(run.empty()||(!previous.empty()&&previous!=run))throw std::runtime_error("private strategy confirmation run scope mismatch");
                nb_resume_expected_run=run;
            }
            ~ExactSelectedRun(){nb_resume_expected_run=std::move(previous);}
        } exact_run(nb_run_id);
        const auto phase=nb_string(nb_state,"phase");const auto round=nb_number(nb_state,"round");
        const auto confirmation=nb_state.object.find("nativeBandConfirmed");
        if(confirmation!=nb_state.object.end()&&confirmation->second.kind!=cnjson::Value::Boolean)
            throw std::runtime_error("private strategy confirmation marker invalid");
        const bool confirmed=confirmation==nb_state.object.end()||confirmation->second.boolean;
        if(!confirmed&&(phase!="loading"||round!=0))throw std::runtime_error("private unconfirmed strategy outside its draft preview");
        if(cncombat::preparation_resume(phase,round,nb_string(nb_state,"modeId"))||
           (phase=="special"&&cncombat::state_allowed(phase,round,nb_string(nb_state,"modeId")))){
            if(nb_string(nb_state,"bandId")!=band_id)throw std::runtime_error("private resume must retain the saved band");
            nb_last_band_selection=std::string("resume-retained:")+band_id;
            report("private-native-resume-existing-band-retained-no-command");return true;
        }
        if(phase!="loading"||round!=0)throw std::runtime_error("private band selection outside initial loading or first-preparation resume");
        if(confirmed&&nb_string(nb_state,"bandId")==band_id){native_battle_session_prepare(nb_string(nb_state,"modeId"),band_id);nb_last_band_selection=std::string("loading-retained:")+band_id;return true;}
        if(confirmation!=nb_state.object.end()&&confirmed)throw std::runtime_error("private confirmed strategy cannot be replaced");
        auto request=nb_object();request.object["actId"]=json_text("act2autochess");
        request.object["requestId"]=json_text("native-band-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++nb_request_sequence));
        request.object["type"]=json_text("StrongholdPrivateSelectBand");request.object["round"]=nb_integer(0);
        auto payload=nb_object();payload.object["bandId"]=json_text(band_id);request.object["payload"]=payload;
        auto route=L"/bridge/act2autochess/native-session/"+std::wstring(nb_run_id.begin(),nb_run_id.end())+L"/protocol";
        cnjson::Value response;try{response=nb_http(route,&request);}catch(const std::exception&){response=nb_http(route,&request);}
        if(!nb_true(response,"ok"))throw std::runtime_error("private band selection rejected: "+nb_string(response,"error"));
        const auto& accepted=nb_at(response,"state");
        if(accepted.object.count("nativeBandConfirmed")&&!nb_true(accepted,"nativeBandConfirmed"))
            throw std::runtime_error("private band selection was not formally confirmed");
        nb_accept_reply(response);++nb_commands;
        // Reuse the prelaunch DTO consumer to validate the selected band and HP
        // before the native UI emits SceneStart. No battle DataCenter exists yet.
        native_battle_session_prepare(nb_string(nb_state,"modeId"),band_id);
        if(nb_string(nb_state,"bandId")!=band_id)throw std::runtime_error("private selected band acknowledgement mismatch");
        nb_last_band_selection=std::string("loading-selected:")+band_id;report("private-native-band-selection-persisted-and-converted");return true;
    }catch(const std::exception& ex){{std::lock_guard<std::recursive_mutex> lock(nb_data_lock);nb_last_band_selection=std::string("rejected:")+ex.what();}nb_error("select-band",ex);return false;}
}

void native_private_identifier_reset_projection_error();
void native_private_identifier_require_projection_success();
void nb_fill(void* container,uint32_t token,const cnjson::Value& value){
    NB_ProjectionScope projection_scope;
    TemporaryRoots roots;auto m=nb_method(token);auto argument=build_value(api.il2cpp_method_get_param(m,0),value,roots);void* arguments[]={argument.argument};
    native_private_identifier_reset_projection_error();invoke(m,container,arguments);native_private_identifier_require_projection_success();
}
void nb_link_local_save(void* self){
    auto save=field_object(self,"m_localSave");if(!save)throw std::runtime_error("private LocalSave constructor did not initialize");
    static const std::pair<const char*,const char*> mappings[]={
        {"sceneGameData","sceneGameData"},{"playerStaticData","playerStaticData"},{"sceneStateData","sceneStateData"},
        {"prepareStateData","prepareStateData"},{"spPrepareStateData","spPrepareStateData"},{"selfBattleData","selfBattleData"},
        {"helpBattleData","helpBattleData"},{"bossRoundInfo","bossRoundInfo"},{"bossBattleData","bossBattleData"},
        {"playerRunTimeData","playerRunTimeData"},{"settleData","settleData"},{"sceneRoundEnemyData","roundEnemyData"}};
    for(const auto& map:mappings)set_object_field(save,profile_field(api.il2cpp_object_get_class(save),map.first),nb_model(map.second));
    int32_t index=0,local_id=10000;set_field(self,profile_field(api.il2cpp_object_get_class(self),"m_selfPlayerIndex"),&index);set_field(self,profile_field(api.il2cpp_object_get_class(self),"m_generateId"),&local_id);
}
void nb_update(void* dc,uint32_t token,const char* module){void* args[]={nb_model(module)};invoke(nb_method(token),dc,args);}
void nb_initialize_server_enemies(void* manager,const char*& step,void* serverData=nullptr){
    step="LevelEnemyManager.server-preflight";
    if(!manager)throw std::runtime_error("private native level enemy manager missing");
    for(const char* field:{"m_gameData","m_allBattleLevelIds","m_roundEnemyData"})
        if(!field_object(manager,field))throw std::runtime_error(std::string("private native enemy field missing: ")+field);
    auto data=serverData?serverData:nb_model("roundEnemyData");
    if(!field_object(data,"roundEnemies"))throw std::runtime_error("private native server round enemy list missing");
    // Init (0xb63746..0xb637d3) also runs the offline random generator,
    // loading every future round and replacing enemy actions locally. Keep
    // its catalog selection, then consume the authoritative server list.
    step="LevelEnemyManager.GenerateAllBattleLevelIds";
    invoke(nb_method(0x6013833),manager);
    auto ids=field_object(manager,"m_allBattleLevelIds");int32_t count=0;
    if(!read(unbox(invoke(method(api.il2cpp_object_get_class(ids),"get_Count",0),ids)),count)||count<1)
        throw std::runtime_error("private native battle level catalog unavailable");
    step="LevelEnemyManager.UpdateRoundEnemies";
    void* args[]={data};invoke(nb_method(0x6013832),manager,args);
}
void nb_ensure_initial_player_view(void* dc){
    nb_main_thread();
    if(!nb_prepared.load()||!nb_started.load()||!nb_loaded.load())throw std::runtime_error("private initial player view requires completed native preload");
    if(nb_initial_player_view_ready.load())return;
    auto game=nb_model("sceneGameData");int32_t self_index=-1;
    api.il2cpp_field_get_value(game,profile_field(api.il2cpp_object_get_class(game),"selfIndex"),&self_index);
    if(self_index!=0)throw std::runtime_error("private first-map player view requires selfIndex zero");
    // FinishLoading (0xb3efad..0xb3efc9) supplies this baseline before
    // OnMessage can enter the first GM state and expose its shop/player UI.
    nb_update(dc,0x601347e,"prepareStateData");
    void* args[]={&self_index};uint8_t accepted=0;
    if(!read(unbox(invoke(nb_method(0x6013476),dc,args)),accepted)||!accepted)throw std::runtime_error("private initial native player view rejected");
    invoke(nb_method(0x601347c),dc,args);
    nb_initial_player_view_ready.store(true);
}
void nb_apply_state_models(void* dc,int32_t state){
    // Exact OnMessage switch at 0xb2b220: only states 2..7 project phase data.
    // The local MockGarrisonManager.Refresh between prepare/runtime implements
    // native training rules; the private server owns those enemy/player writes.
    switch(state){
        case 2:nb_update(dc,0x601347f,"spPrepareStateData");break;
        case 4:nb_update(dc,0x6013478,"selfBattleData");[[fallthrough]];
        case 3:nb_update(dc,0x601347e,"prepareStateData");nb_update(dc,0x601347b,"playerRunTimeData");break;
        case 5:nb_update(dc,0x6013479,"helpBattleData");break;
        case 6:nb_update(dc,0x601347a,"bossBattleData");break;
        case 7:nb_update(dc,0x6013480,"settleData");break;
        default:break;
    }
}
void nb_update_models(bool change_state,bool round_enemies=false){
    NB_ProjectionScope projection_scope;
    PrivateDataScope private_scope(true);
    auto dc=nb_centre();auto gm=nb_manager();if(!dc||!gm)throw std::runtime_error("private native game/data center unavailable");
    nb_ensure_initial_player_view(dc);
    int32_t previous_round=0;api.il2cpp_field_get_value(dc,profile_field(api.il2cpp_object_get_class(dc),"<currentRound>k__BackingField"),&previous_round);
    if(round_enemies){auto manager=field_object(gm,"levelEnemyManager");if(!manager)throw std::runtime_error("private native enemy manager missing");void* args[]={nb_model("roundEnemyData")};invoke(nb_method(0x6013832),manager,args);}
    auto scene=nb_model("sceneStateData");int32_t state=0,round=0;api.il2cpp_field_get_value(scene,profile_field(api.il2cpp_object_get_class(scene),"state"),&state);api.il2cpp_field_get_value(scene,profile_field(api.il2cpp_object_get_class(scene),"round"),&round);
    nb_update(dc,0x601347b,"playerRunTimeData");nb_update(dc,0x6013477,"sceneStateData");nb_update(dc,0x6013481,"bossRoundInfo");
    native_broadcast_before_state_change(state);
    if(change_state){
        uint8_t changed=previous_round!=round||(!nb_phase_applied&&round>0);void* args[]={&state,&changed};
        cngarrison::TransitionScope transition(nb_native_transition_depth,state==4||state==6);
        invoke(nb_method(0x60136a8),gm,args);nb_phase_applied=true;
    }
    // These models retain the same GarrisonBond objects. Preserve the pinned
    // native OnMessage order; a phase-entry delta is not copied over here.
    nb_apply_state_models(dc,state);
    invoke(nb_method(0x6013494),dc);
}
void nb_apply_messages(const cnjson::Value& response){
    NB_BroadcastReplyScope broadcast_reply_scope(response);
    NB_ProjectionScope projection_scope;
    PrivateDataScope private_scope(true);
    const auto& state=nb_at(response,"state");nb_check_state(state);if(nb_string(state,"runId")!=nb_run_id)throw std::runtime_error("private battle DTO run mismatch");
    nb_accept_reply(response); // Native state-change handlers may synchronously request another operation.
    const auto& messages=nb_at(response,"messages");if(messages.kind!=cnjson::Value::Array||messages.array.size()>64)throw std::runtime_error("private DTO batch invalid");
    for(const auto& message:messages.array){auto name=nb_string(message,"type");const auto& payload=nb_at(message,"payload");auto cut=name.find_last_of('+');if(cut!=std::string::npos)name=name.substr(cut+1);
        if(name=="AutoChessBattleSceneAllStateSyncDn"){auto all=nb_at(payload,"allStateSyncData");native_broadcast_project_identity(all);nb_fill(nb_ui(),0x602b5f4,all);nb_update_models(true,true);}
        else if(name=="AutoChessBattleSceneChangeStatusDn"){nb_fill(nb_ui(),0x602b5f5,nb_at(payload,"sceneStatus"));nb_update_models(true,true);}
        else if(name=="AutoChessScenePreparationStatusResp"){nb_fill(nb_ui(),0x602b5f6,nb_at(payload,"preparationStatus"));nb_update_models(false);}
        else if(name=="AutoChessBattleSceneChangePositionDn"){nb_fill(nb_ui(),0x602b5f8,nb_at(payload,"boardStatus"));nb_update_models(false);}
        else if(name=="AutoChessBattleSceneSpPreparationPickDn"){nb_fill(nb_ui(),0x602b5f9,nb_at(payload,"updateInfo"));nb_update_models(false);}
        else if(name=="AutoChessBattleSceneShopFrozeDn"){} // Authoritative PreparationStatusResp follows in this private batch.
        else throw std::runtime_error("private native response adapter unavailable: "+name);
    }
    native_broadcast_receive(response);++nb_applied_batches;
}
bool nb_send(const char* type,const cnjson::Value& payload,const char* stable_id=nullptr,int32_t stable_round=-1){
    nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);if(!nb_prepared.load()||!nb_started.load())throw std::runtime_error("private native session not initialized");
    if(nb_suppress_outbound())return false;
    if(native_garrison_pending())throw std::runtime_error("private garrison confirmation pending; retry after queued layers are acknowledged");
    if(std::strcmp(type,"AutoChessBattleScenePauseUp")==0&&nb_string(nb_state,"phase")!="preparation")
        throw std::runtime_error("private native first battle does not support mid-battle pause");
    if(stable_round!=-1&&(!stable_id||stable_round<1||stable_round>15||(std::strcmp(type,"AutoChessBattleSceneSelfBattleFinishUp")!=0&&std::strcmp(type,"AutoChessBattleSceneSelfBattleAddBondUp")!=0&&std::strcmp(type,"StrongholdPrivateBossBattleFinish")!=0)))
        throw std::runtime_error("private fixed-round retry is restricted to introductory final or garrison reports");
    NB_CommandScope command_scope;
    auto request=nb_object();request.object["actId"]=json_text("act2autochess");request.object["requestId"]=json_text(stable_id?stable_id:("native-"+std::to_string(GetCurrentProcessId())+"-"+std::to_string(++nb_request_sequence)));
    request.object["type"]=json_text(type);request.object["payload"]=payload;request.object["round"]=stable_round==-1?nb_at(nb_state,"round"):nb_integer(stable_round);
    const auto endpoint=L"/bridge/act2autochess/native-session/"+std::wstring(nb_run_id.begin(),nb_run_id.end())+L"/protocol";
    cnjson::Value response;
    try{response=nb_http(endpoint,&request);}catch(const std::exception&){response=nb_http(endpoint,&request);} // Identical request ID/body on uncertain outcome.
    nb_apply_messages(response);++nb_commands;
    if(!nb_true(response,"ok")){report((std::string("private-native-command-rejected: ")+nb_string(response,"error")).c_str());return false;}
    report("private-native-command-applied-runtime-unverified");return true;
}
void nb_recover_after_error(){
    if(nb_suppress_outbound())return;
    try{nb_main_thread();if(nb_started.load()&&!nb_run_id.empty()){
        // A full model replacement cannot restore a running Unity battle.
        if(nb_string(nb_state,"phase")=="battle"||nb_string(nb_state,"phase")=="boss")return;
        auto route=L"/bridge/act2autochess/native-session/"+std::wstring(nb_run_id.begin(),nb_run_id.end());nb_apply_messages(nb_http(route));}}
    catch(const std::exception& ex){nb_error("recover-command",ex);}
}

using NB_Void0=void(*)(void*,const void*);
NB_Void0 nb_old_generate=nullptr,nb_old_start=nullptr,nb_old_finish=nullptr,nb_old_message=nullptr,nb_old_load_ready=nullptr;
void nb_generate(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{if(!nb_in_scope(self)){lease.release();nb_old_generate(self,method_info);return;}nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        if(!nb_prepared.load())throw std::runtime_error("private training economy blocked: no prepared server session");
        PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;if(nb_initial_scene_json.empty())throw std::runtime_error("private typed scene preflight missing");nb_check_container(nb_ui());protect("native-battle-local-player",self);
        nb_link_local_save(self);++nb_initializations;
        report("private-native-local-save-converted-runtime-unverified");
    }catch(const std::exception& ex){nb_error("GenerateLocalSave",ex);}
}
void nb_start(void* self,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    const char* step="scope";
    try{if(!nb_in_scope(self)){lease.release();nb_old_start(self,method_info);return;}nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        if(!nb_prepared.load()||nb_retained("native-battle-local-player")!=self)throw std::runtime_error("private converted LocalPlayer unavailable");
        step="native-context";
        PrivateDataScope private_scope(true);NB_ProjectionScope projection_scope;auto dc=nb_centre();auto gm=nb_manager();if(!dc||!gm)throw std::runtime_error("private native Start context missing");
        step="DataCenter.InitData";
        void* init[]={nb_model("sceneGameData"),nb_model("playerStaticData")};invoke(nb_method(0x6013475),dc,init);
        step="LevelEnemyManager.Init";
        nb_initialize_server_enemies(field_object(gm,"levelEnemyManager"),step);
        // Native Start also defers its first preparation/player projection to
        // FinishLoading. The GM starts map/effect managers after this bridge
        // returns; runtime projection already calls UpdatePlayerBattleData.
        // Our matching barrier is native preload completion, followed by the
        // private LoadingReady response and nb_update_models() on the UI tick.
        nb_started.store(true);report("private-native-data-center-initialized-awaiting-native-preload");
    }catch(const std::exception& ex){nb_error((std::string("LocalPlayer.Start/")+step).c_str(),ex);}
}
void nb_finish(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    // DataCenter may already be disposed during native scene teardown. The
    // retained exact private bridge owns this cleanup; never invoke its dead
    // singleton getter merely to rediscover an identity already retained.
    auto owner=nb_retained("native-battle-local-player");
    if(!self||self!=owner){lease.release();nb_old_finish(self,m);return;}
    native_battle_session_cancel();report("private-native-session-detached-save-retained");
    }catch(const std::exception& ex){nb_error("LocalPlayer.Finish",ex);}}
void nb_message(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{if(!nb_in_scope(self)){lease.release();nb_old_message(self,m);return;}++nb_blocked;report("private-native-training-OnMessage-suppressed");}catch(const std::exception& ex){nb_error("OnMessage",ex);}}
void native_battle_data_loaded(){
    if(nb_prepared.load())nb_loaded.store(true);
    // Cache exact private-scene eligibility outside the exception handler.
    // Diagnostic availability must not alter native loading behavior.
    try{
        nb_main_thread();std::lock_guard<std::recursive_mutex> lock(nb_data_lock);
        bool eligible=nb_prepared.load()&&nb_started.load()&&nb_loaded.load()&&!nb_run_id.empty()&&
            nb_string(nb_state,"activityId")=="act2autochess"&&
            cncombat::stage_allowed(nb_string(nb_state,"stageId"),nb_string(nb_state,"modeId"));
        auto dc=eligible?nb_centre():nullptr;auto gm=eligible?nb_manager():nullptr;
        eligible=eligible&&dc&&gm&&field_object(gm,"dataCenter")==dc&&act(field_object(dc,"<actId>k__BackingField"));
        native_fault_capture_scope(eligible);
    }catch(...){native_fault_capture_scope(false);}
}
void nb_load_ready(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{if(!nb_in_scope(self)){lease.release();nb_old_load_ready(self,m);return;}nb_main_thread();native_battle_data_loaded();}catch(const std::exception& ex){nb_error("ReqLoadReady",ex);}}
void native_battle_data_tick(){
    RuntimeLease lease(runtime_gate);if(!lease||!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||closing.load())return;
    if(nb_ready_sent){if(!nb_phase_applied)try{nb_main_thread();nb_update_models(true,true);}catch(const std::exception& ex){nb_error("resume-native-state",ex);}native_garrison_tick();native_combat_tick();return;}
    if(nb_ready_attempts>=3||(nb_last_ready_attempt&&GetTickCount64()-nb_last_ready_attempt<5000))return;
    nb_last_ready_attempt=GetTickCount64();++nb_ready_attempts;
    try{nb_main_thread();if(!nb_requires_loading_ready(nb_state)){nb_ready_sent=true;return;}const auto id=nb_loading_ready_request_id(nb_state);nb_ready_sent=nb_send("AutoChessBattleSceneLoadingReadyUp",nb_object(),id.c_str());if(nb_ready_sent)report("private-native-loading-handshake-applied-runtime-unverified");}
    catch(const std::exception& ex){nb_error("loading-ready",ex);nb_recover_after_error();}
}

template<unsigned Index,class... Args> struct NB_BlockedRequest{
    using Function=void(*)(void*,Args...,const void*);static inline Function original=nullptr;
    static void call(void* self,Args... args,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
        if(!nb_in_scope(self)){lease.release();original(self,args...,m);return;}++nb_blocked;report((std::string("private-native-request-blocked: ")+nb_spec(Index).name).c_str());
    }catch(const std::exception& ex){nb_error("blocked-request",ex);}}
};
template<class Function> void nb_hook(uint32_t token,Function handler,Function& original){
    const auto& spec=nb_spec(token);auto m=bind_method(spec);void* target=nullptr;if(!read(m,target)||!target)throw std::runtime_error("private battle method pointer missing: "+std::string(spec.name));
    void* replacement=nullptr;std::memcpy(&replacement,&handler,sizeof(handler));void* trampoline=nullptr;
    const auto status=MH_CreateHook(target,replacement,&trampoline);
    if(status!=MH_OK){char context[256]{};std::snprintf(context,sizeof(context),"private battle hook create failed: %s token=0x%08x target=%p status=%s (%d)",spec.name,unsigned(token),target,MH_StatusToString(status),int(status));throw std::runtime_error(context);}
    std::memcpy(&original,&trampoline,sizeof(original));
}

using NB_Buy=void(*)(void*,int32_t,bool,void*,const void*);NB_Buy nb_old_buy=nullptr;
using NB_Frozen=void(*)(void*,bool,const void*);NB_Frozen nb_old_frozen=nullptr,nb_old_prepare=nullptr;
NB_Void0 nb_old_refresh=nullptr,nb_old_upgrade=nullptr,nb_old_move=nullptr,nb_old_quit=nullptr,nb_old_pause=nullptr,nb_old_resume=nullptr;
using NB_Equip=void(*)(void*,int32_t,int32_t,int32_t,const void*);NB_Equip nb_old_equip=nullptr;
using NB_Destroy=void(*)(void*,uint64_t,const void*);NB_Destroy nb_old_destroy=nullptr;
using NB_Choice=void(*)(void*,int32_t,const void*);NB_Choice nb_old_sp_choice=nullptr,nb_old_self_choice=nullptr;
void nb_choice(void* self,int32_t slot,const void* info,bool special,NB_Choice original){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{
        if(!nb_in_scope(self)){lease.release();original(self,slot,info);return;}
        if(nb_suppress_outbound())return;nb_main_thread();
        if(!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||
           nb_string(nb_state,"phase")!=(special?"special":"preparation"))throw std::runtime_error("private choice requires its loaded decision phase");
        if(slot<0||slot>5)throw std::runtime_error("private choice slot outside native bounds");
        PrivateDataScope private_scope(true);auto payload=nb_object();payload.object["slotId"]=nb_integer(slot);
        nb_send(special?"AutoChessBattleSceneSpPreparationPickUp":"AutoChessBattleSceneSelfChoicePickUp",payload);
    }catch(const std::exception& error){nb_error(special?"ReqSpPrepareSelect":"ReqSelfChoiceSelect",error);nb_recover_after_error();}
}
void nb_sp_choice(void* self,int32_t slot,const void* info){nb_choice(self,slot,info,true,nb_old_sp_choice);}
void nb_self_choice(void* self,int32_t slot,const void* info){nb_choice(self,slot,info,false,nb_old_self_choice);}
void nb_buy(void* self,int32_t slot,bool special,void* callback,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{if(!nb_in_scope(self)){lease.release();nb_old_buy(self,slot,special,callback,m);return;}if(nb_suppress_outbound())return;TemporaryRoots roots;roots.retain(callback);
        PrivateDataScope private_scope(true);auto payload=nb_object();payload.object["slotId"]=nb_integer(slot);payload.object["isSpecial"]=nb_boolean(special);
        if(nb_send("AutoChessBattleSceneShopBuyChessUp",payload)&&callback)invoke(method(api.il2cpp_object_get_class(callback),"Invoke",0),callback);
    }catch(const std::exception& ex){nb_error("ReqBuyChess",ex);nb_recover_after_error();}
}
void nb_simple(void* self,const void* m,NB_Void0 original,const char* protocol){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    try{if(!nb_in_scope(self)){lease.release();original(self,m);return;}if(nb_suppress_outbound())return;nb_send(protocol,nb_object());}catch(const std::exception& ex){nb_error(protocol,ex);nb_recover_after_error();}}
void nb_refresh(void* s,const void* m){nb_simple(s,m,nb_old_refresh,"AutoChessBattleSceneShopRefreshUp");}
void nb_upgrade(void* s,const void* m){nb_simple(s,m,nb_old_upgrade,"AutoChessBattleSceneShopUpgradeUp");}
void nb_quit(void* s,const void* m);
void nb_pause(void* s,const void* m);
void nb_resume(void* s,const void* m){nb_simple(s,m,nb_old_resume,"AutoChessBattleSceneResumeUp");}
void nb_frozen(void* s,bool frozen,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(s)){lease.release();nb_old_frozen(s,frozen,m);return;}if(nb_suppress_outbound())return;auto payload=nb_object();payload.object["isFrozen"]=nb_boolean(frozen);nb_send("AutoChessBattleSceneShopFrozeUp",payload);
    }catch(const std::exception& ex){nb_error("ReqShopFrozen",ex);nb_recover_after_error();}}
void nb_equip(void* s,int32_t equip,int32_t target,int32_t replaced,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(s)){lease.release();nb_old_equip(s,equip,target,replaced,m);return;}if(nb_suppress_outbound())return;auto payload=nb_object();payload.object["equipChessInstId"]=nb_integer(equip);payload.object["charChessInstId"]=nb_integer(target);payload.object["isChangeEquip"]=nb_boolean(replaced>=0);payload.object["unloadInstId"]=nb_integer(replaced);nb_send("AutoChessBattleSceneUseEquipUp",payload);
    }catch(const std::exception& ex){nb_error("ReqEquipItem",ex);nb_recover_after_error();}}
int32_t nb_native_int(void* object,const char* name){int32_t result=0;if(!object)throw std::runtime_error("private native integer object null");api.il2cpp_field_get_value(object,profile_field(api.il2cpp_object_get_class(object),name),&result);return result;}
void nb_move(void* s,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(s)){lease.release();nb_old_move(s,m);return;}if(nb_suppress_outbound())return;native_combat_sync_board();
    }catch(const std::exception& ex){nb_error("ReqMoveChess",ex);nb_recover_after_error();}}
void nb_destroy(void* s,uint64_t pos,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;try{
    if(!nb_in_scope(s)){lease.release();nb_old_destroy(s,pos,m);return;}if(nb_suppress_outbound())return;nb_main_thread();void* args[]={&pos};auto chess=invoke(nb_method(0x60134fe),nullptr,args);if(!chess)throw std::runtime_error("private chess at position unavailable");auto payload=nb_object();payload.object["chessInstId"]=nb_integer(nb_native_int(chess,"instId"));nb_send("AutoChessBattleSceneDestroyChessUp",payload);
    }catch(const std::exception& ex){nb_error("ReqSellOrDestroy",ex);nb_recover_after_error();}}

#include "native_battle_resource_preflight.inc"
#include "native_battle_buff_templates.inc"
#include "native_battle_tutorial.inc"
#include "native_battle_mode_runes.inc"
#include "native_combat.inc"
#include "native_garrison.inc"
#include "native_solo_exit.inc"

std::string native_battle_data_status_json(){std::unique_lock<std::recursive_mutex> lock(nb_data_lock,std::try_to_lock);auto v=nb_object();v.object["snapshotBusy"]=nb_boolean(!lock.owns_lock());v.object["staticPreflightReady"]=nb_boolean(nb_setup_ready.load());v.object["sessionPrepared"]=nb_boolean(nb_prepared.load());v.object["dataCenterInitialized"]=nb_boolean(nb_started.load());v.object["nativeLoadingCallbackObserved"]=nb_boolean(nb_loaded.load());v.object["loadingReadyAccepted"]=nb_boolean(nb_ready_sent);if(lock.owns_lock())v.object["runId"]=json_text(nb_run_id);v.object["initializations"]=nb_integer(nb_initializations.load());v.object["batchesApplied"]=nb_integer(nb_applied_batches.load());v.object["commands"]=nb_integer(nb_commands.load());v.object["failures"]=nb_integer(nb_failures.load());v.object["blockedRequests"]=nb_integer(nb_blocked.load());v.object["projectionEchoesBlocked"]=nb_integer(nb_projection_echoes.load());v.object["reentrantRequestsBlocked"]=nb_integer(nb_reentrant_requests.load());if(lock.owns_lock()){v.object["lastError"]=json_text(nb_last_error);v.object["bandSelection"]=json_text(nb_last_band_selection);}v.object["combat"]=native_combat_status(lock.owns_lock());v.object["garrison"]=native_garrison_status(lock.owns_lock());v.object["broadcast"]=native_broadcast_status(lock.owns_lock());v.object["runtimeAccepted"]=nb_boolean(false);v.object["officialFallback"]=nb_boolean(false);return cnjson::dump(v);}

void native_battle_data_setup(){
    if(nb_setup_ready.load())return;
    // Install hooks before binding their tokens; global setup enables them only
    // after all profile/resource/data contracts have passed their own preflight.
    nb_hook(0x60135e5,nb_generate,nb_old_generate);nb_hook(0x60135c4,nb_start,nb_old_start);nb_hook(0x60135c5,nb_finish,nb_old_finish);nb_hook(0x60135f6,nb_message,nb_old_message);nb_hook(0x60135c3,nb_load_ready,nb_old_load_ready);
    nb_hook(0x60135c6,nb_buy,nb_old_buy);nb_hook(0x60135c7,nb_frozen,nb_old_frozen);nb_hook(0x60135c8,nb_upgrade,nb_old_upgrade);nb_hook(0x60135c9,nb_refresh,nb_old_refresh);nb_hook(0x60135ca,nb_move,nb_old_move);nb_hook(0x60135cc,nb_equip,nb_old_equip);nb_hook(0x60135d8,nb_destroy,nb_old_destroy);nb_hook(0x60135dd,nb_quit,nb_old_quit);nb_hook(0x60135df,nb_pause,nb_old_pause);nb_hook(0x60135e0,nb_resume,nb_old_resume);
    // First-map preparation/shop milestone: unsupported requests are scoped,
    // logged and blocked; their original local training economy never runs.
    using B3=NB_BlockedRequest<0x60135cf,int32_t,bool>;nb_hook(0x60135cf,B3::call,B3::original);
    using B7=NB_BlockedRequest<0x60135d3,void*>;nb_hook(0x60135d3,B7::call,B7::original);
    using B8=NB_BlockedRequest<0x60135d4,int32_t>;nb_hook(0x60135d4,B8::call,B8::original);
    using B9=NB_BlockedRequest<0x60135d5>;nb_hook(0x60135d5,B9::call,B9::original);
    using B10=NB_BlockedRequest<0x60135d6>;nb_hook(0x60135d6,B10::call,B10::original);
    nb_hook(0x60135d7,nb_sp_choice,nb_old_sp_choice);nb_hook(0x60135da,nb_self_choice,nb_old_self_choice);
    using B14=NB_BlockedRequest<0x60135db,void*,void*>;nb_hook(0x60135db,B14::call,B14::original);
    using B15=NB_BlockedRequest<0x60135dc,int32_t,void*,void*>;nb_hook(0x60135dc,B15::call,B15::original);
    using B16=NB_BlockedRequest<0x60135de,int32_t,void*>;nb_hook(0x60135de,B16::call,B16::original);
    native_battle_buff_templates_setup();native_battle_tutorial_setup();native_battle_mode_runes_setup();native_broadcast_setup();native_combat_setup();native_garrison_setup();native_solo_exit_setup();
    for(const auto& spec:NB_DATA_METHODS)bind_method(spec);
    auto all=cls("Torappu.UI.AutoChess.Server.AutoChessBattleAllStateSyncData");uint32_t alignment=0;if(!all||!api.il2cpp_class_is_valuetype(all)||api.il2cpp_class_value_size(all,&alignment)!=64)throw std::runtime_error("private AllStateSync value ABI mismatch");
    auto save=cls("Torappu.Battle.AutoChess.LocalSave");if(!save||!cls(NB_UI_TYPE))throw std::runtime_error("private battle native classes unavailable");
    // Every wrapper.data must be exactly the native model type accepted by
    // LocalSave. Managed references will be assigned with write barriers.
    for(const auto& pair:std::initializer_list<std::pair<const char*,const char*>>{{"sceneGameData","sceneGameData"},{"playerStaticData","playerStaticData"},{"sceneStateData","sceneStateData"},{"prepareStateData","prepareStateData"},{"spPrepareStateData","spPrepareStateData"},{"selfBattleData","selfBattleData"},{"helpBattleData","helpBattleData"},{"bossRoundInfo","bossRoundInfo"},{"bossBattleData","bossBattleData"},{"playerRunTimeData","playerRunTimeData"},{"settleData","settleData"},{"sceneRoundEnemyData","roundEnemyData"}}){
        auto target=profile_field(save,pair.first);auto wrapper=api.il2cpp_class_from_type(api.il2cpp_field_get_type(profile_field(cls(NB_UI_TYPE),pair.second)));auto data=profile_field(wrapper,"data");
        char* expected=api.il2cpp_type_get_name(api.il2cpp_field_get_type(target));if(!expected)throw std::runtime_error("private LocalSave field type missing");bool match=matches_type(api.il2cpp_field_get_type(data),expected);api.il2cpp_free(expected);if(!match)throw std::runtime_error("private LocalSave/UI wrapper type mismatch");
    }
    nb_setup_ready.store(true);report("private-native-data-method-preflight-ready-runtime-unverified");
}
