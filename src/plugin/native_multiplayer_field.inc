// Private Help field ownership and native start barrier. All Unity work stays
// on the observed main thread; the HTTP worker receives immutable JSON only.
#pragma once
void native_multiplayer_clear_preparation_dummies();
bool native_multiplayer_help_carry_ready();
std::string native_multiplayer_help_carry_plan_stamp();
void native_multiplayer_help_carry_begin(void*,const std::string&);
void native_multiplayer_help_carry_task_begin(void*,void*,const std::string&);
static const CN_Method NFF_METHODS[]={
{"Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle+<StartMainBattle>d__0","MoveNext",0x6013d36,0xbfd8a0ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode","get_enablePause",0x6013c5c,0xbdf810ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.BattleController","SetPaused",0x600fc5e,0x631780ULL,2,false,"System.Void",{"System.Boolean","Torappu.Battle.Consts+BattlePauseKey"}},
{"Torappu.Battle.BattleController","get_isPaused",0x600fc37,0x642920ULL,0,false,"System.Boolean",{}},
{"Torappu.Battle.BattleController","get_map",0x600fbe8,0x6431d0ULL,0,false,"Torappu.Battle.Map",{}},
{"Torappu.LevelData+PredefinedData",".ctor",0x6007b42,0x50fcc0ULL,0,false,"System.Void",{}},
{"Torappu.LevelData+PredefinedData+PredefinedCharacter",".ctor",0x6007b49,0x241f930ULL,0,false,"System.Void",{}},
{"Torappu.LevelData",".ctor",0x6007b1a,0x241c3b0ULL,0,false,"System.Void",{}},
};
using NFFBool=uint8_t(*)(void*,const void*);using NFFPause=void(*)(void*,uint8_t,int32_t,const void*);
NFFBool nff_original_move=nullptr,nff_original_enable_pause=nullptr;NFFPause nff_original_pause=nullptr;
thread_local void* nff_move_reentry=nullptr;
thread_local bool nff_pause_override=false;
std::atomic<bool> nff_bindings{false},nff_local_ready{false},nff_ready_sent{false},nff_failed{false},nff_hold{false};
std::atomic<unsigned> nff_loads{0},nff_ready_acks{0},nff_failures{0},nff_wait_frames{0},nff_predefine_rows{0};
std::mutex nff_status_mutex;std::string nff_error,nff_first_error;
cnjson::Value nff_view;cnfield::Plan nff_plan;
mixedhelp::Context nff_context;
cn::native_mixed_help::CnNative* nff_adapter=nullptr;
mixedhelp::concrete::Pipeline* nff_pipeline=nullptr;
std::vector<uint32_t> nff_source_roots;
uint32_t nff_iterator_root=0;
uint32_t nff_visual_data_root=0;uint64_t nff_visual_due=0;
std::atomic<unsigned> nff_visual_checks{0},nff_visual_restored{0};
std::string nff_visual_error;
uint32_t nff_retry_controller_root=0;bool nff_retry_reload=false,nff_retry_started=false;
bool nff_pause_owned=false,nff_gate_seen=false,nff_failure_sent=false,nff_restore_pending=false,nff_restore_started=false,nff_predefines_ready=false;
mixedhelp::RestoreRequest nff_restore;
void* nff_base_level=nullptr;std::string nff_carry_stamp;
void native_multiplayer_world_field_trace(const char*)noexcept;
void* native_multiplayer_world_field_scene_graphic(void*,void*,const std::string&);

