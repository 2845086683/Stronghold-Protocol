// Precise private resource adapter. It does not hook or weaken CrypticConverter,
// VerifySign, RSA, or the native deserializer for any original game asset.
inline constexpr CN_Method NL_LOAD={"Torappu.Battle.LevelUtils","LoadLevel",0x60131d7,0xafee90ULL,1,true,"Torappu.LevelData",{"System.String"}};
inline constexpr CN_Method NL_PARSE={"Torappu.Battle.LevelUtils","ParseLevelData",0x60131da,0xaff000ULL,1,true,"Torappu.LevelData",{"UnityEngine.TextAsset"}};
inline constexpr CN_Method NL_BYTES={"UnityEngine.TextAsset","get_bytes",0x6000a23,0x6b70c20ULL,0,false,"System.Byte[]",{}};
inline constexpr CN_Method NL_BUFFER={"Google.FlatBuffers.ByteBuffer",".ctor",0x6000532,0x664c2d0ULL,2,false,"System.Void",{"System.Byte[]","System.Int32"}};
inline constexpr CN_Method NL_TABLE={"Google.FlatBuffers.Table",".ctor",0x60005c9,0x6657240ULL,2,false,"System.Void",{"System.Int32","Google.FlatBuffers.ByteBuffer"}};
inline constexpr CN_Method NL_UNPACK={"Torappu.FlatBuffers.FlatStoreUtil","UnpackDirectly",0x600a71c,0x3c3f460ULL,2,true,"System.Object",{"System.Type","Google.FlatBuffers.Table"}};
using NativeLevelCall=void*(*)(void*,const void*);
NativeLevelCall nl_old_load=nullptr,nl_old_parse=nullptr;
thread_local const CN_PrivateLevel* nl_loading=nullptr;
std::atomic<unsigned> nl_converted{0},nl_failed{0};
std::atomic<bool> nl_ready{false};
std::mutex nl_error_lock;
std::string nl_error;

