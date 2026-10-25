#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace mixedhelp {
struct Position { int32_t row=0,col=0; };
inline bool operator==(Position a,Position b){return a.row==b.row&&a.col==b.col;}
struct Identity {
    std::string room,run;uint64_t round=0,generation=0;
    bool operator==(const Identity& b)const{return room==b.room&&run==b.run&&round==b.round&&generation==b.generation;}
};
enum class Source:uint8_t { Neutral,Left,Right };
struct Frame {
    int32_t width=0,height=0;std::vector<Source> owners;
    size_t logical(Position p)const{
        if(p.row<0||p.col<0||p.row>=height||p.col>=width)throw std::runtime_error("mixed-map position outside the common frame");
        return size_t(p.row)*size_t(width)+size_t(p.col);
    }
    size_t serialized(Position p)const{
        (void)logical(p);return size_t(height-1-p.row)*size_t(width)+size_t(p.col);
    }
    Source at(Position p)const{return owners.at(logical(p));}
};
template<class AreaResolver> Frame common_frame(int32_t width,int32_t height,AreaResolver area){
    // A scope restriction to the eight locally evidenced catalog frames. Areas
    // still come from native MapAreaManager; no column/offset is inferred here.
    if(width!=21||height!=19)throw std::runtime_error("mixed-map candidate requires the evidenced 21x19 frame");
    Frame result{width,height,{}};size_t left=0,right=0;
    for(int32_t row=0;row<height;++row)for(int32_t col=0;col<width;++col){
        const Position p{row,col};const auto memberships=area(p);
        if(memberships.first&&memberships.second)throw std::runtime_error("mixed-map native left/right areas overlap");
        const auto owner=memberships.first?Source::Left:memberships.second?Source::Right:Source::Neutral;
        result.owners.push_back(owner);left+=owner==Source::Left;right+=owner==Source::Right;
    }
    if(!left||!right||left+right==result.owners.size())throw std::runtime_error("mixed-map native areas lack a side or template seam");
    return result;
}
inline std::string registration_key(const Identity& id,Source side,int seat){
    if(id.room.empty()||id.run.empty()||!id.generation)throw std::runtime_error("mixed-map identity incomplete");
    // Length prefixes prevent delimiter-bearing room/run identifiers colliding.
    return "private-mixed-help/"+std::to_string(id.room.size())+":"+id.room+"/"+
        std::to_string(id.run.size())+":"+id.run+"/"+std::to_string(id.round)+"/"+
        std::to_string(id.generation)+"/"+std::to_string(int(side))+"/"+std::to_string(seat);
}
}
