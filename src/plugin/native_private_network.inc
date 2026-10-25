// Original battle HUD queries this phase, bypassing the lobby mode getter.
#pragma once
inline constexpr CN_Method NPN_BATTLE_PING={"Torappu.UI.AutoChess.Server.AutoChessServiceBattlePhase","GetPing",0x602bd6d,0x27f09f0ULL,0,false,"System.Single",{}};
inline constexpr CN_Method NPN_SERVICE_PING={"Torappu.UI.AutoChess.Server.AutoChessService","get_ping",0x602bcdc,0x27f4f80ULL,0,false,"System.Int32",{}};
using NPN_Ping=float(*)(void*,const void*);
NPN_Ping npn_old_ping=nullptr;
using NPN_ServicePing=int32_t(*)(void*,const void*);
NPN_ServicePing npn_old_service_ping=nullptr;
std::atomic<unsigned> npn_solo_reads{0},npn_multi_reads{0};
std::string native_private_network_status_json(){return "{\"soloHudPingReads\":"+std::to_string(npn_solo_reads.load())+",\"multiHudPingReads\":"+std::to_string(npn_multi_reads.load())+"}";}
int32_t npn_service_ping(void* service,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return -1;
    if(service&&can_read()&&unity_ui_thread.load()==GetCurrentThreadId()){
        auto mode=native_multiplayer_entry_mode();
        if(service==native_multiplayer_entry_service()&&nmd_scope(mode)){
            ++npn_multi_reads;
            if(service==native_multi_entry_terminal_ui_service)return 0;
            return native_multiplayer_transport_ping();
        }
        if(nbb_participant_profile&&nb_prepared.load()&&native_battle_launch_scene_entered.load()&&act(field_object(field_object(service,"<param>k__BackingField"),"activityId"))){
            auto manager=nb_manager(),local_mode=field_object(service,"m_mode"),core=field_object(service,"m_coreImpl");
            if(manager&&nb_in_scope(field_object(manager,"dataBridge"))&&local_mode&&core&&
                api.il2cpp_object_get_class(local_mode)==cls("Torappu.UI.AutoChess.Server.AutoChessTrainingServiceMode")&&
                field_object(local_mode,"m_serviceCore")==core&&field_object(core,"m_service")==service){
                ++npn_solo_reads;return native_multiplayer_solo_ping();
            }
        }
    }
    lease.release();return npn_old_service_ping(service,info);
}
float npn_battle_ping(void* phase,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return -1.0f;
    if(phase&&can_read()&&unity_ui_thread.load()==GetCurrentThreadId()){
        auto core=field_object(phase,"<serviceCore>k__BackingField");
        auto mode=native_multiplayer_entry_mode();
        if(nmd_scope(mode)){
            auto service=native_multiplayer_entry_service();
            if(core&&core==field_object(service,"m_coreImpl")&&phase==field_object(service,"m_curPhase")){
                ++npn_multi_reads;return float(native_multiplayer_transport_ping());
            }
        }
        if(nbb_participant_profile&&nb_prepared.load()&&native_battle_launch_scene_entered.load()){
            auto manager=nb_manager();
            if(manager&&field_object(phase,"m_gameManager")==manager&&nb_in_scope(field_object(manager,"dataBridge"))){
                ++npn_solo_reads;return float(native_multiplayer_solo_ping());
            }
        }
    }
    lease.release();return npn_old_ping(phase,info);
}
void native_private_network_setup(){
    auto phase=cls("Torappu.UI.AutoChess.Server.AutoChessServiceBattlePhase");
    if(!phase)throw std::runtime_error("private ping phase type unavailable");
    struct Field{const char* name;const char* type;size_t offset;};
    for(const auto& item:std::initializer_list<Field>{
        {"<serviceCore>k__BackingField","Torappu.UI.AutoChess.Server.IAutoChessServiceCore",16},
        {"m_gameManager","Torappu.Battle.AutoChess.AutoChessGameManager",32}}){
        auto field=profile_field(phase,item.name);
        if(api.il2cpp_field_get_offset(field)!=item.offset||!matches_type(api.il2cpp_field_get_type(field),item.type))throw std::runtime_error("private ping ownership field changed");
    }
    hook(NPN_BATTLE_PING,npn_battle_ping,npn_old_ping);
    hook(NPN_SERVICE_PING,npn_service_ping,npn_old_service_ping);
}
