#pragma once
#include <filesystem>
#include <fstream>
#include <map>
#include "native_bundle_directory.h"
#include "native_portable_resource_win.h"
#include "cn_resource_identities.h"

namespace cnhotresources {
inline std::string expected(const char* semantic){
    for(const auto& row:CN_RESOURCE_IDENTITIES)if(std::string(semantic)==row.name)return row.cabs;
    throw std::runtime_error("CN resource has no certified CAB identity");
}
inline std::wstring selected(const std::string& canonical){
    // The native index is authoritative. This function only mirrors its local
    // PersistentData / StreamingAssets precedence, never searches another build.
    if(canonical.empty()||canonical.size()>1024||canonical.find_first_of("\\:")!=std::string::npos)
        throw std::runtime_error("unsafe native resource key");
    const auto root=cnportable::executable_game_root();std::wstring key(canonical.begin(),canonical.end());
    for(const auto* folder:{L"Arknights_Data\\PersistentData\\Bundles\\",L"Arknights_Data\\StreamingAssets\\AB\\Windows\\"}){
        auto path=cnportable::local_absolute(root+L"\\"+folder+key,true);
        if(GetFileAttributesW(path.c_str())==INVALID_FILE_ATTRIBUTES)continue;
        cnportable::require_regular_no_reparse({cnportable::Kind::OriginalClient,root,L"",path});return path;
    }
    throw std::runtime_error("current native resource file is missing");
}
inline std::string directory(const std::wstring& path){
    // Re-read after a same-process hot update; only cache an unchanged file stamp.
    WIN32_FILE_ATTRIBUTE_DATA info{};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info))throw std::runtime_error("native resource stat failed");
    uint64_t size=(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
    uint64_t stamp=(uint64_t(info.ftLastWriteTime.dwHighDateTime)<<32)|info.ftLastWriteTime.dwLowDateTime;
    struct Entry{uint64_t size,stamp;std::string identity;};static std::map<std::wstring,Entry> cache;
    auto it=cache.find(path);if(it!=cache.end()&&it->second.size==size&&it->second.stamp==stamp)return it->second.identity;
    std::ifstream stream(std::filesystem::path(path),std::ios::binary);if(!stream)throw std::runtime_error("native resource open failed");
    auto identity=cnbundledir::identity(stream,size);cache[path]={size,stamp,identity};return identity;
}
inline bool matches(const char* semantic,const std::string& canonical){return directory(selected(canonical))==expected(semantic);}
inline bool anonymous(const std::string& name){return name.rfind("anon/",0)==0&&name.size()>9&&name.substr(name.size()-4)==".bin";}
}
