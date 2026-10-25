// Include globally loaded candidate headers before namespace cn. This body is
// included inside namespace cn after the existing IL2CPP helpers are defined.
namespace native_mixed_help {
using namespace ::mixedhelp;
using ::mixedhelp::concrete::Pipeline;
using ::mixedhelp::concrete::LoadPlan;
using ::mixedhelp::concrete::ExtraBinding;
using ::mixedhelp::concrete::EXTRA_BINDINGS;

struct Host {
    std::function<Context()> context;
    std::function<Identity(const Identity&)> restoreTarget;
    std::function<void(const RestoreRequest&)> restore;
};

class CnNative final:public Native {
    Host host;
    std::map<std::pair<std::string,uint32_t>,void*> methods;
    std::map<Method,void*> primitives;
    std::map<std::string,void*> extras;
    std::vector<RestoreRequest> restoreQueue;
    std::set<Object> privateInstances;
    void* abortException=nullptr;uint32_t abortRoot=0;
    using Raise=void(*)(void*);Raise raise=nullptr;
    static std::string type_name(void* type){char* raw=api.il2cpp_type_get_name(type);std::string name=raw?raw:"";if(raw)api.il2cpp_free(raw);return name;}
    static bool field_type(void* object,const char* name,const char* expected){return matches_type(api.il2cpp_field_get_type(profile_field(api.il2cpp_object_get_class(object),name)),expected);}
    template<class Spec>void* bind_exact(const Spec& spec,void* closed=nullptr){
        const auto key=std::make_pair(closed?type_name(api.il2cpp_class_get_type(closed)):std::string(spec.owner),spec.token);
        auto found=methods.find(key);if(found!=methods.end())return found->second;
        auto klass=closed?closed:cls(spec.owner);if(!klass)throw std::runtime_error("mixed-map native class missing");
        void* iterator=nullptr,*selected=nullptr;
        while(auto candidate=api.il2cpp_class_get_methods(klass,&iterator))if(api.il2cpp_method_get_token(candidate)==spec.token){selected=candidate;break;}
        if(!selected||(!closed&&api.il2cpp_method_get_declaring_type(selected)!=klass)||std::strcmp(api.il2cpp_method_get_name(selected),spec.name)||api.il2cpp_method_get_param_count(selected)!=spec.count||api.il2cpp_method_is_generic(selected))throw std::runtime_error("mixed-map class/token/name/arity contract mismatch");
        uint32_t implementation=0;
        if(bool(api.il2cpp_method_get_flags(selected,&implementation)&16)!=spec.isStatic||!matches_type(api.il2cpp_method_get_return_type(selected),spec.result))throw std::runtime_error("mixed-map return/static contract mismatch");
        for(uint32_t i=0;i<spec.count;++i){
            auto param=api.il2cpp_method_get_param(selected,i);
            if(closed){
                // The owned field fixes ListDict<String,MapGraphic>. A native
                // inflated MethodInfo may expose its original VAR signatures.
                const auto kind=api.il2cpp_type_get_type(param);
                if(kind==19){if(api.il2cpp_type_is_byref(param)!=(i==1&&spec.count==2))throw std::runtime_error("mixed-map ListDict generic byref contract mismatch");continue;}
            }
            const char* expected=closed?(i==0?"System.String":"Torappu.Battle.MapGraphic&"):spec.parameters[i];
            if(!matches_type(param,expected))throw std::runtime_error("mixed-map exact parameter type mismatch");
        }
        void* pointer=nullptr;if(!::cn::read(selected,pointer)||!pointer)throw std::runtime_error("mixed-map native code pointer missing");
        MEMORY_BASIC_INFORMATION page{};
        if(!VirtualQuery(pointer,&page,sizeof(page))||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||page.AllocationBase!=runtime||!(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||(page.Protect&PAGE_GUARD))throw std::runtime_error("mixed-map native pointer outside fixed GameAssembly");
        if(spec.rva&&reinterpret_cast<uintptr_t>(pointer)-reinterpret_cast<uintptr_t>(runtime)!=spec.rva)throw std::runtime_error("mixed-map native pointer differs from pinned module RVA");
        methods.emplace(key,selected);return selected;
    }
    void* dynamic_list_method(void* dictionary,const char* id){
        if(!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(dictionary)),"Torappu.ListDict`2<System.String,Torappu.Battle.MapGraphic>"))throw std::runtime_error("mixed-map subgraphic dictionary has foreign generic arguments");
        for(const auto& spec:EXTRA_BINDINGS)if(std::strcmp(spec.id,id)==0)return bind_exact(spec,api.il2cpp_object_get_class(dictionary));
        throw std::runtime_error("mixed-map ListDict declaration missing");
    }
    Object invoke_raw(void* method,Object self,const std::vector<Arg>& args){
        TemporaryRoots roots;std::vector<void*> raw(args.size());std::vector<uint8_t> booleans(args.size());
        for(size_t i=0;i<args.size();++i){const auto& a=args[i];switch(a.kind){
            case Arg::Kind::Object:raw[i]=roots.retain(a.object);break;
            case Arg::Kind::Int32:raw[i]=const_cast<int32_t*>(&a.integer);break;
            case Arg::Kind::Boolean:booleans[i]=a.boolean?1:0;raw[i]=&booleans[i];break;
            case Arg::Kind::Position:raw[i]=const_cast<Position*>(&a.position);break;
            case Arg::Kind::String:raw[i]=roots.retain(string_new(a.string.c_str()));break;
        }}
        return ::cn::invoke(method,roots.retain(self),raw.empty()?nullptr:raw.data());
    }
    std::vector<Object> descendants(Object root,const char* name,TemporaryRoots& roots){
        auto type=roots.retain(type_object(name));auto result=roots.retain(invoke_raw(extras.at("GameObjectChildren"),root,{Arg::ref(type),Arg::flag(true)}));
        const auto length=array_size(result);if(length>32768)throw std::runtime_error("mixed-map component traversal bound");
        std::vector<Object> values;values.reserve(length);
        for(size_t i=0;i<length;++i){auto value=roots.retain(invoke(Method::ArrayGetOne,result,{Arg::i32(int32_t(i))}));if(!value)throw std::runtime_error("mixed-map null component in subtree");values.push_back(value);}
        return values;
    }
    Object transform(Object component,TemporaryRoots& roots){return roots.retain(invoke_raw(extras.at("ComponentTransform"),component,{}));}
    bool below(Object child,const std::vector<Object>& parents){for(auto parent:parents)if(child==parent||boolean(invoke_raw(extras.at("TransformIsChildOf"),child,{Arg::ref(parent)})))return true;return false;}
    void retire_imported_tile_effects(Object map){
        main_thread();const auto context=current_context();
        if(context.map!=map||!context.controller)throw std::runtime_error("tile effect retirement owner changed");
        TemporaryRoots roots;roots.retain(map);std::set<Object> retired;
        // ImportData(force=true) destroys the old Tile hierarchy. Effect.OnDestroy
        // only stops sub-FX coroutines; ParticleEffect.OnFinish unregisters the
        // positional loop limit. Recycle before destruction, including effects
        // under inactive holders, or the next gate at that position is rejected.
        for(int row=0;row<19;++row)for(int col=0;col<21;++col){
            auto tile=roots.retain(invoke(Method::MapGetTile,map,{Arg::grid({row,col})}));
            if(!tile)continue;
            if(auto holder=roots.retain(field(tile,"m_effectHolder")))
                for(auto effect:descendants(holder,"Torappu.Battle.Effects.Effect",roots)){
                    if(!retired.insert(effect).second)continue;
                    if(retired.size()>8192)throw std::runtime_error("tile effect retirement exceeds bound");
                    if(!boolean(invoke_raw(extras.at("EffectFinished"),effect,{})))
                        invoke_raw(extras.at("EffectFinish"),effect,{Arg::flag(true)});
                }
        }
    }
public:
    explicit CnNative(Host value):host(std::move(value)){}
    ~CnNative(){if(abortRoot)hold_free(abortRoot);}
    void bind_native(){
        main_thread();
        if(!host.context||!host.restoreTarget||!host.restore)throw std::runtime_error("mixed-map authoritative host facade incomplete");
        for(const auto& spec:BINDING_DRAFT)primitives.emplace(spec.id,bind_exact(spec));
        for(const auto& spec:EXTRA_BINDINGS)if(std::string(spec.id).rfind("ListDict",0)!=0)extras.emplace(spec.id,bind_exact(spec));
        auto grid=cls("Torappu.GridPosition");uint32_t alignment=0;
        if(!grid||!api.il2cpp_class_is_valuetype(grid)||api.il2cpp_class_value_size(grid,&alignment)!=8)throw std::runtime_error("mixed-map GridPosition must be an 8-byte value");
        FARPROC exported=GetProcAddress(runtime,"il2cpp_raise_exception");if(!exported)throw std::runtime_error("mixed-map managed abort export unavailable");
        if(reinterpret_cast<uintptr_t>(exported)-reinterpret_cast<uintptr_t>(runtime)!=0x49d8b0ULL)throw std::runtime_error("mixed-map managed abort export RVA changed");
        std::memcpy(&raise,&exported,sizeof(raise));
        TemporaryRoots roots;auto exception=roots.retain(object_new(cls("System.InvalidOperationException")));
        void* ctor=nullptr,*iterator=nullptr;auto klass=api.il2cpp_object_get_class(exception);
        while(auto candidate=api.il2cpp_class_get_methods(klass,&iterator))if(std::strcmp(api.il2cpp_method_get_name(candidate),".ctor")==0&&api.il2cpp_method_get_param_count(candidate)==1&&matches_type(api.il2cpp_method_get_param(candidate,0),"System.String")){ctor=candidate;break;}
        if(!ctor)throw std::runtime_error("mixed-map managed exception constructor missing");
        auto message=roots.retain(string_new("Private mixed-map Help load aborted; restore own field before readiness."));void* args[]={message};::cn::invoke(ctor,exception,args);
        abortRoot=hold_new(exception,false);abortException=hold_get(abortRoot);if(abortException!=exception)throw std::runtime_error("mixed-map managed abort root failed");
    }
    void validate_map_schema(Object map){
        static const std::pair<const char*,const char*> fields[]={
            {"map","System.Int16[,]"},{"tiles","Torappu.TileData[]"},{"blockEdges","Torappu.MapData+Edge[]"},
            {"tags","System.String[]"},{"effects","Torappu.MapEffectData[]"},{"layerRects","System.String[]"}};
        for(const auto& entry:fields)if(!field_type(map,entry.first,entry.second))throw std::runtime_error("mixed-map MapData field declaration differs");
        auto matrix=field_object(map,"map"),tiles=field_object(map,"tiles");
        if(!matrix||!tiles||api.il2cpp_class_get_rank(api.il2cpp_object_get_class(matrix))!=2||
           !matches_type(api.il2cpp_class_get_type(api.il2cpp_class_get_element_class(api.il2cpp_object_get_class(matrix))),"System.Int16")||
           api.il2cpp_array_length(matrix)!=399||array_size(tiles)>32768)throw std::runtime_error("mixed-map Int16 matrix/TileData array dimensions differ");
    }
    void* primitive(Method id)const{return primitives.at(id);}
    void* extra(const char* id)const{return extras.at(id);}
    [[noreturn]] void abort_managed_no_raii(){
        // Caller has already exited every C++ cleanup scope. The exception is
        // raised only under the controlled outer runtime_invoke catch boundary.
        raise(abortException);std::abort();
    }
    void flush_restore(){main_thread();auto pending=std::move(restoreQueue);restoreQueue.clear();for(const auto& request:pending)host.restore(request);}
    void main_thread()override{nb_main_thread();}
    Context current_context()override{return host.context();}
    Identity restore_target(const Identity& source)override{return host.restoreTarget(source);}
    Object invoke(Method id,Object self,const std::vector<Arg>& args)override{
        if(id==Method::MapImport&&args.size()==6&&args[2].boolean){
            if(integer(invoke(Method::MapWidth,args[0].object,{}))!=21||integer(invoke(Method::MapHeight,args[0].object,{}))!=19)
                throw std::runtime_error("tile effect retirement outside catalog");
            retire_imported_tile_effects(self);
        }
        auto out=invoke_raw(primitives.at(id),self,args);
        if(id==Method::InstantiateParent){if(!out||out==args.at(0).object||!privateInstances.insert(out).second)throw std::runtime_error("mixed-map private graphic instance aliases an existing root");}
        if(id==Method::Destroy)privateInstances.erase(args.at(0).object);
        return out;
    }
    void unbind_scene_lighting(Object root){
        if(!privateInstances.count(root))throw std::runtime_error("mixed-map lightmap policy requires an owned private instance");
        TemporaryRoots roots;roots.retain(root);
        for(auto renderer:descendants(root,"UnityEngine.Renderer",roots)){
            invoke_raw(extras.at("RendererBakedIndex"),renderer,{Arg::i32(-1)});
            invoke_raw(extras.at("RendererRealtimeIndex"),renderer,{Arg::i32(-1)});
        }
        // Never touch static-batch firstSubMesh/subMeshCount: they refer to
        // preserved combined mesh ranges, independently of Scene lightmaps.
    }
    unsigned restore_missing_gate_effects(Object map,Object expectedData){
        main_thread();const auto context=current_context();
        if(context.map!=map||!context.controller||field(map,"m_data")!=expectedData)throw std::runtime_error("gate effect handoff owner changed");
        if(integer(invoke(Method::MapWidth,expectedData,{}))!=21||integer(invoke(Method::MapHeight,expectedData,{}))!=19)throw std::runtime_error("gate effect handoff outside catalog");
        TemporaryRoots roots;roots.retain(map);roots.retain(expectedData);unsigned restored=0,gates=0;
        for(int row=0;row<19;++row)for(int col=0;col<21;++col){
            auto data=roots.retain(invoke(Method::MapDataItem,expectedData,{Arg::i32(row),Arg::i32(col)}));
            const auto key=text(field(data,"tileKey"));
            if(key!="tile_start"&&key!="tile_end"&&key!="tile_flystart")continue;
            if(++gates>16)throw std::runtime_error("gate effect handoff exceeds catalog bound");
            auto tile=roots.retain(invoke(Method::MapGetTile,map,{Arg::grid({row,col})}));
            if(!tile||text(field(field(tile,"_data"),"tileKey"))!=key)throw std::runtime_error("gate effect handoff tile changed");
            if(boolean(invoke_raw(extras.at("TileHidden"),tile,{})))continue;
            auto go=roots.retain(invoke(Method::GetGameObject,tile,{}));
            if(!boolean(invoke_raw(extras.at("GameObjectActive"),go,{})))continue;
            const auto effectKey=text(field(tile,"_effect"));if(effectKey.empty())continue;
            bool alive=false;
            // A holder survives ClearAllTileEffects; its presence alone does
            // not establish that the native looping gate effect still exists.
            if(auto holder=roots.retain(field(tile,"m_effectHolder"))){
                for(auto effect:descendants(holder,"Torappu.Battle.Effects.Effect",roots))
                    if(text(field(effect,"<effectKey>k__BackingField"))==effectKey&&!boolean(invoke_raw(extras.at("EffectFinished"),effect,{}))){alive=true;break;}
            }
            if(!alive){invoke_raw(extras.at("TileGenerateEffect"),tile,{});++restored;}
        }
        return restored;
    }
    Object allocate(const char* name)override{auto klass=cls(name);if(!klass)throw std::runtime_error("mixed-map allocation class missing");auto value=object_new(klass);::cn::invoke(method(klass,".ctor",0),value);return value;}
    void copy_fields(Object to,Object from)override{copy_native_fields(to,from);}
    Object field(Object value,const char* name)override{return field_object(value,name);}
    void field(Object value,const char* name,Object child)override{set_object_field(value,profile_field(api.il2cpp_object_get_class(value),name),child);}
    Object array(const char* name,size_t length)override{auto klass=cls(name);if(!klass||length>32768)throw std::runtime_error("mixed-map array allocation bound/type");return array_new(klass,length);}
    size_t array_size(Object value)override{if(!value||api.il2cpp_class_get_rank(api.il2cpp_object_get_class(value))!=1)throw std::runtime_error("mixed-map expected rank-one managed array");return api.il2cpp_array_length(value);}
    int32_t integer(Object value)override{int32_t out=0;if(!value||!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(value)),"System.Int32")||!::cn::read(unbox(value),out))throw std::runtime_error("mixed-map boxed Int32 mismatch");return out;}
    bool boolean(Object value)override{uint8_t out=0;if(!value||!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(value)),"System.Boolean")||!::cn::read(unbox(value),out)||out>1)throw std::runtime_error("mixed-map boxed Boolean mismatch");return out!=0;}
    float floating(Object value)override{float out=0;if(!value||!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(value)),"System.Single")||!::cn::read(unbox(value),out))throw std::runtime_error("mixed-map boxed Single mismatch");return out;}
    Position position(Object value)override{Position out;if(!value||!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(value)),"Torappu.GridPosition")||!::cn::read(unbox(value),out))throw std::runtime_error("mixed-map boxed GridPosition mismatch");return out;}
    Object short_box(int16_t value)override{return value_box(cls("System.Int16"),&value);}
    Object type_object(const char* name)override{auto klass=cls(name);if(!klass)throw std::runtime_error("mixed-map reflected class missing");return reflection_type(api.il2cpp_class_get_type(klass));}
    uint32_t retain(Object value)override{return hold_new(value,false);}
    Object retained(uint32_t handle)override{return hold_get(handle);}
    void release(uint32_t handle)override{hold_free(handle);}
    bool unregister_if_owned(Object map,const std::string& key,Object expected)override{
        if(!field_type(map,"m_subMapGraphics","Torappu.ListDict`2<System.String,Torappu.Battle.MapGraphic>"))throw std::runtime_error("mixed-map registry field type mismatch");
        auto dictionary=field_object(map,"m_subMapGraphics");if(!dictionary)return false;
        TemporaryRoots roots;roots.retain(dictionary);auto managedKey=roots.retain(string_new(key.c_str()));void* current=nullptr;void* lookup[]={managedKey,&current};
        if(!boolean(::cn::invoke(dynamic_list_method(dictionary,"ListDictLookup"),dictionary,lookup))||current!=expected)return false;
        void* args[]={managedKey};return boolean(::cn::invoke(dynamic_list_method(dictionary,"ListDictRemove"),dictionary,args));
    }
    void prune_rendering(Object root,const std::vector<Object>& kept,bool shared)override{
        TemporaryRoots roots;roots.retain(root);std::vector<Object> allowed,allTiles;
        for(auto graphic:kept)allowed.push_back(transform(roots.retain(graphic),roots));
        for(auto graphic:descendants(root,"Torappu.Battle.TileGraphic",roots))allTiles.push_back(transform(graphic,roots));
        auto allowedAt=[&](Object component){auto where=transform(component,roots);return below(where,allowed)||(shared&&!below(where,allTiles));};
        for(auto renderer:descendants(root,"UnityEngine.Renderer",roots))if(!allowedAt(renderer))invoke_raw(extras.at("RendererEnable"),renderer,{Arg::flag(false)});
        for(auto light:descendants(root,"UnityEngine.Light",roots))if(!allowedAt(light))invoke_raw(extras.at("BehaviourEnable"),light,{Arg::flag(false)});
        for(auto camera:descendants(root,"UnityEngine.Camera",roots))invoke_raw(extras.at("BehaviourEnable"),camera,{Arg::flag(false)});
    }
    void request_own_reload(const RestoreRequest& request)override{restoreQueue.push_back(request);}
};
}
