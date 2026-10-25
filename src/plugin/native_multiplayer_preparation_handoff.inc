// Private native lifecycle arbitration after dispatch/NFF/entry helpers.
namespace preparation_view_native {
preparation_view::Handoff* handoff=nullptr;
cnjson::Value lastFieldView;std::string retiringFieldHash;
std::function<void(const cnjson::Value&)> nativeDispatch;

bool bootstrap_owner_current(const preparation_view::Session& origin,uint64_t epoch){
 if(epoch!=native_multi_entry_service_epoch||!native_multi_entry_controller_root)return false;
 auto controller=hold_get(native_multi_entry_controller_root);
 return controller&&native_multi_entry_target(controller)&&preparation_view::session(native_multi_entry_snapshot)==origin;
}
bool transfer_blocked(){return handoff&&handoff->blocked();}
void initialize_handoff(std::function<void(const cnjson::Value&)> dispatch){
 if(handoff||!coordinator||!adapter)throw std::runtime_error("native scene handoff setup order changed");nativeDispatch=std::move(dispatch);
 preparation_view::TransferHost host;host.mainThread=nb_main_thread;
 host.bootstrapCurrent=bootstrap_owner_current;host.bootstrapLaunchIdle=[](){return !native_battle_launch_in_flight.load()&&!native_battle_launch_pending.load();};
 host.retirePreparation=[](){coordinator->retire();presentedPlan={};};
 host.preparationBusy=[](){return adapter->loader_pending()||coordinator->scene_loading();};
 host.drainPreparation=[](){coordinator->tick(int64_t(native_multiplayer_transport_time()));};
 host.releasePreparation=[](){coordinator->release_retired();adapter->release_retired();};
 host.fieldLoadBusy=[](){if(!nff_pipeline)return false;auto state=nff_pipeline->status();return state==mixedhelp::concrete::Pipeline::State::Preparing||state==mixedhelp::concrete::Pipeline::State::Imported||state==mixedhelp::concrete::Pipeline::State::Attached;};
 host.fieldRetryBusy=[](){return nff_retry_reload;};host.fieldRestoreBusy=[](){return nff_restore_pending;};
 host.fieldActive=[](){return nff_plan.active;};host.fieldTick=[](){native_multiplayer_field_tick();};
 host.retireField=[](){
  if(nff_retry_reload)throw std::runtime_error("native Help retry still owns the shared loader");
  if(lastFieldView.kind!=cnjson::Value::Object||!cnfield::same_load(cnfield::parse(lastFieldView),nff_plan))throw std::runtime_error("native Help retirement lacks its original immutable view");
  retiringFieldHash=nff_plan.hash;auto retired=lastFieldView;retired.object["phase"]=preparation_view::string("result");
  // Only this exact original field module sees the inactive local envelope.
  // No service DTO/NMO snapshot/actor state is rewritten to that synthetic phase.
  native_multiplayer_field_prepare(retired);native_multiplayer_field_tick();
 };
 host.otherLoaderBusy=[](){return (native_battle_launch_in_flight.load()&&native_battle_scene_epoch.pending)||(nmo_reload_controller_root!=0&&!adapter->loader_pending());};
 host.fieldBasePending=[](){return native_multiplayer_entry_active()&&native_multi_entry_controller_root&&
     !native_battle_launch_in_flight.load()&&!adapter->world().controller;};
 host.retainCurrentWorld=[](){return std::static_pointer_cast<void>(std::make_shared<Roots>(adapter->world()));};
 host.validateFieldBase=[](const cnjson::Value& raw){
  auto field=cnfield::parse(raw);if(!field.active)throw std::runtime_error("Help transfer has no authenticated field plan");auto world=adapter->world();
  if(!world.controller||world.levelId!=native_multiplayer_stage_path(world.stage,CN_PRIVATE_LEVELS))throw std::runtime_error("Help transfer base world is not an exact signed stage");
  bool declared=false;for(const auto& row:cnfield::at(cnfield::at(cnfield::at(raw,"nativeField"),"plan"),"restoreBySeat").object){auto source=cnfield::source(cnfield::at(row.second,"source"));if(source.stageId==world.stage)declared=true;}
  if(!declared)throw std::runtime_error("Help transfer would reuse a foreign undeclared base map");
  // This proves only the base source. It never identifies that base world as
  // self or as a completed Help composition; NFF still owns all later proofs.
  nmo_stage=world.stage;
 };
 host.applyNative=[](const cnjson::Value& raw){auto phase=cnfield::text(raw,"phase");if(phase=="help")lastFieldView=raw;
  const bool coldHelp=phase=="help"&&!native_battle_launch_in_flight.load()&&!adapter->world().controller;
  nativeDispatch(phase=="help"&&!coldHelp?raw:effective_view(raw));notify_presented(raw);};
 host.presentRaw=[](const cnjson::Value&){/* Root transport presents raw after successful deferral. No native proof here. */};
 host.fieldOwns=[](const cnjson::Value& raw){auto field=cnfield::parse(raw);return cnfield::same_load(field,nff_plan)&&nff_pipeline&&nff_pipeline->ready()&&mixedhelp::same_context(nff_current(),nff_context)&&nff_predefines_ready;};
 host.fieldAccepted=[](const cnjson::Value& raw){return cnfield::same_load(cnfield::parse(raw),nff_plan);};
 host.shutdownRoots=[](bool drained,bool joined){
  // Destruction here only frees plugin GC handles; never call a managed reset.
  // The VM is about to destroy all Unity/managed objects itself.
  adapter->vm_shutdown_after_drain(drained,joined);coordinator->release_at_vm_shutdown(drained,joined);presentedPlan={};
 };
 handoff=new preparation_view::Handoff(std::move(host));
}
bool dispatch_raw(const cnjson::Value& raw,std::function<void()> before={},std::function<void(const cnjson::Value&)> after={},std::function<void(const std::string&)> rejected={}){
 nb_main_thread();if(!handoff||!adapter||!coordinator)throw std::runtime_error("native scene handoff is not configured");
 auto owner=preparation_view::session(raw);auto phase=cnfield::text(raw,"phase");auto presentation=preparation_view::parse(raw,digest_json,[](const std::string& stage){return native_multiplayer_stage_path(stage,CN_PRIVATE_LEVELS);});const bool prep=phase=="preparation"||phase=="special"||phase=="battle_countdown"||presentation.active;
 const auto current=handoff->owner();const bool changed=current.valid()&&!(current==owner);
 const bool prepOwns=coordinator->current().active||coordinator->scene_loading()||adapter->loader_pending();
 const bool livePreparation=coordinator->scene_loading()||adapter->loader_pending();const bool fieldOwned=nff_plan.active||nff_retry_reload||nff_restore_pending;
 const bool retirement=changed||bool(before)||(phase=="help"&&prepOwns)||(livePreparation&&!prep)||(fieldOwned&&phase!="help");
 if(handoff->blocked()||retirement||before||after||rejected){
  if(nff_plan.active)retiringFieldHash=nff_plan.hash;
  // Reconstructing Help recipients have no field plan. They load their base
  // in BATTLE_WAITING and must not acquire a Help composition lease.
  const bool coldHelp=phase=="help"&&!fieldOwned&&!native_battle_launch_in_flight.load()&&!adapter->world().controller;
  auto kind=phase=="help"&&cnfield::parse(raw).active&&!coldHelp?preparation_view::Transfer::Field:prep?preparation_view::Transfer::Preparation:preparation_view::Transfer::Native;
  handoff->submit(raw,kind,std::move(before),std::move(after),std::move(rejected),retirement);return false;
 }
 if(before)before();if(owner.valid())handoff->observe_preparation(raw);if(phase=="help")lastFieldView=raw;
 nativeDispatch(effective_view(raw));return true;
}
bool retire_entry(const preparation_view::Session& origin,std::function<void()> completion,const std::string& key){nb_main_thread();return handoff&&handoff->request_exit(origin,std::move(completion),key);}
void handoff_tick(){
 static std::string reportedHandoffError;
 if(!handoff)return;handoff->tick();const auto& error=handoff->failure();
 if(!error.empty()&&error!=reportedHandoffError){reportedHandoffError=error;std::runtime_error failure("native scene handoff: "+error);nmd_failed(failure);}
}
bool may_drive_field_lifecycle(){return handoff&&handoff->blocked()&&!retiringFieldHash.empty()&&retiringFieldHash==nff_plan.hash;}
bool may_report_field(){
 if(!handoff)return true;if(handoff->vm_terminal())return false;
 if(handoff->blocked())return lastFieldView.kind==cnjson::Value::Object&&handoff->may_report_field(lastFieldView);
 return true;
}
void shutdown_after_drain(bool drained,bool joined){if(handoff)handoff->shutdown_after_drain(drained,joined);}
}

