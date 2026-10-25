// Include inside namespace cn, after dictionary_find/array_index_method and
// before register_native_dependencies. Root integration points are documented
// in research/native-client/resource-owner-audit/integration.json.
#include "native_resource_owner_policy.h"

std::atomic<unsigned> nro_info_hits{0},nro_native_loads{0},nro_reuses{0},nro_adoptions{0},nro_private_loads{0};
std::map<std::string,std::string> nro_current_names;
std::atomic<unsigned> nro_hot_update_resolutions{0};

// ResourceManager.get_instance (Torappu.Common.dll token 0x6000bf2) returns
// IResourceManager. ResourceOptions can select LocalResourceManager, so validate
// the concrete provider before reading AB-only fields. Prefer the observed hook
// self when available; never reinterpret the interface as an AB provider.
void* nro_require_ab_provider(void* provider){
    auto expected=cls("Torappu.Resource.AB.ABResourceManager");
    if(!provider||!expected||!api.il2cpp_class_is_assignable_from(expected,api.il2cpp_object_get_class(provider)))
        throw std::runtime_error("active resource provider is not ABResourceManager");
    auto manager=field_object(provider,"m_manager");
    auto manager_class=cls("Torappu.Resource.AB.ABResourceManager+BundleManager");
    if(!manager||!manager_class||!api.il2cpp_class_is_assignable_from(manager_class,api.il2cpp_object_get_class(manager))||
       field_object(manager,"m_resManager")!=provider)
        throw std::runtime_error("ABResourceManager manager ownership mismatch");
    return provider;
}
void nro_observe_provider(void* provider){protect("native-resource-provider",nro_require_ab_provider(provider));}
void* nro_active_provider(TemporaryRoots& roots){
    auto observed=held.find("native-resource-provider");
    if(observed!=held.end())return nro_require_ab_provider(roots.retain(hold_get(observed->second)));
    return nro_require_ab_provider(roots.retain(invoke(method(cls("Torappu.Resource.ResourceManager"),"get_instance",0),nullptr)));
}

