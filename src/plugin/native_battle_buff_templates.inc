// Private missing-template overlay. Never writes BuffDB/BuffTemplateDB caches.
// Include inside namespace cn before native_combat.inc; nb_hook is already defined.
#pragma once

using NBT_TryGet=bool(*)(void*,void**,const void*);
using NBT_WriteReference=void(*)(void*,void**,void*);
NBT_TryGet nbt_original_try_get=nullptr;
NBT_WriteReference nbt_write_reference=nullptr;
std::recursive_mutex nbt_lock;
std::set<std::string> nbt_extra_keys;
std::map<std::string,uint32_t> nbt_templates;
uint32_t nbt_holder_root=0;
std::atomic<bool> nbt_setup_ready{false},nbt_preflight_ready{false};
std::atomic<unsigned> nbt_preflights{0},nbt_overrides{0},nbt_forwarded{0},nbt_failures{0};
std::string nbt_last_key,nbt_last_error;
constexpr const char* NBT_HOLDER_PATH=CN_BATTLE_PRIVATE_BUFF_HOLDER;
bool native_multiplayer_battle_buff_templates_scope();

void nbt_record_failure(const std::string& key,const std::exception& error){
    std::lock_guard<std::recursive_mutex> lock(nbt_lock);++nbt_failures;nbt_preflight_ready=false;
    nbt_last_key=key;nbt_last_error="private-buff-template ["+key+"]: "+error.what();native_asset_error(nbt_last_error);
}
void native_battle_buff_templates_reset(){
    std::lock_guard<std::recursive_mutex> lock(nbt_lock);
    for(const auto& entry:nbt_templates)hold_free(entry.second);
    nbt_templates.clear();if(nbt_holder_root)hold_free(nbt_holder_root);nbt_holder_root=0;
    nbt_preflight_ready=false;nbt_last_key.clear();nbt_last_error.clear();
}
bool nbt_private_scope(){
    if(!private_battle_resources.load())return false;
    if(unity_ui_thread.load()!=GetCurrentThreadId())return false;
    auto dc=nb_centre();if(!dc||!act(field_object(dc,"<actId>k__BackingField")))return false;
    // MULTI owns another bridge and never sets the LOCAL session flags. The
    // same private holder supplies its missing templates to the native engine.
    return (nb_prepared.load()&&nb_started.load()&&nb_loaded.load())||native_multiplayer_battle_buff_templates_scope();
}
bool nbt_try_get(void* key,void** result,const void* method_info){
    RuntimeLease lease(runtime_gate);
    if(!lease||closing.load()){lease.release();return nbt_original_try_get(key,result,method_info);}
    std::string name;
    try{
        name=text(key);
        // Native keys, unknown keys and other activities retain exact original
        // arguments and behavior, even while the private scene is prepared.
        if(!nbt_extra_keys.count(name)||!nbt_private_scope()){
            ++nbt_forwarded;lease.release();return nbt_original_try_get(key,result,method_info);
        }
        std::lock_guard<std::recursive_mutex> lock(nbt_lock);
        if(!result||!nbt_preflight_ready.load()||!nbt_write_reference)throw std::runtime_error("private template preflight/output unavailable");
        auto found=nbt_templates.find(name);
        if(found==nbt_templates.end())throw std::runtime_error("required private template was not retained");
        auto value=hold_get(found->second);
        if(!value||!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(value)),"Torappu.Battle.BuffTemplate"))
            throw std::runtime_error("retained private template class/GC target invalid");
        // The native export stores the out reference and applies its write
        // barrier. CN RVA 49d410 ignores owner and forwards slot/value; this
        // supports stack or managed-field out parameters without guessing.
        nbt_write_reference(nullptr,result,value);++nbt_overrides;return true;
    }catch(const std::exception& error){
        if(result&&nbt_write_reference)nbt_write_reference(nullptr,result,nullptr);
        nbt_record_failure(name,error);return false;
    }
}
void nbt_preflight(bool multiplayer){
    std::lock_guard<std::recursive_mutex> lock(nbt_lock);++nbt_preflights;nbt_preflight_ready=false;
    std::map<std::string,uint32_t> staged;uint32_t holder_root=0;std::string current=NBT_HOLDER_PATH;
    try{
        nb_main_thread();
        if(!nbt_setup_ready.load()||!nbt_private_scope()||
           (multiplayer?!native_multiplayer_battle_buff_templates_scope():
            (!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||nb_string(nb_state,"phase")!="preparation")))
            throw std::runtime_error("private template preflight outside prepared activity/UI scope");
        if(nbt_extra_keys.empty()){nbt_preflight_ready=true;return;}
        TemporaryRoots roots;auto id=roots.retain(string_new(NBT_HOLDER_PATH));void* args[]={id};uint8_t exists=0;
        if(!read(unbox(roots.retain(invoke(bound_methods.at(CN_METHOD_exists.token),nullptr,args))),exists)||!exists)
            throw std::runtime_error("private holder ResourceManager.CheckExists failed");
        auto holder=roots.retain(invoke(bound_methods.at(CN_METHOD_load.token),nullptr,args));
        if(!holder||!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(holder)),"BuffTemplateHolder")||!bundle_alive(holder))
            throw std::runtime_error("private BuffTemplateHolder is null/wrong type/destroyed");
        auto list=roots.retain(field_object(holder,"_templates"));
        if(!list)throw std::runtime_error("private holder _templates list missing");
        auto list_class=api.il2cpp_object_get_class(list);int32_t count=0;
        if(!read(unbox(roots.retain(invoke(method(list_class,"get_Count",0),list))),count)||count<0||count>65536)
            throw std::runtime_error("private holder _templates count invalid");
        auto get_item=method(list_class,"get_Item",1);
        for(int32_t index=0;index<count;++index){
            void* item_args[]={&index};auto value=roots.retain(invoke(get_item,list,item_args));
            if(!value)throw std::runtime_error("private holder contains null template");
            auto key=text(field_object(value,"templateKey"));
            if(!nbt_extra_keys.count(key))continue; // CN originals are never indexed or overridden.
            current=key;
            if(!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(value)),"Torappu.Battle.BuffTemplate")||
               !field_object(value,"eventToActions"))throw std::runtime_error("private extra template type/actions missing");
            if(staged.count(key))throw std::runtime_error("private holder contains duplicate required key");
            auto handle=hold_new(value,false);if(!handle)throw std::runtime_error("private extra template GC retention failed");staged[key]=handle;
        }
        for(const auto& key:nbt_extra_keys)if(!staged.count(key)){current=key;throw std::runtime_error("private holder required key missing");}
        holder_root=hold_new(holder,false);if(!holder_root)throw std::runtime_error("private holder GC retention failed");
        for(const auto& entry:nbt_templates)hold_free(entry.second);
        nbt_templates.swap(staged);staged.clear();if(nbt_holder_root)hold_free(nbt_holder_root);nbt_holder_root=holder_root;holder_root=0;
        nbt_last_key.clear();nbt_last_error.clear();nbt_preflight_ready=true;
    }catch(const std::exception& error){
        for(const auto& entry:staged)hold_free(entry.second);
        if(holder_root)hold_free(holder_root);
        nbt_record_failure(current,error);throw std::runtime_error(nbt_last_error);
    }
}
void native_battle_buff_templates_preflight(){nbt_preflight(false);}
void native_multiplayer_battle_buff_templates_preflight(){
    // The original MULTI enemy Init runs before prepared units or automatic
    // deployment. Retain once; ordinary polls/rounds do not reload the holder.
    nb_main_thread();if(!nbt_private_scope()||!native_multiplayer_battle_buff_templates_scope())
        throw std::runtime_error("private MULTI template preflight outside current bridge ownership");
    if(!nbt_preflight_ready.load())nbt_preflight(true);
}
cnjson::Value native_battle_buff_templates_status(bool details){
    std::unique_lock<std::recursive_mutex> lock(nbt_lock,std::try_to_lock);auto value=nb_object();
    value.object["snapshotBusy"]=nb_boolean(!lock.owns_lock());value.object["setupReady"]=nb_boolean(nbt_setup_ready.load());
    value.object["preflightReady"]=nb_boolean(nbt_preflight_ready.load());value.object["preflights"]=nb_integer(nbt_preflights.load());
    value.object["overrides"]=nb_integer(nbt_overrides.load());value.object["forwarded"]=nb_integer(nbt_forwarded.load());value.object["failures"]=nb_integer(nbt_failures.load());
    if(details&&lock.owns_lock()){
        value.object["requiredKeys"]=nb_integer(nbt_extra_keys.size());value.object["retainedTemplates"]=nb_integer(nbt_templates.size());
        value.object["holderPath"]=json_text(NBT_HOLDER_PATH);value.object["lastKey"]=json_text(nbt_last_key);value.object["lastError"]=json_text(nbt_last_error);
    }return value;
}
void native_battle_buff_templates_setup(){
    if(nbt_setup_ready.load())return;
    if(!SO_LoadIl2CppExport(runtime,"il2cpp_gc_wbarrier_set_field",nbt_write_reference))
        throw std::runtime_error("private template managed-reference write barrier export missing");
    auto holder=cls("BuffTemplateHolder"),value=cls("Torappu.Battle.BuffTemplate");
    if(!holder||!value||api.il2cpp_class_is_valuetype(value)||
       !matches_type(api.il2cpp_field_get_type(profile_field(holder,"_templates")),"System.Collections.Generic.List`1<Torappu.Battle.BuffTemplate>")||
       !matches_type(api.il2cpp_field_get_type(profile_field(value,"templateKey")),"System.String")||
       !matches_type(api.il2cpp_field_get_type(profile_field(value,"eventToActions")),"Torappu.Battle.BuffTemplate+EventToActionMap"))
        throw std::runtime_error("private template native holder/value metadata mismatch");
    std::set<std::string> required;
    for(const auto* key:CN_BATTLE_EXTRA_BUFF_TEMPLATES){
        if(!key||!*key||std::strlen(key)>240)throw std::runtime_error("private template required-key manifest invalid");
        if(!required.insert(key).second)throw std::runtime_error("private template required-key manifest duplicated");
    }
    nb_hook(0x60131c3,nbt_try_get,nbt_original_try_get);nbt_extra_keys.swap(required);nbt_setup_ready=true;
}
