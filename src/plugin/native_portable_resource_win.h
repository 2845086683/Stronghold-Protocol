#pragma once
#include <windows.h>
#include "native_portable_resource_policy.h"

namespace cnportable {
inline std::wstring executable_game_root() {
    std::vector<wchar_t> buffer(32768);const DWORD length=GetModuleFileNameW(nullptr,buffer.data(),DWORD(buffer.size()));
    if(!length||length>=buffer.size())throw std::runtime_error("portable executable path unavailable");
    const std::wstring executable(buffer.data(),length);const auto slash=executable.find_last_of(L"/\\");
    if(slash==std::wstring::npos||lower(executable.substr(slash+1))!=L"arknights.exe")throw std::runtime_error("portable resources require the Arknights executable");
    return local_absolute(executable.substr(0,slash),false);
}
inline void require_regular_no_reparse(const Plan& value) {
    const auto resolved=local_absolute(value.path,true),root=local_absolute(value.game_root,false);
    if(lower(resolved).rfind(lower(root)+L"\\",0)!=0)throw std::runtime_error("portable resolved path escaped game root");
    // Inspect all ancestors, including ones above GameRoot, and the file. Never
    // follow a junction, symlink or other reparse point as a private asset root.
    size_t end=3;
    for(;;) {
        const auto current=resolved.substr(0,end);const DWORD flags=GetFileAttributesW(current.c_str());
        if(flags==INVALID_FILE_ATTRIBUTES)throw std::runtime_error("portable resource or ancestor is missing");
        if(flags&FILE_ATTRIBUTE_REPARSE_POINT)throw std::runtime_error("portable resource path redirected by reparse point");
        const bool last=end==resolved.size();
        if(last?(flags&FILE_ATTRIBUTE_DIRECTORY):!(flags&FILE_ATTRIBUTE_DIRECTORY))throw std::runtime_error("portable resource file/directory kind differs");
        if(last)break;
        end=resolved.find(L'\\',end+1);if(end==std::wstring::npos)end=resolved.size();
    }
}
inline std::wstring resolve_bundle_path(const std::wstring& compiled_path,const std::wstring& actual_game_root) {
    const auto value=plan(compiled_path,actual_game_root);require_regular_no_reparse(value);return value.path;
}
inline std::wstring resolve_bundle_path(const std::wstring& compiled_path) {
    return resolve_bundle_path(compiled_path,executable_game_root());
}
}
