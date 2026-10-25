#pragma once
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>

namespace cnidentifier {
inline bool scope(bool projecting,unsigned private_depth,bool on_ui){return projecting&&private_depth&&on_ui;}
class Index {
    std::map<std::int32_t,std::string> values;
public:
    void add(std::int32_t identifier,const std::string& chess_id,const std::string& dictionary_key){
        if(identifier<0||chess_id.empty()||chess_id.size()>256||chess_id.find('\0')!=std::string::npos||chess_id.rfind("chess_",0)!=0||chess_id!=dictionary_key)
            throw std::runtime_error("private chess identifier definition invalid");
        if(!values.emplace(identifier,chess_id).second)throw std::runtime_error("private chess identifier duplicate");
    }
    const std::string& find(std::int32_t identifier)const{
        auto found=values.find(identifier);if(found==values.end())throw std::runtime_error("private chess identifier missing: "+std::to_string(identifier));return found->second;
    }
    std::size_t size()const{return values.size();}
};
}
