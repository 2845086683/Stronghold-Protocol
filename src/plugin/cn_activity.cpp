#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <winsock2.h>
#include <windows.h>
#include <winhttp.h>
#include <wincrypt.h>
#include <bcrypt.h>
#include <atomic>
#include <array>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <thread>
#include "native_json.h"
#include "native_multiplayer_transport_policy.h"
#include "native_multiplayer_dispatch_policy.h"
#include "native_multiplayer_boss_policy.h"
#include "native_multiplayer_map_headers.h"
#include "native_multiplayer_field_policy.h"
#include "native_multiplayer_stage_path.h"
#include "native_multiplayer_mode_runes.h"
#include "native_multiplayer_preparation_handoff_policy.h"
#include "native_multiplayer_own_start_policy.h"
#include "native_multiplayer_dead_battle_policy.h"
#include "native_multiplayer_help_counter_policy.h"
#include "runtime_gate.h"
#include "config_trace.h"
#include "native_battle_launch_policy.h"
#include "native_level_compat.h"
#include "native_backend_bootstrap_policy.h"
#include "native_private_liveness_policy.h"
#include "native_resource_owner_policy.h"
#include "native_private_identifier_policy.h"
#include "native_combat_policy.h"
#include "native_battle_timing.h"
#include "native_broadcast_policy.h"
#include "native_battle_resource_preflight_policy.h"
#include "native_garrison_policy.h"
#include "native_garrison_transition_policy.h"
#include "native_pending_journal.h"
#include "il2cpp_api.h"
#include "MinHook.h"
#include "cn_build_profile.h"
#include "native_portable_resource_win.h"
#include "native_hot_update_resources.h"

