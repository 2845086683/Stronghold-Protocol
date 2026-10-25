// Include after native_single_entry.inc inside namespace cn. Keep the LOCAL
// prepare model, team transport and the original selection callbacks intact.
#pragma once
inline constexpr CN_Method CN_SINGLE_BAND_RANGE={"Torappu.UI.AutoChess.AutoChessBandChooseViewModel","_LoadDataFromBandIdRange",0x602b5c9,0x27593a0ULL,2,false,"System.Void",{"Torappu.UI.AutoChess.AutoChessPrepareModel","System.Collections.Generic.IEnumerable`1<System.String>"}};
inline constexpr CN_Method CN_SINGLE_BAND_ENABLE={"Torappu.UI.AutoChess.AutoChessUtil","CheckIfBandEnable",0x602b54a,0x27609a0ULL,3,true,"System.Boolean",{"System.String","System.String","Torappu.ActAutoChessModeType"}};
inline constexpr CN_Method CN_SINGLE_TRAINING_TEAM={"Torappu.UI.AutoChess.Server.AutoChessTrainingServiceMode+TrainingTeamData",".ctor",0x602bd5b,0x27f7290ULL,1,false,"System.Void",{"System.String"}};
using NativeSingleBandRange=void(*)(void*,void*,void*,const void*);
using NativeSingleBandEnable=bool(*)(void*,void*,int32_t,const void*);
using NativeSingleTrainingTeam=void(*)(void*,void*,const void*);
NativeSingleBandRange native_single_old_band_range=nullptr;
NativeSingleBandEnable native_single_old_band_enable=nullptr;
NativeSingleTrainingTeam native_single_old_training_team=nullptr;
std::atomic<unsigned> native_single_band_ranges{0},native_single_band_range_failures{0},native_single_band_filter_queries{0};
std::atomic<unsigned> native_single_team_choice_projections{0};
thread_local void* native_single_band_range_activity=nullptr;
struct NativeSingleBandRangeScope{
    void* previous;
    explicit NativeSingleBandRangeScope(void* activity):previous(native_single_band_range_activity){native_single_band_range_activity=activity;}
    ~NativeSingleBandRangeScope(){native_single_band_range_activity=previous;}
};
void native_single_training_team(void* self,void* activity,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()||!private_view.load()||!native_single_mode_captured||!cncombat::mode_allowed(nb_selected_mode)||
       !native_battle_training_depth||!private_data_depth||!native_battle_launch_session_prepared.load()){
        lease.release();native_single_old_training_team(self,activity,info);return;
    }
    // Keep native self identity/status and LOCAL state progression, removing
    // tutorial NPC members and reservations before the first team publication.
    native_single_old_training_team(self,activity,info);
    try{
        if(!cnlaunch::target_activity(text(activity)))return;
        native_battle_launch_require_ui();
        if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.Server.AutoChessTrainingServiceMode+TrainingTeamData"))
            throw std::runtime_error("private strategy team owner class mismatch");
        native_prebattle_team_initialize(self,activity);
        native_single_team_choices_ready=true;++native_single_team_choice_projections;
    }catch(const std::exception& error){native_single_team_choices_ready=false;++native_single_band_range_failures;native_battle_launch_reject(error.what());}
}
bool native_single_band_enable(void* activity,void* band,int32_t mode,const void* info){
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load())return false;
    // BandData.ModeTypeList contains SINGLE for all forty strategies, whereas
    // LOCAL is a tutorial subset. Change only this nested catalog query; its
    // original implementation still rejects absent/disabled catalog entries.
    if(can_read()&&mode==0&&native_single_band_range_activity&&private_data_depth&&
       private_view.load()&&native_single_mode_captured&&cncombat::mode_allowed(nb_selected_mode)){
        try{
            if(cnlaunch::target_activity(text(activity))&&text(activity)==text(native_single_band_range_activity)){
                native_battle_launch_require_ui();++native_single_band_filter_queries;return native_single_old_band_enable(activity,band,1,info);
            }
        }catch(const std::exception& error){++native_single_band_range_failures;native_battle_launch_reject(error.what());return false;}
    }
    lease.release();return native_single_old_band_enable(activity,band,mode,info);
}

