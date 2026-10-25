// Included inside namespace cn by native_battle_launch.inc before setup.
// Intercept before CreateTeam constructs an official matchmaking request.
#pragma once
inline constexpr CN_Method CN_SINGLE_CREATE_TEAM={"Torappu.UI.AutoChess.AutoChessPrepareController","CreateTeam",0x602b3dc,0x271f6e0ULL,3,false,"System.Void",{"System.String","System.Boolean","System.Boolean"}};
inline constexpr CN_Method CN_SINGLE_START_TRAINING={"Torappu.UI.AutoChess.AutoChessPrepareController","StartTrainingMode",0x602b3db,0x27215a0ULL,0,false,"System.Void",{}};
using NativeSingleCreate=void(*)(void*,void*,bool,bool,const void*);
NativeSingleCreate native_single_old_create=nullptr;
NativeBattleVoid native_single_old_training=nullptr;
std::atomic<unsigned> native_single_entries{0};
bool native_single_mode_captured=false; // UI-thread-owned; no background reads.
bool native_single_team_choices_ready=false;

void native_single_create(void* self,void* mode,bool precise,bool match,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()){lease.release();native_single_old_create(self,mode,precise,match,info);return;}
    std::string selected="<unreadable>";
    try{
        const auto activity=native_battle_prepare_activity(self,"m_actId");
        if(!cnlaunch::target_activity(activity)){lease.release();native_single_old_create(self,mode,precise,match,info);return;}
        native_battle_launch_require_ui();selected=text(mode);
        if(native_multiplayer_entry_create(self,mode,precise,match,info))return;
        // SINGLE's native confirmation path also passes its UI matchFlag.
        // Only the exact mode ID determines whether this is a private single
        // entry; neither UI flag permits an official CreateTeam request here.
        if(!cncombat::mode_allowed(selected))throw std::runtime_error("private activity supports independent single-player modes only");
        if(native_battle_launch_in_flight.load()||native_battle_launch_pending.load())throw std::runtime_error("private native scene is already active or pending");
        if(!native_single_old_training)throw std::runtime_error("private native LOCAL entry unavailable");
        TemporaryRoots roots;roots.retain(self);roots.retain(mode);
        // Original StartTrainingMode copies page input and changes that copy to
        // LOCAL before using the original service/team/strategy/loading flow.
        // Actual mode/map come from the durable private session at Util.Start.
        const auto previous_mode=nb_selected_mode;const bool previous_captured=native_single_mode_captured,previous_choices=native_single_team_choices_ready,previous_fresh=nb_fresh_entry_requested;
        nb_selected_mode=selected;native_single_mode_captured=true;native_single_team_choices_ready=false;nb_fresh_entry_requested=true;PrivateDataScope private_scope(true);
        try{native_single_old_training(self,bound_methods.at(CN_SINGLE_START_TRAINING.token));}
        catch(...){nb_selected_mode=previous_mode;native_single_mode_captured=previous_captured;native_single_team_choices_ready=previous_choices;nb_fresh_entry_requested=previous_fresh;throw;}
        ++native_single_entries;report("private-independent-mode-routed-to-native-local-entry");
    }catch(const std::exception& error){native_battle_launch_reject((std::string(error.what())+" selected="+selected+" precise="+(precise?"true":"false")+" match="+(match?"true":"false")).c_str());}
}
void native_single_training(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()){lease.release();native_single_old_training(self,info);return;}
    try{
        const auto activity=native_battle_prepare_activity(self,"m_actId");
        if(!cnlaunch::target_activity(activity)){lease.release();native_single_old_training(self,info);return;}
        native_battle_launch_require_ui();const auto previous_mode=nb_selected_mode;const bool previous_captured=native_single_mode_captured,previous_choices=native_single_team_choices_ready,previous_fresh=nb_fresh_entry_requested;
        nb_selected_mode="mode_single_funny";native_single_mode_captured=true;native_single_team_choices_ready=false;nb_fresh_entry_requested=true;
        PrivateDataScope scope(true);try{native_single_old_training(self,info);}
        catch(...){nb_selected_mode=previous_mode;native_single_mode_captured=previous_captured;native_single_team_choices_ready=previous_choices;nb_fresh_entry_requested=previous_fresh;throw;}
    }catch(const std::exception& error){native_battle_launch_reject(error.what());}
}
void native_single_entry_setup(){
    hook(CN_SINGLE_CREATE_TEAM,native_single_create,native_single_old_create);
    hook(CN_SINGLE_START_TRAINING,native_single_training,native_single_old_training);
}