void* nff_method(uint32_t token){return bound_methods.at(token);}
void* nff_keep(void* value){if(!value)throw std::runtime_error("private field source is null");auto handle=hold_new(value,false);if(!handle||hold_get(handle)!=value)throw std::runtime_error("private field source root failed");nff_source_roots.push_back(handle);return value;}
void nff_release_sources(){for(auto it=nff_source_roots.rbegin();it!=nff_source_roots.rend();++it)hold_free(*it);nff_source_roots.clear();}
void* nff_map(void* controller){return controller?invoke(nff_method(0x600fbe8),controller):nullptr;}
mixedhelp::Context nff_current(){
    auto value=nff_context;
    // Ordinary observation imports precede the first Help plan. Resolve the
    // live native owner even when no Help context has ever been constructed.
    // An empty/foreign Help identity never grants privateHelp ownership.
    if(nmo_controller()!=value.controller){value.controller=nmo_controller();value.map=nff_map(value.controller);value.privateHelp=false;}
    else if(value.controller&&nff_map(value.controller)!=value.map){value.map=nff_map(value.controller);value.privateHelp=false;}
    return value;
}
void native_multiplayer_field_schedule_gate_check()noexcept{
    // A view-indexer switch can retire gate effects while retaining MapData.
    // Queue only after the original switch succeeds, never on board deltas.
    try{
        nb_main_thread();if(!native_multiplayer_entry_active()||!nff_adapter)return;
        const auto visual=nff_current();
        if(!visual.controller||visual.controller!=nmo_controller()||visual.map!=nff_map(visual.controller))return;
        auto data=field_object(visual.map,"m_data");if(!data)return;
        if(!nff_visual_data_root||hold_get(nff_visual_data_root)!=data){
            auto root=hold_new(data,false);if(!root||hold_get(root)!=data){if(root)hold_free(root);throw std::runtime_error("gate visual data retention failed");}
            if(nff_visual_data_root)hold_free(nff_visual_data_root);nff_visual_data_root=root;
        }
        nff_visual_due=GetTickCount64()+100;
    }catch(const std::exception& error){nff_visual_due=0;std::lock_guard<std::mutex> lock(nff_status_mutex);if(nff_visual_error.empty())nff_visual_error=error.what();}
    catch(...){nff_visual_due=0;}
}
void nff_fail(const std::string& message){nff_failed.store(true);nff_local_ready.store(false);++nff_failures;std::lock_guard<std::mutex> lock(nff_status_mutex);nff_error=message;if(nff_first_error.empty())nff_first_error=message;}
void nff_pause(bool value){
    if(!nff_context.controller||nmo_controller()!=nff_context.controller){nff_pause_owned=false;return;}
    if(value==nff_pause_owned)return;
    uint8_t enabled=value?1:0;int32_t key=7;void* args[]={&enabled,&key};const bool before=nff_pause_override;nff_pause_override=true;
    if(value)nff_pause_owned=true; // A partial managed failure still needs key cleanup.
    try{invoke(nff_method(0x600fc5e),nff_context.controller,args);}catch(...){nff_pause_override=before;throw;}nff_pause_override=before;
    if(value&&!nmh_result<uint8_t>(invoke(nff_method(0x600fc37),nff_context.controller)))throw std::runtime_error("private field native server pause key did not take effect");
    nff_pause_owned=value;
}
uint8_t nff_enable_pause(void* self,const void* info){if(nff_pause_override&&native_multiplayer_entry_active())return 1;return nff_original_enable_pause(self,info);}
void nff_set_pause(void* self,uint8_t value,int32_t key,const void* info){
    if(!value&&key==7&&self==nff_context.controller&&nff_hold.load()&&!nff_pause_override)value=1;
    nff_original_pause(self,value,key,info);
}
mixedhelp::Stage nff_stage(const cnfield::Source& source,int32_t seat){
    const CN_Asset* asset=nullptr;for(const auto& row:CN_ASSETS)if(source.assetKey==row.alias){asset=&row;break;}
    if(!asset||asset->scene||source.assetKey!=asset->asset)throw std::runtime_error("private field graphic has no fixed native resource owner");
    const CN_Bundle* bundle=nullptr;for(const auto& row:CN_BUNDLES)if(std::strcmp(row.name,asset->bundle)==0){bundle=&row;break;}
    if(!bundle||source.graphicHash!=bundle->hash)throw std::runtime_error("private field graphic hash differs from compiled owner");
    TemporaryRoots roots;auto stage=roots.retain(string_new(native_multiplayer_stage_path(source.stageId,CN_PRIVATE_LEVELS).c_str()));void* args[]={stage};
    auto level=nff_keep(invoke(nmo_method(0x60131d7),nullptr,args));auto map=nff_keep(field_object(level,"mapData"));
    auto key=roots.retain(string_new(source.assetKey.c_str()));auto prefab=nff_keep(load_asset(key));
    if(!api.il2cpp_class_is_assignable_from(cls("UnityEngine.GameObject"),api.il2cpp_object_get_class(prefab)))throw std::runtime_error("private field graphic is not a GameObject prefab");
    return {seat,source.stageId,text(field_object(level,"levelId")),source.graphicHash,map,level,prefab};
}
cnfield::Source nff_source_for_stage(const std::string& stage){
    const auto& plan=cnfield::at(cnfield::at(nff_view,"nativeField"),"plan");
    for(const auto& row:cnfield::at(plan,"restoreBySeat").object){auto source=cnfield::source(cnfield::at(row.second,"source"));if(source.stageId==stage)return source;}
    throw std::runtime_error("private field base stage has no authenticated source");
}
mixedhelp::concrete::LoadPlan nff_build(void* controller,void* mapData,void* escapedLevel){
    nb_main_thread();if(!nff_plan.active||nff_plan.aborted||nff_failed.load())throw std::runtime_error("private field cannot load an inactive/failed plan");
    if(nff_retry_reload){
        if(!nff_retry_started||!nff_retry_controller_root||controller==hold_get(nff_retry_controller_root))throw std::runtime_error("private field retry has not replaced the obsolete Unity controller");
        TemporaryRoots roots;auto inout=roots.retain(invoke(method(cls("Torappu.Battle.BattleInOut"),"get_instance",0),nullptr));
        auto input=roots.retain(invoke(nmo_method(0x600f5cc),inout));auto info=roots.retain(native_battle_boxed_field(input,"stageInfo",roots));auto ownLevel=roots.retain(field_object(input,"levelData"));
        if(!info||text(field_object(info,"stageId"))!=nff_plan.restore.stageId||!ownLevel||text(field_object(ownLevel,"levelId"))!=native_multiplayer_stage_path(nff_plan.restore.stageId,CN_PRIVATE_LEVELS))throw std::runtime_error("private field retry did not load the authenticated own-stage input");
        nmo_stage=nff_plan.restore.stageId;nmo_release_reload();hold_free(nff_retry_controller_root);nff_retry_controller_root=0;nff_retry_reload=false;nff_retry_started=false;
    }
    if(!nmo_initial_stage()||nmo_stage.empty())throw std::runtime_error("private field base scene identity unavailable");
    auto map=nff_map(controller);if(!map||controller!=nmo_controller())throw std::runtime_error("private field controller/map ownership differs");
    auto levelId=text(field_object(escapedLevel,"levelId"));auto wanted=nff_plan.route.substr(nff_plan.route.find_last_of('/')+1);
    if(levelId!=wanted&&levelId!=nff_plan.route)throw std::runtime_error("private field native load is not the escaped route template");
    native_multiplayer_clear_preparation_dummies();
    mixedhelp::concrete::LoadPlan plan;plan.context={{nff_plan.room,nff_plan.run,uint64_t(nff_plan.round),uint64_t(nff_plan.generation)},controller,map,true};nff_context=plan.context;
    plan.selfSeat=nff_plan.self;plan.singleOwner=nff_plan.single;plan.routeIdentity=nff_plan.route;plan.planHash=nff_plan.hash;
    plan.self=nff_stage(nff_plan.restore,nff_plan.self);plan.neutral=nff_stage(nff_source_for_stage(nmo_stage),-1);nff_base_level=plan.neutral.levelData;plan.neutral.mapData=mapData;plan.neutral.levelData=escapedLevel;
    plan.left=nff_stage(nff_plan.segments.front().source,nff_plan.segments.front().seat);
    plan.right=nff_plan.single?plan.left:nff_stage(nff_plan.segments.back().source,nff_plan.segments.back().seat);
    if(plan.left.stageId==plan.neutral.stageId&&plan.right.stageId==plan.neutral.stageId&&plan.self.stageId==plan.neutral.stageId)
        plan.sceneGraphic=native_multiplayer_world_field_scene_graphic(controller,map,plan.neutral.stageId);
    plan.areaManager=nff_keep(field_object(nb_manager(),"mapAreaManager"));nff_keep(controller);nff_keep(map);++nff_loads;return plan;
}
void* nff_empty_list(void* rowClass,TemporaryRoots& roots){
    // Reuse the exact closed runtime list class already owned by battleData.
    auto list=roots.retain(object_new(rowClass));invoke(method(rowClass,".ctor",0),list);return list;
}
void* nff_predefines(const mixedhelp::concrete::LoadPlan& plan){
    TemporaryRoots roots;auto dc=roots.retain(nb_centre()),model=roots.retain(field_object(dc,"battleData"));
    if(!model)throw std::runtime_error("private field terrain model unavailable");
    auto original=roots.retain(field_object(model,"levelPredefineData"));if(!original)throw std::runtime_error("private field terrain source absent");
    auto data=nff_keep(object_new(api.il2cpp_object_get_class(original)));invoke(nff_method(0x6007b42),data);copy_native_fields(data,original);
    for(const auto* name:{"tokenCards","characterCards"}){
        for(const auto* stage:{&plan.neutral,&plan.left,&plan.right}){auto defs=field_object(stage==&plan.neutral?nff_base_level:stage->levelData,"predefines");auto cards=defs?field_object(defs,name):nullptr;if(cards&&nff_adapter->array_size(cards))throw std::runtime_error("private field unreviewed predefined cards");}
    }
    for(const auto& field:std::array<std::pair<const char*,const char*>,2>{{{"tokenInsts","m_predefinedTokens"},{"characterInsts","m_predefinedChars"}}}){
        std::vector<void*> selected,originRows;
        for(const auto source:{mixedhelp::Source::Neutral,mixedhelp::Source::Left,mixedhelp::Source::Right}){
            if(plan.singleOwner&&source!=mixedhelp::Source::Neutral)continue;
            const auto& stage=plan.singleOwner?plan.left:source==mixedhelp::Source::Left?plan.left:source==mixedhelp::Source::Right?plan.right:plan.neutral;
            auto level=plan.singleOwner||source!=mixedhelp::Source::Neutral?stage.levelData:nff_base_level;
            auto defs=roots.retain(field_object(level,"predefines"));if(!defs)throw std::runtime_error("private field stage predefines absent");
            auto hard=roots.retain(field_object(level,"hardPredefines"));
            if(hard)for(const char* key:{"tokenInsts","characterInsts","tokenCards","characterCards"}){auto rows=field_object(hard,key);if(rows&&nff_adapter->array_size(rows))throw std::runtime_error("private field unreviewed hard predefines");}
            auto rows=roots.retain(field_object(defs,field.first));if(!rows)throw std::runtime_error("private field stage predefined array absent");
            const auto count=nff_adapter->array_size(rows);if(count>4096)throw std::runtime_error("private field predefined row bound");
            for(size_t i=0;i<count;++i){auto row=roots.retain(nff_adapter->invoke(mixedhelp::Method::ArrayGetOne,rows,{mixedhelp::Arg::i32(int32_t(i))}));auto boxed=roots.retain(nmo_position(row,roots));auto pos=nff_adapter->position(boxed);if(nff_pipeline->layout().at(pos)!=source)continue;
                // Null and repeated aliases already occur in the authored CN
                // token arrays. Keep native identity/alias handling unchanged.
                auto clone=nff_keep(object_new(api.il2cpp_object_get_class(row)));invoke(nff_method(0x6007b49),clone);copy_native_fields(clone,row);selected.push_back(clone);
                auto origin=nff_keep(object_new(api.il2cpp_object_get_class(row)));invoke(nff_method(0x6007b49),origin);copy_native_fields(origin,row);originRows.push_back(origin);++nff_predefine_rows;
            }
        }
        auto array=nff_keep(nff_adapter->array("Torappu.LevelData+PredefinedData+PredefinedCharacter",selected.size()));
        auto originalList=roots.retain(field_object(model,field.second));if(!originalList)throw std::runtime_error("private field native terrain list absent");
        auto list=nff_keep(nff_empty_list(api.il2cpp_object_get_class(originalList),roots));auto origins=nff_keep(nff_empty_list(api.il2cpp_object_get_class(originalList),roots));auto add=method(api.il2cpp_object_get_class(list),"Add",1);
        for(size_t i=0;i<selected.size();++i){nff_adapter->invoke(mixedhelp::Method::ArraySetOne,array,{mixedhelp::Arg::ref(originRows[i]),mixedhelp::Arg::i32(int32_t(i))});void* args[]={selected[i]};invoke(add,list,args);void* origin_args[]={originRows[i]};invoke(add,origins,origin_args);}
        set_object_field(data,profile_field(api.il2cpp_object_get_class(data),field.first),array);
        set_object_field(model,profile_field(api.il2cpp_object_get_class(model),field.second),list);
        const char* origin=std::strcmp(field.second,"m_predefinedTokens")==0?"m_originPredefinedTokens":"m_originPredefinedChars";
        set_object_field(model,profile_field(api.il2cpp_object_get_class(model),origin),origins);
    }
    set_object_field(model,profile_field(api.il2cpp_object_get_class(model),"levelPredefineData"),data);
    auto level=nff_keep(object_new(api.il2cpp_object_get_class(plan.neutral.levelData)));invoke(nff_method(0x6007b1a),level);copy_native_fields(level,plan.neutral.levelData);
    set_object_field(level,profile_field(api.il2cpp_object_get_class(level),"predefines"),data);
    set_object_field(level,profile_field(api.il2cpp_object_get_class(level),"mapData"),nff_pipeline->map_data());
    // Base _StartBattle later retrieves this exact backing field for another
    // Rune.PreprocessLevelData pass. It must see the same private level that
    // LoadGameWithRune and Map.Init receive, while keeping native route data.
    set_object_field(model,profile_field(api.il2cpp_object_get_class(model),"<levelData>k__BackingField"),level);
    nff_predefines_ready=true;return level;
}