// Entry-preview milestone: native activity UI and private player data. Normal
// remote battle starts are blocked until the private native battle bridge exists.
namespace cn {
SO_Il2CppApi api;
HMODULE module=nullptr,runtime=nullptr;
std::atomic<bool> ready{false},closing{false},active{false},private_view{false};
std::atomic<bool> private_battle_resources{false};
RuntimeGate runtime_gate(closing);
std::atomic<bool> setup_busy{false};
std::atomic<bool> observing_model{false},model_ready{false};
std::atomic<unsigned> routed{0},asset_hits{0},profile_hits{0},blocked_battles{0},time_patches{0},asset_registrations{0};
std::atomic<unsigned> local_roster_count{0},bindings_validated{0};
std::atomic<unsigned> runtime_target_differences{0};
std::atomic<unsigned> private_syncs{0},private_sync_failures{0},context_model_reads{0};
std::atomic<unsigned> blocked_activity_writes{0};
std::atomic<unsigned> private_roster_resets{0},blocked_guidebooks{0};
std::atomic<unsigned> blocked_assists{0};
std::atomic<unsigned> local_saves{0},local_save_failures{0},local_config_completions{0},ignored_reward_clicks{0};
std::atomic<unsigned> cancelled_battle_joins{0};
void* join_entry_class=nullptr;
std::atomic<DWORD> unity_ui_thread{0};
std::mutex report_lock;
std::recursive_mutex cache_lock;
std::string stage="bootstrap";
ConfigTrace config_trace;
thread_local uint64_t config_sequence=0;
thread_local std::string config_step;
void config_phase(const char* step,bool accepted=false,bool applied=false,bool completed=false){config_step=step;config_trace.update(config_sequence,step,accepted,applied,completed);}
struct ConfigAttempt{
    uint64_t previous_sequence=config_sequence;std::string previous_step=config_step;
    explicit ConfigAttempt(const char* operation){config_sequence=config_trace.begin(operation);config_step="identify-activity";}
    ~ConfigAttempt(){config_sequence=previous_sequence;config_step=previous_step;}
};
void* (*string_new)(const char*)=nullptr;
void* (*object_new)(void*)=nullptr;
void* (*unbox)(void*)=nullptr;
void* (*reflection_type)(void*)=nullptr;
void (*set_field)(void*,void*,void*)=nullptr;
void (*set_object_field)(void*,void*,void*)=nullptr;
uint32_t (*hold_new)(void*,bool)=nullptr;
uint32_t (*hold_weak)(void*,bool)=nullptr;
void* (*hold_get)(uint32_t)=nullptr;
void (*hold_free)(uint32_t)=nullptr;
void* (*value_box)(void*,void*)=nullptr;
void* (*array_new)(void*,uintptr_t)=nullptr;
std::string activity_json,model_json,perm_json,char_map_json,sync_json;
std::map<std::string,uint32_t> held,bundles;
std::map<uint32_t,void*> bound_methods;
std::map<std::string,std::pair<uint64_t,uint64_t>> target_evidence;
std::map<std::string,unsigned> asset_requests;
std::map<std::string,unsigned> asset_errors;
std::map<std::string,const CN_Asset*> registered_asset_paths;
void* request_class=nullptr;
thread_local std::string missing_method;
thread_local unsigned private_data_depth=0;
struct PrivateDataScope{bool enabled;explicit PrivateDataScope(bool value):enabled(value){if(enabled)++private_data_depth;}~PrivateDataScope(){if(enabled)--private_data_depth;}};

template<class T> bool read(const void* p,T& out){SIZE_T n=0;return p&&ReadProcessMemory(GetCurrentProcess(),p,&out,sizeof(out),&n)&&n==sizeof(out);}
std::wstring path(HMODULE m){wchar_t b[32768]{};DWORD n=GetModuleFileNameW(m,b,32768);return n&&n<32768?std::wstring(b,n):L"";}
std::wstring folder(const std::wstring& p){return p.substr(0,p.find_last_of(L"\\/"));}
bool same(const std::wstring& a,const std::wstring& b){return !a.empty()&&_wcsicmp(a.c_str(),b.c_str())==0;}
std::string utf8(const std::wstring& s){int n=WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),nullptr,0,nullptr,nullptr);std::string v(size_t(n),'\0');if(n)WideCharToMultiByte(CP_UTF8,0,s.data(),int(s.size()),v.data(),n,nullptr,nullptr);return v;}
std::string quote(const std::string& s){std::string v="\"";for(unsigned char c:s){if(c=='"'||c=='\\'){v+='\\';v+=char(c);}else if(c>=32)v+=char(c);}return v+'"';}
std::string native_battle_report();
std::string native_backend_status_json();
std::string native_resource_owner_status_json();
std::string native_fault_capture_status_json();
std::string native_session_presence_status_json();
std::string native_solo_status_ui_status_json();
std::string native_preparation_tokens_status_json();
std::string native_shop_free_refresh_status_json();
std::string native_item_target_rules_status_json();
void native_broadcast_observe_identity(void* real_model);
void native_garrison_shutdown();
void* native_session_presence_sync_response();
bool native_session_presence_reconnect(void*,void*,const void*);
void native_private_liveness_shutdown();
void native_multiplayer_transport_shutdown();
void native_multiplayer_own_start_shutdown();
void native_multiplayer_drain_main();
bool native_multiplayer_entry_create(void*,void*,bool,bool,const void*);
bool native_multiplayer_entry_service_start(void*,void*,const void*);
bool native_multiplayer_entry_service_reconnect(void*,void*,const void*);
bool native_multiplayer_entry_active();
bool native_multiplayer_dispatch_ready();
bool native_multiplayer_observation_reloading();
bool native_multiplayer_observation_starting_reload();
void native_multiplayer_preparation_view_setup();
bool native_multiplayer_preparation_view_claim_initial_owner(const preparation_view::Session&,uint64_t);
bool native_multiplayer_preparation_view_finalizing();
bool native_multiplayer_preparation_view_retire(const preparation_view::Session&,std::function<void()>,const std::string&);
bool native_multiplayer_preparation_view_dispatch(const cnjson::Value&,std::function<void()> = {},std::function<void(const cnjson::Value&)> = {},std::function<void(const std::string&)> = {});
void native_multiplayer_preparation_view_notify_presented(const cnjson::Value&);
void native_multiplayer_preparation_view_operation_setup();
void native_multiplayer_preparation_view_require_request(const std::string&);
void native_multiplayer_preparation_view_tick();
void native_multiplayer_preparation_view_loader_completed();
void native_multiplayer_preparation_view_shutdown();
cnjson::Value native_multiplayer_preparation_view_effective(const cnjson::Value&);
bool native_multiplayer_preparation_view_owns_presentation(const cnjson::Value&);
bool native_multiplayer_preparation_view_owns_scene_transition();
bool native_multiplayer_preparation_view_drive_field_lifecycle();
bool native_multiplayer_preparation_view_report_field();
bool native_multiplayer_preparation_view_may_create_preparation_scene(const cnjson::Value&);
bool native_multiplayer_preparation_view_allows_initial_stage(const cnjson::Value&,const std::string&);
void native_multiplayer_boss_tick();
void native_multiplayer_hp_preview_tick();
void native_multiplayer_deadline_tick();
void native_multiplayer_field_tick();
bool native_multiplayer_mode_runes_load(void*,const void*);
bool native_multiplayer_launch_util(void*,const void*);
bool native_multiplayer_launch_controller(void*,void*,const void*,void*&);
std::string native_multiplayer_transport_status_json();
std::string native_multiplayer_profile_http(const wchar_t*,const std::string*);
std::string native_multiplayer_report();
std::string native_multiplayer_social_status_json();
std::string native_private_network_status_json();
void report(const char* value){
    std::lock_guard<std::mutex> lock(report_lock);stage=value;
    wchar_t base[32768]{};DWORD n=GetEnvironmentVariableW(L"LOCALAPPDATA",base,32768);if(!n||n>=32768)return;
    const auto dest=std::wstring(base)+L"\\StrongholdProtocol\\captures\\CN-native";
    for(size_t i=3;i<dest.size();++i)if(dest[i]==L'\\')CreateDirectoryW(dest.substr(0,i).c_str(),nullptr);CreateDirectoryW(dest.c_str(),nullptr);
    auto file=dest+L"\\cn-activity-"+std::to_wstring(GetCurrentProcessId())+L".json",temp=file+L".tmp";
    FILETIME created{},exited{},kernel{},user{};GetProcessTimes(GetCurrentProcess(),&created,&exited,&kernel,&user);
    ULARGE_INTEGER creation{};creation.LowPart=created.dwLowDateTime;creation.HighPart=created.dwHighDateTime;
    SYSTEMTIME now{};GetSystemTime(&now);char timestamp[40]{};std::snprintf(timestamp,sizeof(timestamp),"%04u-%02u-%02uT%02u:%02u:%02u.%03uZ",now.wYear,now.wMonth,now.wDay,now.wHour,now.wMinute,now.wSecond,now.wMilliseconds);
    std::string s="{\"version\":\"0.1.116\",\"pid\":"+std::to_string(GetCurrentProcessId())+",\"processStartedFileTime\":"+quote(std::to_string(creation.QuadPart))+",\"updatedUtc\":"+quote(timestamp)+",\"stage\":"+quote(stage)+
        ",\"active\":"+(active.load()?"true":"false")+",\"privateView\":"+(private_view.load()?"true":"false")+
        ",\"activityId\":\"act2autochess\",\"metadataSha256\":\""+CN_METADATA_HASH+"\",\"entryPreviewOnly\":false,\"nativeBattleBridgeReady\":false,\"nativeBattleExperimentEnabled\":true,\"officialFallback\":false,\"nativePlayerModelObserved\":"+(model_ready.load()?"true":"false")+
        ",\"routedRequests\":"+std::to_string(routed.load())+",\"assetOverlayHits\":"+std::to_string(asset_hits.load())+
        ",\"profileReads\":"+std::to_string(profile_hits.load())+",\"timePatches\":"+std::to_string(time_patches.load())+",\"nativeAssetRegistrations\":"+std::to_string(asset_registrations.load())+",\"localConfigurationSaves\":"+std::to_string(local_saves.load())+",\"localConfigurationFailures\":"+std::to_string(local_save_failures.load())+",\"localConfigurationCompletions\":"+std::to_string(local_config_completions.load())+",\"ignoredRewardMedalClicks\":"+std::to_string(ignored_reward_clicks.load())+
        ",\"blockedBattleStarts\":"+std::to_string(blocked_battles.load())+",\"blockedActivityWrites\":"+std::to_string(blocked_activity_writes.load())+",\"privateRosterResets\":"+std::to_string(private_roster_resets.load())+",\"blockedGuidebookStarts\":"+std::to_string(blocked_guidebooks.load())+",\"blockedFriendAssistOpens\":"+std::to_string(blocked_assists.load())+",\"privateSyncCompletions\":"+std::to_string(private_syncs.load())+",\"privateSyncFailures\":"+std::to_string(private_sync_failures.load())+",\"activityContextModelReads\":"+std::to_string(context_model_reads.load())+",\"executable\":"+quote(utf8(path(nullptr)))+"}\n";
    std::string targets=",\"localRosterCount\":"+std::to_string(local_roster_count.load())+",\"bindingsValidated\":"+std::to_string(bindings_validated.load())+",\"runtimeTargetDifferences\":"+std::to_string(runtime_target_differences.load())+",\"targets\":{";bool first=true;
    for(const auto& item:target_evidence){if(!first)targets+=',';first=false;targets+=quote(item.first)+":{\"staticCandidateRva\":"+std::to_string(item.second.first)+",\"runtimeRva\":"+std::to_string(item.second.second)+'}';}targets+="},\"activityAssetRequests\":{";first=true;for(const auto& item:asset_requests){if(!first)targets+=',';first=false;targets+=quote(item.first)+":"+std::to_string(item.second);}targets+="},\"activityAssetErrors\":{";first=true;for(const auto& item:asset_errors){if(!first)targets+=',';first=false;targets+=quote(item.first)+":"+std::to_string(item.second);}targets+='}';s.insert(s.size()-2,targets);
    auto trace_json=[](const ConfigTraceEntry& entry){return "{\"sequence\":"+std::to_string(entry.sequence)+",\"operation\":"+quote(entry.operation)+",\"phase\":"+quote(entry.phase)+",\"backendAccepted\":"+(entry.backendAccepted?"true":"false")+",\"privateDtoApplied\":"+(entry.privateDtoApplied?"true":"false")+",\"completed\":"+(entry.completed?"true":"false")+",\"error\":"+quote(entry.error)+"}";};
    ConfigTraceEntry last_failure;auto entries=config_trace.snapshot(last_failure);std::string traces=",\"configurationHistory\":[";first=true;for(const auto& entry:entries){if(!first)traces+=',';first=false;traces+=trace_json(entry);}traces+="],\"lastConfigurationFailure\":"+(last_failure.sequence?trace_json(last_failure):"null");s.insert(s.size()-2,traces);
    s.insert(s.size()-2,",\"cancelledBattleJoins\":"+std::to_string(cancelled_battle_joins.load())+",\"nativeBattle\":"+native_battle_report()+",\"privateBackend\":"+native_backend_status_json()+",\"nativeResourceOwnership\":"+native_resource_owner_status_json()+",\"nativeFaultCapture\":"+native_fault_capture_status_json()+",\"nativeSessionPresence\":"+native_session_presence_status_json()+",\"nativeSoloStatusUi\":"+native_solo_status_ui_status_json()+",\"nativePreparationTokens\":"+native_preparation_tokens_status_json()+",\"nativeShopFreeRefresh\":"+native_shop_free_refresh_status_json()+",\"nativeItemTargetRules\":"+native_item_target_rules_status_json()+",\"nativeMultiplayer\":"+native_multiplayer_report()+",\"nativeNetwork\":"+native_private_network_status_json());
    HANDLE h=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(h==INVALID_HANDLE_VALUE)return;DWORD wrote=0;
    bool ok=WriteFile(h,s.data(),DWORD(s.size()),&wrote,nullptr)&&wrote==s.size();if(ok)FlushFileBuffers(h);CloseHandle(h);if(ok)MoveFileExW(temp.c_str(),file.c_str(),MOVEFILE_REPLACE_EXISTING);
}
bool hash(const std::wstring& p,const char* expected){
    HANDLE file=CreateFileW(p.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_SEQUENTIAL_SCAN,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
    BCRYPT_ALG_HANDLE a=nullptr;BCRYPT_HASH_HANDLE h=nullptr;DWORD size=0,n=0;bool ok=BCryptOpenAlgorithmProvider(&a,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptGetProperty(a,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&size),4,&n,0)>=0;
    std::vector<unsigned char> obj(ok?size:0);if(ok)ok=BCryptCreateHash(a,&h,obj.data(),size,nullptr,0,0)>=0;unsigned char b[65536];
    while(ok){DWORD count=0;if(!ReadFile(file,b,sizeof(b),&count,nullptr)){ok=false;break;}if(!count)break;ok=BCryptHashData(h,b,count,0)>=0;}
    unsigned char d[32]{};if(ok)ok=BCryptFinishHash(h,d,32,0)>=0;if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);CloseHandle(file);
    char text[65]{};const char* digits="0123456789abcdef";for(size_t i=0;i<32;++i){text[2*i]=digits[d[i]>>4];text[2*i+1]=digits[d[i]&15];}return ok&&std::strcmp(text,expected)==0;
}
std::wstring native_bundle_path(const CN_Bundle& source){
    const auto plan=cnportable::plan(source.path,cnportable::executable_game_root());
    if(plan.kind==cnportable::Kind::OriginalClient)return plan.path;
    cnportable::require_regular_no_reparse(plan);return plan.path;
}
void native_verify_bundle_hashes(){
    for(const auto& source:CN_BUNDLES){
        // Official resources belong to the current native index. Validate their
        // CAB/name at load time, after login's hot update has finished. Private
        // overlays remain byte-pinned and can never fall back to official data.
        if(cnportable::plan(source.path,cnportable::executable_game_root()).kind==cnportable::Kind::OriginalClient)continue;
        const auto resolved=native_bundle_path(source);
        if(!hash(resolved,source.hash))throw std::runtime_error("activity bundle changed");
    }
}
#include "native_fault_capture.inc"
#include "native_backend_bootstrap.inc"
std::string text(void* s){
    if(!s||closing.load())return {};int n=api.il2cpp_string_length(s);if(n<0||n>4096)return {};
    const auto* source=api.il2cpp_string_chars(s);std::vector<uint16_t> chars(static_cast<size_t>(n),uint16_t{});SIZE_T used=0;
    if(n&&(!ReadProcessMemory(GetCurrentProcess(),source,chars.data(),size_t(n)*2,&used)||used!=size_t(n)*2))return {};
    int len=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,reinterpret_cast<wchar_t*>(chars.data()),n,nullptr,0,nullptr,nullptr);std::string out(size_t(len),'\0');
    if(len)WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,reinterpret_cast<wchar_t*>(chars.data()),n,out.data(),len,nullptr,nullptr);return out;
}
void* cls(const std::string& full){
    size_t size=0;auto assemblies=api.il2cpp_domain_get_assemblies(api.il2cpp_domain_get(),&size);if(!assemblies||size>4096)return nullptr;
    auto plus=full.find('+');auto top=full.substr(0,plus);auto dot=top.find_last_of('.');std::string space=dot==std::string::npos?"":top.substr(0,dot),name=dot==std::string::npos?top:top.substr(dot+1);
    for(size_t i=0;i<size;++i){auto image=api.il2cpp_assembly_get_image(assemblies[i]);auto c=api.il2cpp_class_from_name(image,space.c_str(),name.c_str());if(!c)continue;
        size_t start=plus;while(c&&start!=std::string::npos){auto end=full.find('+',start+1);auto nested=full.substr(start+1,end==std::string::npos?end:end-start-1);void* iter=nullptr,*found=nullptr;
            while(auto child=api.il2cpp_class_get_nested_types(c,&iter)){const char* n=api.il2cpp_class_get_name(child);if(n&&nested==n){found=child;break;}}c=found;start=end;
        }if(c)return c;
    }return nullptr;
}
void* method(void* c,const char* name,int count){
    for(auto k=c;k;k=api.il2cpp_class_get_parent(k)){void* it=nullptr;while(auto m=api.il2cpp_class_get_methods(k,&it)){const char* n=api.il2cpp_method_get_name(m);if(n&&std::strcmp(n,name)==0&&int(api.il2cpp_method_get_param_count(m))==count&&!api.il2cpp_method_is_generic(m))return m;}}
    missing_method=std::string(c?api.il2cpp_class_get_name(c):"<class missing>")+"."+name+"/"+std::to_string(count);return nullptr;
}
std::string exception_description(void* error,unsigned depth=0){
    auto c=api.il2cpp_object_get_class(error);const char* name=api.il2cpp_class_get_name(c);std::string detail=name?name:"managed exception";
    for(auto parent=c;parent;parent=api.il2cpp_class_get_parent(parent)){
        auto f=api.il2cpp_class_get_field_from_name(parent,"_message");if(!f)f=api.il2cpp_class_get_field_from_name(parent,"message");
        if(!f||api.il2cpp_type_get_type(api.il2cpp_field_get_type(f))!=14)continue;
        void* message=nullptr;api.il2cpp_field_get_value(error,f,&message);auto value=text(message);if(!value.empty())detail+="; "+value;break;
    }
    if(depth<3)for(auto parent=c;parent;parent=api.il2cpp_class_get_parent(parent)){
        auto f=api.il2cpp_class_get_field_from_name(parent,"_innerException");if(!f)f=api.il2cpp_class_get_field_from_name(parent,"inner_exception");
        if(!f)continue;void* inner=nullptr;api.il2cpp_field_get_value(error,f,&inner);if(inner&&inner!=error)detail+="; inner: "+exception_description(inner,depth+1);break;
    }
    // Read the original managed stack before a later callback replaces this
    // exception. A failed stack getter must not hide the original error.
    if(depth==0)if(auto getter=method(c,"get_StackTrace",0)){
        void* nested=nullptr;auto stack=api.il2cpp_runtime_invoke(getter,error,nullptr,&nested);
        if(!nested&&stack){auto value=text(stack);if(!value.empty())detail+="; stack: "+value;}
    }
    return detail;
}
void* invoke(void* m,void* self,void** args=nullptr){if(!m||closing.load())throw std::runtime_error(closing.load()?"runtime closing":("method unavailable: "+missing_method));void* error=nullptr;auto out=api.il2cpp_runtime_invoke(m,self,args,&error);if(error){auto owner=api.il2cpp_method_get_declaring_type(m);const char* name=api.il2cpp_method_get_name(m);throw std::runtime_error(std::string(owner?api.il2cpp_class_get_name(owner):"<type>")+"."+(name?name:"<method>")+": "+exception_description(error));}return out;}
void* field_object(void* self,const char* name){if(!self)return nullptr;auto f=api.il2cpp_class_get_field_from_name(api.il2cpp_object_get_class(self),name);void* value=nullptr;if(f)api.il2cpp_field_get_value(self,f,&value);return value;}
void* protect(const std::string& key,void* value){if(!value)throw std::runtime_error("null native object");auto h=hold_new(value,false);if(!h)throw std::runtime_error("cannot retain object");auto it=held.find(key);if(it!=held.end())hold_free(it->second);held[key]=h;return value;}
struct TemporaryRoots {
    std::vector<uint32_t> handles;
    void* retain(void* object){if(!object)return nullptr;auto handle=hold_new(object,false);if(!handle)throw std::runtime_error("temporary native root unavailable");handles.push_back(handle);return object;}
    ~TemporaryRoots(){for(auto handle:handles)hold_free(handle);}
};
struct NativeValue {void* object=nullptr;void* argument=nullptr;};
void* profile_field(void* c,const std::string& key){
    std::string mapped=key;
    if(std::strcmp(api.il2cpp_class_get_name(c),"PlayerCharacter")==0){
        static const std::map<std::string,std::string> aliases={{"skills","m_skills"},{"defaultSkillIndex","m_defaultSkillIndex"},{"skin","m_skinId"},{"currentEquip","m_selectEquip"},{"equip","m_equips"},{"voiceLan","m_voiceLan"}};
        auto alias=aliases.find(key);if(alias!=aliases.end())mapped=alias->second;
    }
    for(auto parent=c;parent;parent=api.il2cpp_class_get_parent(parent)){auto f=api.il2cpp_class_get_field_from_name(parent,mapped.c_str());if(f)return f;}
    throw std::runtime_error(std::string("private field unavailable: ")+api.il2cpp_class_get_name(c)+"."+key);
}
NativeValue build_value(void* type,const cnjson::Value& value,TemporaryRoots& roots,unsigned depth=0){
    if(depth>64||!type)throw std::runtime_error("private native type bound");auto c=api.il2cpp_class_from_type(type);if(!c)throw std::runtime_error("private native class missing");
    bool is_value=api.il2cpp_class_is_valuetype(c);int kind=api.il2cpp_type_get_type(type);
    if(value.kind==cnjson::Value::Null){if(is_value)throw std::runtime_error("null private value type");return {};}
    if(kind==14){if(value.kind!=cnjson::Value::String||value.string.find('\0')!=std::string::npos)throw std::runtime_error("private string required");auto s=roots.retain(string_new(value.string.c_str()));return {s,s};}
    if(api.il2cpp_class_is_enum(c))kind=api.il2cpp_type_get_type(api.il2cpp_class_enum_basetype(c));
    if(kind==2||(kind>=4&&kind<=11)){
        std::array<uint8_t,8> data{};
        if(kind==2){if(value.kind!=cnjson::Value::Boolean)throw std::runtime_error("private boolean required");data[0]=value.boolean?1:0;}
        else{
            if(value.kind!=cnjson::Value::Integer)throw std::runtime_error("private integer required");auto n=value.integer;
            unsigned bytes=kind<=5?1:(kind<=7?2:(kind<=9?4:8));bool unsigned_type=(kind%2)==1;
            if(unsigned_type){uint64_t maximum=bytes==8?UINT64_MAX:((uint64_t(1)<<(bytes*8))-1);if(n<0||uint64_t(n)>maximum)throw std::runtime_error("private unsigned integer range");}
            else if(bytes<8){auto maximum=(int64_t(1)<<(bytes*8-1))-1;if(n< -maximum-1||n>maximum)throw std::runtime_error("private signed integer range");}
            std::memcpy(data.data(),&n,bytes);
        }
        auto boxed=roots.retain(value_box(c,data.data()));if(!boxed)throw std::runtime_error("private primitive boxing failed");return {boxed,unbox(boxed)};
    }
    const std::string name=api.il2cpp_class_get_name(c);
    if(kind==20||kind==29){
        if(value.kind!=cnjson::Value::Array||api.il2cpp_class_get_rank(c)!=1)throw std::runtime_error("private one-dimensional array required");auto element=api.il2cpp_class_get_element_class(c);
        auto array=roots.retain(array_new(element,value.array.size()));void* setter=nullptr;
        for(auto parent=api.il2cpp_object_get_class(array);parent&&!setter;parent=api.il2cpp_class_get_parent(parent)){
            void* iterator=nullptr;while(auto candidate=api.il2cpp_class_get_methods(parent,&iterator))if(std::strcmp(api.il2cpp_method_get_name(candidate),"SetValue")==0&&api.il2cpp_method_get_param_count(candidate)==2&&api.il2cpp_type_get_type(api.il2cpp_method_get_param(candidate,1))==8){setter=candidate;break;}
        }
        if(!setter)throw std::runtime_error("private array setter missing");
        for(size_t index=0;index<value.array.size();++index){auto item=build_value(api.il2cpp_class_get_type(element),value.array[index],roots,depth+1);int32_t position=int32_t(index);void* args[]={item.object,&position};invoke(setter,array,args);}return {array,array};
    }
    auto object=roots.retain(object_new(c));if(!object)throw std::runtime_error("private object creation failed: "+name);
    if(auto constructor=method(c,".ctor",0))invoke(constructor,is_value?unbox(object):object);
    if(name=="Dictionary`2"||name=="ListDict`2"){
        if(value.kind!=cnjson::Value::Object)throw std::runtime_error("private dictionary required: "+name);auto add=method(c,"Add",2);if(!add||api.il2cpp_type_get_type(api.il2cpp_method_get_param(add,0))!=14)throw std::runtime_error("private string dictionary signature");
        for(const auto& pair:value.object){auto key=roots.retain(string_new(pair.first.c_str()));auto child=build_value(api.il2cpp_method_get_param(add,1),pair.second,roots,depth+1);void* args[]={key,child.argument};invoke(add,object,args);}return {object,object};
    }
    if(name=="List`1"){
        if(value.kind!=cnjson::Value::Array)throw std::runtime_error("private list required");auto add=method(c,"Add",1);if(!add)throw std::runtime_error("private list signature");
        for(const auto& item:value.array){auto child=build_value(api.il2cpp_method_get_param(add,0),item,roots,depth+1);void* args[]={child.argument};invoke(add,object,args);}return {object,object};
    }
    if(value.kind!=cnjson::Value::Object)throw std::runtime_error("private DTO required: "+name);
    for(const auto& pair:value.object){
        try{auto field=profile_field(c,pair.first);if(api.il2cpp_field_get_flags(field)&16)throw std::runtime_error("private static field rejected");auto ft=api.il2cpp_field_get_type(field);auto child=build_value(ft,pair.second,roots,depth+1);
            if(api.il2cpp_class_is_valuetype(api.il2cpp_class_from_type(ft)))set_field(object,field,child.argument);else set_object_field(object,field,child.object);
        }catch(const std::exception& ex){throw std::runtime_error(name+"."+pair.first+": "+ex.what());}
    }return {object,is_value?unbox(object):object};
}
void* json(void* c,const std::string& payload,const std::string* retain_key=nullptr){
    if(!c)throw std::runtime_error("private profile class unavailable");auto tree=cnjson::parse(payload);TemporaryRoots roots;
    auto result=build_value(api.il2cpp_class_get_type(c),tree,roots).object;
    if(retain_key)protect(*retain_key,result);return result;
}
void* cached(const std::string& key,const char* type,const std::string& payload){std::lock_guard<std::recursive_mutex> lock(cache_lock);auto it=held.find(key);if(it!=held.end())return hold_get(it->second);return json(cls(type),payload,&key);}
bool act(void* id){return text(id)=="act2autochess";}
bool activity_route(const std::string& path){
    return path.rfind("/activity/autochessSeason/",0)==0;
}
std::string private_http(const wchar_t* route,const std::string* payload=nullptr,bool ui=false){
    if(nbb_participant_profile)return native_multiplayer_profile_http(route,payload);
    HINTERNET session=WinHttpOpen(L"StrongholdCNActivity/0.1.116",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0);if(!session)throw std::runtime_error("local session");
    WinHttpSetTimeouts(session,ui?500:1500,ui?500:1500,ui?700:2000,ui?700:2000);auto began=GetTickCount64();HINTERNET conn=private_backend_connect(session);HINTERNET req=conn?WinHttpOpenRequest(conn,payload?L"POST":L"GET",route,nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,private_backend_request_flags()):nullptr;
    DWORD redirect=WINHTTP_OPTION_REDIRECT_POLICY_NEVER;auto bytes=payload?DWORD(payload->size()):0;
    bool ok=req&&(!payload||payload->size()<=1024*1024)&&WinHttpSetOption(req,WINHTTP_OPTION_REDIRECT_POLICY,&redirect,sizeof(redirect))&&private_backend_headers(req)&&WinHttpSendRequest(req,payload?L"Content-Type: application/json\r\n":WINHTTP_NO_ADDITIONAL_HEADERS,payload?DWORD(-1):0,payload?const_cast<char*>(payload->data()):WINHTTP_NO_REQUEST_DATA,bytes,bytes,0)&&WinHttpReceiveResponse(req,nullptr);
    DWORD status=0,size=4;if(ok)ok=WinHttpQueryHeaders(req,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX);
    std::string result;while(ok){if(GetTickCount64()-began>(ui?2500:10000)){ok=false;break;}DWORD count=0;if(!WinHttpQueryDataAvailable(req,&count)){ok=false;break;}if(!count)break;if(result.size()+count>8*1024*1024){ok=false;break;}auto previous=result.size();result.resize(previous+count);DWORD read_count=0;if(!WinHttpReadData(req,result.data()+previous,count,&read_count)){ok=false;break;}result.resize(previous+read_count);}
    if(req)WinHttpCloseHandle(req);if(conn)WinHttpCloseHandle(conn);WinHttpCloseHandle(session);if(!ok||result.empty())throw std::runtime_error("private backend unavailable");if(status!=200)throw std::runtime_error("private backend HTTP "+std::to_string(status));return result;
}
std::string get(const wchar_t* route,bool ui=false){return private_http(route,nullptr,ui);}
using Getter=void*(*)(void*,const void*);using StaticGetter=void*(*)(void*,const void*);using Void0=void(*)(void*,const void*);
Getter old_model=nullptr;StaticGetter old_player=nullptr;Void0 old_sort=nullptr;
using Recent=void*(*)(void*,int64_t,const void*);Recent old_recent=nullptr;
using BoolAct=bool(*)(void*,const void*);BoolAct old_in_player=nullptr,old_training=nullptr;
using Unlock=bool(*)(void*,void**,const void*);Unlock old_unlock=nullptr;
using Parse=void*(*)(void*,void*,void*,const void*);Parse old_parse=nullptr;
using Load=void*(*)(void*,void*,const void*);Load old_ab_load=nullptr,old_ab_async=nullptr,old_preprocess=nullptr;
using AssetLoaded=void(*)(void*,void*,void*,void*,const void*);AssetLoaded old_asset_loaded=nullptr;
using StaticLoad=void*(*)(void*,const void*);StaticLoad old_load=nullptr,old_async=nullptr;
using Exists=bool(*)(void*,void*,const void*);Exists old_ab_exists=nullptr;
using StaticExists=bool(*)(void*,const void*);StaticExists old_exists=nullptr;
using LoadCallback=void(*)(void*,void*,const void*);LoadCallback old_load_callback=nullptr;
using ABCallback=void(*)(void*,void*,void*,const void*);ABCallback old_ab_callback=nullptr;
using ControllerGetter=void*(*)(void*,const void*);ControllerGetter old_controller=nullptr;
using SyncRequest=void*(*)(void*,const void*);SyncRequest old_sync_request=nullptr;
using InitModel=void(*)(void*,void*,const void*);InitModel old_init_model=nullptr;
using MainSprite=void*(*)(void*,int32_t,const void*);MainSprite old_main_sprite=nullptr;
using CharGetter=void*(*)(void*,void*,const void*);CharGetter old_char=nullptr;
// Both public APIs are static. Normal/rejoin pass a 32-byte CommonJoinEntry
// value through its Win64 indirect payload pointer; training passes a delegate.
using ServiceStart=void(*)(void*,void*,const void*);ServiceStart old_start=nullptr,old_reconnect=nullptr,old_training_start=nullptr;
using Secure=bool(*)(void*,void**,int32_t*,const void*);Secure old_secure=nullptr;
using Header=void*(*)(void*,void*,const void*);Header old_header=nullptr;
using Invoke=void*(*)(void*,void*,void**,void**);Invoke old_invoke=nullptr;
using Shutdown=void(*)();Shutdown old_shutdown=nullptr;
bool can_read(){return active.load()&&!closing.load()&&api.il2cpp_thread_current()!=nullptr;}
void* model(){auto p=cached("model","Torappu.PlayerDataModel",model_json);if(!field_object(p,"troop"))throw std::runtime_error("local troop missing");return p;}
void copy_native_fields(void* target,void* source){
    if(!target||!source||target==source||api.il2cpp_object_get_class(target)!=api.il2cpp_object_get_class(source))throw std::runtime_error("native copy class mismatch");
    unsigned levels=0,fields=0;for(auto c=api.il2cpp_object_get_class(source);c;c=api.il2cpp_class_get_parent(c)){
        if(++levels>32)throw std::runtime_error("native copy inheritance bound");void* iterator=nullptr;
        while(auto f=api.il2cpp_class_get_fields(c,&iterator)){if(++fields>4096)throw std::runtime_error("native copy field bound");if(api.il2cpp_field_get_flags(f)&16)continue;
            auto fc=api.il2cpp_class_from_type(api.il2cpp_field_get_type(f));if(!fc)throw std::runtime_error("native copy field class missing");
            if(api.il2cpp_class_is_valuetype(fc)){uint32_t alignment=0;auto size=api.il2cpp_class_value_size(fc,&alignment);if(size<1||size>65536)throw std::runtime_error("native copy value size");std::vector<uint8_t> bytes(size_t(size),uint8_t{});api.il2cpp_field_get_value(source,f,bytes.data());set_field(target,f,bytes.data());}
            else{void* value=nullptr;api.il2cpp_field_get_value(source,f,&value);set_object_field(target,f,value);}
        }
    }
}
void* clone_season_map(void* map_class,void* old_map,TemporaryRoots& roots){
    auto map=roots.retain(object_new(map_class));if(!map||map==old_map)throw std::runtime_error("season map clone aliases source");
    if(old_map){void* iterator=nullptr,*ctor=nullptr;while(auto m=api.il2cpp_class_get_methods(map_class,&iterator))if(std::strcmp(api.il2cpp_method_get_name(m),".ctor")==0&&api.il2cpp_method_get_param_count(m)==2&&api.il2cpp_class_from_type(api.il2cpp_method_get_param(m,0))==map_class){ctor=m;break;}void* args[]={old_map,nullptr};invoke(ctor,map,args);}
    else invoke(method(map_class,".ctor",0),map);return map;
}
void* context_model(void* original){
    std::lock_guard<std::recursive_mutex> lock(cache_lock);if(!original)throw std::runtime_error("original model not ready");auto source=held.find("view-source"),view=held.find("view-model");
    if(source!=held.end()&&view!=held.end()&&hold_get(source->second)==original){++context_model_reads;return hold_get(view->second);}
    TemporaryRoots roots;roots.retain(original);auto clone=roots.retain(invoke(method(api.il2cpp_object_get_class(original),"ShallowClone",0),original));
    if(!clone||clone==original)throw std::runtime_error("player model clone aliases source");auto local=model();for(auto name:{"troop","autoChessPerm","emoticon"})set_object_field(clone,profile_field(api.il2cpp_object_get_class(clone),name),field_object(local,name));
    // Keep ordinary activity modules and unrelated season records in detached
    // containers. Never modify PlayerData.m_data, rawData or any account object.
    auto original_activity=field_object(original,"activity");if(!original_activity)throw std::runtime_error("original activity model missing");
    auto activity=roots.retain(object_new(api.il2cpp_object_get_class(original_activity)));copy_native_fields(activity,original_activity);
    auto map_field=profile_field(api.il2cpp_object_get_class(activity),"actAutoChessActivityList");auto map_class=api.il2cpp_class_from_type(api.il2cpp_field_get_type(map_field));auto old_map=field_object(original_activity,"actAutoChessActivityList");auto map=clone_season_map(map_class,old_map,roots);
    auto key=roots.retain(string_new("act2autochess"));uint8_t exists=0;void* key_args[]={key};if(!read(unbox(invoke(method(map_class,"ContainsKey",1),map,key_args)),exists))throw std::runtime_error("season clone key check failed");
    void* set_args[]={key,hold_get(held.at("player"))};invoke(method(map_class,exists?"set_Item":"Add",2),map,set_args);set_object_field(activity,map_field,map);set_object_field(clone,profile_field(api.il2cpp_object_get_class(clone),"activity"),activity);
    protect("view-source",original);protect("view-model",clone);++context_model_reads;return clone;
}
void* player(){++profile_hits;return cached("player","Torappu.PlayerActivity+PlayerActAutoChessActivity",activity_json);}
std::string native_solo_exit_finish_url();
std::string native_multiplayer_participant_token();
std::string native_multiplayer_social_settlement_url();
void* route(void* self,void* entry,void* code,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;const auto path=text(code);
    // The original LOCAL finish service is an asynchronous UI teardown ACK,
    // not another private battle result. An acknowledged save or final report
    // authorizes a distinct exact route, including after the page detaches.
    if(path=="/activity/autochessSeason/finishGuideBattle"){
        if(!can_read())return nullptr;
        const auto local=native_solo_exit_finish_url();
        if(local.empty()){report("private-saved-exit-finish-without-proof-blocked");return nullptr;}
        ++routed;return string_new(local.c_str());
    }
    if(path=="/activity/autochessSeason/settleGame"&&nbb_participant_profile){
        if(!can_read())return nullptr;
        try{const auto local=native_multiplayer_social_settlement_url();++routed;return string_new((private_backend_origin()+local).c_str());}
        catch(const std::exception& ex){report((std::string("private-native-settlement-route-blocked: ")+ex.what()).c_str());return nullptr;}
    }
    if(path.rfind("/activity/autochessSeason/",0)==0&&!private_view.load()){report("private-activity-request-outside-open-scope-blocked");return nullptr;}
    // The native result page also sends syncInfo directly through UISender,
    // outside the intercepted preparation-controller getter. It needs the
    // same participant-owned native response after the battle scene detaches.
    if(path=="/activity/autochessSeason/syncInfo"&&nbb_participant_profile){
        if(!can_read())return nullptr;++routed;
        return string_new((private_backend_origin()+"/bridge/act2autochess/multiplayer/native-sync-info").c_str());
    }
    if(activity_route(path)){if(!can_read())return nullptr;++routed;return string_new((private_backend_origin()+path).c_str());}
    lease.release();return old_parse(self,entry,code,m);
}
void* local_player(void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()&&act(id)){try{return player();}catch(...){report("private-profile-error");return nullptr;}}lease.release();return old_player(id,m);}
void* local_model(void* self,const void* m){
    auto result=old_model(self,m);
    RuntimeLease lease(runtime_gate);
    if(lease&&can_read()&&api.il2cpp_thread_current())native_broadcast_observe_identity(result);
    if(private_data_depth){if(!lease||!can_read())return nullptr;try{return context_model(result);}catch(const std::exception& ex){report((std::string("private-context-model-error: ")+ex.what()).c_str());return nullptr;}}
    if(lease&&observing_model.load()&&!closing.load()&&api.il2cpp_thread_current()){
        if(result){auto troop=field_object(result,"troop");if(troop&&field_object(troop,"chars"))model_ready.store(true);}
    }
    return result;
}
void* controller_player(void* self,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){try{auto getter=method(api.il2cpp_object_get_class(self),"GetActId",0);if(getter&&act(invoke(getter,self)))return player();}catch(...){report("private-controller-data-error");return nullptr;}}lease.release();return private_view.load()?nullptr:old_controller(self,m);}
bool in_player(void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()&&act(id))return true;lease.release();return old_in_player(id,m);}
bool training(void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()&&act(id))return true;lease.release();return old_training(id,m);}
bool unlock(void* id,void** alert,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()&&act(id)){if(alert)*alert=nullptr;return true;}lease.release();return old_unlock(id,alert,m);}
void* character(void* self,void* id,const void* m){
    RuntimeLease lease(runtime_gate);if(private_data_depth){if(!lease||!can_read())return nullptr;try{std::lock_guard<std::recursive_mutex> lock(cache_lock);auto it=held.find("char-map");if(it==held.end())throw std::runtime_error("validated character map missing");auto map=hold_get(it->second);void* args[]={id};return invoke(method(api.il2cpp_object_get_class(map),"get_Item",1),map,args);}catch(...){report("private-character-unavailable");return nullptr;}}
    lease.release();return old_char(self,id,m);
}
void patch_times(void* self){
    if(time_patches.load()||!can_read()||unity_ui_thread.load()!=GetCurrentThreadId())return;auto list=field_object(self,"m_timeSortedActInfos");if(!list)return;auto c=api.il2cpp_object_get_class(list);int32_t count=0;auto count_box=invoke(method(c,"get_Count",0),list);if(!read(unbox(count_box),count)||count<1||count>8192)return;
    for(int32_t i=0;i<count;++i){void* args[]={&i};auto row=invoke(method(c,"get_Item",1),list,args);if(!act(field_object(row,"id")))continue;auto rc=api.il2cpp_object_get_class(row);
        auto type_field=api.il2cpp_class_get_field_from_name(rc,"type");int32_t activity_type=0;if(!type_field)throw std::runtime_error("activity type missing");api.il2cpp_field_get_value(row,type_field,&activity_type);if(activity_type!=61)throw std::runtime_error("activity type is not AUTOCHESS_SEASON");
        FILETIME clock{};GetSystemTimeAsFileTime(&clock);ULARGE_INTEGER ticks{};ticks.LowPart=clock.dwLowDateTime;ticks.HighPart=clock.dwHighDateTime;int64_t now=int64_t((ticks.QuadPart-116444736000000000ULL)/10000000ULL);
        int64_t start=now-60,end=now+30LL*86400,reward_end=end+7LL*86400;uint8_t yes=1;for(auto name:{"startTime","endTime","rewardEndTime"}){auto f=api.il2cpp_class_get_field_from_name(rc,name);if(!f||api.il2cpp_type_get_type(api.il2cpp_field_get_type(f))!=10)throw std::runtime_error("activity timestamp layout");set_field(row,f,std::strcmp(name,"startTime")==0?&start:(std::strcmp(name,"rewardEndTime")==0?&reward_end:&end));}
        auto display=api.il2cpp_class_get_field_from_name(rc,"displayOnHome");if(display)set_field(row,display,&yes);++time_patches;old_sort(self,bound_methods.at(CN_METHOD_sort.token));report("activity-time-overridden");break;
    }
}
void sorted(void* self,const void* m){old_sort(self,m);RuntimeLease lease(runtime_gate);if(lease)try{patch_times(self);}catch(...){report("activity-time-patch-failed");}}
void* recent(void* self,int64_t now,const void* m){RuntimeLease lease(runtime_gate);if(lease)try{patch_times(self);}catch(...){report("activity-time-patch-failed");}lease.release();return old_recent(self,now,m);}
using OpenPage=void(*)(void*,void*,void*,const void*);OpenPage old_open=nullptr;
void open_page(void* id,void* name,void* options,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){unity_ui_thread.store(GetCurrentThreadId());private_view.store(act(id));}lease.release();old_open(id,name,options,m);}
using Loaded=void(*)(void*,const void*);Loaded old_loaded=nullptr;
Loaded old_home_enter=nullptr,old_home_resume=nullptr;
void home_enter(void* self,const void* m){unity_ui_thread.store(GetCurrentThreadId());private_view.store(false);old_home_enter(self,m);}
void home_resume(void* self,const void* m){unity_ui_thread.store(GetCurrentThreadId());private_view.store(false);old_home_resume(self,m);}
void loaded(void* self,const void* m){
    RuntimeLease lease(runtime_gate);if(lease&&can_read()){unity_ui_thread.store(GetCurrentThreadId());try{auto getter=method(api.il2cpp_object_get_class(self),"GetActId",0);if(!getter)throw std::runtime_error("activity ID getter unavailable");private_view.store(act(invoke(getter,self)));}catch(...){private_view.store(true);report("activity-scope-unresolved");return;}}
    lease.release();PrivateDataScope scope(private_view.load());old_loaded(self,m);
}
void init_model(void* self,void* id,const void* m){RuntimeLease lease(runtime_gate);bool target=lease&&can_read()&&act(id);if(target)private_view.store(true);lease.release();PrivateDataScope scope(target);old_init_model(self,id,m);}
// Preview rewards/mission writes are stopped before UISender starts a request.
// Their shared endpoint names cannot be classified from a mutable page flag:
// an already queued request may be processed after leaving the activity page.
using ActivityWrite=void(*)(void*,const void*);
using MissionItemWrite=void(*)(void*,void*,const void*);
ActivityWrite old_reward_single=nullptr,old_reward_all=nullptr,old_mission_all=nullptr;
MissionItemWrite old_mission_item=nullptr;
bool preview_write_blocked(void* self,bool mission){
    if(!can_read())return false;
    auto id=mission?field_object(field_object(self,"m_inputParam"),"displayId"):field_object(self,"m_actId");
    if(!act(id))return false;
    ++blocked_activity_writes;report(mission?"private-native-mission-write-pending":"private-native-reward-write-pending");return true;
}
void reward_single(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(preview_write_blocked(self,false))return;lease.release();old_reward_single(self,m);}
void reward_all(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(preview_write_blocked(self,false))return;lease.release();old_reward_all(self,m);}
void mission_all(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(preview_write_blocked(self,true))return;lease.release();old_mission_all(self,m);}
void mission_item(void* self,void* item,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(preview_write_blocked(self,true))return;lease.release();old_mission_item(self,item,m);}
ActivityWrite old_guidebook=nullptr;
MissionItemWrite old_roster_reset=nullptr,old_roster_sync=nullptr,old_shop_load=nullptr;
using ShopRefresh=void(*)(void*,bool,void*,const void*);ShopRefresh old_shop_refresh=nullptr;
using ShopDiySelected=void*(*)(void*,int32_t,const void*);ShopDiySelected old_shop_diy_selected=nullptr;
int32_t roster_input_offset=-1;
void guidebook(void* self,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(can_read())try{if(act(invoke(method(api.il2cpp_object_get_class(self),"GetActId",0),self))){++blocked_guidebooks;report("private-automatic-guidebook-skipped");return;}}
    catch(...){report("guidebook-activity-scope-unresolved");return;}
    lease.release();old_guidebook(self,m);
}
void roster_reset(void* self,void* raw_data,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;bool target=false;
    if(can_read()){
        void* input=nullptr;
        if(!raw_data||roster_input_offset<0||!read(static_cast<char*>(raw_data)+roster_input_offset,input)){report("private-roster-reset-input-unreadable");return;}
        target=act(field_object(input,"actId"));
        if(target&&unity_ui_thread.load()!=GetCurrentThreadId()){report("private-roster-reset-outside-ui-thread");return;}
    }
    if(target){++private_roster_resets;report("private-roster-reset-scoped");}
    lease.release();PrivateDataScope scope(target);old_roster_reset(self,raw_data,m);
}
void roster_sync(void* self,void* input,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;bool target=can_read()&&act(field_object(input,"actId"));lease.release();PrivateDataScope scope(target);old_roster_sync(self,input,m);}
void shop_load(void* self,void* id,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;bool target=can_read()&&act(id);lease.release();PrivateDataScope scope(target);old_shop_load(self,id,m);}
void shop_refresh(void* self,bool reset,void* raw_focus,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;bool target=false;
    if(can_read())try{target=act(invoke(bound_methods.at(CN_METHOD_shop_id.token),self));}catch(...){report("shop-refresh-activity-scope-unresolved");return;}
    lease.release();PrivateDataScope scope(target);old_shop_refresh(self,reset,raw_focus,m);
}
void* shop_diy_selected(void* self,int32_t level,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;bool target=false;
    if(can_read())try{target=act(invoke(bound_methods.at(CN_METHOD_shop_id.token),self));}catch(...){report("shop-diy-selected-activity-scope-unresolved");return nullptr;}
    lease.release();PrivateDataScope scope(target);return old_shop_diy_selected(self,level,m);
}
using SaveDiy=void(*)(void*,void*,int32_t,const void*);SaveDiy old_save_diy=nullptr;
ActivityWrite old_save_quick=nullptr,old_save_detail=nullptr;
ActivityWrite old_assist_top=nullptr,old_assist_detail=nullptr;
MissionItemWrite old_save_cancel=nullptr;
void* shop_model(void* self){auto model=field_object(field_object(field_object(self,"m_stateBean"),"m_shopProperty"),"m_value");if(!model)throw std::runtime_error("shop model missing");return model;}
bool private_shop(void* self){return act(invoke(bound_methods.at(CN_METHOD_shop_id.token),shop_model(self)));}
bool shop_write_blocked(void* self,bool assist=false){
    if(!can_read())return false;
    try{
        if(!private_shop(self))return false;
        if(assist)++blocked_assists;else ++blocked_activity_writes;report(assist?"private-native-friend-assist-pending":"private-native-roster-save-pending");return true;
    }catch(const std::exception& ex){if(assist)++blocked_assists;else ++blocked_activity_writes;report((std::string("shop-action-activity-scope-unresolved: ")+ex.what()).c_str());return true;}
}
void* instance_argument(void* object);
cnjson::Value json_text(const std::string& text){cnjson::Value v;v.kind=cnjson::Value::String;v.string=text;return v;}
cnjson::Value native_deploy_pool(void* pool,bool diy){
    cnjson::Value result;result.kind=cnjson::Value::Object;if(!pool)return result;TemporaryRoots roots;roots.retain(pool);auto c=api.il2cpp_object_get_class(pool);int32_t count=0;
    if(!read(unbox(invoke(method(c,"get_Count",0),pool)),count)||count<0||count>4096)throw std::runtime_error("native deploy pool size invalid");
    auto keys=roots.retain(invoke(method(c,"get_Keys",0),pool));auto iterator=roots.retain(invoke(method(api.il2cpp_object_get_class(keys),"GetEnumerator",0),keys));auto ic=api.il2cpp_object_get_class(iterator);
    auto move=method(ic,"MoveNext",0),current=method(ic,"get_Current",0);auto get_item=method(c,"get_Item",1);
    for(int32_t index=0;index<count;++index){uint8_t next=0;if(!read(unbox(invoke(move,instance_argument(iterator))),next)||!next)throw std::runtime_error("native deploy pool iterator ended early");
        auto key=roots.retain(invoke(current,instance_argument(iterator)));auto name=text(key);if(name.empty()||name.size()>256)throw std::runtime_error("native deploy key invalid");void* args[]={key};auto item=roots.retain(invoke(get_item,pool,args));if(!item)throw std::runtime_error("native deploy item null");
        cnjson::Value entry;entry.kind=cnjson::Value::Object;auto field=profile_field(api.il2cpp_object_get_class(item),"skillIndex");if(api.il2cpp_type_get_type(api.il2cpp_field_get_type(field))!=8)throw std::runtime_error("native deploy skill field mismatch");int32_t skill=0;api.il2cpp_field_get_value(item,field,&skill);cnjson::Value number;number.kind=cnjson::Value::Integer;number.integer=skill;entry.object["skillIndex"]=number;
        for(auto field_name:{"currentEquip","diyChar","origChessId"}){if(!diy&&std::strcmp(field_name,"currentEquip"))continue;auto f=profile_field(api.il2cpp_object_get_class(item),field_name);if(api.il2cpp_type_get_type(api.il2cpp_field_get_type(f))!=14)throw std::runtime_error("native deploy string field mismatch");void* value=nullptr;api.il2cpp_field_get_value(item,f,&value);entry.object[field_name]=value?json_text(text(value)):cnjson::Value{};}
        if(!result.object.emplace(name,std::move(entry)).second)throw std::runtime_error("native deploy duplicate key");
    }
    return result;
}
void drop_cache(const std::string& name){auto item=held.find(name);if(item!=held.end()){hold_free(item->second);held.erase(item);}}
void refresh_private_activity(const std::string& payload){
    std::lock_guard<std::recursive_mutex> lock(cache_lock);const std::string staging="player-staged";auto fresh=json(cls("Torappu.PlayerActivity+PlayerActAutoChessActivity"),payload,&staging);TemporaryRoots roots;roots.retain(fresh);
    // Only the independently constructed model's season map is changed. A new
    // contextual clone will be built from the unmodified ordinary player model.
    auto base=held.find("model");if(base==held.end())throw std::runtime_error("independent private model missing");auto activity=field_object(hold_get(base->second),"activity");auto map=field_object(activity,"actAutoChessActivityList");if(!map)throw std::runtime_error("independent private season map missing");
    auto key=roots.retain(string_new("act2autochess"));void* args[]={key,fresh};invoke(method(api.il2cpp_object_get_class(map),"set_Item",2),map,args);
    protect("player",fresh);activity_json=payload;drop_cache("view-model");drop_cache("view-source");drop_cache(staging);
}
void commit_private_config(const wchar_t* route,const cnjson::Value& request){
    config_phase("private-post");
    auto body=cnjson::dump(request);auto response=cnjson::parse(private_http(route,&body,true));auto status=response.object.find("result");
    if(status==response.object.end()||status->second.kind!=cnjson::Value::Integer||status->second.integer!=0)throw std::runtime_error("private config commit not acknowledged");
    config_phase("private-profile-read",true);auto payload=get(L"/bridge/act2autochess/player-activity",true);config_phase("private-dto-apply");refresh_private_activity(payload);++local_saves;config_phase("private-dto-applied",false,true);
}
void config_callback(void* self,const CN_Method& spec){
    config_phase("native-response-construct");TemporaryRoots roots;auto callback=bound_methods.at(spec.token);auto response_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(callback,0));auto response=roots.retain(object_new(response_class));invoke(method(response_class,".ctor",0),response);void* args[]={response};config_phase("native-success-callback");invoke(callback,self,args);++local_config_completions;config_phase("completed",false,false,true);report("private-native-config-completed");
}
bool config_target(void* self){if(!can_read())throw std::runtime_error("private config runtime unavailable");if(unity_ui_thread.load()!=GetCurrentThreadId())throw std::runtime_error("config save outside observed Unity UI thread");return private_shop(self);}
cnjson::Value config_request(){cnjson::Value request;request.kind=cnjson::Value::Object;request.object["actId"]=json_text("act2autochess");return request;}
void config_failed(const std::exception& ex){++local_save_failures;config_trace.update(config_sequence,config_step.c_str(),false,false,false,ex.what());report((std::string("private-native-config-failed: ")+ex.what()).c_str());}
void save_diy(void* self,void* selected,int32_t level,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;ConfigAttempt attempt("diy");bool target=false;try{target=config_target(self);}catch(const std::exception& ex){config_failed(ex);return;}if(!target){config_phase("other-activity-original");lease.release();old_save_diy(self,selected,level,m);return;}PrivateDataScope scope(true);config_phase("native-payload-export");
    try{void* args[]={selected,&level};TemporaryRoots roots;auto pool=roots.retain(invoke(bound_methods.at(CN_METHOD_diy_build.token),self,args));if(!pool)throw std::runtime_error("native DIY pool unavailable");auto request=config_request();request.object["diyChessPool"]=native_deploy_pool(pool,true);commit_private_config(L"/activity/autochessSeason/setChessPoolDiyChar",request);config_callback(self,CN_METHOD_diy_suc);}catch(const std::exception& ex){config_failed(ex);}
}
void save_deploy(void* self,const void* m,bool detail){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;ConfigAttempt attempt(detail?"detail":"quick");bool target=false;try{target=config_target(self);}catch(const std::exception& ex){config_failed(ex);return;}if(!target){config_phase("other-activity-original");lease.release();(detail?old_save_detail:old_save_quick)(self,m);return;}PrivateDataScope scope(true);config_phase("native-payload-export");
    try{TemporaryRoots roots;auto pool=roots.retain(invoke(bound_methods.at(CN_METHOD_cached_pool.token),shop_model(self)));auto values=native_deploy_pool(pool,false);
        if(values.object.empty()&&!detail){config_phase("native-empty-config-callback");invoke(bound_methods.at(CN_METHOD_quick_done.token),self);++local_config_completions;config_phase("completed",false,false,true);report("private-native-empty-config-completed");return;}
        if(!values.object.empty()){auto request=config_request();request.object["chessPool"]=std::move(values);commit_private_config(L"/activity/autochessSeason/setChessPoolDeploy",request);}
        config_callback(self,detail?CN_METHOD_detail_suc:CN_METHOD_quick_suc);
    }catch(const std::exception& ex){config_failed(ex);}
}
void save_quick(void* self,const void* m){save_deploy(self,m,false);}
void save_detail(void* self,const void* m){save_deploy(self,m,true);}
void save_cancel(void* self,void* id,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;ConfigAttempt attempt("remove");bool target=false;try{target=config_target(self);}catch(const std::exception& ex){config_failed(ex);return;}if(!target){config_phase("other-activity-original");lease.release();old_save_cancel(self,id,m);return;}PrivateDataScope scope(true);config_phase("native-payload-export");
    try{auto name=text(id);if(name.empty())throw std::runtime_error("private cancel chessId missing");auto request=config_request();request.object["chessId"]=json_text(name);commit_private_config(L"/activity/autochessSeason/removeChessPoolChar",request);config_callback(self,CN_METHOD_cancel_suc);}catch(const std::exception& ex){config_failed(ex);}
}
void assist_top(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(shop_write_blocked(self,true))return;lease.release();old_assist_top(self,m);}
void assist_detail(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(shop_write_blocked(self,true))return;lease.release();old_assist_detail(self,m);}
Getter old_tutorial_key=nullptr;using ResumeKey=void*(*)(void*,void*,const void*);ResumeKey old_tutorial_resume_key=nullptr;
ActivityWrite old_shop_guidebook=nullptr,old_entry_rewards=nullptr,old_entry_medals=nullptr,old_entry_daily=nullptr;
bool controller_target(void* self){return act(invoke(method(api.il2cpp_object_get_class(self),"GetActId",0),self));}
void* tutorial_key(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;if(can_read())try{if(controller_target(self)){++blocked_guidebooks;report("private-activity-tutorial-disabled-before-start");return nullptr;}}catch(...){report("tutorial-activity-scope-unresolved");return nullptr;}lease.release();return old_tutorial_key(self,m);}
void* tutorial_resume_key(void* self,void* from,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;if(can_read())try{if(controller_target(self))return nullptr;}catch(...){report("tutorial-resume-activity-scope-unresolved");return nullptr;}lease.release();return old_tutorial_resume_key(self,from,m);}
void shop_guidebook(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(can_read())try{if(private_shop(self)){++blocked_guidebooks;report("private-shop-guidebook-disabled-before-start");return;}}catch(...){report("shop-guidebook-activity-scope-unresolved");return;}lease.release();old_shop_guidebook(self,m);}
bool ignore_entry_reward(void* self){if(!can_read())return false;try{auto model=invoke(bound_methods.at(CN_METHOD_entry_model.token),self);if(!model)throw std::runtime_error("activity entry model missing");if(!act(field_object(model,"actId")))return false;++ignored_reward_clicks;report("private-activity-reward-or-medal-click-ignored");return true;}catch(...){report("reward-entry-activity-scope-unresolved");return true;}}
void entry_rewards(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(ignore_entry_reward(self))return;lease.release();old_entry_rewards(self,m);}
void entry_medals(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(ignore_entry_reward(self))return;lease.release();old_entry_medals(self,m);}
void entry_daily(void* self,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;if(ignore_entry_reward(self))return;lease.release();old_entry_daily(self,m);}
void* sync_request(void* self,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;if(!can_read())return nullptr;bool target=false;auto fallback=hold_get(held.at("sync-fallback-handler"));
    try{target=act(invoke(method(api.il2cpp_object_get_class(self),"GetActId",0),self));}catch(...){if(private_view.load()){++private_sync_failures;report("private-sync-controller-scope-unresolved");return fallback;}lease.release();return old_sync_request(self,m);}
    if(!target){lease.release();return old_sync_request(self,m);}PrivateDataScope scope(true);TemporaryRoots roots;void* handler=nullptr;
    try{
        if(unity_ui_thread.load()!=GetCurrentThreadId())throw std::runtime_error("private sync outside observed Unity UI thread");
        auto handler_class=api.il2cpp_class_from_type(api.il2cpp_method_get_return_type(bound_methods.at(CN_METHOD_sync.token)));handler=roots.retain(object_new(handler_class));invoke(method(handler_class,".ctor",0),handler);
        auto response=roots.retain(native_session_presence_sync_response());
        if(!field_object(response,"changed"))throw std::runtime_error("local sync changed list missing");void* args[]={response};invoke(bound_methods.at(CN_METHOD_handle_sync.token),self,args);
        invoke(method(handler_class,"MarkFinish",0),handler);uint8_t waiting=1;if(!read(unbox(invoke(method(handler_class,"get_keepWaiting",0),handler)),waiting)||waiting)throw std::runtime_error("local sync handler not completed");
        protect("sync-last-handler",handler);++private_syncs;report("private-native-sync-completed");return handler;
    }catch(const std::exception& ex){
        ++private_sync_failures;report((std::string("private-native-sync-failed: ")+ex.what()).c_str());
        // No UISender request/mask was started. Always complete a constructed
        // handler locally; failure must not fall back to the official endpoint.
        return fallback;
    }
}
bool prelaunch_audio_scope(){
    return (private_view.load()||private_data_depth)&&unity_ui_thread.load()==GetCurrentThreadId();
}
bool overlay_bundle_allowed(const CN_Bundle& source){
    return !source.battleOnly||private_battle_resources.load()||(source.prelaunchAudio&&prelaunch_audio_scope());
}
const CN_Asset* overlay_asset(void* id){
    auto key=text(id);for(auto& ch:key)if(ch>='A'&&ch<='Z')ch=char(ch-'A'+'a');if(key.rfind("dyn/",0)==0)key.erase(0,4);
    for(const auto& asset:CN_ASSETS)if(key==asset.alias&&(!asset.battleOnly||private_battle_resources.load()||(asset.prelaunchAudio&&prelaunch_audio_scope())))return &asset;return nullptr;
}
void* instance_argument(void* object){return api.il2cpp_class_is_valuetype(api.il2cpp_object_get_class(object))?unbox(object):object;}
bool bundle_alive(void* bundle){
    if(!bundle)return false;auto field=profile_field(api.il2cpp_object_get_class(bundle),"m_CachedPtr");
    if(api.il2cpp_type_get_type(api.il2cpp_field_get_type(field))!=24)throw std::runtime_error("Unity native object pointer field mismatch");
    void* native=nullptr;api.il2cpp_field_get_value(bundle,field,&native);return native!=nullptr;
}
void* load_profile_bundle_with_native_owner(const CN_Bundle& source,TemporaryRoots& roots);
void ensure_bundles(){
    if(unity_ui_thread.load()!=GetCurrentThreadId())throw std::runtime_error("activity asset request outside observed Unity UI thread");
    // A managed GC root does not prevent Unity from unloading its native AB.
    // Reuse only live wrappers, and rescan/reload when the manager released one.
    bool complete=true;for(const auto& source:CN_BUNDLES){if(!overlay_bundle_allowed(source))continue;auto cached=bundles.find(source.name);if(cached==bundles.end()){complete=false;continue;}
        if(!bundle_alive(hold_get(cached->second))){hold_free(cached->second);bundles.erase(cached);complete=false;}}
    if(complete)return;
    // Unity operations execute only on a thread observed in native UI callbacks.
    auto ab=cls("UnityEngine.AssetBundle");TemporaryRoots roots;
    // This CN build strips AssetBundle.GetAllLoadedAssetBundles. Its retained
    // nongeneric Resources API provides the same loaded-object enumeration.
    auto resources=cls("UnityEngine.Resources");void* find_args[]={roots.retain(reflection_type(api.il2cpp_class_get_type(ab)))};
    auto all=roots.retain(invoke(method(resources,"FindObjectsOfTypeAll",1),nullptr,find_args));
    if(!all)throw std::runtime_error("loaded bundle enumeration unavailable");
    auto iter=roots.retain(invoke(method(api.il2cpp_object_get_class(all),"GetEnumerator",0),instance_argument(all)));
    if(!iter)throw std::runtime_error("loaded bundle iterator unavailable");
    auto ic=api.il2cpp_object_get_class(iter);auto move=method(ic,"MoveNext",0),current=method(ic,"get_Current",0);
    if(!current)current=method(ic,"System.Collections.IEnumerator.get_Current",0);
    for(unsigned limit=0;limit<4096;++limit){uint8_t yes=0;if(!read(unbox(invoke(move,instance_argument(iter))),yes))throw std::runtime_error("bundle iterator result unavailable");if(!yes)break;
        auto item=invoke(current,instance_argument(iter));if(!bundle_alive(item))continue;auto name=text(invoke(method(api.il2cpp_object_get_class(item),"get_name",0),item));
        for(const auto& source:CN_BUNDLES)if(name==source.name&&!bundles.count(name)){auto h=hold_new(item,false);if(!h)throw std::runtime_error("bundle root unavailable");bundles[name]=h;break;}
        if(limit==4095)throw std::runtime_error("bundle enumeration bound");
    }
    for(const auto& source:CN_BUNDLES){if(!overlay_bundle_allowed(source)||bundles.count(source.name))continue;
        auto bundle=roots.retain(load_profile_bundle_with_native_owner(source,roots));
        if(!bundle_alive(bundle))throw std::runtime_error(std::string("bundle load rejected: ")+source.name);auto h=hold_new(bundle,false);if(!h)throw std::runtime_error("bundle root unavailable");bundles[source.name]=h;
    }
}
#include "native_asset_load_diagnostics.inc"
void* load_asset(void* id){
    std::lock_guard<std::recursive_mutex> lock(cache_lock);auto found=overlay_asset(id);if(!found||found->scene)return nullptr;ensure_bundles();
    auto ab=cls("UnityEngine.AssetBundle"),obj=cls("UnityEngine.Object");TemporaryRoots roots;roots.retain(id);
    auto it=bundles.find(found->bundle);if(it==bundles.end())throw std::runtime_error(std::string("overlay root bundle missing: ")+found->bundle+" asset="+found->asset);
    auto bundle=roots.retain(hold_get(it->second));void* args[]={roots.retain(string_new(found->asset)),roots.retain(reflection_type(api.il2cpp_class_get_type(obj)))};
    auto result=roots.retain(invoke(method(ab,"LoadAsset",2),bundle,args));if(!result)throw std::runtime_error(native_asset_load_failure(*found,id,bundle,it->second));++asset_hits;report("native-fallback-asset-loaded");return result;
}
void* main_sprite(void* id,int32_t level,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()&&act(id))try{return load_asset(string_new("activity/[uc]act2autochess/private/terminal_entry_sprite.asset"));}catch(const std::exception& ex){report((std::string("terminal-activity-sprite-failed: ")+ex.what()).c_str());return nullptr;}lease.release();return old_main_sprite(id,level,m);}
void native_asset_error(const std::string& error){{std::lock_guard<std::mutex> lock(report_lock);if(asset_errors.count(error)||asset_errors.size()<16)++asset_errors[error];}report("native-resource-registration-failed");}
void dictionary_set(void* map,void* key,void* value){if(!map)throw std::runtime_error("resource dictionary missing");void* args[]={key,value};invoke(method(api.il2cpp_object_get_class(map),"set_Item",2),map,args);}
void* dictionary_find(void* map,void* key){if(!map)throw std::runtime_error("resource dictionary missing");void* value=nullptr;void* args[]={key,&value};uint8_t present=0;if(!read(unbox(invoke(method(api.il2cpp_object_get_class(map),"TryGetValue",2),map,args)),present))throw std::runtime_error("resource dictionary result missing");return present?value:nullptr;}
void* array_index_method(void* array,const char* name,int count){
    for(auto c=api.il2cpp_object_get_class(array);c;c=api.il2cpp_class_get_parent(c)){void* it=nullptr;while(auto m=api.il2cpp_class_get_methods(c,&it))if(std::strcmp(api.il2cpp_method_get_name(m),name)==0&&int(api.il2cpp_method_get_param_count(m))==count&&api.il2cpp_type_get_type(api.il2cpp_method_get_param(m,count-1))==8)return m;}
    throw std::runtime_error("resource array index method missing");
}
#include "native_resource_owner.inc"
void register_native_dependencies(void* provider,TemporaryRoots& roots){register_profile_native_dependencies(provider,roots);}

