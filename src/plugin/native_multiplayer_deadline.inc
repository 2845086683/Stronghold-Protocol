// Native Unity owns combat and task cleanup. HTTP owns the phase deadline.
// Run on every original UI tick, independently of command/network queues.
#pragma once
#include "native_multiplayer_help_outcome_policy.h"
#include "native_multiplayer_help_completion.inc"
std::string nmd_deadline_key;
void* nmd_deadline_manager=nullptr;
std::atomic<unsigned> nmd_deadline_finishes{0};

void native_multiplayer_deadline_tick(){
    if(closing.load()||!native_multiplayer_entry_active()||nmd_receive_depth||native_battle_data_projecting())return;
    try{
        nb_main_thread();const auto& view=nmd_last_view;
        if(view.kind!=cnjson::Value::Object)return;
        const auto reconstruction=view.object.find("selfReconstruction");
        if(reconstruction!=view.object.end()&&reconstruction->second.kind==cnjson::Value::Object)return;
        const auto phase=nb_string(view,"phase");
        if(phase!="battle"&&phase!="help"&&phase!="boss"){nmd_deadline_key.clear();nmd_deadline_manager=nullptr;return;}
        if(nb_string(view,"roomId")!=native_multiplayer_transport_room()||!nmh_scope())return;
        PrivateDataScope private_scope(true);
        auto manager=nb_manager(),centre=nb_centre();TemporaryRoots roots;
        roots.retain(manager);roots.retain(centre);
        if(nb_native_int(centre,"<currentRound>k__BackingField")!=nb_number(view,"round"))return;
        auto game=roots.retain(field_object(manager,"gameStatus"));
        auto status=roots.retain(field_object(centre,"battleStatus"));
        if(!game||!status||!nmh_result<uint8_t>(invoke(bound_methods.at(0x60136eb),game))||
           nmh_result<uint8_t>(invoke(bound_methods.at(0x60138e9),status)))return;
        if(phase=="help"){
            if(cnhelp_outcome::authority(view)!=nb_number(view,"seat")||!cnhelp_outcome::running(view))return;
        }else if(phase=="battle"&&nb_number(nb_at(nb_at(view,"selfState"),"player"),"hp")<=0&&!cndead_battle::valid(view,true))return;
        bool due=false;
        if(phase=="boss"){
            const auto& sync=nb_at(nb_at(nb_at(view,"dtoProjection"),"allStateSync"),"payload");
            const auto& detail=nb_at(nb_at(sync,"allStateSyncData"),"sceneDetail");
            // The server publishes one-HP-per-second updates. Countdown zero
            // alone must not end a boss; actual shared HP exhaustion does.
            // bossRoundDetail.hp is the immutable battle seed. Runtime player
            // HP and native Step action 9 carry the current authoritative HP.
            const auto& players=nb_at(detail,"runtimePlayers");int64_t hp=0;
            if(players.kind!=cnjson::Value::Array||players.array.empty())throw std::runtime_error("private boss deadline runtime roster absent");
            for(const auto& player:players.array)hp+=std::max<int32_t>(0,nb_number(player,"hp"));
            due=hp==0;
        }else if(phase=="help"){
            due=cnhelp_outcome::expired(view,native_multiplayer_transport_time());
        }else{
            const auto& deadline=nb_at(view,"deadlineTs");
            due=deadline.kind==cnjson::Value::Integer&&deadline.integer>0&&
                native_multiplayer_transport_time()>=double(deadline.integer);
        }
        const bool help_complete=phase=="help"&&cnhelp_outcome::journal_complete(view);
        if(!due&&!help_complete)return;
        const auto key=nb_string(view,"roomId")+":"+nb_string(nb_at(view,"selfState"),"runId")+":"+
            std::to_string(nb_number(view,"matchSequence"))+":"+std::to_string(nb_number(view,"round"))+":"+phase;
        if(key==nmd_deadline_key&&manager==nmd_deadline_manager)return;
        nmd_deadline_key=key;nmd_deadline_manager=manager;
        const bool help_perfect=help_complete&&nb_at(nb_at(view,"jointDefense"),"escapes").object.empty();
        int32_t result=help_perfect?1:2;void* args[]={&result};
        // Original OnFinishGame marks battleFinished and requests the original
        // self/help finish. Native task teardown stops waves and combat once.
        invoke(bound_methods.at(CN_BATTLE_MANAGER_FINISH.token),manager,args);
        ++nmd_deadline_finishes;report("private-multiplayer-native-deadline-finished");
    }catch(const std::exception& ex){nmd_failed(ex);}
}
void native_multiplayer_deadline_setup(){
    native_multiplayer_help_completion_setup();
    static const CN_Method methods[]={
        {"Torappu.Battle.AutoChess.AutoChessGameStatus","get_inBattleState",0x60136eb,0xb4b580ULL,0,false,"System.Boolean",{}},
        {"Torappu.Battle.AutoChess.AutoChessBattleStatusDataModel","get_battleFinished",0x60138e9,0xba6fd0ULL,0,false,"System.Boolean",{}},
    };
    for(const auto& spec:methods)bind_method(spec);
    profile_field(cls("Torappu.Battle.AutoChess.AutoChessGameManager"),"gameStatus");
    profile_field(cls("Torappu.Battle.DataCenter.AutoChessDataCenter"),"<currentRound>k__BackingField");
}
