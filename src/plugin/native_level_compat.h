#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace cnlevels {
inline std::string asset_name_key(std::string value){
    for(auto& c:value)if(c>='A'&&c<='Z')c=char(c-'A'+'a');
    return value;
}
inline bool asset_name_matches(const std::string& actual,const std::string& expected){
    // Resource aliases are case-insensitive, while original boss TextAsset
    // names retain their authored uppercase _S suffix. Preserve the complete
    // basename; only ASCII case is equivalent here.
    return !expected.empty()&&asset_name_key(actual)==asset_name_key(expected);
}
inline std::string key(std::string value){
    for(auto& c:value){if(c=='\\')c='/';if(c>='A'&&c<='Z')c=char(c-'A'+'a');}
    for(const char* prefix:{"dyn/gamedata/levels/","gamedata/levels/"}){
        const std::string p(prefix);if(value.rfind(p,0)==0){value.erase(0,p.size());break;}
    }
    if(value.size()>6&&value.compare(value.size()-6,6,".bytes")==0)value.erase(value.size()-6);
    return value;
}
inline bool scope(bool exact_activity,bool prepared,bool in_flight,bool ui_thread,bool closing){
    return exact_activity&&prepared&&in_flight&&ui_thread&&!closing;
}
inline uint32_t u32(const std::vector<uint8_t>& data,size_t at){
    if(at>data.size()||data.size()-at<4)throw std::runtime_error("private level integer outside bytes");
    return uint32_t(data[at])|(uint32_t(data[at+1])<<8)|(uint32_t(data[at+2])<<16)|(uint32_t(data[at+3])<<24);
}
inline uint16_t u16(const std::vector<uint8_t>& data,size_t at){
    if(at>data.size()||data.size()-at<2)throw std::runtime_error("private level short outside bytes");
    return uint16_t(data[at])|uint16_t(uint16_t(data[at+1])<<8);
}
// Original content has a 128-byte signature prefix. Only our own hash-pinned
// payloads use this adapter; all other assets retain the original signed parser.
inline int32_t root_position(const std::vector<uint8_t>& data,size_t prefix){
    if(prefix!=128||data.size()<prefix+8||data.size()>8*1024*1024)throw std::runtime_error("private level content size invalid");
    const uint64_t root=uint64_t(prefix)+u32(data,prefix);
    if(root<prefix+4||root>data.size()-4||root>INT32_MAX)throw std::runtime_error("private level root outside content");
    const int64_t table=int64_t(root)-int32_t(u32(data,size_t(root)));
    if(table<int64_t(prefix)||table>int64_t(data.size()-4))throw std::runtime_error("private level vtable outside content");
    const auto length=u16(data,size_t(table)),object_size=u16(data,size_t(table)+2);
    if(length<4||(length%2)||object_size<4||uint64_t(table)+length>data.size()||root+object_size>data.size())throw std::runtime_error("private level root table invalid");
    return int32_t(root);
}
}