void* register_overlay_bundle(void* provider,const CN_Asset& asset,TemporaryRoots& roots){
    if(auto info=native_profile_bundle_info(provider,asset.bundle,roots,true))return info;
    auto manager=field_object(provider,"m_manager"),active_map=field_object(manager,"m_activeBundlesMap"),infos=field_object(manager,"m_bundleNameToBundleInfoMap");
    auto name=roots.retain(string_new(asset.bundle));
    if(auto holder=dictionary_find(active_map,name)){auto info=field_object(holder,"<info>k__BackingField");uint8_t destroyed=0;
        auto f=profile_field(api.il2cpp_object_get_class(holder),"<isDestroyed>k__BackingField");api.il2cpp_field_get_value(holder,f,&destroyed);
        if(!destroyed&&info&&field_object(holder,"<ab>k__BackingField")==hold_get(bundles.at(asset.bundle)))return info;
        throw std::runtime_error("activity bundle name already owned by another native holder");
    }
    auto groups=field_object(manager,"m_activeBundleSCCGroups");if(!groups)throw std::runtime_error("native SCC groups missing");
    int32_t size=0;if(!read(unbox(invoke(method(api.il2cpp_object_get_class(groups),"get_Length",0),groups)),size)||size<1||size>65534)throw std::runtime_error("native SCC array length invalid");
    auto info=roots.retain(object_new(cls("Torappu.Resource.AB.ABResourceManager+BundleInfo")));invoke(method(api.il2cpp_object_get_class(info),".ctor",0),info);
    uint8_t no=0,yes=1;auto c=api.il2cpp_object_get_class(info);set_object_field(info,profile_field(c,"name"),name);set_field(info,profile_field(c,"isCacheable"),&no);set_field(info,profile_field(c,"isRetained"),&yes);set_field(info,profile_field(c,"sccIndex"),&size);
    auto holder=roots.retain(object_new(cls("Torappu.Resource.AB.BundleHolder")));void* ctor_args[]={hold_get(bundles.at(asset.bundle)),info};invoke(method(api.il2cpp_object_get_class(holder),".ctor",2),holder,ctor_args);
    // A dedicated valid SCC and one retained reference keep native unload/ref
    // accounting intact. Existing CN bundle groups and holders are not replaced.
    int32_t retained_ref=1;void* ref_args[]={&retained_ref};invoke(method(api.il2cpp_object_get_class(holder),"AddAssetRef",1),holder,ref_args);
    auto element=api.il2cpp_class_get_element_class(api.il2cpp_object_get_class(groups));auto group=roots.retain(object_new(element));invoke(method(element,".ctor",0),group);void* add_args[]={holder};invoke(method(element,"Add",1),group,add_args);
    auto expanded=roots.retain(array_new(element,uintptr_t(size)+1));auto getter=array_index_method(groups,"GetValue",1),setter=array_index_method(expanded,"SetValue",2);
    for(int32_t i=0;i<size;++i){void* get_args[]={&i};auto item=invoke(getter,groups,get_args);void* set_args[]={item,&i};invoke(setter,expanded,set_args);}void* last_args[]={group,&size};invoke(setter,expanded,last_args);
    set_object_field(manager,profile_field(api.il2cpp_object_get_class(manager),"m_activeBundleSCCGroups"),expanded);
    dictionary_set(infos,name,info);dictionary_set(active_map,name,holder);return info;
}
void register_overlay_asset(void* provider,void* normalized,const CN_Asset& asset){
    std::lock_guard<std::recursive_mutex> lock(cache_lock);TemporaryRoots roots;roots.retain(normalized);const char* step="existing-mapping";
    try{
    nro_observe_provider(provider);
    // Check the current provider, rather than treating a C++ path cache as proof
    // that a rebuilt manager still has the native mapping and a live holder.
    auto manager=field_object(provider,"m_manager"),asset_map=field_object(provider,"m_assetNameToBundleInfoMap");
    auto metas=field_object(field_object(provider,"assetMetas"),"m_metaMap");auto native_info=native_profile_bundle_info(provider,asset.bundle,roots);auto bundle_name=roots.retain(native_info?field_object(native_info,"name"):string_new(asset.bundle));
    if(manager&&asset_map&&metas){auto info=dictionary_find(asset_map,normalized),meta=dictionary_find(metas,normalized);auto active_map=field_object(manager,"m_activeBundlesMap");
        auto holder=active_map?dictionary_find(active_map,bundle_name):nullptr;
        if(info&&meta&&holder&&field_object(holder,"<info>k__BackingField")==info&&text(field_object(meta,"m_path"))==asset.asset&&text(field_object(meta,"m_bundleName"))==text(bundle_name)){
            uint8_t destroyed=1;api.il2cpp_field_get_value(holder,profile_field(api.il2cpp_object_get_class(holder),"<isDestroyed>k__BackingField"),&destroyed);
            if(!destroyed&&bundle_alive(field_object(holder,"<ab>k__BackingField"))){registered_asset_paths[text(normalized)]=&asset;return;}
        }
    }
    step="ensure-live-bundles";ensure_bundles();
    // Register borrowed dependencies as native holders too. Otherwise Unity has
    // their CABs loaded but later CN UI loads would attempt duplicate bundles.
    step="register-dependencies";register_native_dependencies(provider,roots);
    step="register-root-holder";auto info=register_overlay_bundle(provider,asset,roots);
    step="construct-asset-meta";auto meta=roots.retain(object_new(cls("Torappu.Resource.AB.RuntimeAssetMeta")));auto mc=api.il2cpp_object_get_class(meta);invoke(method(mc,".ctor",0),meta);
    std::string path=asset.asset;auto slash=path.find_last_of('/');auto name=path.substr(slash==std::string::npos?0:slash+1);
    set_object_field(meta,profile_field(mc,"m_name"),roots.retain(string_new(name.c_str())));set_object_field(meta,profile_field(mc,"m_path"),roots.retain(string_new(path.c_str())));
    set_object_field(meta,profile_field(mc,"m_resPath"),normalized);set_object_field(meta,profile_field(mc,"m_bundleName"),roots.retain(field_object(info,"name")));
    // Publish the path only after its bundle and metadata exist. Original generic
    // loaders retain type checking, AsyncResource handling and _OnAssetLoaded.
    step="publish-asset-mapping";dictionary_set(metas,normalized,meta);dictionary_set(asset_map,normalized,info);registered_asset_paths[text(normalized)]=&asset;++asset_registrations;report("native-activity-resource-registered");
    }catch(const std::exception& ex){throw std::runtime_error(std::string(asset.bundle)+" @"+step+" ["+asset.alias+"]: "+ex.what());}
}
void asset_loaded(void* self,void* value,void* bundle,void* id,const void* m){
    old_asset_loaded(self,value,bundle,id,m);RuntimeLease lease(runtime_gate);if(lease&&can_read()&&value){std::lock_guard<std::recursive_mutex> lock(cache_lock);
        if(registered_asset_paths.count(text(id))){++asset_hits;report("native-registered-asset-loaded");}
    }
}
void trace_asset(void* id,const char* source){auto path=text(id);if(path.find("autochess")==std::string::npos)return;bool first=false;{std::lock_guard<std::mutex> lock(report_lock);auto key=std::string(source)+":"+path;auto it=asset_requests.find(key);if(it!=asset_requests.end())++it->second;else if(asset_requests.size()<128){asset_requests[key]=1;first=true;}}if(first)report("activity-asset-request-observed");}
bool overlay_exists(void* id){return overlay_asset(id)!=nullptr;}
void* wrap_async(void* value){auto c=cls("Torappu.Resource.AsyncResource");auto created=object_new(c);void* args[]={value};void* ctor=nullptr;void* it=nullptr;while(auto candidate=api.il2cpp_class_get_methods(c,&it)){if(std::strcmp(api.il2cpp_method_get_name(candidate),".ctor")||api.il2cpp_method_get_param_count(candidate)!=1)continue;char* n=api.il2cpp_type_get_name(api.il2cpp_method_get_param(candidate,0));bool match=n&&std::strcmp(n,"UnityEngine.Object")==0;if(n)api.il2cpp_free(n);if(match){ctor=candidate;break;}}invoke(ctor,created,args);return created;}
void* load(void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"static-load");try{if(auto v=load_asset(id))return v;}catch(const std::exception& ex){native_asset_error(std::string("native-asset-load-failed: ")+text(id)+": "+ex.what());return nullptr;}}lease.release();return old_load(id,m);}
void* asynchronous(void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"static-async");try{if(auto value=load_asset(id))return wrap_async(value);}catch(const std::exception& ex){native_asset_error(std::string("native-async-overlay-failed: ")+text(id)+": "+ex.what());return nullptr;}}lease.release();return old_async(id,m);}
bool exists(void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"static-exists");if(overlay_exists(id))return true;}lease.release();return old_exists(id,m);}
void* ab_load(void* self,void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"ab-load");try{if(auto value=load_asset(id))return value;}catch(const std::exception& ex){native_asset_error(std::string("native-ab-load-failed: ")+text(id)+": "+ex.what());return nullptr;}}lease.release();return old_ab_load(self,id,m);}
void* ab_async(void* self,void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"ab-async");try{if(auto value=load_asset(id))return wrap_async(value);}catch(const std::exception& ex){native_asset_error(std::string("native-ab-async-failed: ")+text(id)+": "+ex.what());return nullptr;}}lease.release();return old_ab_async(self,id,m);}
bool ab_exists(void* self,void* id,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"ab-exists");if(overlay_exists(id))return true;}lease.release();return old_ab_exists(self,id,m);}
bool overlay_callback(void* id,void* callback){auto value=load_asset(id);if(!value)return false;if(callback){uint8_t yes=1;void* args[]={&yes,value};invoke(method(api.il2cpp_object_get_class(callback),"Invoke",2),callback,args);}return true;}
void load_callback(void* id,void* callback,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"static-callback");try{if(overlay_callback(id,callback))return;}catch(const std::exception& ex){native_asset_error(std::string("native-static-callback-failed: ")+text(id)+": "+ex.what());return;}}lease.release();old_load_callback(id,callback,m);}
void ab_callback(void* self,void* id,void* callback,const void* m){RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"ab-callback");try{if(overlay_callback(id,callback))return;}catch(const std::exception& ex){native_asset_error(std::string("native-ab-callback-failed: ")+text(id)+": "+ex.what());return;}}lease.release();old_ab_callback(self,id,callback,m);}
void* preprocess(void* self,void* id,const void* m){
    auto normalized=old_preprocess(self,id,m);RuntimeLease lease(runtime_gate);if(lease&&can_read()){trace_asset(id,"ab-preprocess");
        if(auto asset=overlay_asset(id))try{
            // The page's resource request precedes controller.OnLoaded. Scope
            // its construction too; home entry res_holder requests stay outside.
            if(std::strcmp(asset->asset,"dyn/activity/[uc]act2autochess/prefabs/act2autochess_activity_stage.prefab")==0&&unity_ui_thread.load()==GetCurrentThreadId())private_view.store(true);
            if(!normalized)throw std::runtime_error("native path normalization returned null");trace_asset(normalized,"ab-normalized");register_overlay_asset(self,normalized,*asset);
        }catch(const std::exception& ex){native_asset_error(ex.what());}
    }return normalized;
}
using SceneAsync=void*(*)(void*,void*,int32_t,const void*);using SceneSync=bool(*)(void*,void*,int32_t,const void*);
SceneAsync old_scene_async=nullptr;SceneSync old_scene_sync=nullptr;
void prepare_native_scene(void* self,void* id){
    auto asset=overlay_asset(id);if(!asset||!asset->scene)return;
    if(!private_battle_resources.load()||unity_ui_thread.load()!=GetCurrentThreadId())throw std::runtime_error("private scene outside validated battle resource scope");
    TemporaryRoots roots;roots.retain(id);auto normalized=roots.retain(old_preprocess(self,id,bound_methods.at(CN_METHOD_ab_preprocess.token)));
    if(!normalized)throw std::runtime_error("private scene normalization failed");
    register_overlay_asset(self,normalized,*asset);
    auto manager=field_object(self,"m_manager"),holders=field_object(manager,"m_activeBundlesMap");
    auto holder=dictionary_find(holders,roots.retain(string_new(asset->bundle)));uint8_t scene_bundle=0;
    if(!holder)throw std::runtime_error("private scene holder missing");
    api.il2cpp_field_get_value(holder,profile_field(api.il2cpp_object_get_class(holder),"<isSceneBundle>k__BackingField"),&scene_bundle);
    if(!scene_bundle)throw std::runtime_error("private map bundle has no native Unity scene");
    auto basename=[](std::string value){for(auto& c:value){if(c=='\\')c='/';if(c>='A'&&c<='Z')c=char(c-'A'+'a');}auto slash=value.find_last_of('/');if(slash!=std::string::npos)value.erase(0,slash+1);auto dot=value.find_last_of('.');if(dot!=std::string::npos)value.erase(dot);return value;};
    auto names=roots.retain(invoke(method(cls("UnityEngine.AssetBundle"),"GetAllScenePaths",0),field_object(holder,"<ab>k__BackingField")));
    int32_t count=0;if(!names||!read(unbox(invoke(method(api.il2cpp_object_get_class(names),"get_Length",0),names)),count)||count<1||count>64)throw std::runtime_error("private map scene list invalid");
    auto getter=array_index_method(names,"GetValue",1);bool matched=false;
    for(int32_t i=0;i<count;++i){void* args[]={&i};auto scene=roots.retain(invoke(getter,names,args));if(basename(text(scene))==basename(text(id)))matched=true;}
    if(!matched)throw std::runtime_error("private map requested basename differs from bundled Unity scene");
    trace_asset(id,"native-scene");
}
void* scene_async(void* self,void* id,int32_t mode,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return nullptr;
    if(can_read())try{prepare_native_scene(self,id);}catch(const std::exception& ex){native_asset_error(ex.what());return nullptr;}
    lease.release();return old_scene_async(self,id,mode,m);
}
bool scene_sync(void* self,void* id,int32_t mode,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;
    if(can_read())try{prepare_native_scene(self,id);}catch(const std::exception& ex){native_asset_error(ex.what());return false;}
    lease.release();return old_scene_sync(self,id,mode,m);
}
bool private_start_param(void* param){
    auto id=field_object(param,"activityId");return id?act(id):private_view.load();
}
void cancel_join(void* callback){if(!callback)return;uint8_t no=0;void* args[]={&no};invoke(method(api.il2cpp_object_get_class(callback),"Invoke",1),callback,args);++cancelled_battle_joins;}
void guarded_start(void* raw_entry,void* param,const void* m,ServiceStart original){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(!can_read()){lease.release();original(raw_entry,param,m);return;}
    bool target=false;try{target=private_start_param(param);}catch(const std::exception& ex){report((std::string("battle-start-identity-unresolved: ")+ex.what()).c_str());return;}
    if(!target){lease.release();original(raw_entry,param,m);return;}
    if(native_multiplayer_entry_service_start(raw_entry,param,m))return;
    if(original==old_reconnect&&native_multiplayer_entry_service_reconnect(raw_entry,param,m))return;
    if(original==old_reconnect&&native_session_presence_reconnect(raw_entry,param,m))return;
    ++blocked_battles;
    try{
        cnlaunch::CommonJoinEntryRaw payload{};if(!join_entry_class||!read(raw_entry,payload))throw std::runtime_error("native CommonJoinEntry payload unavailable");
        TemporaryRoots roots;auto entry=roots.retain(value_box(join_entry_class,&payload));cancel_join(field_object(entry,"onJoinDone"));report("private-native-battle-bridge-pending-join-cancelled");
    }catch(const std::exception& ex){report((std::string("private-start-callback-unavailable: ")+ex.what()).c_str());}
}
void start(void* entry,void* param,const void* m){guarded_start(entry,param,m,old_start);}
void reconnect(void* entry,void* param,const void* m){guarded_start(entry,param,m,old_reconnect);}
bool native_battle_launch_training(void*,void*,const void*);
void start_training(void* param,void* callback,const void* m){
    RuntimeLease lease(runtime_gate);if(!lease||closing.load())return;
    if(native_battle_launch_training(param,callback,m))return;
    if(!can_read()){lease.release();old_training_start(param,callback,m);return;}
    bool target=false;try{target=private_start_param(param);}catch(const std::exception& ex){report((std::string("training-start-identity-unresolved: ")+ex.what()).c_str());return;}
    if(!target){lease.release();old_training_start(param,callback,m);return;}
    ++blocked_battles;try{cancel_join(callback);report("private-native-training-bridge-pending-join-cancelled");}catch(const std::exception& ex){report((std::string("private-training-callback-unavailable: ")+ex.what()).c_str());}
}
bool secure(void* url,void** output,int32_t* error,const void* m){RuntimeLease lease(runtime_gate);if(!lease||closing.load())return false;if(can_read()&&text(url).rfind(private_backend_origin()+"/",0)==0){if(output)*output=url;if(error)*error=0;return true;}lease.release();return old_secure(url,output,error,m);}
void* headers(void* self,void* request,const void* m){
    RuntimeLease lease(runtime_gate);if(lease&&can_read()){bool is_private=false;if(request_class){auto f=api.il2cpp_class_get_field_from_name(request_class,"serviceCode");if(!f){lease.release();return old_header(self,request,m);}auto off=api.il2cpp_field_get_offset(f);auto header=api.il2cpp_object_header_size();void* code=nullptr;if(off>=header&&read(static_cast<char*>(request)+off-header,code))is_private=activity_route(text(code));}
        if(is_private){try{auto c=api.il2cpp_class_from_type(api.il2cpp_method_get_return_type(bound_methods.at(CN_METHOD_headers.token)));auto access_token=nbb_access_token;if(nbb_participant_profile)access_token=native_multiplayer_participant_token();auto value="{\"Content-Type\":\"application/json\",\"x-local-profile\":\"local-stronghold\""+std::string(access_token.empty()?"":",\"Authorization\":"+quote("Bearer "+access_token))+"}";return json(c,value);}catch(...){report("private-header-generation-failed");return nullptr;}}}
    lease.release();return closing.load()?nullptr:old_header(self,request,m);
}
void* runtime_invoke(void* m,void* o,void** args,void** error){auto result=old_invoke(m,o,args,error);RuntimeLease lease(runtime_gate);if(lease&&!closing.load()&&!ready.load()&&api.il2cpp_thread_current()&&api.il2cpp_domain_get())ready.store(true);return result;}
void shutdown(){native_fault_capture_scope(false);active.store(false);runtime_gate.close_and_drain();while(setup_busy.load())Sleep(1);native_multiplayer_transport_shutdown();native_multiplayer_own_start_shutdown();native_multiplayer_preparation_view_shutdown();native_garrison_shutdown();native_fault_capture_shutdown();native_private_liveness_shutdown();report("runtime-shutdown");old_shutdown();}

