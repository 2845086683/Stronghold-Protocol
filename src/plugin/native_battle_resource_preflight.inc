// Included inside namespace cn after native battle JSON/model helpers and before
// native_combat.inc. The policy header is included outside namespace cn.
#pragma once

std::recursive_mutex nbr_lock;
std::map<std::string,uint32_t> nbr_prefab_roots;
std::atomic<unsigned> nbr_attempts{0},nbr_verified_paths{0},nbr_failures{0};
std::atomic<bool> nbr_passed{false};
std::string nbr_last_path,nbr_last_error;
int32_t nbr_last_round=0;
unsigned nbr_required_paths=0;

void native_battle_resources_reset(){
    std::lock_guard<std::recursive_mutex> lock(nbr_lock);
    for(const auto& entry:nbr_prefab_roots)hold_free(entry.second);
    nbr_prefab_roots.clear();nbr_passed=false;nbr_last_path.clear();nbr_last_error.clear();nbr_last_round=0;nbr_required_paths=0;
}
void native_battle_resources_preflight(){
    std::lock_guard<std::recursive_mutex> lock(nbr_lock);
    ++nbr_attempts;nbr_passed=false;nbr_last_error.clear();nbr_last_path="<scope>";
    try{
        nb_main_thread();
        if(!nb_prepared.load()||!nb_started.load()||!nb_loaded.load()||!private_battle_resources.load())
            throw std::runtime_error("globalbuff preflight requires an active private battle resource scope");
        if(nb_string(nb_state,"phase")!="preparation")throw std::runtime_error("globalbuff preflight requires preparation");
        nbr_last_round=nb_number(nb_state,"round");nbr_last_path="<manifest>";
        // This complete reachability manifest is generated independently of
        // CN_ASSETS. Missing aliases must fail instead of disappearing here.
        const auto paths=cnbattleresources::required_global_buffs(CN_BATTLE_REQUIRED_GLOBALBUFFS);
        nbr_required_paths=unsigned(paths.size());
        TemporaryRoots roots;
        for(const auto& path:paths){
            nbr_last_path=path;
            auto id=roots.retain(string_new(path.c_str()));void* args[]={id};
            uint8_t exists=0;
            if(!read(unbox(roots.retain(invoke(bound_methods.at(CN_METHOD_exists.token),nullptr,args))),exists)||!exists)
                throw std::runtime_error("ResourceManager.CheckExists returned false/unreadable");
            auto prefab=roots.retain(invoke(bound_methods.at(CN_METHOD_load.token),nullptr,args));
            if(!prefab)throw std::runtime_error("ResourceManager.Load returned null");
            if(!matches_type(api.il2cpp_class_get_type(api.il2cpp_object_get_class(prefab)),"UnityEngine.GameObject"))
                throw std::runtime_error("ResourceManager.Load returned a non-GameObject");
            if(!bundle_alive(prefab))throw std::runtime_error("ResourceManager.Load returned a destroyed Unity object");
            auto retained=hold_new(prefab,false);
            if(!retained)throw std::runtime_error("globalbuff prefab GC retention failed");
            auto previous=nbr_prefab_roots.find(path);
            if(previous!=nbr_prefab_roots.end())hold_free(previous->second);
            nbr_prefab_roots[path]=retained;++nbr_verified_paths;
        }
        nbr_passed=true;nbr_last_path.clear();
    }catch(const std::exception& error){
        ++nbr_failures;nbr_last_error="battle-globalbuff-preflight ["+nbr_last_path+"]: "+error.what();
        // Unlike overlay registration errors, this records an unmapped path
        // too. Propagate to the Ready guard; no authoritative Ready is sent.
        native_asset_error(nbr_last_error);
        throw std::runtime_error(nbr_last_error);
    }
}
cnjson::Value native_battle_resources_status(bool details){
    std::unique_lock<std::recursive_mutex> lock(nbr_lock,std::try_to_lock);
    auto value=nb_object();value.object["snapshotBusy"]=nb_boolean(!lock.owns_lock());
    value.object["attempts"]=nb_integer(nbr_attempts.load());value.object["verifiedPaths"]=nb_integer(nbr_verified_paths.load());
    value.object["failures"]=nb_integer(nbr_failures.load());value.object["passed"]=nb_boolean(nbr_passed.load());
    if(details&&lock.owns_lock()){
        value.object["round"]=nb_integer(nbr_last_round);value.object["requiredPaths"]=nb_integer(nbr_required_paths);
        value.object["retainedPrefabs"]=nb_integer(nbr_prefab_roots.size());
        value.object["lastPath"]=json_text(nbr_last_path);value.object["lastError"]=json_text(nbr_last_error);
    }
    return value;
}