void native_single_band_range(void* self,void* prepare,void* range,const void* info){
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load())return;
    if(!can_read()||!private_view.load()||!native_single_mode_captured||
       !cncombat::mode_allowed(nb_selected_mode)||!private_data_depth||
       !native_battle_launch_session_prepared.load()){
        lease.release();native_single_old_band_range(self,prepare,range,info);return;
    }
    try{
        const auto activity=native_battle_prepare_activity(prepare,"<actId>k__BackingField");
        if(!cnlaunch::target_activity(activity)){lease.release();native_single_old_band_range(self,prepare,range,info);return;}
        native_battle_launch_require_ui();
        if(!native_single_team_choices_ready)throw std::runtime_error("private strategy team choices are not initialized");
        if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.AutoChessBandChooseViewModel")||
           api.il2cpp_object_get_class(prepare)!=cls("Torappu.UI.AutoChess.AutoChessPrepareModel"))
            throw std::runtime_error("private band range owner class mismatch");
        const auto prepare_class=api.il2cpp_object_get_class(prepare);
        auto local_field=profile_field(prepare_class,"<isLocalMode>k__BackingField");
        auto mode_field=profile_field(prepare_class,"<modeType>k__BackingField");
        if((api.il2cpp_field_get_flags(local_field)&16)||(api.il2cpp_field_get_flags(mode_field)&16)||
           !matches_type(api.il2cpp_field_get_type(local_field),"System.Boolean")||
           !matches_type(api.il2cpp_field_get_type(mode_field),"Torappu.ActAutoChessModeType"))
            throw std::runtime_error("private band range LOCAL field contract mismatch");
        uint8_t local=0;int32_t mode=-1;
        api.il2cpp_field_get_value(prepare,local_field,&local);api.il2cpp_field_get_value(prepare,mode_field,&mode);
        if(!local||mode!=0){lease.release();native_single_old_band_range(self,prepare,range,info);return;}
        TemporaryRoots roots;roots.retain(self);roots.retain(prepare);roots.retain(range);
        auto id=roots.retain(field_object(prepare,"<actId>k__BackingField"));void* args[]={id};
        auto data=roots.retain(invoke(bound_methods.at(CN_BATTLE_ACT_DATA.token),nullptr,args));
        if(!data||api.il2cpp_object_get_class(data)!=cls("Torappu.ActAutoChessData"))throw std::runtime_error("private band range activity data missing");
        auto field=profile_field(api.il2cpp_object_get_class(data),"bandDataListDict");
        if((api.il2cpp_field_get_flags(field)&16)||!matches_type(api.il2cpp_field_get_type(field),"Torappu.ListDict`2<System.String,Torappu.ActAutoChessData+ActAutoChessBandData>"))
            throw std::runtime_error("private band range activity catalog contract mismatch");
        auto bands=roots.retain(field_object(data,"bandDataListDict"));
        if(!bands)throw std::runtime_error("private band range activity catalog missing");
        auto keys_method=method(api.il2cpp_object_get_class(bands),"get_Keys",0);
        if(!keys_method||!matches_type(api.il2cpp_method_get_return_type(keys_method),"System.Collections.Generic.ICollection`1<System.String>"))
            throw std::runtime_error("private band range Keys contract mismatch");
        auto keys=roots.retain(invoke(keys_method,bands));
        if(!keys)throw std::runtime_error("private band range Keys missing");
        // Native LoadData inlines the four-entry training iterator. Replace its
        // argument at the common downstream loader; do not change team.modeId,
        // PrepareModel.modeType or the view's mode and callback dispatch.
        {NativeSingleBandRangeScope scope(id);native_single_old_band_range(self,prepare,keys,info);}
        ++native_single_band_ranges;
        report("private-native-full-strategy-range-loaded");
    }catch(const std::exception& error){
        ++native_single_band_range_failures;
        native_battle_launch_reject((std::string("private strategy range: ")+error.what()).c_str());
    }
}
void native_single_band_range_setup(){
    hook(CN_SINGLE_BAND_RANGE,native_single_band_range,native_single_old_band_range);
    hook(CN_SINGLE_BAND_ENABLE,native_single_band_enable,native_single_old_band_enable);
    hook(CN_SINGLE_TRAINING_TEAM,native_single_training_team,native_single_old_training_team);
}
