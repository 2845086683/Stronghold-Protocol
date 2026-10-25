#pragma once
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace cnbattleresources {
inline std::string global_buff_path(std::string value){
    for(auto& character:value)if(character>='A'&&character<='Z')character=char(character-'A'+'a');
    if(value.rfind("dyn/",0)==0)value.erase(0,4);
    if(value.size()>=7&&value.compare(value.size()-7,7,".prefab")==0)value.resize(value.size()-7);
    const std::string prefix="battle/prefabs/[uc]globalbuffs/";
    if(value.rfind(prefix,0)!=0||value.size()==prefix.size()||value.size()>240||
       value.find("..")!=std::string::npos||value.find("//")!=std::string::npos)
        throw std::runtime_error("invalid required globalbuff resource path: "+value);
    for(const auto character:value)if(!((character>='a'&&character<='z')||
        (character>='0'&&character<='9')||character=='/'||character=='_'||character=='-'||character=='['||character==']'))
        throw std::runtime_error("invalid required globalbuff resource path: "+value);
    return value;
}
template<class Range>std::vector<std::string> required_global_buffs(const Range& manifest){
    std::set<std::string> paths;
    for(const auto* raw:manifest){
        if(!raw)throw std::runtime_error("null required globalbuff resource path");
        paths.insert(global_buff_path(raw));
        if(paths.size()>4096)throw std::runtime_error("required globalbuff manifest exceeds bound");
    }
    if(paths.empty())throw std::runtime_error("required globalbuff resource manifest is empty");
    return {paths.begin(),paths.end()};
}
}