void nff_ensure_adapter(){
    if(nff_adapter)return;
    native_mixed_help::Host host;
    host.context=nff_current;
    host.restoreTarget=[](const mixedhelp::Identity& source){
        auto target=source;if(nff_view.kind==cnjson::Value::Object&&cnmp::text(nff_view,"roomId")==source.room){target.round=uint64_t(cnmp::integer(nff_view,"round"));target.generation=uint64_t(cnmp::integer(nff_view,"generation"));}
        return target;
    };
    host.restore=[](const mixedhelp::RestoreRequest& request){nff_restore=request;nff_restore_pending=true;nff_restore_started=false;};
    // Runtime owners intentionally outlive static C++ destruction. They are
    // reset while the runtime is live, never freed after il2cpp_shutdown.
    auto adapter=new native_mixed_help::CnNative(std::move(host));
    try{adapter->bind_native();}catch(...){delete adapter;throw;}
    nff_adapter=adapter;nff_pipeline=new mixedhelp::concrete::Pipeline(*adapter);
    mixedheight::Host height;height.current=nff_current;
    height.containsMap=[](void* map){return nff_pipeline&&nff_pipeline->contains_map(map)&&!nff_failed.load();};
    height.graphicFor=[](void* map,mixedhelp::Position pos){return nff_pipeline->graphic_for(map,pos);};
    height.graphicHeight=[](void* graphic,int32_t type){return nff_adapter->floating(nff_adapter->invoke(mixedhelp::Method::MapGraphicHeight,graphic,{mixedhelp::Arg::i32(type)}));};
    height.fromVector=[](mixedheight::V2)->mixedhelp::Position{throw std::runtime_error("native converter not installed");};
    height.abortManaged=[](){nff_adapter->abort_managed_no_raii();};height.failed=nff_fail;native_mixed_height::install(std::move(height));
    native_mixed_help::HooksHost hooks;
    hooks.target=[](void* self,void*,void* level){
        if(!nff_plan.active||!native_multiplayer_entry_active()||self!=nmo_controller())return false;
        auto id=text(field_object(level,"levelId"));return id.find("escaped_")!=std::string::npos;
    };
    hooks.plan=nff_build;hooks.heightRoutesReady=native_mixed_height::ready;hooks.tryHeightRoute=native_mixed_height::try_route;
    hooks.prepared=[](const mixedhelp::concrete::LoadPlan& plan){native_multiplayer_help_carry_begin(plan.context.controller,nff_carry_stamp);return nff_predefines(plan);};
    hooks.loading=[](bool pending){if(pending)nff_local_ready.store(false);};hooks.failed=nff_fail;
    hooks.lifecycle=native_multiplayer_world_field_trace;
    native_mixed_help::install(*adapter,*nff_pipeline,std::move(hooks));
}
#include "native_multiplayer_world_field.inc"
void native_multiplayer_world_field_trace(const char* phase)noexcept{native_world_field::trace(phase);}
void nff_restore_in_place(){
    if(!nff_restore_pending||!nff_pipeline)return;
    auto controller=nmo_controller(),map=nff_map(controller);
    if(!controller||controller!=nff_context.controller||map!=nff_context.map)throw std::runtime_error("private field own import lost the original scene owner");
    auto identity=nff_restore.target;nff_context={identity,controller,map,false};
    native_multiplayer_world_field_restore(nff_restore.self);
    nff_pipeline->on_own_import(identity,controller,map,nff_restore.self.stageId);
    nff_restore_pending=false;nff_restore_started=false;nff_release_sources();
}
uint8_t nff_move(void* self,const void* info){
    if(nff_move_reentry==self){nff_move_reentry=nullptr;return nff_original_move(self,info);}
    if(!native_multiplayer_entry_active()||!nff_plan.active)return nff_original_move(self,info);
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return 0;
    try{
        nb_main_thread();if(nff_failed.load())return 0;
        if(nff_retry_reload){set_object_field(self,profile_field(api.il2cpp_object_get_class(self),"<>2__current"),nullptr);return 1;}
        if(!nff_iterator_root)nff_iterator_root=hold_new(self,false);
        if(!nff_iterator_root||hold_get(nff_iterator_root)!=self)throw std::runtime_error("private help has another live start iterator");
        int32_t state=0;api.il2cpp_field_get_value(self,profile_field(api.il2cpp_object_get_class(self),"<>1__state"),&state);
        // Original state zero yields before carrying status. Map loading may
        // still be pending then; state one is the exact pre-Apply boundary.
        if(state==1||state==2){if(!nff_pipeline||!nff_pipeline->ready()||!mixedhelp::same_context(nff_current(),nff_context))throw std::runtime_error("private help start before its composite owner committed");}
        if(state==1){auto task=field_object(self,"<>4__this");if(!task||api.il2cpp_object_get_class(task)!=cls("Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle"))throw std::runtime_error("private help iterator task owner differs");native_multiplayer_help_carry_task_begin(task,nff_context.controller,nff_carry_stamp);}
        if(state==2){
            native_multiplayer_mode_runes_require_applied();
            native_multiplayer_help_complete_pending();
            native_multiplayer_help_require_ready();native_multiplayer_observation_require_ready();
            if(!nff_predefines_ready||!native_multiplayer_help_carry_ready())throw std::runtime_error("private help native terrain/carry did not finish before start");
            nff_gate_seen=true;nff_local_ready.store(true);
            if(!nff_plan.authorized){nff_hold.store(true);nff_pause(true);set_object_field(self,profile_field(api.il2cpp_object_get_class(self),"<>2__current"),nullptr);++nff_wait_frames;return 1;}
            nff_hold.store(false);nff_pause(false);
        }
        TemporaryRoots roots;roots.retain(self);void* error=nullptr;nff_move_reentry=self;
        auto result=roots.retain(api.il2cpp_runtime_invoke(nff_method(0x6013d36),self,nullptr,&error));nff_move_reentry=nullptr;
        if(error||!result)throw std::runtime_error("private help original start coroutine raised managed error");return nmh_result<uint8_t>(result);
    }catch(const std::exception& ex){nff_move_reentry=nullptr;nff_fail(ex.what());return 0;}
    catch(...){nff_move_reentry=nullptr;nff_fail("private help start native host failed");return 0;}
}
void native_multiplayer_field_prepare(const cnjson::Value& view){
    nb_main_thread();auto plan=cnfield::parse(view);nff_view=view;
    if(!plan.active){
        nff_plan=plan;nff_hold.store(false);nff_pause(false);
        if(nff_pipeline&&nff_pipeline->ready())nff_pipeline->finish();
        // Fill/OnBattleStatusChanged may synchronously create the next native
        // prepare/start task. An accepted inactive field must already own its
        // restored Map, rather than relying on a later frame's field_tick.
        // A failed import keeps restore_pending and propagates to the handoff;
        // its next attempt retries without presenting a half-restored field.
        if(nff_adapter)nff_adapter->flush_restore();
        if(nff_restore_pending)nff_restore_in_place();
        return;
    }
    if(plan.catalog!=CN_BACKEND_CATALOG_HASH)throw std::runtime_error("private field catalog differs from compiled data");
    if(!cnfield::same_load(plan,nff_plan)){
        if(nff_pipeline&&nff_pipeline->status()!=mixedhelp::concrete::Pipeline::State::Idle){
            // An acknowledged new Help attempt retires its previous native
            // composition inside the same scene. A transport retry cannot
            // dispose a live controller or start another battle_loader.
            nff_hold.store(false);nff_pause(false);
            if(nff_pipeline->ready())nff_pipeline->finish();
            nff_adapter->flush_restore();if(nff_restore_pending)nff_restore_in_place();
            if(nff_pipeline->status()!=mixedhelp::concrete::Pipeline::State::Idle){
                auto state=nff_pipeline->status();if(state==mixedhelp::concrete::Pipeline::State::Preparing||state==mixedhelp::concrete::Pipeline::State::Imported||state==mixedhelp::concrete::Pipeline::State::Attached)throw std::runtime_error("private field retry cannot replace an executing native import");
                ++nff_context.identity.generation;nff_context.privateHelp=false;nff_pipeline->abandon_stale();auto old=nff_pipeline;nff_pipeline=new mixedhelp::concrete::Pipeline(*nff_adapter);native_mixed_help::pipeline=nff_pipeline;delete old;
            }
            nff_retry_reload=nff_retry_started=false;
        }
        nff_release_sources();nff_context={};nff_carry_stamp=native_multiplayer_help_carry_plan_stamp();
        nff_local_ready.store(false);nff_ready_sent.store(false);nff_failed.store(false);nff_failure_sent=false;nff_gate_seen=false;nff_predefines_ready=false;
        if(nff_iterator_root)hold_free(nff_iterator_root);nff_iterator_root=0;
    }
    nff_plan=std::move(plan);nff_ensure_adapter();
    if(nff_plan.aborted){nff_hold.store(true);nff_fail("private field loading aborted by authority");}
    else if(nff_plan.authorized&&nff_gate_seen){nff_hold.store(false);nff_pause(false);}
}
void native_multiplayer_field_apply(const cnjson::Value&){
    nb_main_thread();if(!nff_plan.active)return;
    // Dispatch must present loading before the asynchronous original coroutine
    // can restore units and produce its ready ACK. Report a real failure via
    // the authority queue after presentation instead of blocking that cursor.
    try{native_multiplayer_help_require_no_failure();if(nmo_failed)throw std::runtime_error("private field terrain failed: "+nmo_error);}
    catch(const std::exception& ex){nff_fail(ex.what());}
}
bool native_multiplayer_field_transition_pending(){return nff_restore_pending||nff_retry_reload||(nff_plan.active&&!nff_failed.load()&&(!nff_plan.authorized||!nff_local_ready.load()));}
void native_multiplayer_field_require_prebuilt_ready(){
    nb_main_thread();
    // The original Help parent _StartBattle state1 yields _Prebuilt (which
    // deploys units); state2 creates StartMainBattle. That child alone applies
    // carry and produces localReady. Requiring its ACK before either parent
    // step is a circular wait. This earlier boundary proves the committed map
    // and exact authenticated plan, while the child still owns carry/ACK/start.
    const mixedhelp::Identity expected{nff_plan.room,nff_plan.run,uint64_t(nff_plan.round),uint64_t(nff_plan.generation)};
    if(!native_multiplayer_entry_active()||!nff_plan.active||nff_plan.aborted||nff_failed.load()||nff_restore_pending||nff_retry_reload||
       !nff_pipeline||!nff_pipeline->ready()||!nff_predefines_ready||!nff_context.privateHelp||!(nff_context.identity==expected)||
       !mixedhelp::same_context(nff_current(),nff_context)||nff_pipeline->active_plan().planHash!=nff_plan.hash)
        throw std::runtime_error("private help prebuilt has no committed current map/plan");
}
void native_multiplayer_field_require_ready(){
    if(nff_restore_pending||nff_retry_reload)throw std::runtime_error("private native field is rebuilding its authenticated own scene");
    if(nff_plan.active&&(!nff_plan.authorized||nff_failed.load()||!nff_local_ready.load()))throw std::runtime_error("private help field has not crossed the native loading barrier");
}
void native_multiplayer_field_tick(){
    nb_main_thread();
    if(nff_adapter)nff_adapter->flush_restore();
    if(nff_restore_pending&&!nff_plan.active){
        if(!native_multiplayer_entry_active())return;
        nff_restore_in_place();
    }
    // Check once after the native task/Tile.Start handoff, never scan/rebuild
    // the battlefield on an ordinary poll. A new MapData supersedes the old
    // check; errors stay visual diagnostics and cannot kick a player out.
    if(native_multiplayer_entry_active()&&nff_adapter&&
       !nmd_receive_depth&&!nff_restore_pending&&!nff_failed.load()&&(!nff_plan.active||nff_local_ready.load())){
        try{
            const auto visual=nff_current();
            if(visual.controller&&visual.controller==nmo_controller()&&visual.map==nff_map(visual.controller)){
            auto data=field_object(visual.map,"m_data");
            if(data&&(!nff_visual_data_root||hold_get(nff_visual_data_root)!=data)){
                native_multiplayer_field_schedule_gate_check();
            }else if(nff_visual_due&&GetTickCount64()>=nff_visual_due){
                nff_visual_due=0;
                nff_visual_restored+=nff_adapter->restore_missing_gate_effects(visual.map,data);++nff_visual_checks;
                native_multiplayer_world_field_trace("after-native-gate-handoff-check");
            }
            }
        }catch(const std::exception& error){nff_visual_due=0;std::lock_guard<std::mutex> lock(nff_status_mutex);if(nff_visual_error.empty())nff_visual_error=error.what();}
    }
    if(!nff_plan.active||!native_multiplayer_entry_active())return;
    bool presented=false;
    {std::lock_guard<std::mutex> lock(nmp_mutex);presented=nmp_state.room==nff_plan.room&&nmp_state.visible_generation==nff_plan.envelopeGeneration;}
    if((!presented||cnmp::integer(nmd_last_view,"generation")!=nff_plan.envelopeGeneration)&&!native_multiplayer_preparation_view_drive_field_lifecycle())return;
    if(nff_retry_reload&&!nff_failed.load()){
        try{throw std::runtime_error("private field legacy scene retry is not allowed inside a live battle");}
        catch(const std::exception& ex){nff_fail(ex.what());}
    }
    if(!native_multiplayer_preparation_view_report_field())return;
    auto payload=nb_object();payload.object["planHash"]=json_text(nff_plan.hash);payload.object["loadAttempt"]=nb_integer(nff_plan.attempt);
    if(nff_failed.load()&&!nff_failure_sent){payload.object["reason"]=json_text("runtime");if(native_multiplayer_enqueue("field.failed",payload))nff_failure_sent=true;}
    else if(nff_local_ready.load()&&!nff_ready_sent.load()&&!nff_plan.aborted){
        if(native_multiplayer_enqueue("field.ready",payload)){nff_ready_sent.store(true);++nff_ready_acks;}
    }
}
void native_multiplayer_field_reset(){
    nb_main_thread();nff_hold.store(false);nff_pause(false);
    if(nff_visual_data_root)hold_free(nff_visual_data_root);nff_visual_data_root=0;nff_visual_due=0;
    if(nff_pipeline&&nff_pipeline->status()!=mixedhelp::concrete::Pipeline::State::Idle){++nff_context.identity.generation;nff_context.privateHelp=false;nff_pipeline->abandon_stale();auto old=nff_pipeline;nff_pipeline=new mixedhelp::concrete::Pipeline(*nff_adapter);native_mixed_help::pipeline=nff_pipeline;delete old;}
    if(nff_iterator_root)hold_free(nff_iterator_root);nff_iterator_root=0;nff_release_sources();
    if(nff_retry_controller_root)hold_free(nff_retry_controller_root);nff_retry_controller_root=0;nff_retry_reload=false;nff_retry_started=false;
    nff_plan={};nff_view={};nff_context={};nff_restore_pending=false;nff_restore_started=false;nff_pause_owned=false;nff_base_level=nullptr;nff_carry_stamp.clear();nff_local_ready.store(false);nff_ready_sent.store(false);nff_failed.store(false);nff_failure_sent=false;nff_gate_seen=false;nff_predefines_ready=false;
}
void native_multiplayer_field_setup(){
    for(const auto& spec:NFF_METHODS){auto selected=bind_method(spec);void* pointer=nullptr;if(!read(selected,pointer)||reinterpret_cast<uintptr_t>(pointer)-reinterpret_cast<uintptr_t>(runtime)!=spec.rva)throw std::runtime_error("private field exact lifecycle RVA changed");}
    for(const auto& field:std::array<std::pair<const char*,const char*>,3>{{{"<>1__state","System.Int32"},{"<>2__current","System.Object"},{"<>4__this","Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle"}}}){
        auto type=cls("Torappu.Battle.GameMode.GameModeFactory+AutoChessGameMode+MainGameInHelpBattle+<StartMainBattle>d__0");if(!matches_type(api.il2cpp_field_get_type(profile_field(type,field.first)),field.second))throw std::runtime_error("private field original coroutine state fields changed");
    }
    std::vector<native_mixed_help::HookInstall> created;
    try{
        native_mixed_help::create_exact(nff_method(0x6013d36),nff_move,nff_original_move,created);
        native_mixed_help::create_exact(nff_method(0x6013c5c),nff_enable_pause,nff_original_enable_pause,created);
        native_mixed_help::create_exact(nff_method(0x600fc5e),nff_set_pause,nff_original_pause,created);
    }catch(...){for(auto it=created.rbegin();it!=created.rend();++it)MH_RemoveHook(it->target);throw;}
    nff_bindings.store(true);
}
std::string native_multiplayer_field_status_json(){
    std::lock_guard<std::mutex> lock(nff_status_mutex);return "{\"bindingsReady\":"+std::string(nff_bindings.load()?"true":"false")+",\"localReady\":"+(nff_local_ready.load()?"true":"false")+",\"readyQueued\":"+(nff_ready_sent.load()?"true":"false")+",\"failed\":"+(nff_failed.load()?"true":"false")+",\"loads\":"+std::to_string(nff_loads.load())+",\"waitFrames\":"+std::to_string(nff_wait_frames.load())+",\"predefineRows\":"+std::to_string(nff_predefine_rows.load())+",\"gateVisualChecks\":"+std::to_string(nff_visual_checks.load())+",\"gateVisualRestored\":"+std::to_string(nff_visual_restored.load())+",\"gateVisualError\":"+quote(nff_visual_error)+",\"firstError\":"+quote(nff_first_error)+",\"lastError\":"+quote(nff_error)+",\"worldLifecycle\":"+native_world_field::trace_json()+",\"nativeRuntimeAccepted\":false}";
}