std::string nro_cn_data_root(){
    wchar_t executable[32768]{};auto length=GetModuleFileNameW(nullptr,executable,32768);
    if(!length||length>=32768)throw std::runtime_error("resource owner executable unavailable");
    auto path=utf8(executable);auto slash=path.find_last_of("/\\");
    if(slash==std::string::npos)throw std::runtime_error("resource owner executable directory unavailable");
    return path.substr(0,slash)+"/Arknights_Data";
}
cnresourceowner::IndexNames nro_index_names(const CN_Bundle& source){
    const auto resolved=native_bundle_path(source);
    return cnresourceowner::names(source.name,utf8(resolved),nro_cn_data_root());
}
bool nro_live_holder(void* holder){
    if(!holder)return false;
    uint8_t destroyed=1;auto f=profile_field(api.il2cpp_object_get_class(holder),"<isDestroyed>k__BackingField");
    api.il2cpp_field_get_value(holder,f,&destroyed);
    return !destroyed&&bundle_alive(field_object(holder,"<ab>k__BackingField"));
}
void* nro_resolve_info(void* provider,const CN_Bundle& source,TemporaryRoots& roots){
    auto keys=nro_index_names(source);if(!keys.installed_cn)return nullptr;
    auto manager=field_object(provider,"m_manager"),infos=field_object(manager,"m_bundleNameToBundleInfoMap");
    if(!manager||!infos)throw std::runtime_error("native bundle index unavailable");
    auto relocated=nro_current_names.find(source.name);
    if(relocated!=nro_current_names.end())keys.candidates.insert(keys.candidates.begin(),relocated->second);
    for(const auto& candidate:keys.candidates){
        auto key=roots.retain(string_new(candidate.c_str()));
        auto info=dictionary_find(infos,key);if(!info)continue;
        auto canonical=text(field_object(info,"name"));
        if(canonical.empty()||!cnresourceowner::safe_relative(cnresourceowner::normalized(canonical)))
            throw std::runtime_error("native bundle canonical name invalid");
        if(!cnhotresources::matches(source.name,canonical))throw std::runtime_error(std::string("native resource CAB identity changed: ")+source.name);
        ++nro_info_hits;return info;
    }
    // Anonymous filenames are content hashes and may disappear on any update.
    // Search ONLY keys in the live native index and require a unique certified
    // serialized-file identity. Never union with an obsolete disk index.
    if(std::any_of(keys.candidates.begin(),keys.candidates.end(),[](const std::string& key){return cnhotresources::anonymous(key);})){ 
        auto collection=roots.retain(invoke(method(api.il2cpp_object_get_class(infos),"get_Keys",0),infos));
        auto iter=roots.retain(invoke(method(api.il2cpp_object_get_class(collection),"GetEnumerator",0),instance_argument(collection)));
        if(!iter)throw std::runtime_error("native bundle keys unavailable");
        auto type=api.il2cpp_object_get_class(iter);auto move=method(type,"MoveNext",0),current=method(type,"get_Current",0);
        void* match=nullptr;std::string matched;
        for(unsigned limit=0;;++limit){
            uint8_t yes=0;if(!read(unbox(invoke(move,instance_argument(iter))),yes))throw std::runtime_error("native bundle key iterator invalid");
            if(!yes)break;if(limit>=30000)throw std::runtime_error("native bundle key limit");
            auto key=roots.retain(invoke(current,instance_argument(iter)));auto name=text(key);if(!cnhotresources::anonymous(name))continue;
            bool same=false;try{same=cnhotresources::matches(source.name,name);}catch(const std::exception&){continue;}
            if(!same)continue;auto info=dictionary_find(infos,key);auto canonical=text(field_object(info,"name"));
            if(!info||canonical!=name)throw std::runtime_error("native anonymous index identity mismatch");
            if(match&&matched!=canonical)throw std::runtime_error("ambiguous native resource CAB identity");
            match=roots.retain(info);matched=canonical;
        }
        if(match){nro_current_names[source.name]=matched;++nro_hot_update_resolutions;++nro_info_hits;return match;}
    }
    std::string location;
    try{location=utf8(native_bundle_path(source));}
    catch(const std::exception& ex){location=std::string("unavailable: ")+ex.what();}
    throw std::runtime_error(std::string("installed CN bundle absent from native index: ")+source.name+" ["+location+"]");
}
void nro_require_bundle_identity(const CN_Bundle& source,void* bundle){
    if(!bundle_alive(bundle)||text(invoke(method(api.il2cpp_object_get_class(bundle),"get_name",0),bundle))!=source.name)
        throw std::runtime_error(std::string("native logical bundle name changed: ")+source.name);
}
void nro_cache_bundle(const CN_Bundle& source,void* bundle){
    if(!bundle_alive(bundle))throw std::runtime_error("native owner bundle is not live");
    auto previous=bundles.find(source.name);
    if(previous!=bundles.end()){
        if(hold_get(previous->second)==bundle)return;
        hold_free(previous->second);bundles.erase(previous);
    }
    auto root=hold_new(bundle,false);if(!root)throw std::runtime_error("native owner bundle root unavailable");
    bundles[source.name]=root;
}
void nro_pin_holder(void* holder,const std::string& canonical){
    const auto key="native-resource-owner:"+canonical;auto previous=held.find(key);
    if(previous!=held.end()&&hold_get(previous->second)==holder)return;
    int32_t retain=1;void* args[]={&retain};invoke(method(api.il2cpp_object_get_class(holder),"AddAssetRef",1),holder,args);
    protect(key,holder);
}
// Replace only ensure_bundles' final raw LoadFromFile call with this function.
// Its preceding live Unity-object enumeration remains useful for boot bundles.
void register_profile_native_dependencies(void* provider,TemporaryRoots& roots);
void* load_profile_bundle_with_native_owner(const CN_Bundle& source,TemporaryRoots& roots){
    const auto resolved=native_bundle_path(source);
    auto names=cnresourceowner::names(source.name,utf8(resolved),nro_cn_data_root());
    if(!names.installed_cn){
        void* args[]={roots.retain(string_new(utf8(resolved).c_str()))};
        auto result=roots.retain(invoke(method(cls("UnityEngine.AssetBundle"),"LoadFromFile",1),nullptr,args));
        ++nro_private_loads;return result;
    }
    auto provider=nro_active_provider(roots);auto manager=field_object(provider,"m_manager");
    // ensure_bundles has already enumerated live Unity CABs. Give these their
    // existing native identities before GetOrLoadBundle recursively loads any
    // dependency, or a boot CAB can still be loaded a second time.
    register_profile_native_dependencies(provider,roots);
    auto info=nro_resolve_info(provider,source,roots);
    auto canonical=roots.retain(field_object(info,"name"));
    auto holder=dictionary_find(field_object(manager,"m_activeBundlesMap"),canonical);
    if(nro_live_holder(holder)){auto bundle=field_object(holder,"<ab>k__BackingField");nro_require_bundle_identity(source,bundle);nro_pin_holder(holder,text(canonical));++nro_reuses;return bundle;}
    void* args[]={info};holder=roots.retain(invoke(method(api.il2cpp_object_get_class(manager),"GetOrLoadBundle",1),manager,args));
    if(!nro_live_holder(holder))throw std::runtime_error(std::string("native index bundle load failed: ")+text(canonical));
    nro_require_bundle_identity(source,field_object(holder,"<ab>k__BackingField"));
    nro_pin_holder(holder,text(canonical));++nro_native_loads;return field_object(holder,"<ab>k__BackingField");
}
void register_profile_native_dependencies(void* provider,TemporaryRoots& roots){
    nro_observe_provider(provider);
    auto manager=field_object(provider,"m_manager"),active_map=field_object(manager,"m_activeBundlesMap"),groups=field_object(manager,"m_activeBundleSCCGroups");
    if(!manager||!active_map||!groups)throw std::runtime_error("native owner maps/SCC groups unavailable");
    int32_t size=0;if(!read(unbox(invoke(method(api.il2cpp_object_get_class(groups),"get_Length",0),groups)),size)||size<1||size>65534)
        throw std::runtime_error("native owner SCC group length invalid");
    auto getter=array_index_method(groups,"GetValue",1),setter=array_index_method(groups,"SetValue",2);
    auto element=api.il2cpp_class_get_element_class(api.il2cpp_object_get_class(groups));
    for(const auto& source:CN_BUNDLES){
        if(!overlay_bundle_allowed(source)||!bundles.count(source.name))continue;
        if(!nro_index_names(source).installed_cn)continue; // Private roots have dedicated original registration.
        auto info=nro_resolve_info(provider,source,roots);
        auto canonical=roots.retain(field_object(info,"name"));auto canonical_text=text(canonical);
        if(auto holder=dictionary_find(active_map,canonical);nro_live_holder(holder)){
            // Prefer the native owner's object; never replace its BundleInfo,
            // holder, original SCC index, or accounting with a second owner.
            nro_require_bundle_identity(source,field_object(holder,"<ab>k__BackingField"));nro_cache_bundle(source,field_object(holder,"<ab>k__BackingField"));
            nro_pin_holder(holder,canonical_text);++nro_reuses;continue;
        }
        auto bundle=hold_get(bundles.at(source.name));if(!bundle_alive(bundle))throw std::runtime_error("unowned cached CN bundle was unloaded");
        nro_require_bundle_identity(source,bundle);
        int32_t index=-1;api.il2cpp_field_get_value(info,profile_field(api.il2cpp_object_get_class(info),"sccIndex"),&index);
        if(index<0||index>=size)throw std::runtime_error("native owner SCC index invalid");
        auto holder=roots.retain(object_new(cls("Torappu.Resource.AB.BundleHolder")));
        void* ctor_args[]={bundle,info};invoke(method(api.il2cpp_object_get_class(holder),".ctor",2),holder,ctor_args);
        auto option_field=profile_field(api.il2cpp_object_get_class(provider),"<options>k__BackingField");
        auto option_class=api.il2cpp_class_from_type(api.il2cpp_field_get_type(option_field));
        if(api.il2cpp_class_is_valuetype(option_class)){
            uint32_t alignment=0;auto bytes=api.il2cpp_class_value_size(option_class,&alignment);
            if(bytes<1||bytes>65536)throw std::runtime_error("native owner options layout invalid");
            std::vector<uint8_t> data(size_t(bytes),uint8_t{});api.il2cpp_field_get_value(provider,option_field,data.data());
            void* args[]={data.data()};invoke(method(api.il2cpp_object_get_class(holder),"LateInit",1),holder,args);
        }else if(auto options=field_object(provider,"<options>k__BackingField")){
            void* args[]={options};invoke(method(api.il2cpp_object_get_class(holder),"LateInit",1),holder,args);
        }
        void* get_args[]={&index};auto group=invoke(getter,groups,get_args);
        if(!group){group=roots.retain(object_new(element));invoke(method(element,".ctor",0),group);
            void* set_args[]={group,&index};invoke(setter,groups,set_args);}
        void* add_args[]={holder};invoke(method(element,"Add",1),group,add_args);
        dictionary_set(active_map,canonical,holder);nro_pin_holder(holder,canonical_text);++nro_adoptions;
    }
}
// Returns the original native BundleInfo for an installed CN profile root.
// Private workspace roots return null and keep their dedicated synthetic path.
// The read-only form supports the asset-registration fast path. The live form
// also verifies/cache-pins the one canonical holder after ensure_bundles.
void* native_profile_bundle_info(void* provider,const char* semantic,TemporaryRoots& roots,bool require_live=false){
    for(const auto& source:CN_BUNDLES){
        if(std::string(source.name)!=semantic)continue;
        if(!nro_index_names(source).installed_cn)return nullptr;
        nro_observe_provider(provider);
        auto info=roots.retain(nro_resolve_info(provider,source,roots));
        if(!require_live)return info;
        auto bundle=roots.retain(load_profile_bundle_with_native_owner(source,roots));
        nro_cache_bundle(source,bundle);
        auto canonical=roots.retain(field_object(info,"name"));
        auto holder=dictionary_find(field_object(field_object(provider,"m_manager"),"m_activeBundlesMap"),canonical);
        if(!nro_live_holder(holder)||field_object(holder,"<info>k__BackingField")!=info||
           field_object(holder,"<ab>k__BackingField")!=bundle)
            throw std::runtime_error("CN root canonical holder/info mismatch");
        return info;
    }
    throw std::runtime_error(std::string("profile root source missing: ")+semantic);
}
std::string native_resource_owner_status_json(){
    return "{\"infoResolutions\":"+std::to_string(nro_info_hits.load())+",\"nativeLoads\":"+std::to_string(nro_native_loads.load())+
        ",\"holderReuses\":"+std::to_string(nro_reuses.load())+",\"loadedBundleAdoptions\":"+std::to_string(nro_adoptions.load())+
        ",\"privateRawLoads\":"+std::to_string(nro_private_loads.load())+",\"hotUpdateResolutions\":"+std::to_string(nro_hot_update_resolutions.load())+",\"cnRawLoads\":0}";
}
