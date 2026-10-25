// The original CN multiplayer service uses positive player indices. Private
// rooms also have seat 0. Keep native observation/actions and fix only the
// three original UI tests that mistake an authenticated observed 0 for unset.
#pragma once
// UISwitchTween.set_isShow's token belongs to Torappu.Common.dll. Its RVA
// is resolved by the original ItemView.Render call, not a token from CSharp.
static const CN_Method CN_MULTIPLAYER_OB_UI_UPDATE={"Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusGroupModel","Update",0x602c152,0x2826700ULL,3,false,"System.Void",{"Torappu.AutoChessData","Torappu.Battle.DataCenter.AutoChessDataCenter","Torappu.Battle.AutoChess.AutoChessGameStatus"}};
static const CN_Method CN_MULTIPLAYER_OB_UI_CANCEL={"Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusGroupModel","CheckIfCancelObValid",0x602c14e,0x28261a0ULL,0,false,"System.Boolean",{}};
static const CN_Method CN_MULTIPLAYER_OB_UI_SLOT={"Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusItemView","Render",0x602c175,0x2828d20ULL,2,false,"System.Void",{"Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusModel","Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusGroupModel"}};
static const CN_Method CN_MULTIPLAYER_OB_UI_TWEEN={"Torappu.UI.UISwitchTween","set_isShow",0x6000a14,0x6761720ULL,1,false,"System.Void",{"System.Boolean"}};
using NMOUIUpdate=void(*)(void*,void*,void*,void*,const void*);
using NMOUICancel=bool(*)(void*,const void*);
using NMOUISlot=void(*)(void*,void*,void*,const void*);
NMOUIUpdate nmoui_old_update=nullptr;
NMOUICancel nmoui_old_cancel=nullptr;
NMOUISlot nmoui_old_slot=nullptr;
void* nmoui_tween_setter=nullptr;

bool nmoui_zero_view(void* centre,void* status){
    if(!native_multiplayer_entry_active()||!nmh_scope()||!nmo_ready||!centre||centre!=nb_centre()||
       !status||status!=field_object(nb_manager(),"gameStatus"))return false;
    const auto& view=nmo_snapshot;
    if(nb_string(view,"activityId")!="act2autochess"||nb_string(view,"phase")!="preparation"||
       nb_string(view,"modeId").rfind("mode_multi_",0)!=0||nb_true(view,"observerOnly")||nmdo_departed(view))return false;
    const auto seat=nb_number(view,"seat");
    if(seat<1||seat>3||nmo_self!=seat||nmo_observed!=0||nmh_int(centre,"<selfPlayerIndex>k__BackingField")!=seat||
       nb_number(nb_at(nb_at(view,"selfState"),"player"),"hp")<=0)return false;
    // Bind to both the presented native target and the still-current private
    // room/match/run. A retained UI object cannot borrow another scene's view.
    const auto& current=native_multi_entry_snapshot;
    if(nb_string(view,"roomId").empty()||nb_string(current,"roomId")!=nb_string(view,"roomId")||
       nb_number(current,"matchSequence")!=nb_number(view,"matchSequence")||nb_number(current,"seat")!=seat||
       nb_string(nb_at(current,"selfState"),"runId")!=nb_string(nb_at(view,"selfState"),"runId")||
       nb_string(current,"phase")!="preparation"||nmdo_departed(current))return false;
    const auto& projection=nb_at(view,"dtoProjection");const auto& policy=nb_at(projection,"view");
    if(nb_number(policy,"recipientSeat")!=seat||nb_number(policy,"writeSeat")!=seat||
       nb_number(policy,"observedSeat")!=0||nb_true(policy,"observationGrantsWriteAccess"))return false;
    const auto& all=nb_at(nb_at(nb_at(projection,"allStateSync"),"payload"),"allStateSyncData");
    if(nb_number(all,"uidIndex")!=seat||nb_number(nb_at(nb_at(all,"sceneDetail"),"preparationStatus"),"uidIndex")!=0)return false;
    bool target=false,recipient=false;
    for(const auto& row:nb_at(view,"roster").array){
        const auto index=nb_number(row,"seat");
        if((index==0||index==seat)&&!nb_true(row,"left")&&!nmdo_flag(row,"quit")&&nb_number(row,"hp")>0){
            if(index==0)target=true;else recipient=true;
        }
    }
    return target&&recipient&&nmh_result<uint8_t>(invoke(nmo_method(0x60136f3),status))!=0&&
        nmh_result<int32_t>(invoke(nmo_method(0x601343f),centre))==0&&
        nmh_result<uint8_t>(invoke(nmo_method(0x601343c),centre))!=0;
}
bool nmoui_group(void* group,void* centre,void* status){
    return group&&api.il2cpp_object_get_class(group)==cls("Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusGroupModel")&&
        nmoui_zero_view(centre,status)&&nmh_int(group,"<selfPlayerIdx>k__BackingField")==nmo_self&&
        nmh_int(group,"m_playerViewIdx")==0;
}
void nmoui_update(void* self,void* data,void* centre,void* status,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    // Keep the complete original model update, including all ordinary states.
    nmoui_old_update(self,data,centre,status,info);
    try{if(nmoui_group(self,centre,status)){
        uint8_t yes=1;set_field(self,profile_field(api.il2cpp_object_get_class(self),"<isObPanelVisible>k__BackingField"),&yes);
    }}catch(const std::exception& ex){nb_error("multiplayer-observation-ui",ex);}
}
bool nmoui_cancel(void* self,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;
    const bool original=nmoui_old_cancel(self,info);if(original)return true;
    try{if(nmoui_group(self,nb_centre(),field_object(nb_manager(),"gameStatus")))return true;}
    catch(const std::exception& ex){nb_error("multiplayer-observation-ui",ex);}
    return original;
}
void nmoui_slot(void* self,void* item,void* group,const void* info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    nmoui_old_slot(self,item,group,info);
    try{
        if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusItemView")||
           !item||api.il2cpp_object_get_class(item)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBattlePlayerStatusModel")||
           field_object(self,"m_itemModel")!=item||nmh_int(item,"<index>k__BackingField")!=nmo_self||
           !nmoui_group(group,nb_centre(),field_object(nb_manager(),"gameStatus")))return;
        // Render inlines CheckIfCancelObValid, so its >0 cannot be fixed by the
        // request-side hook. Use exactly its original tween/property setter.
        auto tween=field_object(self,"m_cancelObTween");if(!tween)throw std::runtime_error("native cancel-observation tween missing");
        uint8_t yes=1;void* args[]={&yes};invoke(nmoui_tween_setter,tween,args);
    }catch(const std::exception& ex){nb_error("multiplayer-observation-ui",ex);}
}
void native_multiplayer_observation_ui_setup(){
    hook(CN_MULTIPLAYER_OB_UI_UPDATE,nmoui_update,nmoui_old_update);
    hook(CN_MULTIPLAYER_OB_UI_CANCEL,nmoui_cancel,nmoui_old_cancel);
    hook(CN_MULTIPLAYER_OB_UI_SLOT,nmoui_slot,nmoui_old_slot);
    nmoui_tween_setter=bind_method(CN_MULTIPLAYER_OB_UI_TWEEN);
}
