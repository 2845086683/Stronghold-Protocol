// Included inside cn after native_pending_journal.inc. Only this worker owns
// WinHTTP and disk IO. It never attaches to IL2CPP or touches a managed object.
#pragma once
#include "native_multiplayer_error_trace.h"
std::mutex nmp_mutex;
std::condition_variable nmp_wake;
std::thread nmp_worker_thread;
std::atomic<bool> nmp_stopping{false},nmp_running{false};
std::atomic<unsigned> nmp_requests{0},nmp_retries{0},nmp_failures{0},nmp_applied{0};
std::atomic<unsigned> nmp_batches{0},nmp_batch_commands{0};
std::atomic<unsigned> nmp_response_bytes{0},nmp_parse_ms{0},nmp_drain_ms{0},nmp_drain_max_ms{0};
std::atomic<int32_t> nmp_round_trip_ms{-1};
std::atomic<int32_t> nmp_solo_round_trip_ms{-1};
std::atomic<uint64_t> nmp_solo_ping_tick{0};
cnmp::State nmp_state;
std::string nmp_error,nmp_nickname="本地博士";
double nmp_server_time=0;
uint64_t nmp_received_tick=0;
uint64_t nmp_room_proof_tick=0;
bool nmp_faulted=false;
bool nmp_initialized=false;
bool nmp_attached=true;
thread_local bool nmp_draining=false;
std::map<std::string,uint64_t> nmp_cookies;
struct NMPCompletion{std::string operation;bool success=false;cnjson::Value reply;std::string error;uint64_t cookie=0;};
std::deque<NMPCompletion> nmp_completions;
void native_multiplayer_entry_completed(const char*,bool,const cnjson::Value&,const std::string&);
void native_multiplayer_entry_apply(const cnjson::Value&);
void native_multiplayer_entry_retry_scene_ready(const cnjson::Value&);
void native_multiplayer_entry_return_completed(uint64_t,bool,const cnjson::Value&,const std::string&);
void native_multiplayer_dispatch_apply(const cnjson::Value&);
void native_multiplayer_dispatch_complete(uint64_t,const cnjson::Value&,bool);
int64_t native_multiplayer_boss_received_step();
int64_t native_multiplayer_boss_epoch();
std::string nmp_random_id(){unsigned char bytes[16]{};if(BCryptGenRandom(nullptr,bytes,sizeof(bytes),BCRYPT_USE_SYSTEM_PREFERRED_RNG)<0)throw std::runtime_error("multiplayer random identity unavailable");const char* digits="0123456789abcdef";std::string out;for(auto v:bytes){out+=digits[v>>4];out+=digits[v&15];}return out;}
std::string nmp_origin_hash(){
    BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash_handle=nullptr;DWORD length=0,used=0;std::string out;
    bool ok=BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptGetProperty(algorithm,BCRYPT_OBJECT_LENGTH,reinterpret_cast<PUCHAR>(&length),sizeof(length),&used,0)>=0;
    std::vector<unsigned char> storage(ok?length:0);unsigned char digest[32]{};
    if(ok)ok=BCryptCreateHash(algorithm,&hash_handle,storage.data(),length,nullptr,0,0)>=0&&BCryptHashData(hash_handle,reinterpret_cast<PUCHAR>(const_cast<char*>(private_backend_origin().data())),ULONG(private_backend_origin().size()),0)>=0&&BCryptFinishHash(hash_handle,digest,sizeof(digest),0)>=0;
    if(hash_handle)BCryptDestroyHash(hash_handle);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);if(!ok)throw std::runtime_error("multiplayer origin hash unavailable");
    const char* digits="0123456789abcdef";for(auto v:digest){out+=digits[v>>4];out+=digits[v&15];}return out;
}
std::wstring nmp_journal_path(bool create){auto root=npj_root(create);if(root.empty())return {};root+=L"\\multiplayer";
    if(!create&&GetFileAttributesW(root.c_str())==INVALID_FILE_ATTRIBUTES){auto error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return {};}
    npj_check_directory(root,create);auto id=nmp_origin_hash();return root+L"\\"+std::wstring(id.begin(),id.end())+L".dpapi";
}
std::string nmp_crypt(const std::string& source,bool encrypt){
    if(source.empty()||source.size()>4*1024*1024)throw std::runtime_error("multiplayer encrypted journal size invalid");
    auto library=GetModuleHandleW(L"crypt32.dll");if(!library)library=LoadLibraryExW(L"crypt32.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);if(!library)throw std::runtime_error("multiplayer credential protection unavailable");
    using Protect=BOOL(WINAPI*)(DATA_BLOB*,LPCWSTR,DATA_BLOB*,PVOID,CRYPTPROTECT_PROMPTSTRUCT*,DWORD,DATA_BLOB*);
    using Unprotect=BOOL(WINAPI*)(DATA_BLOB*,LPWSTR*,DATA_BLOB*,PVOID,CRYPTPROTECT_PROMPTSTRUCT*,DWORD,DATA_BLOB*);
    DATA_BLOB input{DWORD(source.size()),reinterpret_cast<BYTE*>(const_cast<char*>(source.data()))},output{};
    auto protect_address=GetProcAddress(library,"CryptProtectData"),unprotect_address=GetProcAddress(library,"CryptUnprotectData");Protect protect_data=nullptr;Unprotect unprotect_data=nullptr;
    static_assert(sizeof(protect_data)==sizeof(protect_address)&&sizeof(unprotect_data)==sizeof(unprotect_address),"Windows DPAPI pointer ABI");std::memcpy(&protect_data,&protect_address,sizeof(protect_data));std::memcpy(&unprotect_data,&unprotect_address,sizeof(unprotect_data));
    BOOL ok=encrypt?(protect_data&&protect_data(&input,L"Stronghold private multiplayer participant",nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)):
        (unprotect_data&&unprotect_data(&input,nullptr,nullptr,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output));
    if(!ok||!output.pbData||!output.cbData)throw std::runtime_error("multiplayer credential journal protection failed");
    std::string result(reinterpret_cast<char*>(output.pbData),output.cbData);SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return result;
}
struct NMPJournalError:std::runtime_error{using std::runtime_error::runtime_error;};
void nmp_persist_bytes(const cnjson::Value& value){
    const auto bytes=nmp_crypt(cnjson::dump(value),true);const auto destination=nmp_journal_path(true);
    DWORD previous=GetFileAttributesW(destination.c_str());if(previous!=INVALID_FILE_ATTRIBUTES&&(previous&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY)))throw std::runtime_error("multiplayer journal target redirected");
    const auto temporary=destination+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(++npj_temporary_sequence);
    bool ok=false;{NPJ_Handle file(CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL|FILE_FLAG_WRITE_THROUGH,nullptr));DWORD written=0;
        if(file.value==INVALID_HANDLE_VALUE)throw std::runtime_error("multiplayer journal temporary unavailable");ok=WriteFile(file.value,bytes.data(),DWORD(bytes.size()),&written,nullptr)&&written==bytes.size()&&FlushFileBuffers(file.value);}
    if(!ok||!MoveFileExW(temporary.c_str(),destination.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){DeleteFileW(temporary.c_str());throw std::runtime_error("multiplayer journal commit failed");}
}
void nmp_persist(const cnjson::Value& value){try{nmp_persist_bytes(value);}catch(const std::exception& ex){throw NMPJournalError(ex.what());}}
std::string nmp_restore_bytes(){const auto source=nmp_journal_path(false);if(source.empty())return {};
    NPJ_Handle file(CreateFileW(source.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_FLAG_OPEN_REPARSE_POINT,nullptr));
    if(file.value==INVALID_HANDLE_VALUE){auto error=GetLastError();if(error==ERROR_FILE_NOT_FOUND||error==ERROR_PATH_NOT_FOUND)return {};throw std::runtime_error("multiplayer journal open failed");}
    BY_HANDLE_FILE_INFORMATION info{};LARGE_INTEGER size{};if(!GetFileInformationByHandle(file.value,&info)||(info.dwFileAttributes&(FILE_ATTRIBUTE_REPARSE_POINT|FILE_ATTRIBUTE_DIRECTORY))||!GetFileSizeEx(file.value,&size)||size.QuadPart<1||size.QuadPart>4*1024*1024)throw std::runtime_error("multiplayer journal source invalid");
    std::string bytes(size_t(size.QuadPart),'\0');DWORD used=0;if(!ReadFile(file.value,bytes.data(),DWORD(bytes.size()),&used,nullptr)||used!=bytes.size())throw std::runtime_error("multiplayer journal read failed");return nmp_crypt(bytes,false);
}
struct NMPHttpError:std::runtime_error{DWORD status;NMPHttpError(DWORD value,const std::string& message):std::runtime_error(message),status(value){}};
cnjson::Value nmp_http(const std::string& route,const cnjson::Value* payload,const std::string& token,const std::string& view_headers={}){
    if(route.rfind("/bridge/act2autochess/multiplayer/",0)!=0||route.find_first_of("\r\n#")!=std::string::npos)throw std::runtime_error("multiplayer route outside private bridge");
    const auto bytes=payload?cnjson::dump(*payload):std::string{};if(bytes.size()>1024*1024)throw std::runtime_error("multiplayer request exceeds limit");
    NBBInternet session(WinHttpOpen(L"StrongholdCNMultiplayer/1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
    if(!session.value||!WinHttpSetTimeouts(session.value,500,500,1000,1000))throw std::runtime_error("multiplayer transport unavailable");
    NBBInternet connection(private_backend_connect(session.value));const auto wide=nbb_wide(route);
    NBBInternet request(connection.value?WinHttpOpenRequest(connection.value,payload?L"POST":L"GET",wide.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,private_backend_request_flags()):nullptr);
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,disabled=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION,decompression=WINHTTP_DECOMPRESSION_FLAG_GZIP;
    const auto headers=nbb_wide(std::string("Content-Type: application/json\r\nX-Stronghold-Native-View: 1\r\n")+view_headers+(token.empty()?"":"Authorization: Bearer "+token+"\r\n"));
    // Participant credentials deliberately replace, rather than reuse, the
    // operator credential accepted by legacy solo/debug endpoints.
    const auto sent_at=GetTickCount64();
    bool ok=request.value&&WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))&&WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))&&
        WinHttpSetOption(request.value,WINHTTP_OPTION_DECOMPRESSION,&decompression,sizeof(decompression))&&
        WinHttpSendRequest(request.value,headers.c_str(),DWORD(-1),payload?const_cast<char*>(bytes.data()):WINHTTP_NO_REQUEST_DATA,DWORD(bytes.size()),DWORD(bytes.size()),0)&&WinHttpReceiveResponse(request.value,nullptr);
    DWORD status=0,size=sizeof(status);if(ok)ok=WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)!=FALSE;
    std::string body;const auto began=GetTickCount64();while(ok){DWORD count=0;if(nmp_stopping.load()||GetTickCount64()-began>10000||!WinHttpQueryDataAvailable(request.value,&count)){ok=false;break;}if(!count)break;
        if(body.size()+count>8*1024*1024){ok=false;break;}auto previous=body.size();body.resize(previous+count);DWORD used=0;if(!WinHttpReadData(request.value,body.data()+previous,count,&used)||!used){ok=false;break;}body.resize(previous+used);}
    if(!ok||body.empty())throw std::runtime_error("multiplayer private server unavailable");const auto parse_at=GetTickCount64();auto value=cnjson::parse(body);++nmp_requests;
    if(route.rfind("/bridge/act2autochess/multiplayer/rooms/",0)==0){nmp_response_bytes.store(unsigned(body.size()));nmp_parse_ms.store(unsigned(GetTickCount64()-parse_at));}
    if(status!=200&&!(status==201&&(route=="/bridge/act2autochess/multiplayer/participants"||route=="/bridge/act2autochess/multiplayer/rooms"))){
        try{std::lock_guard<std::mutex> lock(nmp_mutex);cnmp_trace::http_failure(route,payload,value,status,nmp_state.latest);}catch(...){} // Diagnostic failure cannot replace the real HTTP failure.
        auto error=value.object.find("error");std::string reason=error!=value.object.end()&&error->second.kind==cnjson::Value::String?error->second.string:"multiplayer HTTP "+std::to_string(status);
        auto detail=value.object.find("detail");if(reason=="protocol_adapter_pending"&&detail!=value.object.end()&&detail->second.kind==cnjson::Value::String&&detail->second.string.size()<160&&detail->second.string.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")==std::string::npos)reason+=":"+detail->second.string;
        throw NMPHttpError(status,reason);}
    const auto& accepted=cnmp::at(value,"ok");const auto& fallback=cnmp::at(value,"officialFallback");
    if(accepted.kind!=cnjson::Value::Boolean||!accepted.boolean||fallback.kind!=cnjson::Value::Boolean||fallback.boolean)throw std::runtime_error("multiplayer private server acknowledgement invalid");
    if(route.rfind("/bridge/act2autochess/multiplayer/rooms",0)==0){
        const auto elapsed=std::min<uint64_t>(9999,GetTickCount64()-sent_at);const auto prior=nmp_round_trip_ms.load();
        nmp_round_trip_ms.store(prior<0?int32_t(elapsed):int32_t((int64_t(prior)*3+elapsed)/4));
    }else if(route.rfind("/bridge/act2autochess/multiplayer/solo/native-session",0)==0){
        const auto elapsed=std::min<uint64_t>(9999,GetTickCount64()-sent_at);const auto prior=nmp_solo_round_trip_ms.load();
        nmp_solo_round_trip_ms.store(prior<0?int32_t(elapsed):int32_t((int64_t(prior)*3+elapsed)/4));nmp_solo_ping_tick.store(GetTickCount64());
    }return value;
}
void nmp_publish(NMPCompletion result){std::lock_guard<std::mutex> lock(nmp_mutex);
    if((result.operation=="snapshot"||result.operation=="heartbeat")&&!nmp_completions.empty()&&(nmp_completions.back().operation=="snapshot"||nmp_completions.back().operation=="heartbeat"))nmp_completions.back()=std::move(result);
    else if(nmp_completions.size()<512)nmp_completions.push_back(std::move(result));else throw std::runtime_error("multiplayer main-thread completion queue full");
}
void nmp_failure(const std::string& error,unsigned& retries,uint64_t& next_io){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_error=error;++nmp_retries;next_io=GetTickCount64()+std::min<uint64_t>(10000,500ull<<std::min<unsigned>(retries++,4));}
void nmp_worker(){
    // The network worker does not own a Unity scene. Capture its native faults
    // even during a scene transition, without swallowing or retrying an AV.
    struct FaultScope{FaultScope(){native_fault_capture_transport_scope(true);}~FaultScope(){native_fault_capture_transport_scope(false);}} fault_scope;
    nmp_running.store(true);
    try{
        auto saved=nmp_restore_bytes();{std::lock_guard<std::mutex> lock(nmp_mutex);if(!nmp_initialized&&!saved.empty())nmp_state.restore(cnjson::parse(saved),private_backend_origin());nmp_initialized=true;}
        uint64_t heartbeat=0,next_io=0;unsigned retries=0;
        while(!nmp_stopping.load()){
            cnmp::Pending work;std::vector<cnmp::Pending> batch;bool has_work=false;std::string token,room;int64_t cursor=0;uint64_t cookie=0;cnjson::Value journal;bool enrolled=false;
            {std::unique_lock<std::mutex> lock(nmp_mutex);
                // Idle polling has a cadence; a new ordered command does not.
                // An actual retry still owns its original backoff and receipt.
                const auto command_ready=[&](){return !nmp_state.pending.empty()&&(!retries||GetTickCount64()>=next_io);};
                const auto now=GetTickCount64();
                const auto delay=nmp_attached&&nmp_state.room_active()?std::min<uint64_t>(250,next_io>now?next_io-now:0):250;
                nmp_wake.wait_for(lock,std::chrono::milliseconds(delay),[&](){return nmp_stopping.load()||command_ready();});if(nmp_stopping.load())break;
                if(GetTickCount64()<next_io&&!command_ready())continue;has_work=!nmp_state.pending.empty();if(!has_work&&!nmp_attached)continue;if(has_work){work=nmp_state.pending.front();auto found=nmp_cookies.find(cnmp::text(work.body,"requestId"));if(found!=nmp_cookies.end())cookie=found->second;
                    // Aggregate only a contiguous prefix of fire-and-forget
                    // combat reports. Native RPC callbacks/economy/scene writes
                    // remain individual and cannot be overtaken by FinishUp.
                    size_t bytes=0;for(const auto& item:nmp_state.pending){if(batch.size()>=16||item.route!=work.route||!cnmp::battle_batchable(item)||nmp_cookies.count(cnmp::text(item.body,"requestId")))break;
                        bytes+=cnjson::dump(item.body).size()+1;if(bytes>900*1024)break;batch.push_back(item);}
                }enrolled=nmp_state.enrolled();token=nmp_state.token;room=nmp_state.room;cursor=nmp_state.visible_event;journal=nmp_state.journal();}
            if(!has_work&&room.empty())continue;
            try{
                if(!enrolled){auto registration=cnmp::object();registration.object["nickname"]=cnmp::string(nmp_nickname);auto reply=nmp_http("/bridge/act2autochess/multiplayer/participants",&registration,{});
                    {std::lock_guard<std::mutex> lock(nmp_mutex);nmp_state.registered(reply);token=nmp_state.token;journal=nmp_state.journal();}nmp_persist(journal);}
                if(has_work)nmp_persist(journal); // Immutable receipt saved before its first byte can be sent.
                cnjson::Value reply;std::string operation=has_work?work.operation:"snapshot";
                const auto first_epoch=native_multiplayer_boss_epoch(),step=native_multiplayer_boss_received_step(),last_epoch=native_multiplayer_boss_epoch();const bool consistent=first_epoch==last_epoch;
                // Every ordered combat ACK also returns the next native frames.
                // Otherwise a busy ActionUp queue starves GET and keeps receiving
                // seq0..127. Cursors describe UI-applied data, never worker receipt.
                const auto room_route="/bridge/act2autochess/multiplayer/rooms/"+room;
                const auto view_headers=std::string("X-Stronghold-After-Event: ")+std::to_string(cursor)+"\r\nX-Stronghold-After-Step: "+std::to_string(consistent?step:-1)+"\r\nX-Stronghold-Step-Epoch: "+std::to_string(consistent?last_epoch:0)+"\r\n";
                const auto command_headers=work.route==room_route+"/commands"?view_headers:std::string{};
                size_t acknowledged=1;
                if(batch.size()>1){auto aggregate=cnmp::object();aggregate.object["commands"]=cnmp::array();for(const auto& item:batch)aggregate.object["commands"].array.push_back(item.body);
                    reply=nmp_http(work.route+"/batch",&aggregate,token,command_headers);acknowledged=cnmp::batch_acknowledged(reply,batch);++nmp_batches;nmp_batch_commands+=unsigned(acknowledged);
                }else if(has_work)reply=nmp_http(work.route,&work.body,token,command_headers);
                else if(GetTickCount64()>=heartbeat){auto body=cnmp::object();body.object["requestId"]=cnmp::string(nmp_random_id());body.object["op"]=cnmp::string("heartbeat");body.object["payload"]=cnmp::object();
                    reply=nmp_http("/bridge/act2autochess/multiplayer/rooms/"+room+"/commands",&body,token,view_headers);operation="heartbeat";heartbeat=GetTickCount64()+5000;}
                else{
                    reply=nmp_http("/bridge/act2autochess/multiplayer/rooms/"+room+"?afterEvent="+std::to_string(cursor)+"&afterStep="+std::to_string(consistent?step:-1)+"&stepEpoch="+std::to_string(consistent?last_epoch:0),nullptr,token);}
                std::string batch_error;
                if(batch.size()>1){const auto& last=cnmp::at(reply,"commandBatchResults").array.back();if(!cnmp::at(last,"accepted").boolean)batch_error=cnmp::text(last,"error")+":"+cnmp::text(cnmp::at(batch[acknowledged-1].body,"payload"),"type");}
                bool accepted=false;{std::lock_guard<std::mutex> lock(nmp_mutex);accepted=nmp_state.accept(reply);if(!accepted&&nmp_state.latest.kind==cnjson::Value::Object&&operation!="scene.quit"&&operation!="room.return")reply=cnmp::command_reply(nmp_state.latest,reply);if(has_work){for(size_t i=0;i<acknowledged;++i){nmp_cookies.erase(cnmp::text(nmp_state.pending.front().body,"requestId"));nmp_state.pending.pop_front();}}journal=nmp_state.journal();nmp_error=batch_error;
                    const auto fault=reply.object.find("fault");nmp_room_proof_tick=nmp_state.room_active()&&(fault==reply.object.end()||fault->second.kind==cnjson::Value::Null)?GetTickCount64():0;
                    auto clock=reply.object.find("serverTime");if(clock!=reply.object.end()&&clock->second.kind==cnjson::Value::Integer){const auto now=GetTickCount64();const auto current=nmp_received_tick?nmp_server_time+double(now-nmp_received_tick)/1000.0:0;nmp_server_time=std::max(current,double(clock->second.integer));nmp_received_tick=now;}}
                // Snapshot/heartbeat may close the room too. Persist that
                // acknowledged transition just like an explicit Leave ACK.
                const auto left=reply.object.find("left");
                if(has_work||(left!=reply.object.end()&&left->second.kind==cnjson::Value::Boolean&&left->second.boolean))nmp_persist(journal);
                const auto phase=reply.object.find("phase");const auto cadence=phase!=reply.object.end()&&phase->second.kind==cnjson::Value::String?(phase->second.string=="battle"?2000:phase->second.string=="boss"?200:500):500;
                if(accepted||has_work)nmp_publish({operation,true,std::move(reply),{},cookie});retries=0;next_io=GetTickCount64()+(has_work?0:cadence);
                if(!batch_error.empty()){++nmp_failures;nmp_publish({"protocol",false,{},batch_error,0});}
            }catch(const NMPHttpError& ex){
                if(ex.status>=500||ex.status==408||ex.status==429){nmp_failure(ex.what(),retries,next_io);continue;}
                {std::lock_guard<std::mutex> lock(nmp_mutex);if(has_work){nmp_state.pending.pop_front();nmp_cookies.erase(cnmp::text(work.body,"requestId"));}nmp_error=ex.what();journal=nmp_state.journal();}
                if(has_work){nmp_persist(journal);nmp_publish({work.operation,false,{},ex.what(),cookie});}++nmp_failures;
                if(std::string(ex.what())=="multiplayer_authentication_required"||ex.status==401||ex.status==403)throw std::runtime_error("saved multiplayer participant rejected; identity preserved");
                next_io=GetTickCount64()+1000;retries=0;
            }catch(const NMPJournalError&){throw;}
            catch(const std::exception& ex){nmp_failure(ex.what(),retries,next_io);}
        }
    }catch(const std::exception& ex){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_error=ex.what();nmp_faulted=true;++nmp_failures;}
    nmp_running.store(false);
}
void native_multiplayer_transport_setup(){std::lock_guard<std::mutex> lock(nmp_mutex);if(nmp_worker_thread.joinable())return;nmp_state.origin=private_backend_origin();nmp_stopping.store(false);nmp_worker_thread=std::thread(nmp_worker);}
void native_multiplayer_transport_shutdown(){nmp_stopping.store(true);nmp_wake.notify_all();if(nmp_worker_thread.joinable())nmp_worker_thread.join();}
void native_multiplayer_transport_detach(){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_attached=false;}
void native_multiplayer_transport_detach(const preparation_view::Session& origin){
    std::lock_guard<std::mutex> lock(nmp_mutex);
    if(origin.valid()&&nmp_state.room==origin.room&&nmp_state.latest.kind==cnjson::Value::Object&&preparation_view::session(nmp_state.latest)==origin)nmp_attached=false;
}
void native_multiplayer_transport_attach(const preparation_view::Session& origin){
    std::lock_guard<std::mutex> lock(nmp_mutex);
    if(!origin.valid())throw std::runtime_error("private party attach requires a valid acknowledged scene owner");
    // An old scene can finish retiring after a newer party/run was accepted.
    // Its native teardown still completes, but it cannot alter that owner's polling.
    if(nmp_state.room!=origin.room||nmp_state.latest.kind!=cnjson::Value::Object||!(preparation_view::session(nmp_state.latest)==origin))return;
    nmp_attached=true;nmp_wake.notify_all();
}
bool native_multiplayer_enqueue_request(const std::string& operation,const cnjson::Value& payload,uint64_t cookie){
    try{const auto request=nmp_random_id();{std::lock_guard<std::mutex> lock(nmp_mutex);if(!nmp_initialized||nmp_faulted||nmp_stopping.load())throw std::runtime_error("multiplayer transport stopped or restoring identity");nmp_state.enqueue(operation,request,payload);if(operation=="reconnect"||operation=="create"||operation=="join")nmp_attached=true;if(cookie)nmp_cookies[request]=cookie;}nmp_wake.notify_all();return true;}
    catch(const std::exception& ex){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_error=ex.what();++nmp_failures;return false;}
}
bool native_multiplayer_transport_command(const std::string& operation,const cnjson::Value& payload){return native_multiplayer_enqueue_request(operation,payload,0);}
bool native_multiplayer_enqueue(const std::string& operation,const cnjson::Value& payload){return native_multiplayer_transport_command(operation,payload);}
cnjson::Value native_broadcast_cached_identity();
void native_multiplayer_capture_display_profile(cnjson::Value&);
void nmp_capture_roster(cnjson::Value& payload){std::lock_guard<std::recursive_mutex> lock(cache_lock);if(!activity_json.empty()){auto profile=cnjson::parse(activity_json);payload.object["chessSquad"]=cnmp::at(profile,"chessSquad");}native_multiplayer_capture_display_profile(payload);}
bool native_multiplayer_transport_create(const std::string& mode,bool precise){(void)precise;try{auto payload=cnmp::object();payload.object["modeId"]=cnmp::string(mode);nmp_capture_roster(payload);return native_multiplayer_transport_command("create",payload);}catch(const std::exception& ex){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_error=ex.what();++nmp_failures;return false;}}
bool native_multiplayer_transport_join(const std::string& code){try{auto payload=cnmp::object();payload.object["roomCode"]=cnmp::string(code);nmp_capture_roster(payload);return native_multiplayer_transport_command("join",payload);}catch(const std::exception& ex){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_error=ex.what();++nmp_failures;return false;}}
bool native_multiplayer_transport_active(){std::lock_guard<std::mutex> lock(nmp_mutex);return nmp_state.room_active()||!nmp_state.pending.empty();}
cnjson::Value native_multiplayer_transport_snapshot(){std::lock_guard<std::mutex> lock(nmp_mutex);return nmp_state.latest;}
std::string native_multiplayer_transport_room(){std::lock_guard<std::mutex> lock(nmp_mutex);return nmp_state.room;}
bool native_multiplayer_transport_live(){std::lock_guard<std::mutex> lock(nmp_mutex);return !nmp_faulted&&nmp_attached&&nmp_state.room_active()&&nmp_room_proof_tick&&GetTickCount64()-nmp_room_proof_tick<=8000;}
int32_t native_multiplayer_transport_ping(){return native_multiplayer_transport_live()?nmp_round_trip_ms.load():-1;}
int32_t native_multiplayer_solo_ping(){const auto tick=nmp_solo_ping_tick.load();return tick&&GetTickCount64()-tick<=8000?nmp_solo_round_trip_ms.load():-1;}
std::string native_multiplayer_participant_token(){std::lock_guard<std::mutex> lock(nmp_mutex);if(nmp_state.token.empty())throw std::runtime_error("participant native finish identity unavailable");return nmp_state.token;}
double native_multiplayer_transport_time(){std::lock_guard<std::mutex> lock(nmp_mutex);return nmp_received_tick?nmp_server_time+double(GetTickCount64()-nmp_received_tick)/1000.0:0;}
void native_multiplayer_drain_main(){
    if(!can_read()||unity_ui_thread.load()!=GetCurrentThreadId()||nmp_draining)return;
    struct DrainScope{uint64_t start=GetTickCount64();DrainScope(){nmp_draining=true;}~DrainScope(){const auto elapsed=unsigned(GetTickCount64()-start);nmp_drain_ms.store(elapsed);nmp_drain_max_ms.store(std::max(nmp_drain_max_ms.load(),elapsed));nmp_draining=false;}} drain_scope;
    std::deque<NMPCompletion> ready;{std::lock_guard<std::mutex> lock(nmp_mutex);ready.swap(nmp_completions);}
    for(auto& result:ready){
      auto completed=result.cookie?std::make_shared<bool>(false):std::shared_ptr<bool>{};
      auto complete=[cookie=result.cookie,completed](const cnjson::Value& reply,bool success){if(!cookie||*completed)return;*completed=true;native_multiplayer_dispatch_complete(cookie,reply,success);};
      try{
        // A successful Quit is a terminal acknowledgement, not another world
        // presentation. Do not prepare/reload the abandoned scene before Kick.
        if(result.operation=="scene.quit"){
            native_multiplayer_entry_completed("scene.quit",result.success,result.reply,result.error);
            complete(result.reply,result.success);continue;
        }
        if(result.operation=="room.return")native_multiplayer_entry_return_completed(result.cookie,result.success,result.reply,result.error);
        // HTTP already in flight when the original scene retired can still
        // arrive. Its data remains in the durable worker snapshot, but must not
        // resurrect a departed Unity world. Return/Leave own explicit ACKs.
        if(result.success&&(result.operation=="snapshot"||result.operation=="heartbeat")){
            const auto notice=result.reply.object.find("left");const bool closed=notice!=result.reply.object.end()&&notice->second.kind==cnjson::Value::Boolean&&notice->second.boolean;
            std::lock_guard<std::mutex> lock(nmp_mutex);if(!nmp_attached&&!closed)continue;
        }
        bool snapshot=result.success&&result.reply.object.count("generation");
        if(snapshot){std::lock_guard<std::mutex> lock(nmp_mutex);if(nmp_state.latest.kind==cnjson::Value::Object&&nmp_state.room==cnmp::text(result.reply,"roomId")){result.reply=cnmp::command_reply(nmp_state.latest,result.reply);nmp_state.visible_generation=std::max(nmp_state.visible_generation,cnmp::integer(result.reply,"generation"));}else snapshot=false;}
        const auto left=result.reply.object.find("left");const bool leaving=result.success&&left!=result.reply.object.end()&&left->second.kind==cnjson::Value::Boolean&&left->second.boolean;
        if(result.success&&result.reply.object.count("generation")&&!snapshot&&!leaving){complete(result.reply,false);continue;}
        if(leaving)snapshot=false; // Leave receipts retire the old scene; they are not a new scene presentation.
        const bool joining=result.operation=="create"||result.operation=="join"||(result.operation=="reconnect"&&!native_multiplayer_entry_active());
        if(snapshot){
            const bool entryBefore=joining;
            std::function<void()> before;
            if(entryBefore)before=[result](){native_multiplayer_entry_completed(result.operation.c_str(),true,result.reply,result.error);};
            auto after=[operation=result.operation,error=result.error,entryBefore,complete](const cnjson::Value& applied){if(entryBefore||operation=="room.return")native_multiplayer_entry_apply(applied);else native_multiplayer_entry_completed(operation.c_str(),true,applied,error);native_multiplayer_entry_retry_scene_ready(applied);complete(applied,true);};
            // Only a joining failure needs the original response. Regular boss
            // callbacks must not retain multiple copies of the full room JSON.
            auto rejected=[operation=result.operation,reply=joining?result.reply:cnjson::Value{},joining,complete](const std::string& reason){if(joining)native_multiplayer_entry_completed(operation.c_str(),false,reply,reason);auto error=cnmp::object();error.object["error"]=cnmp::string(reason);complete(error,false);};
            native_multiplayer_preparation_view_dispatch(result.reply,std::move(before),std::move(after),std::move(rejected));
            {std::lock_guard<std::mutex> lock(nmp_mutex);nmp_state.presented(result.reply);++nmp_applied;}
            native_multiplayer_preparation_view_notify_presented(result.reply);
        }else{
            native_multiplayer_entry_completed(result.operation.c_str(),result.success,result.reply,result.error);
            if(!result.success){result.reply=cnmp::object();result.reply.object["error"]=cnmp::string(result.error);}complete(result.reply,result.success);
        }
      }catch(const std::exception& ex){try{auto error=cnmp::object();error.object["error"]=cnmp::string(ex.what());complete(error,false);}catch(...){}std::lock_guard<std::mutex> lock(nmp_mutex);nmp_error=ex.what();++nmp_failures;}
    }
    // An original scene loader can finish without another HTTP completion.
    try{native_multiplayer_preparation_view_tick();}catch(const std::exception& ex){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_error=ex.what();++nmp_failures;}
}
std::string native_multiplayer_transport_status_json(){std::unique_lock<std::mutex> lock(nmp_mutex,std::try_to_lock);
    auto value=cnmp::object();value.object["workerRunning"].kind=cnjson::Value::Boolean;value.object["workerRunning"].boolean=nmp_running.load();value.object["requests"]=cnmp::number(nmp_requests.load());value.object["retries"]=cnmp::number(nmp_retries.load());value.object["failures"]=cnmp::number(nmp_failures.load());value.object["appliedSnapshots"]=cnmp::number(nmp_applied.load());
    value.object["roundTripMs"]=cnmp::number(nmp_round_trip_ms.load());
    value.object["lastResponseBytes"]=cnmp::number(nmp_response_bytes.load());value.object["lastJsonParseMs"]=cnmp::number(nmp_parse_ms.load());
    value.object["lastMainDrainMs"]=cnmp::number(nmp_drain_ms.load());value.object["maxMainDrainMs"]=cnmp::number(nmp_drain_max_ms.load());
    value.object["batches"]=cnmp::number(nmp_batches.load());value.object["batchedCommands"]=cnmp::number(nmp_batch_commands.load());
    value.object["soloRoundTripMs"]=cnmp::number(nmp_solo_round_trip_ms.load());
    value.object["firstFailureEvidence"]=cnmp_trace::ledger.status();
    if(lock.owns_lock()){value.object["roomId"]=cnmp::string(nmp_state.room);value.object["pendingCount"]=cnmp::number(nmp_state.pending.size());value.object["generation"]=cnmp::number(nmp_state.visible_generation);value.object["eventCursor"]=cnmp::number(nmp_state.visible_event);value.object["lastError"]=cnmp::string(nmp_error);}return cnjson::dump(value);
}
cnjson::Value native_multiplayer_solo_http(const wchar_t* raw_route,const cnjson::Value* payload){
    if(!raw_route)throw std::runtime_error("participant solo route missing");
    const std::wstring route(raw_route),native=L"/bridge/act2autochess/native-session",activity=L"/activity/autochessSeason/";
    std::string target="/bridge/act2autochess/multiplayer/solo/";
    if(route==native||route.rfind(native+L"/",0)==0)target+="native-session"+utf8(route.substr(native.size()));
    else if(route.rfind(activity,0)==0)target+="activity/"+utf8(route.substr(activity.size()));
    else throw std::runtime_error("participant solo route outside activity scope");
    std::string token;{std::lock_guard<std::mutex> lock(nmp_mutex);token=nmp_state.token;}
    if(token.empty())throw std::runtime_error("participant solo identity not ready");
    auto envelope=nmp_http(target,payload,token);return cnmp::at(envelope,"payload");
}
std::string native_multiplayer_profile_http(const wchar_t* raw_route,const std::string* raw_payload){
    // Called by the plugin's background initialization and existing bounded
    // preparation-config UI seam. It never reads the shared solo profile.
    if(!raw_route)throw std::runtime_error("participant profile route missing");
    std::string token;{std::lock_guard<std::mutex> lock(nmp_mutex);token=nmp_state.token;}
    if(token.empty()){
        if(nmp_worker_thread.joinable())throw std::runtime_error("participant profile identity not ready");
        auto saved=nmp_restore_bytes();if(!saved.empty()){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_state.restore(cnjson::parse(saved),private_backend_origin());token=nmp_state.token;}
        if(token.empty()){auto body=cnmp::object();body.object["nickname"]=cnmp::string(nmp_nickname);auto reply=nmp_http("/bridge/act2autochess/multiplayer/participants",&body,{});cnjson::Value journal;
            {std::lock_guard<std::mutex> lock(nmp_mutex);nmp_state.origin=private_backend_origin();nmp_state.registered(reply);token=nmp_state.token;journal=nmp_state.journal();}nmp_persist(journal);}
    }
    const std::wstring route(raw_route);const std::wstring prefix=L"/activity/autochessSeason/";
    if(raw_payload){
        if(route.rfind(prefix,0)!=0)throw std::runtime_error("participant profile write outside preparation configuration");
        const auto operation=route.substr(prefix.size());if(operation==L"syncInfo"){auto bootstrap=nmp_http("/bridge/act2autochess/multiplayer/bootstrap",nullptr,token);auto reply=cnmp::at(bootstrap,"syncInfo");reply.object["result"]=cnmp::number(0);return cnjson::dump(reply);}
        if(operation==L"quitSingleGame"||operation==L"getFriendCharAssistList"){auto body=cnjson::parse(*raw_payload);return cnjson::dump(native_multiplayer_solo_http(raw_route,&body));}
        if(operation!=L"setChessPoolDeploy"&&operation!=L"setChessPoolDiyChar"&&operation!=L"removeChessPoolChar")throw std::runtime_error("participant profile activity operation is not implemented");
        auto request=cnmp::object();request.object["requestId"]=cnmp::string(nmp_random_id());request.object["op"]=cnmp::string(std::string(operation.begin(),operation.end()));request.object["payload"]=cnjson::parse(*raw_payload);
        nmp_http("/bridge/act2autochess/multiplayer/profile",&request,token);return "{\"result\":0}";
    }
    const std::map<std::wstring,const char*> fields={{L"/bridge/act2autochess/player-activity","playerActivity"},{L"/bridge/act2autochess/player-model","playerModel"},{L"/bridge/act2autochess/perm","perm"},{L"/bridge/act2autochess/char-map","charMap"},{L"/bridge/act2autochess/sync-info","syncInfo"}};
    const auto field=fields.find(route);if(field==fields.end())throw std::runtime_error("participant profile read outside CN preparation bootstrap");
    auto bootstrap=nmp_http("/bridge/act2autochess/multiplayer/bootstrap",nullptr,token);return cnjson::dump(cnmp::at(bootstrap,field->second));
}
std::string native_multiplayer_transport_resume_sync(const std::string& fallback){
    if(nmp_worker_thread.joinable())throw std::runtime_error("private room resume requires startup transport");
    auto saved=nmp_restore_bytes();if(saved.empty())return fallback;std::string token;bool has_room=false;
    {std::lock_guard<std::mutex> lock(nmp_mutex);nmp_state.restore(cnjson::parse(saved),private_backend_origin());nmp_initialized=true;token=nmp_state.token;has_room=nmp_state.room_active();if(has_room)nmp_attached=false;}
    if(!has_room)return fallback;
    auto bootstrap=nmp_http("/bridge/act2autochess/multiplayer/bootstrap",nullptr,token);bool resumed=false;
    auto resume=bootstrap.object.find("nativeResume");if(resume!=bootstrap.object.end()&&resume->second.kind==cnjson::Value::Object){auto snapshot=resume->second.object.find("snapshot");
        if(snapshot!=resume->second.object.end()&&snapshot->second.kind==cnjson::Value::Object&&!cn_departure::scene_departed(snapshot->second)){std::lock_guard<std::mutex> lock(nmp_mutex);nmp_state.accept(snapshot->second);resumed=true;}}
    if(!resumed){
        // An expired/dissolved party has no Continue presence. Release its
        // protected journal only after the same authenticated room confirms it.
        std::string previous;{std::lock_guard<std::mutex> lock(nmp_mutex);previous=nmp_state.room;}
        auto closed=nmp_http("/bridge/act2autochess/multiplayer/rooms/"+previous,nullptr,token);
        auto left=closed.object.find("left");
        if((left==closed.object.end()||left->second.kind!=cnjson::Value::Boolean||!left->second.boolean)&&cn_departure::scene_departed(closed)){
            // After process loss, an acknowledged voluntary scene departure
            // must never rebuild that scene. Explicitly leave its retained party
            // and wait for the authenticated ACK before clearing only room state.
            auto leave=cnmp::object();leave.object["requestId"]=cnmp::string(nmp_random_id());leave.object["op"]=cnmp::string("leave");leave.object["payload"]=cnmp::object();leave.object["generation"]=cnmp::at(closed,"generation");
            closed=nmp_http("/bridge/act2autochess/multiplayer/rooms/"+previous+"/commands",&leave,token);left=closed.object.find("left");
        }
        if(left==closed.object.end()||left->second.kind!=cnjson::Value::Boolean||!left->second.boolean)throw std::runtime_error("persisted private room has no resume or closed receipt");
        cnjson::Value journal;{std::lock_guard<std::mutex> lock(nmp_mutex);if(nmp_state.room!=previous)throw std::runtime_error("private room changed during closed receipt");nmp_state.accept(closed);nmp_state.pending.clear();nmp_cookies.clear();nmp_attached=true;journal=nmp_state.journal();}nmp_persist(journal);
    }
    return cnjson::dump(cnmp::at(bootstrap,"syncInfo"));
}
