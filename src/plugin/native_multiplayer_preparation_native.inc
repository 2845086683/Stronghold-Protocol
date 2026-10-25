// Research integration include inside namespace cn, after the unchanged 0.1.58
// observation/dispatch/field helpers. This does not install into a game process.
namespace preparation_view_native {
using preparation_view::Plan;using preparation_view::World;using preparation_view::Proof;
static const CN_Method NPV_METHODS[]={
{"Torappu.Battle.DataCenter.AutoChessDataCenter","OnPrepareGameLoaded",0x6013486,0xb10cf0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessGameStatus","get_currentState",0x60136f5,0xb4b3b0ULL,0,false,"Torappu.AutoChessGameStateType",{}},
{"Torappu.Battle.AutoChess.AutoChessGameStatus","get_currentSubState",0x60136f7,0xb4b420ULL,0,false,"Torappu.Battle.AutoChess.AutoChessGameStatus+SubState",{}},
{"Torappu.Battle.AutoChess.AutoChessGameManager","PrepareForPrepareState",0x60136b4,0xb41d30ULL,0,false,"System.Void",{}},
{"Torappu.Battle.BattleController","get_map",0x600fbe8,0x6431d0ULL,0,false,"Torappu.Battle.Map",{}},
{"Torappu.Battle.AutoChess.AutoChessBattleDataModel","ApplyGlobalStateSpecialEffects",0x60138ba,0xb9f160ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessEnemyPreviewManager","Load",0x6013814,0xb602f0ULL,0,false,"System.Void",{}},
{"Torappu.Battle.AutoChess.AutoChessEnemyPreviewManager","Unload",0x6013815,0xb60420ULL,0,false,"System.Void",{}},
};
using Callback=void(*)(void*,const void*);Callback originalPrepared=nullptr,originalTerrain=nullptr,originalPrepare=nullptr;thread_local void* preparedPass=nullptr;thread_local void* terrainPass=nullptr;thread_local void* preparePass=nullptr;
struct Roots {
 std::vector<uint32_t> handles;
 explicit Roots(const World& w){try{for(auto p:{w.controller,w.map,w.level,w.centre,w.manager,w.bridge})if(p){auto h=hold_new(p,false);if(!h||hold_get(h)!=p){if(h)hold_free(h);throw std::runtime_error("native view world root failed");}try{handles.push_back(h);}catch(...){hold_free(h);throw;}}}catch(...){for(auto it=handles.rbegin();it!=handles.rend();++it)hold_free(*it);throw;}}
 ~Roots(){for(auto it=handles.rbegin();it!=handles.rend();++it)hold_free(*it);}
};
class Native;
Native* owner=nullptr;
void on_prepared(void*,const void*);
void on_terrain(void*,const void*);
void on_prepare(void*,const void*);
class Native {
 const Plan* scopedPlan=nullptr;World scopedWorld;Proof completed;Plan preparedPlan,contextPlan,naturalFailedPlan;World naturalFailedWorld;std::shared_ptr<Roots> preparedWorldRoots;bool callbackSeen=false,terrainSeen=false;std::string error;
 std::atomic<unsigned> fullRebuilds{0},incrementalUpdates{0},naturalPrepares{0},specialSelections{0};
 std::string terrainFailureHash;int32_t terrainFailureAttempt=-1;uint64_t terrainFailureFieldEpoch=0;World terrainFailureWorld;std::shared_ptr<Roots> terrainFailureRoots;
 Plan loaderPlan;World loaderBefore;std::shared_ptr<Roots> loaderWorldRoots;std::vector<uint32_t> loaderHandles;bool loaderActive=false,loaderTimedOut=false;
 void free_loader_handles(){for(auto i=loaderHandles.rbegin();i!=loaderHandles.rend();++i)hold_free(*i);loaderHandles.clear();loaderWorldRoots.reset();}
 void* selected(uint32_t token){return bound_methods.at(token);}
 static const cnjson::Value& sync(const Plan& p){return cnfield::at(cnfield::at(cnfield::at(p.projection,"allStateSync"),"payload"),"allStateSyncData");}
 void set_view(const Plan& p){
  auto centre=nb_centre();int32_t wanted=p.target;void* args[]={&wanted};
  const bool changed=nmh_result<int32_t>(invoke(nmo_method(0x601343f),centre))!=wanted;
  if(changed)native_multiplayer_clear_preparation_dummies();
  if(!nmh_result<uint8_t>(invoke(nmo_method(0x6013476),centre,args)))throw std::runtime_error("native view selection declined");
  auto area=field_object(nb_manager(),"mapAreaManager");if(!area)throw std::runtime_error("native view map indexer missing");
  invoke(nmo_method(0x6013867),area);invoke(nmo_method(0x601347c),centre,args);
  // The service may already have changed the selected index before this
  // native indexer handoff. Its effect reset still needs the deferred check.
  native_multiplayer_field_schedule_gate_check();
 }
public:
 unsigned full_rebuilds()const{return fullRebuilds.load();}unsigned incremental_updates()const{return incrementalUpdates.load();}unsigned natural_prepares()const{return naturalPrepares.load();}unsigned special_selections()const{return specialSelections.load();}
 void context(const Plan& p){contextPlan=p;nmo_snapshot=p.effective;nmo_self=p.seat;nmo_observed=p.target;nmo_own_stage=cnfield::text(cnfield::at(p.effective,"selfState"),"stageId");nmo_phase=p.phase;nmo_ready=true;}
 World world(){
  TemporaryRoots roots;auto controller=roots.retain(nmo_controller());if(!controller)return {};
  auto map=roots.retain(invoke(selected(0x600fbe8),controller));auto level=roots.retain(invoke(nmo_method(0x600fbea),controller));auto mapData=roots.retain(invoke(nmo_method(0x600fbe9),controller));
  auto centre=roots.retain(nb_centre()),manager=roots.retain(nb_manager());if(!map||!level||!mapData||!centre||!manager||field_object(map,"m_data")!=mapData)return {};
  auto bridge=roots.retain(field_object(manager,"dataBridge"));if(!bridge||api.il2cpp_object_get_class(bridge)!=cls("Torappu.Battle.AutoChess.AutoChessDataBridgeMultiPlayer"))return {};
  auto inout=roots.retain(invoke(method(cls("Torappu.Battle.BattleInOut"),"get_instance",0),nullptr));auto input=roots.retain(invoke(nmo_method(0x600f5cc),inout));auto info=roots.retain(native_battle_boxed_field(input,"stageInfo",roots));auto inputLevel=roots.retain(field_object(input,"levelData"));
  if(!inputLevel)return {};const auto stage=text(field_object(info,"stageId")),baseLevel=text(field_object(inputLevel,"levelId"));
  if(stage.empty()||baseLevel.empty())return {};const auto signedLevel=native_multiplayer_resolve_stage_path(stage,baseLevel,CN_PRIVATE_LEVELS);if(signedLevel.empty())return {};
  // The original round engine replaces controller.levelData with its enemy
  // template while retaining this physical map. Its immutable loader input
  // identifies the base battlefield; the logical combat level is not a scene.
  std::string importedStage,importedLevel;
  if(native_multiplayer_world_field_info(controller,map,importedStage,importedLevel))return {controller,map,inputLevel,centre,manager,bridge,importedStage,importedLevel,native_multiplayer_world_field_epoch()};
  return {controller,map,inputLevel,centre,manager,bridge,stage,signedLevel,native_multiplayer_world_field_epoch()};
 }
 void bind(){
  if(owner)throw std::runtime_error("native preparation view callback already owned");
  // All declarations must bind before either callback/executor hook is created.
  for(const auto& spec:NPV_METHODS){auto bound=bind_method(spec);void* pointer=nullptr;if(!read(bound,pointer)||reinterpret_cast<uintptr_t>(pointer)-reinterpret_cast<uintptr_t>(runtime)!=spec.rva)throw std::runtime_error("native preparation view exact method RVA changed");}
  for(const auto& constant:std::array<std::pair<const char*,int32_t>,3>{{{"SP_PREPARE",2},{"PREPARE",3},{"BATTLE_WAITING",100}}}){int32_t value=-1;api.il2cpp_field_static_get_value(profile_field(cls("Torappu.AutoChessGameStateType"),constant.first),&value);if(value!=constant.second)throw std::runtime_error("native preparation view exact state enum changed");}
  {int32_t value=-1;api.il2cpp_field_static_get_value(profile_field(cls("Torappu.Battle.AutoChess.AutoChessGameStatus+SubState"),"MAIN"),&value);if(value!=3)throw std::runtime_error("native preparation MAIN substate changed");}
  std::vector<native_mixed_help::HookInstall> created;
  try{
   native_mixed_help::create_exact(selected(0x6013486),on_prepared,originalPrepared,created);
   native_mixed_help::create_exact(selected(0x60138ba),on_terrain,originalTerrain,created);
   native_mixed_help::create_exact(selected(0x60136b4),on_prepare,originalPrepare,created);
   // Startup setup(false) probes declarations and creates hooks too. Only the
   // existing global activation after setup(true) may enable these detours.
  }catch(...){for(auto it=created.rbegin();it!=created.rend();++it){if(it->enabled)MH_DisableHook(it->target);MH_RemoveHook(it->target);}originalPrepared=originalTerrain=originalPrepare=nullptr;throw;}
  owner=this;
 }
 void seed(const Plan& p){
  nb_main_thread();if(!native_multiplayer_entry_active()||native_multiplayer_transport_room()!=p.room)throw std::runtime_error("native view seed lost private room ownership");
  if(!native_multiplayer_dispatch_projection_applied(p.effective))throw std::runtime_error("native view projection has not been applied by its module dispatcher");
  // This staging context must also be used by dispatch during an async loader.
  // It is presentation, never a server watchSeat or write-seat commitment.
  // The original bridge has already received the one actual module change.
  // A proof refresh must not issue another FullFill/OnBattleStatusChanged.
  context(p);
 }
 void reload(const Plan& p){
  // Launch exclusively owns the first Unity scene. Authenticated field changes
  // inside it import the original map/graphic without another battle_loader.
  if(loaderActive||nmo_reload_controller_root)throw std::runtime_error("native field switch cannot replace an active initial load");
  auto current=world();if(!current.controller||!current.map)throw std::runtime_error("native field switch requires the completed initial battle scene");
  if(native_multiplayer_stage_path(p.stage,CN_PRIVATE_LEVELS)!=p.level)throw std::runtime_error("native field switch target is outside signed resources");
  native_multiplayer_world_field_switch(cnfield::at(p.rawPlan,"source"),p.target,current.stage==p.stage);
 }

 bool poll(const Plan& p){
  if(!loaderActive)return nmo_poll_reload();
  if(!preparation_view::same_owner(p,loaderPlan))throw std::runtime_error("native loader poll belongs to another attempt");
  try{nmo_poll_reload();}catch(const std::exception&){loaderTimedOut=true;}
  auto now=world();
  if(!now.controller||now.controller==loaderBefore.controller||now.stage!=loaderPlan.stage||now.levelId!=loaderPlan.level||!native_battle_scene_ready(now.controller,now.manager))return false;
  // A new exact controller/map/level and the real preload completion gate are
  // required even if the old observer handle has already disappeared.
  if(nmo_reload_controller_root)nmo_release_reload();nmo_stage=loaderPlan.stage;
  loaderActive=false;loaderTimedOut=false;free_loader_handles();loaderBefore={};loaderPlan={};return true;
 }
 bool loader_pending()const{return loaderActive;}
 bool loader_expired()const{return loaderTimedOut;}
 const Plan& loader_plan()const{return loaderPlan;}
 void release_retired(){if(loaderActive)throw std::runtime_error("cannot release a live original loader lease");free_loader_handles();terrainFailureRoots.reset();terrainFailureWorld={};terrainFailureHash.clear();terrainFailureAttempt=-1;completed={};preparedPlan={};contextPlan={};naturalFailedPlan={};naturalFailedWorld={};preparedWorldRoots.reset();scopedWorld={};}
 void vm_shutdown_after_drain(bool gateDrained,bool workerJoined){if(!gateDrained||!workerJoined)throw std::runtime_error("native loader shutdown lacks the final runtime barrier");scopedPlan=nullptr;free_loader_handles();terrainFailureRoots.reset();loaderActive=false;loaderPlan={};loaderBefore={};completed={};preparedPlan={};contextPlan={};naturalFailedPlan={};naturalFailedWorld={};preparedWorldRoots.reset();scopedWorld={};}
 void prepared(void* centre){
  if(!scopedPlan)return;auto now=world();if(centre!=scopedWorld.centre||!preparation_view::same_world(now,scopedWorld))throw std::runtime_error("native prepare callback belongs to another scene");
  if(!terrainSeen)throw std::runtime_error("native prepare callback precedes the scoped original terrain executor");
  if(nmo_failed)throw std::runtime_error("native prepare callback retained a terrain error: "+nmo_error);
  completed={now,scopedPlan->hash,scopedPlan->terrain,scopedPlan->projectionHash,scopedPlan->attempt,scopedPlan->target,scopedPlan->revision,true,false};callbackSeen=true;
 }
 void failed(const std::string& value)noexcept{try{error=value;callbackSeen=false;if(nmo_failed&&scopedPlan){terrainFailureRoots=std::make_shared<Roots>(scopedWorld);terrainFailureWorld=scopedWorld;terrainFailureHash=scopedPlan->hash;terrainFailureAttempt=scopedPlan->attempt;terrainFailureFieldEpoch=native_multiplayer_world_field_epoch();}}catch(...){callbackSeen=false;}}
 bool wants_callback(void* centre)const{return scopedPlan&&centre==scopedWorld.centre;}
 bool wants_terrain(void* model)const{return scopedPlan&&field_object(scopedWorld.centre,"battleData")==model;}
 void terrain_complete(void* model){
  if(!wants_terrain(model)||!preparation_view::same_world(world(),scopedWorld)||nmo_failed)throw std::runtime_error("native terrain completion belongs to another model/world");
  if(nmh_result<int32_t>(invoke(nmo_method(0x601343f),scopedWorld.centre))!=scopedPlan->target||cnjson::dump(cnfield::at(nmo_snapshot,"dtoProjection"))!=cnjson::dump(scopedPlan->projection))throw std::runtime_error("native terrain completion lost its target/projection lease");
  terrainSeen=true;
 }
 bool valid(const Plan& p,const Proof& proof){
  if(!preparation_view::same_world(world(),proof.world)||nmo_failed||nmh_int(proof.world.centre,"<selfPlayerIndex>k__BackingField")!=p.seat)return false;
  if(!preparation_view::proof_matches_phase(p,completed)||!preparation_view::proof_matches_phase(p,proof)||!preparation_view::same_world(completed.world,proof.world)||!preparation_view::same_runtime(preparedPlan,p)||(preparation_view::same_runtime(naturalFailedPlan,p)&&preparation_view::same_world(naturalFailedWorld,proof.world)))return false;
  // A completed prepare receipt belongs to this retained world, not to the
  // current MAIN tick. The original phase task may already be yielding out of
  // PREPARE before the next HTTP envelope reports BATTLE. Requiring MAIN here
  // mistakes that normal transition for corruption and requests another scene.
  if(nmh_result<int32_t>(invoke(nmo_method(0x601343f),proof.world.centre))!=p.target||(nmh_result<uint8_t>(invoke(nmo_method(0x601343c),proof.world.centre))!=0)!=(p.target!=p.seat))return false;
  return cnjson::dump(cnfield::at(nmo_snapshot,"dtoProjection"))==cnjson::dump(p.projection);
 }
 bool can_rebuild(const Plan& p,const World& bound){
  nb_main_thread();if(!preparation_view::same_world(world(),bound))return false;
  auto status=field_object(bound.manager,"gameStatus");if(!status)return false;
  return nmh_result<int32_t>(invoke(selected(0x60136f5),status))==cnfield::number(cnfield::at(sync(p),"sceneDetail"),"state")&&nmh_result<int32_t>(invoke(selected(0x60136f7),status))==3;
 }
 bool task_prepare(void* manager){
  nb_main_thread();if(scopedPlan||!contextPlan.active||preparation_view::runtime_state(contextPlan)==2||native_multiplayer_transport_room()!=contextPlan.room)return false;
  const auto session=preparation_view::session(native_multi_entry_snapshot);
  if(!session.valid()||session.party||session.room!=contextPlan.room||session.run!=contextPlan.run||session.match!=contextPlan.match)return false;
  auto bound=world();if(bound.manager!=manager||bound.stage!=contextPlan.stage||bound.levelId!=contextPlan.level)return false;
  auto status=field_object(manager,"gameStatus");if(!status||nmh_result<int32_t>(invoke(selected(0x60136f5),status))!=preparation_view::runtime_state(contextPlan))return false;
  auto p=contextPlan;
  try{
   // Native service notifications can schedule this task again while the same
   // round/world/terrain is already prepared. Use the same incremental decision
   // as the proof coordinator; a shop/board/peer-ready delta must never finish
   // all units or call DoLoadGameInternal for a second time.
   rebuild(p,bound,true);
   // PreloadDataInPrepare starts Load immediately after this callback. Retire
   // any preview started by an earlier manual handoff before that one Load.
   invoke(selected(0x6013815),field_object(manager,"enemyPreviewer"));
   ++naturalPrepares;naturalFailedPlan={};naturalFailedWorld={};
  }
  catch(const std::exception& ex){naturalFailedPlan=p;naturalFailedWorld=bound;error=ex.what();}
  return true;
 }
 Proof rebuild(const Plan& p,const World& bound,bool nativePreload=false){
  nb_main_thread();if(!preparation_view::same_world(world(),bound)||nmh_int(bound.centre,"<selfPlayerIndex>k__BackingField")!=p.seat)throw std::runtime_error("native view rebuild actor/world changed");
  retire_error(p,bound);auto status=field_object(bound.manager,"gameStatus");
  const auto before=nmh_result<int32_t>(invoke(selected(0x60136f5),status));const auto expected=cnfield::number(cnfield::at(sync(p),"sceneDetail"),"state");if(before!=expected)throw std::runtime_error("native view rebuild is outside its exact prepare/wait phase");
  if(preparation_view::same_runtime(naturalFailedPlan,p)&&preparation_view::same_world(naturalFailedWorld,bound))throw std::runtime_error("original native prepare task failed: "+error);
  if(expected==2){
   // Official TC 0.6.1 trace, events-001:485..546: SP_PREPARE and its
   // three choice updates do not prepare/deploy the board. The one original
   // PrepareForPrepareState follows the transition to PREPARE. Keep that
   // distinction instead of clearing units here to manufacture terrain proof.
   auto retained=std::make_shared<Roots>(bound);
   if(nmh_result<int32_t>(invoke(nmo_method(0x601343f),bound.centre))!=p.target)set_view(p);
   const auto index=nmh_result<int32_t>(invoke(nmo_method(0x601343f),bound.centre));const auto inOb=nmh_result<uint8_t>(invoke(nmo_method(0x601343c),bound.centre))!=0;
   if(index!=p.target||inOb!=(p.target!=p.seat)||!can_rebuild(p,bound))throw std::runtime_error("native special selection lost its world/phase/target");
   nmo_snapshot=p.effective;completed={bound,p.hash,p.terrain,p.projectionHash,p.attempt,index,p.revision,false,inOb,true};preparedPlan=p;preparedWorldRoots=std::move(retained);++specialSelections;return completed;
  }
  if(completed.prepared&&preparedWorldRoots&&preparation_view::same_world(completed.world,bound)&&preparation_view::same_prepared_scene(preparedPlan,p)){
   // Fill/OnBattleStatusChanged already used the original MULTI bridge's dirty
   // module path. Retain its selected view and the actual prepare+terrain proof
   // for this unchanged world lifetime. Updating the same view again would
   // clear its instance cache and re-dirty the whole battle model.
   // Do not call PrepareForPrepareState here: it finishes every unit and runs
   // DoLoadGameInternal, discarding the board for an ordinary shop/ready delta.
   nmo_snapshot=p.effective;
   // Official rest-phase Observe/Cancel (events-001:735..780) changes the
   // preparation model without another full preparation or logical game load.
   // Reuse this physical map/terrain proof; only a genuinely changed target
   // needs the original view/indexer update. Other maps/terrain still rebuild.
   if(preparedPlan.target!=p.target||preparedPlan.targetRun!=p.targetRun)set_view(p);
   const auto index=nmh_result<int32_t>(invoke(nmo_method(0x601343f),bound.centre));const auto inOb=nmh_result<uint8_t>(invoke(nmo_method(0x601343c),bound.centre))!=0;
   if(!preparation_view::same_world(world(),bound)||index!=p.target||inOb!=(p.target!=p.seat)||nmh_result<int32_t>(invoke(selected(0x60136f5),status))!=before)throw std::runtime_error("native incremental prepare view lost its world/phase/target");
   completed={bound,p.hash,p.terrain,p.projectionHash,p.attempt,index,p.revision,true,inOb};preparedPlan=p;++incrementalUpdates;return completed;
  }
  return execute_prepare(p,bound,nativePreload);
 }
 void retire_error(const Plan& p,const World& bound){
  if(nmo_failed){
   // Only this adapter's recorded terrain failure may be retired, and only
   // after an authenticated new load identity has produced another world.
   // Polls of the failed attempt and unrelated Help/terminal failures survive.
   if(!terrainFailureRoots||(terrainFailureHash==p.hash&&terrainFailureAttempt==p.attempt)||(preparation_view::same_world(bound,terrainFailureWorld)&&native_multiplayer_world_field_epoch()==terrainFailureFieldEpoch))throw std::runtime_error(nmo_error);
   nmo_failed=false;nmo_error.clear();terrainFailureRoots.reset();terrainFailureWorld={};terrainFailureHash.clear();terrainFailureAttempt=-1;
  }
 }
 Proof execute_prepare(const Plan& p,const World& bound,bool nativePreload){
  retire_error(p,bound);
  auto status=field_object(bound.manager,"gameStatus");const auto before=nmh_result<int32_t>(invoke(selected(0x60136f5),status));
  completed={};preparedPlan={};preparedWorldRoots.reset();
  auto retained=std::make_shared<Roots>(bound);set_view(p);nmo_snapshot=p.effective;scopedPlan=&p;scopedWorld=bound;callbackSeen=false;terrainSeen=false;error.clear();
  ++fullRebuilds;try{void* managedError=nullptr;preparePass=bound.manager;api.il2cpp_runtime_invoke(selected(0x60136b4),bound.manager,nullptr,&managedError);preparePass=nullptr;if(managedError)throw std::runtime_error("original native prepare task raised managed failure");}catch(...){preparePass=nullptr;scopedPlan=nullptr;throw;}
  scopedPlan=nullptr;
  if(!error.empty()||!callbackSeen||!preparation_view::same_world(world(),bound)||nmo_failed)throw std::runtime_error("native view original prepare/terrain did not complete: "+error);
  if(nmh_result<int32_t>(invoke(selected(0x60136f5),status))!=before)throw std::runtime_error("native view original prepare changed the service phase");
  // Original DataCenter.PrepareForPrepareState resets its view at the tail.
  // Reapply the exact native chain after the successful prepare callback.
  set_view(p);auto index=nmh_result<int32_t>(invoke(nmo_method(0x601343f),bound.centre));auto inOb=nmh_result<uint8_t>(invoke(nmo_method(0x601343c),bound.centre))!=0;
  if(index!=p.target||inOb!=(p.target!=p.seat)||nmh_int(bound.centre,"<selfPlayerIndex>k__BackingField")!=p.seat)throw std::runtime_error("native view final selection or actor changed");
  // The native preload coroutine calls preview.Load after Prepare, but an
  // in-phase watch/return handoff invokes Prepare directly. Start its preview
  // only after the final target/terrain is installed, using the native enemy
  // and Boss preview generator. Ordinary incremental updates never enter here.
  if(!nativePreload&&before==3){
   auto preview=field_object(bound.manager,"enemyPreviewer");
   if(!preview||api.il2cpp_object_get_class(preview)!=cls("Torappu.Battle.AutoChess.AutoChessEnemyPreviewManager"))throw std::runtime_error("native preparation enemy preview manager missing");
   invoke(selected(0x6013815),preview);invoke(selected(0x6013814),preview);
  }
  completed.target=index;completed.inOb=inOb;preparedPlan=p;preparedWorldRoots=std::move(retained);return completed;
 }
};
void on_prepare(void* self,const void* info){
 if(preparePass==self){preparePass=nullptr;originalPrepare(self,info);return;}
 if(!owner||!native_multiplayer_entry_active()){originalPrepare(self,info);return;}
 RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
 try{if(!owner->task_prepare(self)){lease.release();originalPrepare(self,info);}}
 catch(const std::exception& ex){owner->failed(ex.what());}catch(...){owner->failed("original native preparation task host failed");}
}
void on_prepared(void* self,const void* info){
 if(preparedPass==self){preparedPass=nullptr;originalPrepared(self,info);return;}
 if(!owner||!owner->wants_callback(self)||!native_multiplayer_entry_active()){originalPrepared(self,info);return;}
 RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
 try{TemporaryRoots roots;roots.retain(self);void* error=nullptr;preparedPass=self;api.il2cpp_runtime_invoke(bound_methods.at(0x6013486),self,nullptr,&error);preparedPass=nullptr;
  if(error)throw std::runtime_error("native prepare callback raised managed failure");owner->prepared(self);
 }catch(const std::exception& ex){preparedPass=nullptr;owner->failed(ex.what());}catch(...){preparedPass=nullptr;owner->failed("native prepare callback host failed");}
}
void on_terrain(void* self,const void* info){
 if(terrainPass==self){terrainPass=nullptr;originalTerrain(self,info);return;}
 if(!owner||!owner->wants_terrain(self)||!native_multiplayer_entry_active()){originalTerrain(self,info);return;}
 RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
 try{TemporaryRoots roots;roots.retain(self);void* error=nullptr;terrainPass=self;api.il2cpp_runtime_invoke(bound_methods.at(0x60138ba),self,nullptr,&error);terrainPass=nullptr;
  if(error)throw std::runtime_error("native original terrain executor raised managed failure");owner->terrain_complete(self);
 }catch(const std::exception& ex){terrainPass=nullptr;owner->failed(ex.what());}catch(...){terrainPass=nullptr;owner->failed("native terrain executor host failed");}
}
}
