// Included inside namespace cn, after native_battle_data.inc.
// No UI Text writes, map changes, terminal records, player HP writes or scheduler edits.
#pragma once
#include "native_multiplayer_help_counter_policy.h"

// IL2CPP inlined the HelpRoundStatus getter into this outer getter. The HUD
// calls this exact method; hooking only 601390a would miss the real consumer.
static const CN_Method NMHC_METHODS[]={
{"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","get_enemyEscapedDisplay",0x60138ec,0xba7230ULL,0,false,"System.Int32",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","get_lostHpDisplay",0x60138ee,0xba7380ULL,0,false,"System.Int32",{}},
{"Torappu.Battle.DataCenter.AutoChessDataCenter+AutoChessDataModelBase","MarkDirty",0x601349a,0xb18170ULL,0,false,"System.Void",{}},
};
using NMHCGetter=int32_t(*)(void*,const void*);
NMHCGetter nmhc_original_remaining=nullptr;
NMHCGetter nmhc_original_lost=nullptr;
std::mutex nmhc_mutex;
cnmulti_help_counter::Snapshot nmhc_snapshot;
cnmulti_help_counter::BossPreview nmhc_boss;
int32_t nmhc_boss_last=-1;
std::atomic<unsigned> nmhc_applied{0},nmhc_dirty{0},nmhc_reads{0},nmhc_failures{0};
std::string nmhc_error;
void* native_multiplayer_dispatch_scene();
void* native_multiplayer_dispatch_bridge();
std::string native_multiplayer_transport_room();
double native_multiplayer_transport_time();

int32_t nmhc_int(void* object,const char* name){
    if(!object)throw std::runtime_error("help counter native owner unavailable");
    auto field=profile_field(api.il2cpp_object_get_class(object),name);if(!matches_type(api.il2cpp_field_get_type(field),"System.Int32"))throw std::runtime_error("help counter native field declaration changed");
    int32_t result=0;api.il2cpp_field_get_value(object,field,&result);return result;
}
bool nmhc_private(){
    if(closing.load()||!can_read()||unity_ui_thread.load()!=GetCurrentThreadId()||!native_multiplayer_dispatch_scene())return false;
    auto centre=nb_centre(),manager=nb_manager(),bridge=native_multiplayer_dispatch_bridge();
    return centre&&manager&&bridge&&field_object(manager,"dataCenter")==centre&&field_object(manager,"dataBridge")==bridge&&
        act(field_object(centre,"<actId>k__BackingField"))&&api.il2cpp_object_get_class(bridge)==cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer");
}
bool nmhc_owner(void* model,const cnmulti_help_counter::Snapshot& snapshot){
    if(!snapshot.active||!nmhc_private()||native_multiplayer_transport_room()!=snapshot.room)return false;
    auto centre=nb_centre();if(!model||field_object(centre,"battleStatus")!=model)return false;
    auto status=field_object(model,"helpBattleStatus");
    return status&&field_object(model,"m_roundStatus")==status&&
        api.il2cpp_object_get_class(status)==cls("Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel+HelpBattleRoundStatus")&&
        nmhc_int(centre,"<selfPlayerIndex>k__BackingField")==snapshot.seat&&nmhc_int(centre,"<currentRound>k__BackingField")==snapshot.round;
}
bool nmhc_boss_owner(void* model,const cnmulti_help_counter::BossPreview& snapshot){
    if(snapshot.room.empty()||!nmhc_private()||native_multiplayer_transport_room()!=snapshot.room)return false;
    auto centre=nb_centre();if(!model||field_object(centre,"battleStatus")!=model)return false;
    auto status=field_object(model,"bossBattleStatus");
    return status&&field_object(model,"m_roundStatus")==status&&
        api.il2cpp_object_get_class(status)==cls("Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel+BossBattleRoundStatus")&&
        nmhc_int(centre,"<selfPlayerIndex>k__BackingField")==snapshot.seat&&nmhc_int(centre,"<currentRound>k__BackingField")==snapshot.round;
}
void native_multiplayer_hp_preview_tick(){
    nb_main_thread();cnmulti_help_counter::BossPreview snapshot;
    {std::lock_guard<std::mutex> lock(nmhc_mutex);snapshot=nmhc_boss;}
    if(snapshot.room.empty())return;
    auto model=field_object(nb_centre(),"battleStatus");if(!nmhc_boss_owner(model,snapshot))return;
    const auto lost=snapshot.lost(native_multiplayer_transport_time(),nmhc_int(field_object(model,"bossBattleStatus"),"playerTotalHp"));
    if(lost!=nmhc_boss_last){nmhc_boss_last=lost;invoke(bound_methods.at(0x601349a),model);++nmhc_dirty;}
}
int32_t nmhc_get_remaining(void* model,const void* info){
    RuntimeLease lease(runtime_gate);
    if(lease){try{cnmulti_help_counter::Snapshot snapshot;{std::lock_guard<std::mutex> lock(nmhc_mutex);snapshot=nmhc_snapshot;}
        if(nmhc_owner(model,snapshot)){++nmhc_reads;return snapshot.remaining;}
    }catch(const std::exception& error){++nmhc_failures;{std::lock_guard<std::mutex> lock(nmhc_mutex);nmhc_error=error.what();}nb_error("multiplayer-help-counter-read",error);}}
    lease.release();return nmhc_original_remaining(model,info);
}
int32_t nmhc_get_lost(void* model,const void* info){
    RuntimeLease lease(runtime_gate);
    if(lease){try{cnmulti_help_counter::Snapshot snapshot;{std::lock_guard<std::mutex> lock(nmhc_mutex);snapshot=nmhc_snapshot;}
        // Original Help OnEnemyReachExit increments this display by one for
        // each enemy belonging to self, including indexed summons. It does
        // not use Enemy.lifePoint or change the player's settled HP here.
        // An observer must read the authority's confirmed exits, not its own
        // independently simulated finish/escape journal.
        if(nmhc_owner(model,snapshot)){++nmhc_reads;return snapshot.escaped;}
        cnmulti_help_counter::BossPreview boss;{std::lock_guard<std::mutex> lock(nmhc_mutex);boss=nmhc_boss;}
        if(nmhc_boss_owner(model,boss)){
            const auto total=std::max(0,nmhc_int(field_object(model,"bossBattleStatus"),"playerTotalHp"));
            return std::min(total,std::max(nmhc_original_lost(model,info),boss.lost(native_multiplayer_transport_time(),total)));
        }
    }catch(const std::exception& error){++nmhc_failures;{std::lock_guard<std::mutex> lock(nmhc_mutex);nmhc_error=error.what();}nb_error("multiplayer-help-loss-read",error);}}
    lease.release();return nmhc_original_lost(model,info);
}
void native_multiplayer_help_counter_prepare(const cnjson::Value& view){
    // Dispatch seam: after the stale-envelope check, BEFORE Fill/OnBattleStatusChanged.
    // The original synchronous HUD refresh can then read the new immutable count.
    nb_main_thread();auto next=cnmulti_help_counter::parse(view);bool dirty;
    {std::lock_guard<std::mutex> lock(nmhc_mutex);if(cnmulti_help_counter::stale(nmhc_snapshot,next))return;
        dirty=cnmulti_help_counter::display_changed(nmhc_snapshot,next);nmhc_snapshot=std::move(next);
        auto boss=cnmulti_help_counter::boss_preview(view);
        if(boss.room!=nmhc_boss.room||boss.round!=nmhc_boss.round||boss.deadline!=nmhc_boss.deadline)nmhc_boss_last=-1;
        nmhc_boss=std::move(boss);++nmhc_applied;}
    if(dirty&&nmhc_private()){
        TemporaryRoots roots;auto model=roots.retain(field_object(nb_centre(),"battleStatus"));
        if(model){invoke(bound_methods.at(0x601349a),model);++nmhc_dirty;}
    }
}
void native_multiplayer_help_counter_reset(){std::lock_guard<std::mutex> lock(nmhc_mutex);nmhc_snapshot={};nmhc_boss={};nmhc_boss_last=-1;nmhc_error.clear();}
cnjson::Value native_multiplayer_help_counter_status(){
    std::lock_guard<std::mutex> lock(nmhc_mutex);auto value=nb_object();
    value.object["active"]=nb_boolean(nmhc_snapshot.active);value.object["seat"]=nb_integer(nmhc_snapshot.seat);
    value.object["remainingInHelp"]=nb_integer(nmhc_snapshot.remaining);value.object["unvanquishedForSettlement"]=nb_integer(nmhc_snapshot.unvanquished);
    value.object["confirmedEscapesForSelf"]=nb_integer(nmhc_snapshot.escaped);
    value.object["displayReads"]=nb_integer(nmhc_reads.load());value.object["dirtyNotifications"]=nb_integer(nmhc_dirty.load());
    value.object["failures"]=nb_integer(nmhc_failures.load());value.object["lastError"]=json_text(nmhc_error);value.object["nativeValidated"]=nb_boolean(false);return value;
}
void native_multiplayer_help_counter_setup(){
    // Bind and declaration-check named fields; no managed offset is baked in.
    for(const auto& spec:NMHC_METHODS){if(spec.token==0x60138ec)hook(spec,nmhc_get_remaining,nmhc_original_remaining);else if(spec.token==0x60138ee)hook(spec,nmhc_get_lost,nmhc_original_lost);else bind_method(spec);}
    const struct {const char* type;const char* field;const char* value;} fields[]={
        {"Torappu.Battle.DataCenter.AutoChessDataCenter","battleStatus","Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel"},
        {"Torappu.Battle.DataCenter.AutoChessDataCenter","<selfPlayerIndex>k__BackingField","System.Int32"},
        {"Torappu.Battle.DataCenter.AutoChessDataCenter","<currentRound>k__BackingField","System.Int32"},
        {"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","helpBattleStatus","Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel+HelpBattleRoundStatus"},
        {"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","bossBattleStatus","Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel+BossBattleRoundStatus"},
        {"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel+BossBattleRoundStatus","playerTotalHp","System.Int32"},
        {"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","m_roundStatus","Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel+IRoundStatus"},
    };
    for(const auto& spec:fields){auto field=profile_field(cls(spec.type),spec.field);if(!matches_type(api.il2cpp_field_get_type(field),spec.value))throw std::runtime_error("help counter native field contract mismatch");}
}
