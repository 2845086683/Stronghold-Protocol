// Independent backend connection, included inside cn after path/report helpers.
// The client never creates, terminates, or owns a server process.
#pragma once
#include "cn_backend_profile.h"

std::mutex nbb_state_mutex;
std::atomic<unsigned> nbb_started{0},nbb_reused{0},nbb_failed{0};
std::atomic<DWORD> nbb_pid{0};
std::atomic<bool> nbb_healthy{false};
std::string nbb_last_error;
std::string nbb_server_build_id;
cnbackend::Endpoint nbb_endpoint;
std::string nbb_access_token;
bool nbb_participant_profile=false;

struct NBBInternet {
    HINTERNET value=nullptr;explicit NBBInternet(HINTERNET v=nullptr):value(v){}
    ~NBBInternet(){if(value)WinHttpCloseHandle(value);}
    NBBInternet(const NBBInternet&)=delete;NBBInternet& operator=(const NBBInternet&)=delete;
};
std::wstring nbb_wide(const std::string& value){return std::wstring(value.begin(),value.end());}
const std::string& private_backend_origin(){return nbb_endpoint.origin;}
DWORD private_backend_request_flags(){return nbb_endpoint.secure?WINHTTP_FLAG_SECURE:0;}
HINTERNET private_backend_connect(HINTERNET session){auto host=nbb_wide(nbb_endpoint.host);return WinHttpConnect(session,host.c_str(),nbb_endpoint.port,0);}
bool private_backend_headers(HINTERNET request){
    if(nbb_access_token.empty())return true;
    auto header=nbb_wide("Authorization: Bearer "+nbb_access_token+"\r\n");
    return WinHttpAddRequestHeaders(request,header.c_str(),DWORD(-1),WINHTTP_ADDREQ_FLAG_ADD|WINHTTP_ADDREQ_FLAG_REPLACE)!=FALSE;
}
void configure_private_backend(const std::wstring& root){
    auto config=root+L"\\plugin\\StrongholdProtocolCN\\cn-activity.ini";wchar_t endpoint[2048]{},token[1024]{};
    auto count=GetPrivateProfileStringW(L"backend",L"endpoint",L"http://127.0.0.1:16680",endpoint,2048,config.c_str());
    if(count>=2047)throw std::runtime_error("private backend endpoint configuration too long");
    auto parsed=cnbackend::endpoint(utf8(endpoint));
    count=GetPrivateProfileStringW(L"backend",L"access_token",L"",token,1024,config.c_str());
    auto credential=utf8(token);if(count>=1023||!cnbackend::access_token_valid(credential))throw std::runtime_error("private backend access token configuration invalid");
    const bool local_origin=parsed.host=="localhost"||parsed.host=="::1"||parsed.host.rfind("127.",0)==0;
    const auto participant=GetPrivateProfileIntW(L"backend",L"participant_profile",local_origin?0:1,config.c_str());
    if(participant>1)throw std::runtime_error("private backend participant_profile must be 0 or 1");
    nbb_endpoint=std::move(parsed);nbb_access_token=std::move(credential);nbb_participant_profile=participant!=0;
}

class NBBPlatform {
public:
    uint64_t now()const{return GetTickCount64();}
    cnbackend::Probe probe(uint64_t deadline){
        using cnbackend::ProbeKind;
        if(closing.load()||now()>=deadline)return {ProbeKind::unavailable,0,"backend verification cancelled or deadline expired"};
        NBBInternet session(WinHttpOpen(L"StrongholdCNIndependentBackend/1",WINHTTP_ACCESS_TYPE_NO_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
        if(!session.value||!WinHttpSetTimeouts(session.value,250,250,250,250))return {ProbeKind::unavailable,0,"private backend WinHTTP unavailable"};
        NBBInternet connection(private_backend_connect(session.value));
        NBBInternet request(connection.value?WinHttpOpenRequest(connection.value,L"GET",L"/health",nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,private_backend_request_flags()):nullptr);
        DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,disabled=WINHTTP_DISABLE_COOKIES|WINHTTP_DISABLE_AUTHENTICATION;
        if(!request.value||!WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects))||
            !WinHttpSetOption(request.value,WINHTTP_OPTION_DISABLE_FEATURE,&disabled,sizeof(disabled))||!private_backend_headers(request.value)||
            !WinHttpSendRequest(request.value,WINHTTP_NO_ADDITIONAL_HEADERS,0,WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.value,nullptr))
            return {ProbeKind::unavailable,0,"independent private health unavailable"};
        DWORD status=0,size=sizeof(status);
        if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)
            return {ProbeKind::mismatch,0,"independent private health did not return HTTP200"};
        std::string body;
        while(now()<deadline){DWORD available=0;if(!WinHttpQueryDataAvailable(request.value,&available))return {ProbeKind::unavailable,0,"private health read failed"};
            if(!available){auto result=cnbackend::validate_health(body,CN_BACKEND_CATALOG_HASH,"");
                if(result.kind==ProbeKind::healthy){auto value=cnjson::parse(body);std::lock_guard<std::mutex> lock(nbb_state_mutex);nbb_server_build_id=value.object.at("backendBuildId").string;}
                return result;}
            if(body.size()+available>65536)return {ProbeKind::mismatch,0,"private health response exceeds limit"};auto old_size=body.size();body.resize(old_size+available);DWORD used=0;
            if(!WinHttpReadData(request.value,body.data()+old_size,available,&used)||!used)return {ProbeKind::unavailable,0,"private health read incomplete"};body.resize(old_size+used);}
        return {ProbeKind::unavailable,0,"backend health deadline expired"};
    }
};

void ensure_private_backend(){
    try{
        NBBPlatform platform;auto result=cnbackend::ensure(platform);nbb_pid.store(result.pid);nbb_healthy.store(true);++nbb_reused;
        {std::lock_guard<std::mutex> lock(nbb_state_mutex);nbb_last_error.clear();}
        report("independent-private-backend-healthy-client-started-no-process");
    }catch(const std::exception& ex){
        nbb_healthy.store(false);++nbb_failed;{std::lock_guard<std::mutex> lock(nbb_state_mutex);nbb_last_error=ex.what();}
        report("independent-private-backend-failed-closed-no-official-fallback");throw;
    }
}
std::string native_backend_status_json(){
    std::unique_lock<std::mutex> lock(nbb_state_mutex,std::try_to_lock);
    std::string status="{\"started\":0,\"reused\":"+std::to_string(nbb_reused.load())+",\"failed\":"+std::to_string(nbb_failed.load())+
        ",\"pid\":"+std::to_string(nbb_pid.load())+",\"healthy\":"+(nbb_healthy.load()?"true":"false")+",\"snapshotBusy\":"+(lock.owns_lock()?"false":"true")+
        ",\"catalogHash\":"+quote(CN_BACKEND_CATALOG_HASH)+",\"clientBuildId\":"+quote(CN_BACKEND_BUILD_ID)+",\"endpoint\":"+quote(private_backend_origin())+
        ",\"officialFallback\":false,\"clientOwnsProcess\":false,\"readinessBudgetMs\":3000,\"maximumSingleIoTimeoutMs\":250,\"startup\":\"Start Run-ActivityServer.ps1 independently before starting the client.\",\"stop\":\"Stop the independently managed server with Ctrl+C; the client never restarts it.\"";
    if(lock.owns_lock())status+=",\"backendBuildId\":"+quote(nbb_server_build_id)+",\"lastError\":"+quote(nbb_last_error);return status+'}';
}
