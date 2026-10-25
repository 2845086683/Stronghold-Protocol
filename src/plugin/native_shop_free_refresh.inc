// Private free refresh affordability only. The complete native click handler
// still owns special-shop restrictions, dialog cancellation, request and deselect.
// Include after native_battle_launch.inc (and its prebattle owner helpers).
#pragma once
inline constexpr CN_Method CN_SHOP_REFRESH_CLICK={"Torappu.UI.AutoChess.Battle.AutoChessBattleUIController","_OnShopRefresh",0x602befc,0x2803c30ULL,0,false,"System.Void",{}};
inline constexpr CN_Method CN_SHOP_REFRESH_COST={"Torappu.UI.AutoChess.Battle.AutoChessBattleShopViewModel","get_refreshCost",0x602c22f,0x2835dc0ULL,0,false,"System.Int32",{}};
using NativeShopRefreshClick=void(*)(void*,const void*);
using NativeShopRefreshCost=int32_t(*)(void*,const void*);
NativeShopRefreshClick nsfr_old_click=nullptr;
NativeShopRefreshCost nsfr_old_cost=nullptr;
thread_local void* nsfr_controller=nullptr;
thread_local void* nsfr_view=nullptr;
thread_local void* nsfr_shop=nullptr;
std::atomic<bool> nsfr_setup_ready{false};
std::atomic<unsigned> nsfr_clicks{0},nsfr_zero_cost_reads{0},nsfr_failures{0};