std::string normalized_type(const std::string& input){std::string out;for(size_t i=0;i<input.size();++i){char c=input[i];if(c=='`'){while(i+1<input.size()&&input[i+1]>='0'&&input[i+1]<='9')++i;}else if(c!=' '&&c!='\t')out+=c=='+'?'.':c;}return out;}
bool matches_type(void* type,const char* expected){char* name=api.il2cpp_type_get_name(type);bool result=name&&normalized_type(name)==normalized_type(expected);if(name)api.il2cpp_free(name);return result;}
template<class Spec> void* bind_method(const Spec& spec){
    auto existing=bound_methods.find(spec.token);if(existing!=bound_methods.end())return existing->second;
    auto c=cls(spec.type);if(!c)throw std::runtime_error("required class missing");void* it=nullptr,*selected=nullptr;
    while(auto m=api.il2cpp_class_get_methods(c,&it))if(api.il2cpp_method_get_token(m)==spec.token){selected=m;break;}
    if(!selected||std::strcmp(api.il2cpp_method_get_name(selected),spec.name)||api.il2cpp_method_get_param_count(selected)!=spec.parameters||api.il2cpp_method_is_generic(selected))throw std::runtime_error(std::string("method contract mismatch: ")+spec.type+"."+spec.name);
    uint32_t implementation=0;if(bool(api.il2cpp_method_get_flags(selected,&implementation)&16)!=spec.is_static)throw std::runtime_error("method static flag mismatch");
    if(!matches_type(api.il2cpp_method_get_return_type(selected),spec.result))throw std::runtime_error(std::string("native return type mismatch: ")+spec.type+"."+spec.name);
    for(unsigned n=0;n<spec.parameters;++n)if(!matches_type(api.il2cpp_method_get_param(selected,n),spec.arguments[n]))throw std::runtime_error(std::string("native parameter type mismatch: ")+spec.type+"."+spec.name);
    bound_methods[spec.token]=selected;
    void* pointer=nullptr;if(!read(selected,pointer)||!pointer)throw std::runtime_error("native method pointer missing");
    MEMORY_BASIC_INFORMATION page{};if(!VirtualQuery(pointer,&page,sizeof(page))||page.State!=MEM_COMMIT||page.Type!=MEM_IMAGE||page.AllocationBase!=runtime||!(page.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY))||(page.Protect&PAGE_GUARD))throw std::runtime_error(std::string("native target outside pinned GameAssembly executable image: ")+spec.type+"."+spec.name);
    auto runtime_rva=uint64_t(reinterpret_cast<uintptr_t>(pointer)-reinterpret_cast<uintptr_t>(runtime));
    {std::lock_guard<std::mutex> lock(report_lock);target_evidence[std::string(spec.type)+"."+spec.name]={spec.rva,runtime_rva};}
    if(runtime_rva!=spec.rva)++runtime_target_differences;
    ++bindings_validated;return selected;
}
template<class T> void hook(const CN_Method& spec,T handler,T& original){
    if(bound_methods.count(spec.token))return;auto selected=bind_method(spec);void* pointer=nullptr;if(!read(selected,pointer))throw std::runtime_error("validated native pointer unavailable");
    void* replacement=nullptr;std::memcpy(&replacement,&handler,sizeof(handler));void* trampoline=nullptr;
    if(MH_CreateHook(pointer,replacement,&trampoline)!=MH_OK)throw std::runtime_error("hook create");std::memcpy(&original,&trampoline,sizeof(original));
}
template<class T> void export_hook(T target,T handler,T& original){void* p=nullptr,*h=nullptr,*o=nullptr;std::memcpy(&p,&target,sizeof(p));std::memcpy(&h,&handler,sizeof(h));if(MH_CreateHook(p,h,&o)!=MH_OK)throw std::runtime_error("runtime hook");std::memcpy(&original,&o,sizeof(original));if(MH_EnableHook(p)!=MH_OK)throw std::runtime_error("runtime hook enable");}
#include "native_pending_journal.inc"
#include "native_multiplayer_transport.inc"
#include "native_battle_data.inc"
#include "native_battle_launch.inc"
#include "native_multiplayer_entry.inc"
#include "native_multiplayer_help.inc"
#include "native_multiplayer_help_counter.inc"
#include "native_multiplayer_observation.inc"
#include "native_multiplayer_own_start.inc"
#include "native_multiplayer_mode_runes.inc"
#include "native_multiplayer_dispatch.inc"
#include "native_multiplayer_emotes.inc"
#include "native_multiplayer_dead_observation.inc"
#include "native_multiplayer_observation_ui.inc"
#include "native_multiplayer_map_adapter.inc"
#include "native_multiplayer_map_hooks.inc"
#include "native_multiplayer_map_height.inc"
#include "native_multiplayer_field.inc"
#include "native_multiplayer_boss.inc"
#include "native_multiplayer_deadline.inc"
#include "native_multiplayer_enemies.inc"
#include "native_multiplayer_start_level.inc"
#include "native_multiplayer_launch.inc"
namespace preparation_view_native{cnjson::Value status();}
std::string native_multiplayer_report(){return "{\"transport\":"+native_multiplayer_transport_status_json()+",\"dispatch\":"+native_multiplayer_dispatch_status_json()+",\"help\":"+cnjson::dump(native_multiplayer_help_status())+",\"helpCounter\":"+cnjson::dump(native_multiplayer_help_counter_status())+",\"field\":"+native_multiplayer_field_status_json()+",\"observation\":"+cnjson::dump(native_multiplayer_observation_status())+",\"preparationView\":"+cnjson::dump(preparation_view_native::status())+",\"ownStart\":"+cnjson::dump(native_multiplayer_own_start_status())+",\"boss\":"+native_multiplayer_boss_status_json()+",\"social\":"+native_multiplayer_social_status_json()+",\"enemyInitializations\":"+std::to_string(nme_initializations.load())+",\"enemyInitializationFailures\":"+std::to_string(nme_failures.load())+",\"helpDeferredFinishes\":"+std::to_string(nmho_deferred.load())+",\"deadlineFinishes\":"+std::to_string(nmd_deadline_finishes.load())+",\"entryJoins\":"+std::to_string(native_multi_entry_joins.load())+",\"entryRequests\":"+std::to_string(native_multi_entry_requests.load())+",\"entryFailures\":"+std::to_string(native_multi_entry_failures.load())+",\"nativeRuntimeAccepted\":false}";}
#include "native_session_presence.inc"
#include "native_solo_status_ui.inc"
#include "native_preparation_tokens.inc"
#include "native_multiplayer_squad_owner.inc"
#include "native_shop_free_refresh.inc"
#include "native_item_target_rules.inc"
#include "native_private_identifier.inc"
#include "native_level_compat.inc"
#include "native_private_liveness.inc"
#include "native_private_network.inc"
#include "native_private_room_input.inc"
#include "native_private_socket.inc"
#include "native_multiplayer_social.inc"
#include "native_multiplayer_result_personal.inc"
#include "native_private_player_card.inc"
#include "native_multiplayer_preparation_native.inc"
#include "native_multiplayer_preparation_facade.inc"
#include "native_multiplayer_preparation_handoff.inc"
#include "native_multiplayer_preparation_request.inc"
#include "native_multiplayer_preparation_operation.inc"
bool prepare_private_battle(const char* stage){
    if(!native_battle_prepare_for_stage(stage))return false;
    try{native_private_liveness_confirm_current();return true;}
    catch(const std::exception& ex){nb_error("private-liveness-prepare",ex);native_battle_session_cancel();return false;}
}
std::string native_battle_report(){
    return "{\"launchBindingsReady\":"+std::string(native_battle_launch_bindings_ready.load()?"true":"false")+",\"sessionPrepared\":"+(native_battle_launch_session_prepared.load()?"true":"false")+
        ",\"launchPending\":"+(native_battle_launch_pending.load()?"true":"false")+",\"sceneEntered\":"+(native_battle_launch_scene_entered.load()?"true":"false")+
        ",\"loadingComplete\":"+(native_battle_launch_loading_complete.load()?"true":"false")+",\"localStarts\":"+std::to_string(native_battle_launch_local_starts.load())+
        ",\"independentEntries\":"+std::to_string(native_single_entries.load())+
        ",\"fullStrategyRanges\":"+std::to_string(native_single_band_ranges.load())+
        ",\"singleStrategyQueries\":"+std::to_string(native_single_band_filter_queries.load())+
        ",\"strategyRangeFailures\":"+std::to_string(native_single_band_range_failures.load())+
        ",\"soloStrategyChoiceProjections\":"+std::to_string(native_single_team_choice_projections.load())+
        ",\"prebattlePresentation\":{\"registeredModels\":"+std::to_string(npb_models.load())+
        ",\"teamCopies\":"+std::to_string(npb_teams.load())+",\"stageInputs\":"+std::to_string(npb_inputs.load())+
        ",\"selections\":"+std::to_string(npb_selections.load())+",\"bannedLists\":"+std::to_string(npb_banned_lists.load())+
        ",\"chessGroups\":"+std::to_string(npb_chess_groups.load())+",\"battlePanels\":"+std::to_string(npb_hud_models.load())+
        ",\"battleBanCounts\":"+std::to_string(npb_hud_banned_counts.load())+",\"failures\":"+std::to_string(npb_failures.load())+"}"+
        ",\"rejections\":"+std::to_string(native_battle_launch_rejections.load())+",\"firstLaunchError\":"+quote(native_battle_first_launch_error())+",\"lastLaunchError\":"+quote(native_battle_last_launch_error())+",\"sceneLoad\":"+native_battle_scene_status_json()+",\"dataBridge\":"+native_battle_data_status_json()+",\"levelCompatibility\":"+native_level_compat_status()+",\"privateLiveness\":"+native_private_liveness_status_json()+",\"identifierProjection\":"+native_private_identifier_status_json()+"}";
}
void setup(bool enable){
    request_class=cls("Torappu.Network.Request");
    // Build local managed profiles before exposing the old activity. A failed
    // DTO construction must not leave a newly visible entry reading account data.
    auto local=cached("player","Torappu.PlayerActivity+PlayerActAutoChessActivity",activity_json);
    if(!field_object(local,"mode")||!field_object(local,"band"))throw std::runtime_error("player activity incomplete");
    auto local_model=cached("model","Torappu.PlayerDataModel",model_json);
    auto troop=field_object(local_model,"troop");if(!troop||!field_object(troop,"chars"))throw std::runtime_error("local roster incomplete");
    auto chars=field_object(troop,"chars");int32_t char_count=0;auto count=invoke(method(api.il2cpp_object_get_class(chars),"get_Count",0),chars);
    if(!count||!read(unbox(count),char_count)||char_count!=434)throw std::runtime_error("local roster count mismatch");local_roster_count.store(unsigned(char_count));
    auto char_map_field=api.il2cpp_class_get_field_from_name(cls("Torappu.PlayerData"),"m_charIdInstMap");if(!char_map_field)throw std::runtime_error("private char map field missing");
    const std::string char_map_key="char-map";json(api.il2cpp_class_from_type(api.il2cpp_field_get_type(char_map_field)),char_map_json,&char_map_key);
    auto perm=cached("perm","Torappu.PlayerAutoChessPerm",perm_json);
    auto model_class=api.il2cpp_object_get_class(local_model);auto perm_field=api.il2cpp_class_get_field_from_name(model_class,"autoChessPerm");
    if(!perm_field)throw std::runtime_error("permission field missing");set_object_field(local_model,perm_field,perm);
    auto activity_class=cls("Torappu.PlayerActivity");auto activity=object_new(activity_class);
    auto dict_field=api.il2cpp_class_get_field_from_name(activity_class,"actAutoChessActivityList");if(!dict_field)throw std::runtime_error("activity collection missing");
    auto dict_class=api.il2cpp_class_from_type(api.il2cpp_field_get_type(dict_field));auto dict=object_new(dict_class);
    invoke(method(dict_class,".ctor",0),dict);void* add_args[]={string_new("act2autochess"),local};invoke(method(dict_class,"Add",2),dict,add_args);
    set_object_field(activity,dict_field,dict);auto activity_field=api.il2cpp_class_get_field_from_name(model_class,"activity");if(!activity_field)throw std::runtime_error("activity model missing");set_object_field(local_model,activity_field,activity);
    hook(CN_METHOD_parse,route,old_parse);hook(CN_METHOD_secure,secure,old_secure);hook(CN_METHOD_headers,headers,old_header);
    hook(CN_METHOD_player,local_player,old_player);hook(CN_METHOD_model,cn::local_model,old_model);hook(CN_METHOD_character,character,old_char);
    hook(CN_METHOD_controller,controller_player,old_controller);hook(CN_METHOD_in_player,in_player,old_in_player);hook(CN_METHOD_training,training,old_training);hook(CN_METHOD_unlock,unlock,old_unlock);
    hook(CN_METHOD_sort,sorted,old_sort);hook(CN_METHOD_recent,recent,old_recent);hook(CN_METHOD_open,open_page,old_open);hook(CN_METHOD_loaded,loaded,old_loaded);
    bind_method(CN_METHOD_handle_sync);hook(CN_METHOD_sync,sync_request,old_sync_request);hook(CN_METHOD_init_model,init_model,old_init_model);hook(CN_METHOD_main_sprite,main_sprite,old_main_sprite);
    hook(CN_METHOD_reward_single,reward_single,old_reward_single);hook(CN_METHOD_reward_all,reward_all,old_reward_all);hook(CN_METHOD_mission_all,mission_all,old_mission_all);hook(CN_METHOD_mission_item,mission_item,old_mission_item);
    hook(CN_METHOD_roster_reset,roster_reset,old_roster_reset);hook(CN_METHOD_guidebook,guidebook,old_guidebook);
    hook(CN_METHOD_roster_sync,roster_sync,old_roster_sync);hook(CN_METHOD_shop_load,shop_load,old_shop_load);hook(CN_METHOD_shop_refresh,shop_refresh,old_shop_refresh);hook(CN_METHOD_shop_diy_selected,shop_diy_selected,old_shop_diy_selected);bind_method(CN_METHOD_shop_id);
    hook(CN_METHOD_save_diy,save_diy,old_save_diy);hook(CN_METHOD_save_quick,save_quick,old_save_quick);hook(CN_METHOD_save_detail,save_detail,old_save_detail);hook(CN_METHOD_save_cancel,save_cancel,old_save_cancel);
    hook(CN_METHOD_assist_top,assist_top,old_assist_top);hook(CN_METHOD_assist_detail,assist_detail,old_assist_detail);
    for(const auto* spec:{&CN_METHOD_cached_pool,&CN_METHOD_quick_done,&CN_METHOD_quick_suc,&CN_METHOD_detail_suc,&CN_METHOD_diy_build,&CN_METHOD_diy_suc,&CN_METHOD_cancel_suc})bind_method(*spec);
    hook(CN_METHOD_tutorial_key,tutorial_key,old_tutorial_key);hook(CN_METHOD_tutorial_resume_key,tutorial_resume_key,old_tutorial_resume_key);hook(CN_METHOD_shop_guidebook,shop_guidebook,old_shop_guidebook);
    bind_method(CN_METHOD_entry_model);hook(CN_METHOD_entry_rewards,entry_rewards,old_entry_rewards);hook(CN_METHOD_entry_medals,entry_medals,old_entry_medals);hook(CN_METHOD_entry_daily,entry_daily,old_entry_daily);
    // Validate the exact generic constructors and completion path before making
    // the activity visible. These managed checks do not use Unity engine APIs.
    {
        auto reset_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(CN_METHOD_roster_reset.token),0));uint32_t reset_align=0;
        if(!api.il2cpp_class_is_valuetype(reset_class)||api.il2cpp_class_value_size(reset_class,&reset_align)!=56)throw std::runtime_error("native roster reset indirect-value ABI mismatch");
        auto input_field=profile_field(reset_class,"input");auto input_offset=api.il2cpp_field_get_offset(input_field);auto object_header=api.il2cpp_object_header_size();
        if(api.il2cpp_class_is_valuetype(api.il2cpp_class_from_type(api.il2cpp_field_get_type(input_field)))||input_offset<object_header||input_offset-object_header!=0)throw std::runtime_error("native roster reset input field mismatch");roster_input_offset=0;
        auto sync_input_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(CN_METHOD_roster_sync.token),0));
        if(api.il2cpp_class_is_valuetype(sync_input_class)||api.il2cpp_type_get_type(api.il2cpp_field_get_type(profile_field(sync_input_class,"actId")))!=14)throw std::runtime_error("native roster input reference ABI mismatch");
        auto focus_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(CN_METHOD_shop_refresh.token),1));uint32_t focus_align=0;
        if(!api.il2cpp_class_is_valuetype(focus_class)||api.il2cpp_class_value_size(focus_class,&focus_align)!=32)throw std::runtime_error("native shop focus indirect-value ABI mismatch");
        for(const auto* pool_spec:{&CN_METHOD_cached_pool,&CN_METHOD_diy_build}){
            auto pool_class=api.il2cpp_class_from_type(api.il2cpp_method_get_return_type(bound_methods.at(pool_spec->token)));const bool diy=pool_spec==&CN_METHOD_diy_build;
            const auto test=std::string("{\"test-slot\":{\"skillIndex\":1,\"currentEquip\":null")+(diy?",\"diyChar\":\"test-char\",\"origChessId\":null":"")+"}}";
            TemporaryRoots sample_roots;auto sample=sample_roots.retain(json(pool_class,test));if(cnjson::dump(native_deploy_pool(sample,diy))!=cnjson::dump(cnjson::parse(test)))throw std::runtime_error("native configuration export preflight mismatch");
        }
        for(const auto* callback:{&CN_METHOD_quick_suc,&CN_METHOD_detail_suc,&CN_METHOD_diy_suc,&CN_METHOD_cancel_suc}){
            TemporaryRoots response_roots;auto response_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(callback->token),0));auto response=response_roots.retain(object_new(response_class));invoke(method(response_class,".ctor",0),response);
        }
        TemporaryRoots roots;auto copy=clone_season_map(dict_class,dict,roots);auto key=roots.retain(string_new("act2autochess"));auto other=roots.retain(object_new(api.il2cpp_object_get_class(local)));void* args[]={key,other};invoke(method(dict_class,"set_Item",2),copy,args);void* key_args[]={key};
        if(invoke(method(dict_class,"get_Item",1),dict,key_args)!=local||invoke(method(dict_class,"get_Item",1),copy,key_args)!=other)throw std::runtime_error("season copy mutates source container");
        auto shadow=roots.retain(invoke(method(model_class,"ShallowClone",0),local_model));if(!shadow||shadow==local_model)throw std::runtime_error("native shallow clone aliases source");
        auto response=cached("sync-response","Torappu.Activity.AutoChess.ActAutoChessSyncInfoResponse",sync_json);if(!field_object(response,"changed"))throw std::runtime_error("private sync response incomplete");
        auto hc=api.il2cpp_class_from_type(api.il2cpp_method_get_return_type(bound_methods.at(CN_METHOD_sync.token)));auto fallback=roots.retain(object_new(hc));invoke(method(hc,".ctor",0),fallback);invoke(method(hc,"MarkFinish",0),fallback);uint8_t waiting=1;
        if(!read(unbox(invoke(method(hc,"get_keepWaiting",0),fallback)),waiting)||waiting)throw std::runtime_error("private sync completion preflight failed");protect("sync-fallback-handler",fallback);
    }
    hook(CN_METHOD_home_enter,home_enter,old_home_enter);hook(CN_METHOD_home_resume,home_resume,old_home_resume);
    hook(CN_METHOD_load,load,old_load);hook(CN_METHOD_async,asynchronous,old_async);hook(CN_METHOD_exists,exists,old_exists);
    static_assert(CN_METHOD_load.is_static&&CN_METHOD_async.is_static&&CN_METHOD_exists.is_static&&CN_METHOD_load_callback.is_static,"ResourceManager facade is static");
    static_assert(!CN_METHOD_ab_load.is_static&&!CN_METHOD_ab_async.is_static&&!CN_METHOD_ab_exists.is_static&&!CN_METHOD_ab_callback.is_static&&!CN_METHOD_ab_preprocess.is_static,"AB resource provider is an instance");
    hook(CN_METHOD_load_callback,load_callback,old_load_callback);hook(CN_METHOD_ab_load,ab_load,old_ab_load);hook(CN_METHOD_ab_async,ab_async,old_ab_async);hook(CN_METHOD_ab_callback,ab_callback,old_ab_callback);hook(CN_METHOD_ab_exists,ab_exists,old_ab_exists);hook(CN_METHOD_ab_preprocess,preprocess,old_preprocess);
    hook(CN_METHOD_ab_asset_loaded,asset_loaded,old_asset_loaded);
    hook(CN_METHOD_ab_scene_async,scene_async,old_scene_async);hook(CN_METHOD_ab_scene,scene_sync,old_scene_sync);
    hook(CN_METHOD_start,start,old_start);hook(CN_METHOD_reconnect,reconnect,old_reconnect);hook(CN_METHOD_start_training,start_training,old_training_start);
    static_assert(CN_METHOD_start.is_static&&CN_METHOD_reconnect.is_static&&CN_METHOD_start_training.is_static,"AutoChessService launch APIs are static");
    join_entry_class=api.il2cpp_class_from_type(api.il2cpp_method_get_param(bound_methods.at(CN_METHOD_start.token),0));uint32_t join_alignment=0;
    if(!join_entry_class||!api.il2cpp_class_is_valuetype(join_entry_class)||api.il2cpp_class_value_size(join_entry_class,&join_alignment)!=32)throw std::runtime_error("CommonJoinEntry indirect-value ABI mismatch");
    auto join_callback_class=api.il2cpp_class_from_type(api.il2cpp_field_get_type(profile_field(join_entry_class,"onJoinDone")));auto join_invoke=method(join_callback_class,"Invoke",1);
    if(!join_invoke||api.il2cpp_type_get_type(api.il2cpp_method_get_param(join_invoke,0))!=2||api.il2cpp_type_get_type(api.il2cpp_method_get_return_type(join_invoke))!=1)throw std::runtime_error("CommonJoinEntry completion signature mismatch");
    native_battle_data_setup();native_battle_launch_configure(prepare_private_battle,native_battle_data_ready);
    native_battle_launch_set_tick(native_battle_data_tick);native_battle_launch_set_loaded(native_battle_data_loaded);native_battle_launch_setup();
    native_session_presence_setup();
    native_solo_status_ui_setup();
    native_preparation_tokens_setup();
    native_multiplayer_squad_owner_setup();
    native_shop_free_refresh_setup();
    native_item_target_rules_setup();
    native_level_compat_setup();
    native_private_liveness_setup(enable);
    native_multiplayer_social_setup();
    native_multiplayer_result_personal_setup();
    native_private_player_card_setup();
    native_private_identifier_setup();
    native_multiplayer_entry_setup();
    native_multiplayer_dispatch_setup();native_multiplayer_dead_observation_setup();native_multiplayer_observation_ui_setup();native_private_network_setup();native_private_room_input_setup();native_private_socket_setup();
    native_multiplayer_emotes_setup();
    native_multiplayer_boss_setup();
    native_multiplayer_deadline_setup();
    native_multiplayer_help_setup();
    native_multiplayer_help_counter_setup();
    native_multiplayer_observation_setup();
    native_multiplayer_field_setup();
    native_multiplayer_mode_runes_setup();
    native_multiplayer_preparation_view_setup();native_multiplayer_enemies_setup();
    native_multiplayer_preparation_view_operation_setup();
    if(!enable){report("discovery-validated-local-profiles-and-bindings");return;}
    if(closing.load())throw std::runtime_error("runtime closing before activity hooks");
    if(MH_QueueEnableHook(MH_ALL_HOOKS)!=MH_OK||MH_ApplyQueued()!=MH_OK)throw std::runtime_error("hook activation");
    if(closing.load())throw std::runtime_error("runtime closing during activity hook activation");
    active.store(true);native_multiplayer_transport_setup();native_fault_capture_install();report("entry-preview-hooks-active");
}
struct Args{std::wstring proxy;uint32_t mode;};
DWORD WINAPI worker(void* raw){
    std::unique_ptr<Args> args(static_cast<Args*>(raw));void* attached=nullptr;
    try{
        auto root=folder(args->proxy);if(!same(path(nullptr),root+L"\\Arknights.exe")||!same(path(module),root+L"\\plugin\\StrongholdProtocolCN\\StrongholdCNActivity.dll"))throw std::runtime_error("process path");
        for(const auto& f:CN_FILES)if(!hash(root+L"\\"+f.path,f.hash))throw std::runtime_error("client build changed");
        for(unsigned n=0;n<1800&&!closing.load();++n){runtime=GetModuleHandleW(L"GameAssembly.dll");if(runtime)break;Sleep(100);}if(!runtime||!same(path(runtime),root+L"\\GameAssembly.dll")||!SO_LoadIl2CppApi(runtime,api))throw std::runtime_error("runtime exports");
        if(args->mode==0){report("probe-verified-no-hooks");return 0;}
        native_verify_bundle_hashes();
        configure_private_backend(root);ensure_private_backend();
#define EXTRA(symbol,var) if(!SO_LoadIl2CppExport(runtime,symbol,var))throw std::runtime_error("required export")
        EXTRA("il2cpp_string_new",string_new);EXTRA("il2cpp_object_new",object_new);EXTRA("il2cpp_object_unbox",unbox);EXTRA("il2cpp_type_get_object",reflection_type);EXTRA("il2cpp_field_set_value",set_field);EXTRA("il2cpp_field_set_value_object",set_object_field);EXTRA("il2cpp_gchandle_new",hold_new);EXTRA("il2cpp_gchandle_new_weakref",hold_weak);EXTRA("il2cpp_gchandle_get_target",hold_get);
        EXTRA("il2cpp_gchandle_free",hold_free);EXTRA("il2cpp_value_box",value_box);EXTRA("il2cpp_array_new",array_new);
#undef EXTRA
        activity_json=get(L"/bridge/act2autochess/player-activity");model_json=get(L"/bridge/act2autochess/player-model");perm_json=get(L"/bridge/act2autochess/perm");char_map_json=get(L"/bridge/act2autochess/char-map");sync_json=get(L"/bridge/act2autochess/sync-info");
        sync_json=native_multiplayer_transport_resume_sync(sync_json);
        if(MH_Initialize()!=MH_OK)throw std::runtime_error("hook runtime initialization");export_hook(api.il2cpp_shutdown,shutdown,old_shutdown);export_hook(api.il2cpp_runtime_invoke,runtime_invoke,old_invoke);
        for(unsigned n=0;n<1800&&!ready.load()&&!closing.load();++n)Sleep(100);if(!ready.load()||closing.load())throw std::runtime_error("runtime ready timeout");
        setup_busy.store(true);if(closing.load())throw std::runtime_error("runtime closing");
        attached=api.il2cpp_thread_attach(api.il2cpp_domain_get());if(!attached)throw std::runtime_error("runtime attach");
        // Observe the game's existing model without copying account contents.
        // Runtime-invoke readiness alone can precede login/GameData setup.
        hook(CN_METHOD_model,cn::local_model,old_model);observing_model.store(true);
        if(MH_EnableHook(reinterpret_cast<char*>(runtime)+CN_METHOD_model.rva)!=MH_OK)throw std::runtime_error("model observer activation");
        report("waiting-native-player-model");
        // Login is user-driven; keep the passive model observer until login or shutdown.
        while(!model_ready.load()&&!closing.load())Sleep(100);
        observing_model.store(false);if(closing.load())throw std::runtime_error("runtime closing while waiting for native player model");
        setup(args->mode==2);
        api.il2cpp_thread_detach(attached);attached=nullptr;setup_busy.store(false);
        if(args->mode==1)MH_DisableHook(MH_ALL_HOOKS);
    }catch(const std::exception& ex){active.store(false);observing_model.store(false);native_multiplayer_transport_shutdown();native_fault_capture_shutdown();native_private_liveness_shutdown();if(attached)api.il2cpp_thread_detach(attached);setup_busy.store(false);MH_DisableHook(MH_ALL_HOOKS);report(ex.what());}
    return 0;
}
std::atomic<bool> started{false};
}
extern "C" __declspec(dllexport) int32_t __cdecl SO_BootstrapFromDependency(const wchar_t* proxy,uint32_t mode){if(!proxy||mode>2)return -1;if(cn::started.exchange(true))return -2;auto p=new(std::nothrow)cn::Args{proxy,mode};if(!p)return -3;HANDLE h=CreateThread(nullptr,0,cn::worker,p,0,nullptr);if(!h){delete p;return -4;}CloseHandle(h);return 0;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){cn::module=module;DisableThreadLibraryCalls(module);HMODULE pinned=nullptr;GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(&SO_BootstrapFromDependency),&pinned);}return TRUE;}
