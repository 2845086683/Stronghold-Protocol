#pragma once
#include "native_json.h"
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace cnbackend {
enum class ProbeKind { healthy, refused, mismatch, unavailable };
struct Probe {ProbeKind kind=ProbeKind::unavailable;std::uint32_t pid=0;std::string detail;};
struct Result {bool started=false;bool reused=false;std::uint32_t pid=0;};
struct Endpoint {std::string origin="http://127.0.0.1:16680",host="127.0.0.1";unsigned short port=16680;bool secure=false;};
inline Endpoint endpoint(const std::string& value){
    Endpoint out;out.origin=value;std::size_t begin=0;
    if(value.rfind("http://",0)==0)begin=7;
    else if(value.rfind("https://",0)==0){begin=8;out.secure=true;}
    else throw std::runtime_error("private backend endpoint must use http or https");
    auto authority=value.substr(begin);if(authority.empty()||authority.size()>253)throw std::runtime_error("private backend endpoint authority invalid");
    auto colon=authority.find(':');out.host=authority.substr(0,colon);
    if(out.host.empty()||out.host.front()=='.'||out.host.back()=='.'||out.host.front()=='-'||out.host.back()=='-')throw std::runtime_error("private backend endpoint host invalid");
    for(char c:out.host)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'))throw std::runtime_error("private backend endpoint must be an origin without credentials, path, query or fragment");
    out.port=out.secure?443:80;
    if(colon!=std::string::npos){auto text=authority.substr(colon+1);unsigned port=0;if(text.empty()||text.size()>5)throw std::runtime_error("private backend endpoint port invalid");
        for(char c:text){if(c<'0'||c>'9')throw std::runtime_error("private backend endpoint port invalid");port=port*10+unsigned(c-'0');}
        if(!port||port>65535)throw std::runtime_error("private backend endpoint port invalid");out.port=static_cast<unsigned short>(port);}
    return out;
}
inline bool access_token_valid(const std::string& token){if(token.empty())return true;if(token.size()<32||token.size()>512)return false;for(unsigned char c:token)if(c<33||c>126)return false;return true;}

inline std::wstring quote_argument(std::wstring_view value){
    if(value.find(L'\0')!=std::wstring_view::npos)throw std::runtime_error("backend argument contains NUL");
    std::wstring out=L"\"";std::size_t slashes=0;
    for(wchar_t c:value){if(c==L'\\'){++slashes;continue;}
        if(c==L'\"'){out.append(slashes*2+1,L'\\');out+=c;}
        else{out.append(slashes,L'\\');out+=c;}slashes=0;}
    out.append(slashes*2,L'\\');out+=L'\"';return out;
}
inline bool absolute_executable(std::wstring_view path){
    return path.size()>13&&((path[0]>=L'A'&&path[0]<=L'Z')||(path[0]>=L'a'&&path[0]<=L'z'))&&path[1]==L':'&&(path[2]==L'\\'||path[2]==L'/')&&
        path.substr(path.size()-10)==L"python.exe"&&(path[path.size()-11]==L'\\'||path[path.size()-11]==L'/')&&path.find(L'\0')==std::wstring_view::npos&&path.find(L'\"')==std::wstring_view::npos;
}
inline std::wstring join_arguments(const std::vector<std::wstring>& args){
    std::wstring out;for(const auto& arg:args){if(!out.empty())out+=L' ';out+=quote_argument(arg);}return out;
}
inline std::wstring command_line(const std::wstring& executable,const std::wstring& catalog,const std::wstring& database){
    if(!absolute_executable(executable))throw std::runtime_error("private backend executable profile invalid");
    return join_arguments({executable,L"-E",L"-s",L"-S",L"-B",L"-m",L"stronghold_server",L"--port",L"16680",L"--catalog",catalog,L"--database",database});
}
inline Probe validate_health(const std::string& raw,const std::string& catalog,const std::string& build){
    try{
        const auto v=cnjson::parse(raw);
        auto member=[&](const char* key)->const cnjson::Value&{if(v.kind!=cnjson::Value::Object)throw std::runtime_error("object required");return v.object.at(key);};
        auto boolean=[&](const char* key,bool expected){const auto& f=member(key);return f.kind==cnjson::Value::Boolean&&f.boolean==expected;};
        auto string=[&](const char* key,const std::string& expected){const auto& f=member(key);return f.kind==cnjson::Value::String&&f.string==expected;};
        const auto& version=member("nativeSessionBridgeVersion");const auto& pid=member("pid");
        const auto& identity=member("backendBuildId");
        bool build_matches=build.empty()?(identity.kind==cnjson::Value::String&&identity.string.size()==64&&identity.string.find_first_not_of("0123456789abcdef")==std::string::npos):string("backendBuildId",build);
        if(!boolean("ok",true)||!boolean("officialWireCompatible",false)||!string("purpose","CN native activity private backend")||
            !string("catalogHash",catalog)||!build_matches||version.kind!=cnjson::Value::Integer||version.integer!=1||
            pid.kind!=cnjson::Value::Integer||pid.integer<=0||pid.integer>UINT32_MAX)return {ProbeKind::mismatch,0,"loopback backend identity mismatch"};
        return {ProbeKind::healthy,std::uint32_t(pid.integer),{}};
    }catch(const std::exception&){return {ProbeKind::mismatch,0,"loopback health JSON invalid or incomplete"};}
}

// The client only verifies an independently managed service. In particular a
// refused connection must never create a child or revive an older local server.
template<class Platform> Result ensure(Platform& p){
    const auto deadline=p.now()+3000;
    auto probe=p.probe(deadline);
    if(probe.kind==ProbeKind::healthy)return {false,true,probe.pid};
    throw std::runtime_error("independent private backend unavailable; start Run-ActivityServer.ps1 before the client: "+probe.detail);
}
}