bool nl_scope(){
    if(native_multiplayer_level_scope())return true;
    if(!cnlevels::scope(native_battle_launch_session_prepared.load(),native_battle_session_active(),native_battle_launch_in_flight.load(),
        can_read()&&unity_ui_thread.load()==GetCurrentThreadId(),closing.load()))return false;
    // _DoBattleStart and _ProcessStartBattleSucceed both read the level before
    // SetInParams publishes this battle's identity. This TLS is entered only
    // after the exact act2 LOCAL input and private session have been validated.
    if(native_battle_utility_depth>0)return true;
    try{return cnlaunch::target_activity(native_battle_current_activity());}catch(...){return false;}
}
std::string nl_scope_failure_reason(){
    if(closing.load())return "runtime closing";
    if(!native_battle_launch_session_prepared.load())return "private launch session not prepared";
    if(!native_battle_session_active())return "private data session not active";
    if(!native_battle_launch_in_flight.load())return "private launch not in flight";
    if(!can_read())return "private resource access not available";
    if(unity_ui_thread.load()!=GetCurrentThreadId())return "private level load outside observed Unity UI thread";
    if(native_battle_utility_depth>0)return "private level scope changed during validated utility launch";
    try{
        const auto activity=native_battle_current_activity();
        return activity.empty()?"BattleInOut activity identity missing":("BattleInOut activity outside act2: "+activity);
    }catch(const std::exception& ex){return std::string("BattleInOut activity unavailable: ")+ex.what();}
    catch(...){return "BattleInOut activity unavailable";}
}
std::string native_level_last_error(){std::lock_guard<std::mutex> lock(nl_error_lock);return nl_error;}
void nl_record_failure(const char* step,const std::exception& ex){
    const char* label=step&&*step?step:"private-level";
    {std::lock_guard<std::mutex> lock(nl_error_lock);nl_error=std::string(label)+": "+ex.what();}
    // report() reads native_level_last_error() while holding report_lock.
    // Release our error lock before nb_error() enters that reporting path.
    ++nl_failed;nb_error(label,ex);
}
std::string nl_sha256(const std::vector<uint8_t>& data){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE state=nullptr;DWORD size=0,used=0;
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&
        BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),sizeof(size),&used,0)>=0;
    std::vector<uint8_t> storage(ok?size:0);if(ok)ok=BCryptCreateHash(algorithm,&state,storage.data(),size,nullptr,0,0)>=0;
    uint8_t digest[32]{};if(ok)ok=BCryptHashData(state,const_cast<PUCHAR>(data.data()),ULONG(data.size()),0)>=0&&BCryptFinishHash(state,digest,sizeof(digest),0)>=0;
    if(state)BCryptDestroyHash(state);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);
    if(!ok)throw std::runtime_error("private level SHA256 unavailable");
    const char* hex="0123456789abcdef";std::string result;for(auto v:digest){result+=hex[v>>4];result+=hex[v&15];}return result;
}
void* nl_load(void* level,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;
    const CN_PrivateLevel *requested=nullptr,*target=nullptr;
    const auto key=cnlevels::key(text(level));for(const auto& allowed:CN_PRIVATE_LEVELS)if(key==allowed.levelId){requested=&allowed;break;}
    // An already prepared session alone is not a battle launch. Preserve
    // ordinary native reads until the validated utility call or its later
    // private scene has actually begun loading.
    const bool private_attempt=native_battle_utility_depth>0||native_battle_launch_in_flight.load();
    if(requested&&private_attempt){
        try{
            if(!nl_ready.load())throw std::runtime_error("private level adapter preflight not ready");
            if(!nl_scope())throw std::runtime_error(nl_scope_failure_reason());
            target=requested;
        }catch(const std::exception& ex){nl_record_failure("private-level-load-scope",ex);return nullptr;}
    }
    // Preserve original LoadLevel bookkeeping, including LevelData.levelId.
    struct Restore{const CN_PrivateLevel* prior;explicit Restore(const CN_PrivateLevel* next):prior(nl_loading){nl_loading=next;}~Restore(){nl_loading=prior;}} restore(target);
    return nl_old_load(level,method_info);
}
void* nl_parse(void* text_asset,const void* method_info){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;
    if(!nl_loading){lease.release();return nl_old_parse(text_asset,method_info);}
    try{
        if(!nl_ready.load()||!nl_scope()||!text_asset)throw std::runtime_error("private level parse outside exact loading scope");
        PrivateDataScope private_scope(true);TemporaryRoots roots;roots.retain(text_asset);
        auto name=text(invoke(method(api.il2cpp_object_get_class(text_asset),"get_name",0),text_asset));
        if(!cnlevels::asset_name_matches(name,nl_loading->assetName))throw std::runtime_error(std::string("private level TextAsset name mismatch: expected=")+nl_loading->assetName+" actual="+name+" level="+nl_loading->levelId);
        auto bytes=roots.retain(invoke(bound_methods.at(NL_BYTES.token),text_asset));
        if(!bytes)throw std::runtime_error("private level bytes missing");
        auto type=api.il2cpp_object_get_class(bytes);auto count=api.il2cpp_array_length(bytes);auto header=api.il2cpp_array_object_header_size();
        if(api.il2cpp_class_get_rank(type)!=1||api.il2cpp_class_get_element_class(type)!=cls("System.Byte")||api.il2cpp_array_element_size(type)!=1||
            count!=nl_loading->byteCount||count<136||count>8*1024*1024||header<24||header>128)throw std::runtime_error("private level byte array layout mismatch");
        std::vector<uint8_t> content(size_t(count),uint8_t{});SIZE_T copied=0;
        if(!ReadProcessMemory(GetCurrentProcess(),static_cast<const char*>(bytes)+header,content.data(),content.size(),&copied)||copied!=content.size())throw std::runtime_error("private level bytes unreadable");
        if(nl_sha256(content)!=nl_loading->sha256)throw std::runtime_error("private level content differs from pinned adaptation");
        int32_t position=128,root=cnlevels::root_position(content,128);
        auto buffer=roots.retain(object_new(cls("Google.FlatBuffers.ByteBuffer")));void* buffer_args[]={bytes,&position};invoke(bound_methods.at(NL_BUFFER.token),buffer,buffer_args);
        auto table=roots.retain(object_new(cls("Google.FlatBuffers.Table")));void* table_args[]={&root,buffer};invoke(bound_methods.at(NL_TABLE.token),unbox(table),table_args);
        auto level_type=roots.retain(reflection_type(api.il2cpp_class_get_type(cls("Torappu.LevelData"))));void* unpack_args[]={level_type,unbox(table)};
        auto result=roots.retain(invoke(bound_methods.at(NL_UNPACK.token),nullptr,unpack_args));
        if(!result||api.il2cpp_object_get_class(result)!=cls("Torappu.LevelData"))throw std::runtime_error("private level native conversion returned wrong class");
        ++nl_converted;report("private-native-level-schema-converted");return result;
    }catch(const std::exception& ex){nl_record_failure("private-level-schema",ex);return nullptr;}
}
std::string native_level_compat_status(){return "{\"preflightReady\":"+std::string(nl_ready.load()?"true":"false")+",\"converted\":"+std::to_string(nl_converted.load())+",\"failures\":"+std::to_string(nl_failed.load())+",\"lastError\":"+quote(native_level_last_error())+",\"originalSignatureValidationUnchanged\":true}";}
void native_level_compat_setup(){
    hook(NL_LOAD,nl_load,nl_old_load);hook(NL_PARSE,nl_parse,nl_old_parse);
    for(const auto* spec:{&NL_BYTES,&NL_BUFFER,&NL_TABLE,&NL_UNPACK}){
        // Tokens are local to an IL2CPP image. This small adapter uses additional
        // assemblies; refuse a collision rather than borrowing another method.
        if(bound_methods.count(spec->token))throw std::runtime_error("private level method token collision");
        bind_method(*spec);
    }
    auto table=cls("Google.FlatBuffers.Table");uint32_t alignment=0;
    if(!table||!api.il2cpp_class_is_valuetype(table)||api.il2cpp_class_value_size(table,&alignment)!=16)throw std::runtime_error("private level Table value ABI mismatch");
    for(const auto& item:std::array<std::pair<const char*,const char*>,2>{{{"<bb_pos>k__BackingField","System.Int32"},{"<bb>k__BackingField","Google.FlatBuffers.ByteBuffer"}}}){
        auto field=profile_field(table,item.first);const auto expected=std::strcmp(item.first,"<bb_pos>k__BackingField")==0?16u:24u;
        if((api.il2cpp_field_get_flags(field)&16)||api.il2cpp_field_get_offset(field)!=expected||!matches_type(api.il2cpp_field_get_type(field),item.second))throw std::runtime_error("private level Table fields mismatch");
    }
    if(api.il2cpp_class_is_valuetype(cls("Torappu.LevelData")))throw std::runtime_error("private LevelData reference ABI mismatch");
    nl_ready.store(true);
}
