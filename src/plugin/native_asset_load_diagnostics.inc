// Include inside namespace cn after ensure_bundles, before load_asset.
// A failed load is described without another LoadAsset, an asset-name guess,
// or an alternate provider. CN strips Contains/GetAllAssetNames entirely.
#pragma once

std::string nad_type_name(void* type){
    if(!type)return "<null>";
    auto name=api.il2cpp_type_get_name(type);std::string result=name?name:"<unnamed>";
    if(name)api.il2cpp_free(name);return result;
}
std::string nad_object_class(void* object){
    return object?nad_type_name(api.il2cpp_class_get_type(api.il2cpp_object_get_class(object))):"<null>";
}
std::string native_asset_load_failure(const CN_Asset& asset,void* id,void* bundle,uint32_t handle){
    // Values are JSON quoted so bracketed paths and managed exception text
    // cannot obscure the boundary between the requested and observed fields.
    std::string result="overlay LoadAsset returned null {\"request\":"+quote(text(id))+
        ",\"alias\":"+quote(asset.alias)+",\"asset\":"+quote(asset.asset)+
        ",\"bundle\":"+quote(asset.bundle)+",\"requestedType\":\"UnityEngine.Object\""+
        ",\"cacheHandle\":"+std::to_string(handle)+",\"bundleClass\":"+quote(nad_object_class(bundle));
    const CN_Bundle* source=nullptr;
    for(const auto& candidate:CN_BUNDLES)if(std::strcmp(candidate.name,asset.bundle)==0){source=&candidate;break;}
    if(source){
        result+=",\"sourceSha256\":"+quote(source->hash);
        try{const auto resolved=native_bundle_path(*source);result+=",\"sourcePath\":"+quote(utf8(resolved));}
        catch(const std::exception& ex){result+=",\"sourceResolutionError\":"+quote(ex.what());}
    }
    else result+=",\"profileBundleMissing\":true";
    try{
        if(!bundle)throw std::runtime_error("cached bundle GC target is null");
        const bool live=bundle_alive(bundle);result+=std::string(",\"bundleAlive\":")+(live?"true":"false");
        if(live){
            TemporaryRoots roots;auto bundle_class=api.il2cpp_object_get_class(bundle);
            const auto name=text(roots.retain(invoke(method(bundle_class,"get_name",0),bundle)));
            result+=",\"nativeBundleName\":"+quote(name)+",\"bundleNameMatches\":"+(name==asset.bundle?"true":"false");
            auto getter=method(cls("UnityEngine.AssetBundle"),"get_isStreamedSceneAssetBundle",0);
            if(!getter||api.il2cpp_method_get_token(getter)!=0x6000006||
               api.il2cpp_method_get_param_count(getter)!=0||api.il2cpp_method_is_generic(getter)||
               nad_type_name(api.il2cpp_method_get_return_type(getter))!="System.Boolean")
                throw std::runtime_error("CN retained AssetBundle scene-kind getter contract mismatch");
            uint8_t scene=0;
            if(!read(unbox(roots.retain(invoke(getter,bundle))),scene))throw std::runtime_error("native scene-kind getter returned no Boolean");
            result+=std::string(",\"isStreamedSceneAssetBundle\":")+(scene?"true":"false");
        }
    }catch(const std::exception& error){result+=",\"diagnosticError\":"+quote(error.what());}
    // The retained API cannot separate a missing container key from failed
    // MonoScript deserialization. State that uncertainty instead of asserting
    // that the asset is absent solely because LoadAsset returned null.
    return result+",\"assetIndexInspection\":\"Contains/GetAllAssetNames stripped in pinned CN\"}";
}