struct NativeShopFreeRefreshScope{
    void* controller;void* view;void* shop;
    NativeShopFreeRefreshScope():controller(nsfr_controller),view(nsfr_view),shop(nsfr_shop){nsfr_controller=nsfr_view=nsfr_shop=nullptr;}
    ~NativeShopFreeRefreshScope(){nsfr_controller=controller;nsfr_view=view;nsfr_shop=shop;}
    NativeShopFreeRefreshScope(const NativeShopFreeRefreshScope&)=delete;
    NativeShopFreeRefreshScope& operator=(const NativeShopFreeRefreshScope&)=delete;
};
bool native_multiplayer_preparation_view_may_write_board();
bool nsfr_multi_scene(){
    if(!can_read()||!private_view.load()||!private_battle_resources.load()||unity_ui_thread.load()!=GetCurrentThreadId()||
       !native_multiplayer_entry_active()||!native_multiplayer_transport_live()||!native_multiplayer_preparation_view_may_write_board())return false;
    const auto raw=native_multiplayer_transport_snapshot();const auto& shown=native_multi_entry_snapshot;
    if(nb_string(raw,"roomId")!=native_multiplayer_transport_room()||nb_string(raw,"roomId")!=nb_string(shown,"roomId")||
       nb_number(raw,"seat")!=nb_number(shown,"seat")||nb_number(raw,"matchSequence")!=nb_number(shown,"matchSequence")||
       nb_number(raw,"round")!=nb_number(shown,"round")||nb_string(raw,"phase")!="preparation"||nb_string(shown,"phase")!="preparation"||
       !cnmp::mode(nb_string(raw,"modeId")))return false;
    const auto& own=nb_at(raw,"selfState");const auto& visible=nb_at(shown,"selfState");
    return nb_string(own,"activityId")=="act2autochess"&&nb_string(own,"runId")==nb_string(visible,"runId")&&
        nb_string(own,"modeId")==nb_string(raw,"modeId")&&nb_string(own,"stageId")==nb_string(visible,"stageId");
}
bool nsfr_scene(){
    if(native_multiplayer_entry_active())return nsfr_multi_scene();
    if(!can_read()||!private_view.load()||!private_battle_resources.load()||!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||
       !native_battle_launch_session_prepared.load()||!native_single_mode_captured||unity_ui_thread.load()!=GetCurrentThreadId()||
       nb_run_id.empty()||nb_state.kind!=cnjson::Value::Object)return false;
    return nb_string(nb_state,"activityId")=="act2autochess"&&nb_string(nb_state,"runId")==nb_run_id&&
        nb_string(nb_state,"modeId")==nb_selected_mode&&cncombat::stage_allowed(nb_string(nb_state,"stageId"),nb_selected_mode)&&
        nb_string(nb_state,"phase")=="preparation";
}
bool nsfr_hud_owner(void* view){
    if(!native_multiplayer_entry_active())return npb_hud_owner(view);
    if(!nsfr_multi_scene()||!native_battle_manager_root||!view||api.il2cpp_object_get_class(view)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModel"))return false;
    auto manager=hold_get(native_battle_manager_root),dc=nb_centre(),bridge=native_multiplayer_dispatch_bridge();
    if(!manager||manager!=nb_manager()||!dc||!bridge||field_object(manager,"dataCenter")!=dc||field_object(manager,"dataBridge")!=bridge)return false;
    const auto mode=nb_string(native_multiplayer_transport_snapshot(),"modeId");
    if(!act(field_object(dc,"<actId>k__BackingField"))||!act(field_object(view,"<actId>k__BackingField"))||
       text(field_object(dc,"<modeId>k__BackingField"))!=mode||text(field_object(view,"<modeId>k__BackingField"))!=mode)return false;
    auto data=field_object(dc,"<actData>k__BackingField"),bonds=field_object(dc,"<bannedBonds>k__BackingField");
    return data&&bonds&&field_object(view,"<autoChessActData>k__BackingField")==data&&field_object(view,"<bannedBonds>k__BackingField")==bonds;
}
void* nsfr_click_view(void* controller,TemporaryRoots& roots){
    if(!controller||api.il2cpp_object_get_class(controller)!=cls(CN_SHOP_REFRESH_CLICK.type)||
       !act(field_object(controller,"m_actId")))return nullptr;
    npb_field(controller,"m_modelProperty","Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModelProperty");
    auto property=roots.retain(field_object(controller,"m_modelProperty"));
    if(!property||api.il2cpp_object_get_class(property)!=cls("Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModelProperty"))return nullptr;
    auto getter=method(api.il2cpp_object_get_class(property),"GetValueNotNull",0);uint32_t impl=0;
    if(!getter||(api.il2cpp_method_get_flags(getter,&impl)&16)||
       !matches_type(api.il2cpp_method_get_return_type(getter),"Torappu.UI.AutoChess.Battle.AutoChessBattleUIViewModel"))
        throw std::runtime_error("private free refresh property getter contract");
    return roots.retain(invoke(getter,property));
}
bool nsfr_owned(void* controller,void* view,void* shop,TemporaryRoots& roots){
    if(!nsfr_scene()||!nsfr_hud_owner(view)||!shop||api.il2cpp_object_get_class(shop)!=cls(CN_SHOP_REFRESH_COST.type)||
       nsfr_click_view(controller,roots)!=view)return false;
    npb_field(view,"<shopModel>k__BackingField",CN_SHOP_REFRESH_COST.type);
    npb_field(view,"<shopData>k__BackingField","Torappu.Battle.AutoChess.AutoChessShopDataModel");
    auto dc=nb_centre();
    if(!dc||api.il2cpp_object_get_class(dc)!=cls("Torappu.Battle.DataCenter.AutoChessDataCenter"))return false;
    npb_field(dc,"shopDataModel","Torappu.Battle.AutoChess.AutoChessShopDataModel");
    auto data=roots.retain(field_object(dc,"shopDataModel"));
    return nsfr_scene()&&nsfr_hud_owner(view)&&data&&api.il2cpp_object_get_class(data)==cls("Torappu.Battle.AutoChess.AutoChessShopDataModel")&&
        field_object(view,"<shopModel>k__BackingField")==shop&&field_object(view,"<shopData>k__BackingField")==data;
}
int32_t nsfr_cost(void* self,const void* info){
    RuntimeLease lease(runtime_gate);
    // Preserve original getter exceptions and foreign/Lua getter semantics.
    auto price=nsfr_old_cost(self,info);
    if(!lease||closing.load()||!nsfr_controller||!nsfr_view||self!=nsfr_shop)return price;
    try{
        TemporaryRoots roots;
        if(!nsfr_owned(nsfr_controller,nsfr_view,self,roots))return price;
        auto data=roots.retain(field_object(nsfr_view,"<shopData>k__BackingField"));
        int32_t native_free=0,view_free=0;
        api.il2cpp_field_get_value(data,npb_field(data,"freeRefreshCnt","System.Int32"),&native_free);
        api.il2cpp_field_get_value(self,npb_field(self,"<freeRefreshCnt>k__BackingField","System.Int32"),&view_free);
        const auto canonical=native_multiplayer_entry_active()?nb_at(native_multiplayer_transport_snapshot(),"selfState"):nb_state;
        auto canonical_free=nb_number(nb_at(nb_at(canonical,"player"),"store"),"freeRefreshCnt");
        if(native_free<=0||view_free<=0||canonical_free<=0)return price;
        // Recheck after managed getters: neither a changed scene nor a nested
        // foreign call can borrow the surrounding private click's decision.
        if(!nsfr_owned(nsfr_controller,nsfr_view,self,roots))return price;
        ++nsfr_zero_cost_reads;return 0;
    }catch(const std::exception& error){++nsfr_failures;nb_error("native-shop-free-refresh",error);return price;}
}
void nsfr_click(void* self,const void* info){
    NativeShopFreeRefreshScope scope;
    RuntimeLease lease(runtime_gate);TemporaryRoots roots;
    if(lease&&!closing.load())try{
        if(nsfr_scene()){
            auto view=nsfr_click_view(self,roots);
            auto shop=roots.retain(field_object(view,"<shopModel>k__BackingField"));
            if(nsfr_owned(self,view,shop,roots)){nsfr_controller=self;nsfr_view=view;nsfr_shop=shop;++nsfr_clicks;}
        }
    }catch(const std::exception& error){++nsfr_failures;nb_error("native-shop-free-refresh-scope",error);}
    // Do not catch the original handler or replace its request/selection flow.
    // Keep the lease through the complete private native handler and its hooks.
    nsfr_old_click(self,info);
}
std::string native_shop_free_refresh_status_json(){
    return "{\"setup\":"+std::string(nsfr_setup_ready.load()?"true":"false")+",\"clicks\":"+std::to_string(nsfr_clicks.load())+
        ",\"zeroCostReads\":"+std::to_string(nsfr_zero_cost_reads.load())+",\"failures\":"+std::to_string(nsfr_failures.load())+"}";
}
void native_shop_free_refresh_setup(){
    hook(CN_SHOP_REFRESH_CLICK,nsfr_click,nsfr_old_click);
    hook(CN_SHOP_REFRESH_COST,nsfr_cost,nsfr_old_cost);nsfr_setup_ready=true;
}
