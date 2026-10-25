#pragma once
#include <string>
#include <vector>

namespace cnresourceowner {
inline std::string normalized(std::string value){
    for(auto& ch:value){if(ch=='\\')ch='/';else if(ch>='A'&&ch<='Z')ch=char(ch-'A'+'a');}
    while(value.size()>1&&value.back()=='/')value.pop_back();
    return value;
}
inline bool safe_relative(const std::string& value){
    if(value.empty()||value.front()=='/'||value.find(':')!=std::string::npos)return false;
    if(value==".."||value.rfind("../",0)==0||value.find("/../")!=std::string::npos||
       (value.size()>3&&value.compare(value.size()-3,3,"/..")==0))return false;
    return true;
}
struct IndexNames {
    bool installed_cn=false;
    std::vector<std::string> candidates;
};
inline IndexNames names(const std::string& semantic_name,const std::string& source_path,const std::string& cn_data_root){
    const auto path=normalized(source_path),root=normalized(cn_data_root);
    IndexNames result;
    auto add=[&](const std::string& key){
        if(!safe_relative(key))return;
        for(const auto& current:result.candidates)if(current==key)return;
        result.candidates.push_back(key);
    };
    for(const char* folder:{"/persistentdata/bundles/","/streamingassets/ab/windows/"}){
        const auto prefix=root+folder;
        if(path.rfind(prefix,0)!=0)continue;
        const auto relative=path.substr(prefix.size());
        if(!safe_relative(relative))return result;
        result.installed_cn=true;
        add(relative);
        const auto slash=relative.find_last_of('/');
        add(relative.substr(slash==std::string::npos?0:slash+1));
        break;
    }
    add(normalized(semantic_name));
    return result;
}
enum class Action { ReuseNativeHolder, AdoptAlreadyLoaded, LoadThroughNativeIndex, PrivateRawLoad, RejectMissingNativeInfo };
inline Action action(bool installed_cn,bool info_exists,bool live_holder,bool live_unowned_bundle){
    if(!installed_cn)return Action::PrivateRawLoad;
    if(!info_exists)return Action::RejectMissingNativeInfo;
    if(live_holder)return Action::ReuseNativeHolder;
    if(live_unowned_bundle)return Action::AdoptAlreadyLoaded;
    return Action::LoadThroughNativeIndex;
}
}
