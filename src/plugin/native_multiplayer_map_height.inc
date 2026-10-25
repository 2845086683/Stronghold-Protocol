// Research candidate. Include both height headers globally; this body inside
// namespace cn after the existing API, binding and runtime-gate helpers.
namespace native_mixed_height {
using namespace ::mixedheight;
using MoveNative=void(*)(Object,Object,Object,const void*);
using EntityNative=void(*)(Object,Object,const void*);
using EffectNative=void(*)(Object,uint8_t,const void*);
using GridNative=uint64_t(*)(const Position*,const void*);
using ItemNative=Object(*)(Object,Position,const void*);
using VectorNative=V3*(*)(V3*,Object,const void*);
MoveNative originalFarthest=nullptr,originalNearest=nullptr,originalSector=nullptr;
EntityNative originalEntityEffect=nullptr;EffectNative originalLocalEffect=nullptr;
GridNative originalGridLocal=nullptr;ItemNative originalMapItem=nullptr;
VectorNative originalEntityPosition=nullptr,originalTransformPosition=nullptr;
std::array<void*,size_t(Id::Count)> methods{};
std::unique_ptr<Router> router;
thread_local Object reentry[size_t(Consumer::Count)]{};
bool installed=false;std::string lastError;

#ifndef MIXED_HEIGHT_CALLER_ADDRESS
#define MIXED_HEIGHT_CALLER_ADDRESS() __builtin_return_address(0)
#endif
uintptr_t caller_rva(void* address)noexcept{
    const auto value=reinterpret_cast<uintptr_t>(address),base=reinterpret_cast<uintptr_t>(runtime);
    return value>=base?value-base:0; // Only exact pinned call-site RVAs match.
}
void* resolve(const Binding& spec){
    auto klass=cls(spec.owner);if(!klass)throw std::runtime_error("combat-height declaring class unavailable");
    void* iterator=nullptr,*selected=nullptr;
    while(auto candidate=api.il2cpp_class_get_methods(klass,&iterator))if(api.il2cpp_method_get_token(candidate)==spec.token){selected=candidate;break;}
    if(!selected||std::strcmp(api.il2cpp_method_get_name(selected),spec.name)||api.il2cpp_method_get_param_count(selected)!=spec.count||api.il2cpp_method_is_generic(selected))throw std::runtime_error("combat-height exact class/token/name/arity differs");
    uint32_t implementation=0;
    if(bool(api.il2cpp_method_get_flags(selected,&implementation)&16)!=spec.isStatic||!matches_type(api.il2cpp_method_get_return_type(selected),spec.result))throw std::runtime_error("combat-height return/static declaration differs");
    for(uint32_t i=0;i<spec.count;++i)if(!matches_type(api.il2cpp_method_get_param(selected,i),spec.parameters[i]))throw std::runtime_error("combat-height parameter declaration differs");
    void* target=nullptr;if(!::cn::read(selected,target)||!target||reinterpret_cast<uintptr_t>(target)-reinterpret_cast<uintptr_t>(runtime)!=spec.rva)throw std::runtime_error("combat-height target differs from pinned CN RVA");
    MEMORY_BASIC_INFORMATION page{};
    if(!VirtualQuery(target,&page,sizeof(page))||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||page.AllocationBase!=runtime||!(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||(page.Protect&PAGE_GUARD))throw std::runtime_error("combat-height target outside fixed executable GameAssembly");
    return selected;
}
template<class Function>void install_exact(Id id,Function replacement,Function& original){
    void* target=nullptr,*handler=nullptr,*trampoline=nullptr;
    if(!::cn::read(methods.at(size_t(id)),target)||!target)throw std::runtime_error("combat-height hook target missing");
    std::memcpy(&handler,&replacement,sizeof(handler));
    if(MH_CreateHook(target,handler,&trampoline)!=MH_OK)throw std::runtime_error("combat-height hook ownership collision");
    std::memcpy(&original,&trampoline,sizeof(original));
    if(MH_EnableHook(target)!=MH_OK){MH_RemoveHook(target);throw std::runtime_error("combat-height hook enable failed");}
}
bool take_reentry(Consumer kind,Object self)noexcept{
    auto& pending=reentry[size_t(kind)];if(pending!=self||!self)return false;
    pending=nullptr;return true; // Only the one runtime-invoke reentry bypasses.
}
bool protected_call(Consumer kind,Id id,Object self,Object entity,void** args){
    if(!installed||!router||closing.load()||unity_ui_thread.load()!=GetCurrentThreadId()||!can_read())return false;
    bool selected=false,abort=false;
    {
        RuntimeLease lease(runtime_gate);if(!lease)return false;
        try{
            nb_main_thread();Capture scope;
            if(router->scope(scope,kind,self,entity)){
                selected=true;TemporaryRoots roots;roots.retain(self);if(entity)roots.retain(entity);
                auto previous=Router::active;auto oldReentry=reentry[size_t(kind)];
                Router::active=&scope;reentry[size_t(kind)]=self;
                Object managedError=nullptr;
                // The inner detour calls the complete original trampoline with
                // no C++ cleanup objects. All managed exceptions are caught by
                // this runtime boundary before any outer RAII is destroyed.
                try{api.il2cpp_runtime_invoke(methods.at(size_t(id)),self,args,&managedError);}
                catch(...){Router::active=previous;reentry[size_t(kind)]=oldReentry;throw;}
                Router::active=previous;reentry[size_t(kind)]=oldReentry;
                if(managedError)throw std::runtime_error("combat-height original consumer raised managed error");
            }
        }catch(const std::exception& ex){lastError=ex.what();try{router->callbacks().failed(lastError);}catch(...){}abort=true;}
        catch(...){lastError="combat-height unexpected native host failure";try{router->callbacks().failed(lastError);}catch(...){}abort=true;}
        lease.release();
    }
    // No lease, GC-root owner, Capture, std::string local or other cleanup
    // object remains live when a pre-rooted managed failure is raised.
    if(abort){router->callbacks().abortManaged();std::abort();}
    return selected;
}
void farthest(Object self,Object start,Object target,const void* info){
    if(take_reentry(Consumer::Farthest,self)){originalFarthest(self,start,target,info);return;}
    void* args[]={start,target};if(!protected_call(Consumer::Farthest,Id::Farthest,self,nullptr,args))originalFarthest(self,start,target,info);
}
void nearest(Object self,Object start,Object target,const void* info){
    if(take_reentry(Consumer::Nearest,self)){originalNearest(self,start,target,info);return;}
    void* args[]={start,target};if(!protected_call(Consumer::Nearest,Id::Nearest,self,nullptr,args))originalNearest(self,start,target,info);
}
void sector(Object self,Object start,Object target,const void* info){
    if(take_reentry(Consumer::Sector,self)){originalSector(self,start,target,info);return;}
    void* args[]={start,target};if(!protected_call(Consumer::Sector,Id::Sector,self,nullptr,args))originalSector(self,start,target,info);
}
void entity_effect(Object self,Object entity,const void* info){
    if(take_reentry(Consumer::EntityEffect,self)){originalEntityEffect(self,entity,info);return;}
    void* args[]={entity};if(!protected_call(Consumer::EntityEffect,Id::EntityEffect,self,entity,args))originalEntityEffect(self,entity,info);
}
void local_effect(Object self,uint8_t overwrite,const void* info){
    if(take_reentry(Consumer::LocalEffect,self)){originalLocalEffect(self,overwrite,info);return;}
    void* args[]={&overwrite};if(!protected_call(Consumer::LocalEffect,Id::LocalEffect,self,nullptr,args))originalLocalEffect(self,overwrite,info);
}
// These leaf wrappers hold POD only. A managed getter failure escapes into the
// complete consumer runtime-invoke boundary above; no TLS/RAII cleanup relies
// on foreign exception unwinding through a MinGW destructor.
uint64_t grid_local(const Position* self,const void* info){
    const auto caller=caller_rva(MIXED_HEIGHT_CALLER_ADDRESS());auto result=originalGridLocal(self,info);
    if(self){V2 value;std::memcpy(&value,&result,sizeof(value));Router::local(*self,value,caller);}return result;
}
Object map_item(Object self,Position pos,const void* info){
    const auto caller=caller_rva(MIXED_HEIGHT_CALLER_ADDRESS());auto result=originalMapItem(self,pos,info);Router::tile(self,pos,result,caller);return result;
}
V3* entity_position(V3* out,Object self,const void* info){
    const auto caller=caller_rva(MIXED_HEIGHT_CALLER_ADDRESS());auto result=originalEntityPosition(out,self,info);if(result)Router::entity(self,*result,caller);return result;
}
V3* transform_position(V3* out,Object self,const void* info){
    const auto caller=caller_rva(MIXED_HEIGHT_CALLER_ADDRESS());auto result=originalTransformPosition(out,self,info);if(result)Router::transform(self,*result,caller);return result;
}
Position from_vector(V2 value){
    TemporaryRoots roots;void* args[]={&value};auto boxed=roots.retain(::cn::invoke(methods.at(size_t(Id::FromVector)),nullptr,args));
    Position result;if(!boxed||!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(boxed)),"Torappu.GridPosition")||!::cn::read(unbox(boxed),result))throw std::runtime_error("combat-height native vector conversion failed");return result;
}
void install(Host host){
    nb_main_thread();if(installed||router)throw std::runtime_error("combat-height already installed");
    for(const auto& spec:BINDINGS)methods.at(size_t(spec.id))=resolve(spec);
    for(const auto& type:std::array<std::pair<const char*,int32_t>,3>{{{"Torappu.GridPosition",8},{"UnityEngine.Vector2",8},{"UnityEngine.Vector3",12}}}){
        auto klass=cls(type.first);uint32_t alignment=0;if(!klass||!api.il2cpp_class_is_valuetype(klass)||api.il2cpp_class_value_size(klass,&alignment)!=type.second)throw std::runtime_error("combat-height value ABI changed");
    }
    host.fromVector=from_vector;router=std::make_unique<Router>(std::move(host));
    // All ten declarations/ABIs pass before the first hook is created.
    install_exact(Id::Farthest,farthest,originalFarthest);install_exact(Id::Nearest,nearest,originalNearest);install_exact(Id::Sector,sector,originalSector);
    install_exact(Id::EntityEffect,entity_effect,originalEntityEffect);install_exact(Id::LocalEffect,local_effect,originalLocalEffect);
    install_exact(Id::GridLocal,grid_local,originalGridLocal);install_exact(Id::MapItem,map_item,originalMapItem);
    install_exact(Id::EntityPosition,entity_position,originalEntityPosition);install_exact(Id::TransformPosition,transform_position,originalTransformPosition);
    installed=true;
}
bool ready(){return installed&&router;}
bool try_route(Object map,int32_t type,uintptr_t caller,float& result){return ready()&&router->try_route(map,type,caller,result);}
}
