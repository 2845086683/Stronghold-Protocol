// Private MULTI COMMON difficulty only. Original player/terrain Rune executors
// remain authoritative; incoming failed-player personal effects are not added.
#pragma once
void native_multiplayer_field_require_ready();
void native_multiplayer_field_require_prebuilt_ready();
static const CN_Method NMMR_METHODS[]={
{"Torappu.Battle.AutoChess.AutoChessBattleDataModel","_LoadBattleRunes",0x60138c0,0xba37e0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleDataModel","_GetPackedRuneData",0x60138ca,0xba2b60ULL,1,false,"Torappu.RuneTable+PackedRuneData",{"System.String"}},
{"Torappu.Battle.Runes.AdvancedRuneHolder","AddPackedRuneData",0x6014c0e,0xcd9180ULL,1,false,"System.Void",{"Torappu.RuneTable+PackedRuneData"}},
{"Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInBattle+<_StartBattle>d__7","MoveNext",0x6013d15,0xbfe5a0ULL,0,false,"System.Boolean",{}},
};
cnmulti_mode::Plan nmmr_plan,nmmr_applied_plan;
uint32_t nmmr_controller=0,nmmr_manager=0,nmmr_model=0;
bool nmmr_applied=false,nmmr_failed=false;std::string nmmr_error;
thread_local void* nmmr_load_reentry=nullptr;
thread_local void* nmmr_move_reentry=nullptr;
thread_local unsigned nmmr_original_calls=0;
using NMMRMove=uint8_t(*)(void*,const void*);NMMRMove nmmr_original_move=nullptr;
std::atomic<unsigned> nmmr_rebuilds{0},nmmr_added{0},nmmr_deduplicated{0},nmmr_failures{0},nmmr_starts{0};

void nmmr_release(){for(auto* root:{&nmmr_controller,&nmmr_manager,&nmmr_model}){if(*root)hold_free(*root);*root=0;}nmmr_applied=false;nmmr_applied_plan={};}
void native_multiplayer_mode_runes_reset(){nb_main_thread();native_multiplayer_own_start_reset();nmmr_release();nmmr_plan={};nmmr_failed=false;nmmr_error.clear();}
void nmmr_fail(const std::exception& ex){nmmr_applied=false;nmmr_failed=true;nmmr_error=ex.what();++nmmr_failures;try{nb_error("multiplayer-common-mode-runes",ex);}catch(...){} }
void native_multiplayer_mode_runes_prepare(const cnjson::Value& view){
    nb_main_thread();try{auto next=cnmulti_mode::parse(view);native_multiplayer_own_start_observe(view);
        if(!cnmulti_mode::same(next,nmmr_plan)){nmmr_release();nmmr_failed=false;nmmr_error.clear();}nmmr_plan=std::move(next);
    }catch(const std::exception& ex){nmmr_release();nmmr_plan={};nmmr_fail(ex);throw;}
}
bool nmmr_scope(void* model){
    if(!native_multiplayer_entry_active()||!nmh_scope())return false;
    auto centre=nb_centre();return model&&field_object(centre,"battleData")==model&&api.il2cpp_object_get_class(model)==cls("Torappu.Battle.AutoChess.AutoChessBattleDataModel");
}
uint8_t nmmr_all_mask(){uint8_t mask=0;auto type=cls("Torappu.PlayerSideMask");uint32_t alignment=0;if(api.il2cpp_class_value_size(type,&alignment)!=1)throw std::runtime_error("CN PlayerSideMask ABI changed");api.il2cpp_field_static_get_value(profile_field(type,"ALL"),&mask);if(mask!=0)throw std::runtime_error("CN PlayerSideMask.ALL changed");return mask;}
void nmmr_check_pack(void* packed,const std::string& effect,bool set_all,TemporaryRoots& roots){
    if(!packed||api.il2cpp_object_get_class(packed)!=cls("Torappu.RuneTable+PackedRuneData")||text(field_object(packed,"id"))!=effect)throw std::runtime_error("multiplayer COMMON fresh packed identity mismatch");
    auto runes=roots.retain(field_object(packed,"runes"));if(!runes||nmo_count(runes)!=1)throw std::runtime_error("multiplayer COMMON expected exactly one registered Rune");
    auto rune=roots.retain(nmo_item(runes,0));if(!rune||api.il2cpp_object_get_class(rune)!=cls("Torappu.RuneData")||text(field_object(rune,"key"))!=(effect=="aceffect_enemy_1"?"env_gbuff_new":"enemy_attribute_mul"))throw std::runtime_error("multiplayer COMMON Rune key/class mismatch");
    auto selector=roots.retain(field_object(rune,"selector"));if(!selector||api.il2cpp_object_get_class(selector)!=cls("Torappu.RuneData+Selector"))throw std::runtime_error("multiplayer COMMON selector missing");
    auto field=profile_field(api.il2cpp_object_get_class(selector),"playerSideMask");if(!matches_type(api.il2cpp_field_get_type(field),"Torappu.PlayerSideMask"))throw std::runtime_error("multiplayer COMMON player side mask type mismatch");
    auto mask=nmmr_all_mask();if(set_all)set_field(selector,field,&mask);else{uint8_t actual=0;api.il2cpp_field_get_value(selector,field,&actual);if(actual!=mask)throw std::runtime_error("existing COMMON Rune is not global; refusing ambiguous duplicate");}
}
#include "native_multiplayer_zero_owner_runes.inc"
void native_multiplayer_mode_runes_require_applied(){
    nb_main_thread();if(!native_multiplayer_entry_active())return;
    if(!nmmr_plan.active||nmmr_failed||!nmmr_applied||!cnmulti_mode::same(nmmr_plan,nmmr_applied_plan)||
       !nmmr_scope(hold_get(nmmr_model))||hold_get(nmmr_controller)!=nmo_controller()||hold_get(nmmr_manager)!=nb_manager()||
       native_multiplayer_transport_room()!=nmmr_plan.room)throw std::runtime_error("multiplayer COMMON mode Runes not applied to current Unity world: "+nmmr_error);
}
bool native_multiplayer_mode_runes_load(void* self,const void* method_info){
    // Raw native reentry contains no C++ owner/RAII. Any managed exception is
    // caught by the outer runtime_invoke before C++ scopes are unwound.
    if(nmmr_load_reentry==self){nmmr_load_reentry=nullptr;++nmmr_original_calls;nbmr_original_load(self,method_info);return true;}
    if(!native_multiplayer_entry_active())return false;
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return true;
    try{
        if(!nmmr_scope(self))return false;nb_main_thread();nmmr_release();
        TemporaryRoots roots;roots.retain(self);void* error=nullptr;nmmr_original_calls=0;nmmr_load_reentry=self;
        api.il2cpp_runtime_invoke(nb_method(0x60138c0),self,nullptr,&error);nmmr_load_reentry=nullptr;
        if(error||nmmr_original_calls!=1)throw std::runtime_error("multiplayer COMMON original Rune rebuild failed or was not called once");
        if(!nmmr_plan.active||native_multiplayer_transport_room()!=nmmr_plan.room)throw std::runtime_error("multiplayer COMMON has no current authenticated room plan");
        auto controller=roots.retain(nmo_controller()),manager=roots.retain(nb_manager());if(!controller||!manager)throw std::runtime_error("multiplayer COMMON Unity owners missing");
        auto holder=roots.retain(field_object(self,"runeHolder"));if(!holder||api.il2cpp_object_get_class(holder)!=cls("Torappu.Battle.Runes.AdvancedRuneHolder"))throw std::runtime_error("multiplayer COMMON Rune holder missing");
        PrivateDataScope private_scope(true);NB_ProjectionScope projection;
        nmmr_patch_zero_owner_runes(self,roots);
        auto existing=roots.retain(field_object(holder,"m_runeDataPackList"));if(!existing)throw std::runtime_error("multiplayer COMMON native pack list missing");
        const auto wanted=cnmulti_mode::counts(nmmr_plan.effects);std::map<std::string,size_t> present;
        for(int32_t i=0,count=nmo_count(existing);i<count;++i){auto pack=roots.retain(nmo_item(existing,i));auto id=text(field_object(pack,"id"));if(!cnmulti_mode::allowed(id))continue;nmmr_check_pack(pack,id,false,roots);++present[id];}
        for(const auto& item:present){auto found=wanted.find(item.first);if(found==wanted.end()||item.second>found->second)throw std::runtime_error("original COMMON Rune count exceeds authenticated policy");nmmr_deduplicated+=unsigned(item.second);}
        std::set<void*> fresh;
        for(const auto& effect:nmmr_plan.effects){if(present[effect]){--present[effect];continue;}
            auto key=roots.retain(string_new(effect.c_str()));void* args[]={key};auto packed=roots.retain(invoke(nb_method(0x60138ca),self,args));
            if(!fresh.insert(packed).second)throw std::runtime_error("multiplayer COMMON reused mutable packed data");nmmr_check_pack(packed,effect,true,roots);
            void* add[]={packed};invoke(nb_method(0x6014c0e),holder,add);++nmmr_added;
        }
        nmmr_controller=hold_new(controller,false);nmmr_manager=hold_new(manager,false);nmmr_model=hold_new(self,false);
        if(!nmmr_controller||!nmmr_manager||!nmmr_model)throw std::runtime_error("multiplayer COMMON strong root unavailable");
        nmmr_applied_plan=nmmr_plan;nmmr_failed=false;nmmr_error.clear();nmmr_applied=true;++nmmr_rebuilds;
    }catch(const std::exception& ex){nmmr_load_reentry=nullptr;nmmr_release();nmmr_fail(ex);}catch(...){nmmr_load_reentry=nullptr;nmmr_release();std::runtime_error ex("multiplayer COMMON unknown native rebuild failure");nmmr_fail(ex);}return true;
}
uint8_t nmmr_move(void* self,const void* info){
    if(nmmr_move_reentry==self){nmmr_move_reentry=nullptr;return nmmr_original_move(self,info);}
    if(!native_multiplayer_entry_active()){
        RuntimeLease retiredLease(runtime_gate);
        if(retiredLease&&!closing.load()&&native_multiplayer_own_start_cancel_retired(self))return 0;
        retiredLease.release();return nmmr_original_move(self,info);
    }
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return 0;
    try{
        if(native_multiplayer_own_start_cancel_retired(self))return 0;
        if(!nmh_scope()){if(native_multiplayer_own_start_cancel_unscoped(self))return 0;lease.release();return nmmr_original_move(self,info);}
        if(!self||api.il2cpp_object_get_class(self)!=cls("Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInBattle+<_StartBattle>d__7"))throw std::runtime_error("multiplayer COMMON start iterator class differs");
        // Native state0 calls Manager/Trigger.StartBattle then yields null.
        // State1 yields _Prebuilt; state2 preprocesses enemies (bfe6fc).
        // Own proof gates state0. The Help child can apply carry and ACK only
        // after this parent yields _Prebuilt and creates StartMainBattle.
        auto field=profile_field(api.il2cpp_object_get_class(self),"<>1__state");if(!matches_type(api.il2cpp_field_get_type(field),"System.Int32"))throw std::runtime_error("multiplayer COMMON start state ABI differs");
        int32_t state=0;api.il2cpp_field_get_value(self,field,&state);if(native_multiplayer_own_start_before(self,state))return 1;if(state>=1&&state<=3){
            native_multiplayer_mode_runes_require_applied();
            auto task=field_object(self,"<>4__this");
            const bool help=task&&api.il2cpp_object_get_class(task)==cls("Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle");
            if(help&&state<=2)native_multiplayer_field_require_prebuilt_ready();else native_multiplayer_field_require_ready();
        }
        TemporaryRoots roots;roots.retain(self);void* error=nullptr;nmmr_move_reentry=self;
        auto result=roots.retain(api.il2cpp_runtime_invoke(nb_method(0x6013d15),self,nullptr,&error));nmmr_move_reentry=nullptr;
        if(error||!result)throw std::runtime_error("multiplayer COMMON original battle start coroutine failed");++nmmr_starts;auto value=nmh_result<uint8_t>(result);native_multiplayer_own_start_after(value);return value;
    }catch(const std::exception& ex){nmmr_move_reentry=nullptr;native_multiplayer_own_start_failure(ex.what());native_multiplayer_own_start_abort();nmmr_fail(ex);}catch(...){nmmr_move_reentry=nullptr;std::runtime_error ex("multiplayer COMMON unknown native start failure");native_multiplayer_own_start_failure(ex.what());native_multiplayer_own_start_abort();nmmr_fail(ex);}return 0;
}
void native_multiplayer_mode_runes_setup(){
    for(const auto& spec:NMMR_METHODS)if(spec.token==0x6013d15)hook(spec,nmmr_move,nmmr_original_move);else bind_method(spec);
    nmmr_all_mask();
}