void native_multiplayer_preparation_view_setup(){preparation_view_native::bind_facade();preparation_view_native::initialize_handoff(native_multiplayer_dispatch_apply);}
bool native_multiplayer_preparation_view_dispatch(const cnjson::Value& raw,std::function<void()> before,std::function<void(const cnjson::Value&)> after,std::function<void(const std::string&)> rejected){return preparation_view_native::dispatch_raw(raw,std::move(before),std::move(after),std::move(rejected));}
bool native_multiplayer_preparation_view_claim_initial_owner(const preparation_view::Session& origin,uint64_t epoch){
 nb_main_thread();auto handoff=preparation_view_native::handoff;if(!handoff)return false;
 if(handoff->claim_initial_owner(origin,epoch))return true;
 // Startup sync can already have presented the same logical room before the
 // first Continue callback. No native service has existed in this process.
 if(epoch==0&&native_multi_entry_service_epoch==0&&native_multi_entry_disposed_epoch==0&&
    native_multi_entry_pending=="resume"&&!native_multi_entry_mode_root&&!native_multi_entry_service_root&&!native_multi_entry_disposing)
     return handoff->claim_rejoin_owner(origin,epoch);
 // Do not reinterpret a live service or incomplete disposal as a reconnect.
 // A delayed snapshot may have reclaimed this exact logical owner after the
 // original Dispose returned and entry_reset_now released both native roots.
 if(!epoch||epoch!=native_multi_entry_service_epoch||native_multi_entry_disposed_epoch!=epoch||
    native_multi_entry_mode_root||native_multi_entry_service_root||native_multi_entry_disposing)return false;
 return handoff->claim_rejoin_owner(origin,epoch);
}
bool native_multiplayer_preparation_view_finalizing(){return preparation_view_native::handoff&&preparation_view_native::handoff->finalizing();}
cnjson::Value native_multiplayer_preparation_view_effective(const cnjson::Value& raw){return preparation_view_native::effective_view(raw);}
bool native_multiplayer_preparation_view_owns_presentation(const cnjson::Value& raw){return preparation_view_native::owns_presentation(raw);}
void native_multiplayer_preparation_view_tick(){preparation_view_native::handoff_tick();if(!preparation_view_native::transfer_blocked())preparation_view_native::tick();}
void native_multiplayer_preparation_view_loader_completed(){nb_main_thread();preparation_view_native::handoff_tick();}
bool native_multiplayer_preparation_view_may_start_own_battle(){return !preparation_view_native::transfer_blocked()&&preparation_view_native::may_start_own_battle();}
bool native_multiplayer_preparation_view_may_create_preparation_scene(const cnjson::Value& raw){
 nb_main_thread();const auto phase=cnfield::text(raw,"phase");return (phase=="loading"||phase=="preparation"||phase=="special")&&!preparation_view_native::transfer_blocked()&&(!preparation_view_native::adapter||!preparation_view_native::adapter->loader_pending());
}
bool native_multiplayer_preparation_view_may_write_board(){return !preparation_view_native::transfer_blocked()&&preparation_view_native::may_write_board();}
bool native_multiplayer_preparation_view_may_choose(){return !preparation_view_native::transfer_blocked()&&preparation_view_native::may_choose();}
bool native_multiplayer_preparation_view_owns_scene_transition(){return preparation_view_native::owns_scene_transition();}
bool native_multiplayer_preparation_view_retire(const preparation_view::Session& origin,std::function<void()> done,const std::string& key){return preparation_view_native::retire_entry(origin,std::move(done),key);}
bool native_multiplayer_preparation_view_drive_field_lifecycle(){return preparation_view_native::may_drive_field_lifecycle();}
bool native_multiplayer_preparation_view_report_field(){return preparation_view_native::may_report_field();}
void native_multiplayer_preparation_view_shutdown(){preparation_view_native::shutdown_after_drain(closing.load(),!nmp_running.load()&&!nmp_worker_thread.joinable());}
cnjson::Value native_multiplayer_preparation_view_status(){
 nb_main_thread();auto result=nb_object();result.object["candidateOnly"]=nb_boolean(false);result.object["nativeRuntimeValidated"]=nb_boolean(false);result.object["blocked"]=nb_boolean(preparation_view_native::transfer_blocked());result.object["loaderPending"]=nb_boolean(preparation_view_native::adapter&&preparation_view_native::adapter->loader_pending());result.object["loaderObservedTimeout"]=nb_boolean(preparation_view_native::adapter&&preparation_view_native::adapter->loader_expired());return result;
}
