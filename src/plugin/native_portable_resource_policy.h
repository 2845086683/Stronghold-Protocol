#pragma once
#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

// Exact local resource relocation policy; no old build-path fallback.
namespace cnportable {
enum class Kind { OriginalClient, PrivateOverlay };
struct Plan { Kind kind; std::wstring game_root, relative, path; };
inline wchar_t ascii_lower(wchar_t c) { return c>=L'A'&&c<=L'Z'?wchar_t(c-L'A'+L'a'):c; }
inline std::wstring lower(std::wstring value) { for(auto& c:value)c=ascii_lower(c);return value; }
inline std::wstring local_absolute(std::wstring value,bool file) {
    if(value.empty()||value.size()>32700||value.find(L'\0')!=std::wstring::npos)throw std::runtime_error("portable path empty, truncated or contains NUL");
    std::replace(value.begin(),value.end(),L'/',L'\\');
    if(value.size()<4||!((value[0]>=L'A'&&value[0]<=L'Z')||(value[0]>=L'a'&&value[0]<=L'z'))||value[1]!=L':'||value[2]!=L'\\')throw std::runtime_error("portable path must be an absolute local drive path");
    if(value.find(L':',2)!=std::wstring::npos)throw std::runtime_error("portable path alternate stream or additional drive rejected");
    if(!file&&value.back()==L'\\')value.pop_back();
    if(value.size()<4||value.back()==L'\\')throw std::runtime_error("portable path has no file/directory component");
    size_t begin=3;
    while(begin<value.size()) {
        auto end=value.find(L'\\',begin);if(end==std::wstring::npos)end=value.size();
        const auto part=value.substr(begin,end-begin);
        if(part.empty()||part==L"."||part==L".."||part.back()==L'.'||part.back()==L' '||part.find_first_of(L"<>\"|?*")!=std::wstring::npos)throw std::runtime_error("portable path unsafe component");
        for(auto c:part)if(c<32)throw std::runtime_error("portable path control character");
        auto base=lower(part.substr(0,part.find(L'.')));
        if(base==L"con"||base==L"prn"||base==L"aux"||base==L"nul"||
           (base.size()==4&&(base.rfind(L"com",0)==0||base.rfind(L"lpt",0)==0)&&base[3]>=L'1'&&base[3]<=L'9'))throw std::runtime_error("portable path device component");
        begin=end+1;
    }
    return value;
}
inline Plan plan(const std::wstring& compiled_path,const std::wstring& actual_game_root) {
    const auto source=local_absolute(compiled_path,true), root=local_absolute(actual_game_root,false), folded=lower(source);
    const std::wstring original=L"\\arknights_data\\",overlay=L"\\client-data\\";
    const auto a=folded.find(original),b=folded.find(overlay);
    if((a==std::wstring::npos)==(b==std::wstring::npos))throw std::runtime_error("portable resource source has an unknown or ambiguous root");
    const auto marker=a!=std::wstring::npos?original:overlay;const auto position=a!=std::wstring::npos?a:b;
    if(folded.find(marker,position+1)!=std::wstring::npos)throw std::runtime_error("portable resource source has repeated root markers");
    const auto suffix=source.substr(position+marker.size());if(suffix.empty())throw std::runtime_error("portable resource suffix missing");
    const bool is_original=a!=std::wstring::npos;
    const auto relative=(is_original?L"Arknights_Data\\":L"plugin\\StrongholdProtocolCN\\resources\\client-data\\")+suffix;
    return {is_original?Kind::OriginalClient:Kind::PrivateOverlay,root,relative,local_absolute(root+L"\\"+relative,true)};
}
}
