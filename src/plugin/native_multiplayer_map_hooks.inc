// Included inside namespace cn after cn_native_adapter.inc. No custom C++
// exception may cross an IL2CPP frame. Runtime-invoke is the managed catch edge.
namespace native_mixed_help {
using LoadNative=void(*)(Object,Object,Object,Object,int32_t,Object,void*,void*,const void*);
using InitNative=::mixedhelp::MapInitNative;
using SetDataNative=::mixedhelp::TileSetDataNative;
using HeightNative=::mixedhelp::MapHeightNative;
struct HooksHost {
    std::function<bool(Object,Object,Object)> target;
    std::function<LoadPlan(Object,Object,Object)> plan;
    std::function<bool()> heightRoutesReady;
    std::function<Object(const LoadPlan&)> prepared;
    std::function<bool(Object,int32_t,uintptr_t,float&)> tryHeightRoute;
    std::function<void(bool)> loading;
    std::function<void(const std::string&)> failed;
    std::function<void(const char*)> lifecycle;
};
CnNative* native=nullptr;Pipeline* pipeline=nullptr;HooksHost hooksHost;
LoadNative originalLoad=nullptr;InitNative originalInit=nullptr;SetDataNative originalSetData=nullptr;HeightNative originalHeight=nullptr;
thread_local unsigned controlledLoad=0,passLoad=0,passInit=0,passSetData=0;
struct HeightScope {Object map=nullptr,graphic=nullptr;bool consumed=false;};
thread_local HeightScope heightScope;
std::string hookError;
void observe_lifecycle(const char* phase)noexcept{try{if(hooksHost.lifecycle)hooksHost.lifecycle(phase);}catch(...){}}
void report_failure(const char* text)noexcept{try{hookError=text?text:"mixed-map unknown native host error";if(hooksHost.failed)hooksHost.failed(hookError);}catch(...){} }
void fail_transaction(const char* text)noexcept{
    try{if(pipeline&&(pipeline->status()==Pipeline::State::Preparing||pipeline->ready()))pipeline->abort_before_commit(std::runtime_error(text));}catch(...){}
}

void private_load(Object,Object,Object,Object,int32_t,Object,void*,void*,const void*);
void private_init(Object,Object,Object,Object,Object,const void*);
void private_set_data(Object,Object,Position,Object,int32_t,const void*);
float private_height(Object,int32_t,const void*);

struct HookInstall {void* target=nullptr;bool enabled=false;};
template<class Function>void create_exact(void* method,Function replacement,Function& original,std::vector<HookInstall>& created){
    void* target=nullptr,*handler=nullptr,*trampoline=nullptr;
    if(!::cn::read(method,target)||!target)throw std::runtime_error("mixed-map exact hook target missing");
    std::memcpy(&handler,&replacement,sizeof(handler));
    if(MH_CreateHook(target,handler,&trampoline)!=MH_OK)throw std::runtime_error("mixed-map hook ownership collision");
    created.push_back({target,false});
    std::memcpy(&original,&trampoline,sizeof(original));
}
void install(CnNative& adapter,Pipeline& transaction,HooksHost host){
    adapter.main_thread();if(native||pipeline||!host.target||!host.plan||!host.heightRoutesReady||!host.tryHeightRoute||!host.loading||!host.failed)throw std::runtime_error("mixed-map hook host missing or already installed");
    // Every primitive/extra has already passed class/token/type/RVA binding.
    native=&adapter;pipeline=&transaction;hooksHost=std::move(host);
    std::vector<HookInstall> created;
    try{
        create_exact(adapter.extra("ControllerLoadSeam"),private_load,originalLoad,created);
        create_exact(adapter.primitive(Method::MapInit),private_init,originalInit,created);
        create_exact(adapter.extra("TileSetDataHook"),private_set_data,originalSetData,created);
        create_exact(adapter.extra("MapHeightHook"),private_height,originalHeight,created);
        for(auto& hook:created){if(MH_EnableHook(hook.target)!=MH_OK)throw std::runtime_error("mixed-map hook activation failed");hook.enabled=true;}
    }catch(...){for(auto it=created.rbegin();it!=created.rend();++it){if(it->enabled)MH_DisableHook(it->target);MH_RemoveHook(it->target);}native=nullptr;pipeline=nullptr;hooksHost={};throw;}
}

void private_load(Object self,Object players,Object level,Object data,int32_t difficulty,Object rune,void* options,void* extra,const void* methodInfo){
    // The controlled inner trampoline owns no RuntimeLease, lock, roots or
    // other C++ object needing foreign-exception cleanup.
    if(passLoad){originalLoad(self,players,level,data,difficulty,rune,options,extra,methodInfo);return;}
    bool target=false;
    try{target=native&&hooksHost.target(self,data,level);}catch(const std::exception& ex){report_failure(ex.what());return;}catch(...){report_failure("mixed-map target facade failed");return;}
    if(!target){originalLoad(self,players,level,data,difficulty,rune,options,extra,methodInfo);return;}
    // This outer detour remains outside runtime_invoke's managed catch edge.
    // It returns normally after every managed exception, so its C++ cleanup is
    // never dependent on MSVC exceptions unwinding MinGW RAII.
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    bool completed=false;Object managedError=nullptr;
    try{
        hookError.clear();
        native->main_thread();hooksHost.loading(true);
        if(!hooksHost.heightRoutesReady())throw std::runtime_error("mixed-map movement/effect height routers are not fully bound");
        auto plan=hooksHost.plan(self,data,level);
        if(plan.context.controller!=self||plan.neutral.mapData!=data||plan.neutral.levelData!=level)throw std::runtime_error("mixed-map load plan does not own the exact native arguments");
        if(pipeline->ready()){
            if(!same_context(plan.context,pipeline->active_plan().context)||plan.planHash!=pipeline->active_plan().planHash||native->field(self,"m_mapData")!=pipeline->map_data())throw std::runtime_error("mixed-map duplicate load differs from the committed owner");
            hooksHost.loading(false);return;
        }
        for(const auto* stage:{&plan.neutral,&plan.left,&plan.right,&plan.self})native->validate_map_schema(stage->mapData);
        auto composed=pipeline->prepare(plan);if(!composed)throw std::runtime_error(pipeline->failure());
        pipeline->original_load_entering();
        auto workingLevel=level;
        if(hooksHost.prepared){workingLevel=hooksHost.prepared(plan);pipeline->use_level(workingLevel);}
        // Every root here is an isolated prefab clone, including Neutral.
        // Its baked indices belong to the source Scene, never the live host
        // Scene's lightmap table. Preserve materials and static-batch ranges.
        for(const auto& graphic:pipeline->owned_graphics())if(!graphic.borrowed)native->unbind_scene_lighting(graphic.root);
        observe_lifecycle("help-before-original-load");
        void* args[]={players,workingLevel,composed,&difficulty,rune,options,extra};
        ++controlledLoad;++passLoad;
        api.il2cpp_runtime_invoke(native->extra("ControllerLoadSeam"),self,args,&managedError);
        --passLoad;--controlledLoad;
        observe_lifecycle("help-after-original-load");
        if(managedError)throw std::runtime_error(!hookError.empty()?hookError:("mixed-map original load: "+exception_description(managedError)));
        if(!pipeline->ready())throw std::runtime_error("mixed-map native load returned without a committed map");
        completed=true;
    }catch(const std::exception& ex){
        passLoad=0;controlledLoad=0;passInit=0;passSetData=0;heightScope={};hookError=ex.what();
        fail_transaction(ex.what());report_failure(ex.what());
    }catch(...){
        passLoad=0;controlledLoad=0;passInit=0;passSetData=0;heightScope={};
        fail_transaction("mixed-map native host failed");report_failure("mixed-map native host failed");
    }
    try{hooksHost.loading(!completed);}catch(...){report_failure("mixed-map loading gate callback failed");}
    try{native->flush_restore();}catch(const std::exception& ex){report_failure(ex.what());}catch(...){report_failure("mixed-map own-field restore callback failed");}
}

void private_init(Object self,Object data,Object level,Object disallowed,Object theme,const void* methodInfo){
    if(passInit){originalInit(self,data,level,disallowed,theme,methodInfo);return;}
    if(!controlledLoad||!pipeline){originalInit(self,data,level,disallowed,theme,methodInfo);return;}
    bool abort=false;
    {
        RuntimeLease lease(runtime_gate);
        try{
            if(!lease||closing.load())throw std::runtime_error("mixed-map map commit during shutdown");
            if(!pipeline->owns(self,data))throw std::runtime_error("mixed-map native Map.Init arguments escaped prepared owner");
            native->validate_map_schema(data);
            bool okay=pipeline->commit(data,level,[&](Object composed,Object escaped){
                // A second protected runtime boundary catches native Map.Init
                // exceptions before they can unwind the Pipeline's C++ locals.
                Object error=nullptr;void* args[]={composed,escaped,disallowed,theme};++passInit;
                api.il2cpp_runtime_invoke(native->primitive(Method::MapInit),self,args,&error);--passInit;
                if(error)throw std::runtime_error(!hookError.empty()?hookError:("mixed-map original Map.Init: "+exception_description(error)));
            });
            observe_lifecycle(okay?"help-after-map-commit":"help-map-commit-failed");
            if(!okay){hookError=pipeline->failure();abort=true;}
        }catch(const std::exception& ex){passInit=0;hookError=ex.what();abort=true;}
        catch(...){passInit=0;hookError="mixed-map Map.Init host failed";abort=true;}
        lease.release();
    }
    // Every C++ cleanup scope above has exited. Clear all TLS pointers before
    // raising a managed exception; no GCC destructor is needed on that edge.
    if(abort){passInit=0;passSetData=0;heightScope={};native->abort_managed_no_raii();}
}

void private_set_data(Object self,Object data,Position pos,Object map,int32_t layer,const void* methodInfo){
    if(passSetData){originalSetData(self,data,pos,map,layer,methodInfo);return;}
    if(!controlledLoad||!pipeline||!pipeline->contains_map(map)){originalSetData(self,data,pos,map,layer,methodInfo);return;}
    bool abort=false;const HeightScope previous=heightScope;
    {
        RuntimeLease lease(runtime_gate);
        try{
            if(!lease||closing.load())throw std::runtime_error("mixed-map Tile.SetData during shutdown");
            heightScope={map,pipeline->height_graphic(map,data,pos),false};
            Object error=nullptr;void* args[]={data,&pos,map,&layer};++passSetData;
            api.il2cpp_runtime_invoke(native->extra("TileSetDataHook"),self,args,&error);--passSetData;
            if(error)throw std::runtime_error(!hookError.empty()?hookError:("mixed-map original Tile.SetData: "+exception_description(error)));
            if(!heightScope.consumed)throw std::runtime_error("mixed-map Tile.SetData did not complete its native height binding");
        }catch(const std::exception& ex){passSetData=0;hookError=ex.what();abort=true;}
        catch(...){passSetData=0;hookError="mixed-map Tile.SetData host failed";abort=true;}
        heightScope=previous;lease.release();
    }
    if(abort){passSetData=0;heightScope={};native->abort_managed_no_raii();}
}

float private_height(Object self,int32_t type,const void* methodInfo){
    const auto caller=reinterpret_cast<uintptr_t>(__builtin_return_address(0))-reinterpret_cast<uintptr_t>(runtime);
    bool abort=false;float result=0;
    {
        try{
            if(heightScope.map&&heightScope.map==self&&!heightScope.consumed){heightScope.consumed=true;result=native->floating(native->invoke(Method::MapGraphicHeight,heightScope.graphic,{Arg::i32(type)}));}
            else if(!native||!pipeline||!pipeline->contains_map(self)||!hooksHost.tryHeightRoute(self,type,caller,result))return originalHeight(self,type,methodInfo);
        }
        catch(const std::exception& ex){hookError=ex.what();abort=true;}
        catch(...){hookError="mixed-map native height route failed";abort=true;}
    }
    if(abort){heightScope={};native->abort_managed_no_raii();}
    return result;
}
}
