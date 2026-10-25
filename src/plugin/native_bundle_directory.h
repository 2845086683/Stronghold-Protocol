#pragma once
#include <algorithm>
#include <cstdint>
#include <istream>
#include <stdexcept>
#include <string>
#include <vector>

// Read only the UnityFS directory. Payload decompression/loading stays in Unity.
// Limits are deliberately smaller than a whole bundle; unknown formats fail closed.
namespace cnbundledir {
inline void require(bool ok){if(!ok)throw std::runtime_error("unsupported or malformed UnityFS directory");}
struct Reader {
    const std::vector<uint8_t>& data; size_t pos=0;
    uint64_t number(size_t n){require(n<=8&&pos<=data.size()&&n<=data.size()-pos);uint64_t v=0;while(n--)v=(v<<8)|data[pos++];return v;}
    std::string string(){std::string s;for(size_t i=0;i<256;++i){auto c=number(1);if(!c)return s;s+=char(c);}require(false);return {};}
};
inline std::vector<uint8_t> lz4(const std::vector<uint8_t>& input,size_t size){
    require(size&&size<=4*1024*1024);std::vector<uint8_t> out;out.reserve(size);size_t p=0;
    auto length=[&](size_t n){if(n==15){uint8_t b=255;while(b==255){require(p<input.size());b=input[p++];require(n<=size&&b<=size-n);n+=b;}}return n;};
    while(p<input.size()){
        auto token=input[p++];auto literal=length(token>>4);
        require(literal<=input.size()-p&&literal<=size-out.size());out.insert(out.end(),input.begin()+p,input.begin()+p+literal);p+=literal;
        if(p==input.size())break;
        require(input.size()-p>=2);size_t offset=input[p]|size_t(input[p+1])<<8;p+=2;require(offset&&offset<=out.size());
        auto match=length(token&15);require(match<=size-out.size()&&4<=size-out.size()-match);match+=4;
        while(match--){auto value=out[out.size()-offset];out.push_back(value);}
    }
    require(out.size()==size);return out;
}
inline std::vector<uint8_t> read(std::istream& stream,uint64_t offset,size_t size){
    require(size<=4*1024*1024);std::vector<uint8_t> data(size);stream.clear();stream.seekg(std::streamoff(offset));
    stream.read(reinterpret_cast<char*>(data.data()),std::streamsize(size));require(size_t(stream.gcount())==size);return data;
}
inline std::string identity(std::istream& stream,uint64_t file_size){
    require(file_size>=64&&file_size<=uint64_t(4)*1024*1024*1024);
    auto header=read(stream,0,size_t(std::min<uint64_t>(file_size,512)));Reader h{header};
    require(h.string()=="UnityFS");auto version=h.number(4);require(version==7||version==8);
    h.string();auto engine=h.string();require(engine.rfind("2021.3.",0)==0);
    require(h.number(8)==file_size);auto compressed=h.number(4),expanded=h.number(4),flags=h.number(4);
    require(compressed&&compressed<=4*1024*1024&&expanded&&expanded<=4*1024*1024);
    require((flags&0x40)&&!(flags&~uint64_t(0x2ff)));
    uint64_t offset=(flags&0x80)?file_size-compressed:(h.pos+15)&~uint64_t(15);
    require(offset<=file_size&&compressed<=file_size-offset);
    auto packed=read(stream,offset,size_t(compressed));std::vector<uint8_t> data;
    switch(flags&63){case 0:require(compressed==expanded);data=std::move(packed);break;case 2:case 3:data=lz4(packed,size_t(expanded));break;default:require(false);}
    Reader r{data};r.pos=16;auto blocks=r.number(4);require(blocks&&blocks<=200000);
    uint64_t total=0,stored=0;
    for(uint64_t i=0;i<blocks;++i){auto a=r.number(4),b=r.number(4);r.number(2);require(a&&b);total+=a;stored+=b;require(total<=uint64_t(8)*1024*1024*1024&&stored<=file_size);}
    auto count=r.number(4);require(count&&count<=1024);std::vector<std::string> cabs;
    for(uint64_t i=0;i<count;++i){auto start=r.number(8),size=r.number(8);auto properties=r.number(4);auto name=r.string();
        require(start<=total&&size<=total-start);
        if(!(properties&4))continue;
        std::transform(name.begin(),name.end(),name.begin(),[](unsigned char c){return char(c>='A'&&c<='Z'?c-'A'+'a':c);});
        require(name.size()==36&&name.rfind("cab-",0)==0&&name.find_first_not_of("0123456789abcdef",4)==std::string::npos);cabs.push_back(name);
    }
    require(r.pos==data.size()&&!cabs.empty());std::sort(cabs.begin(),cabs.end());require(std::adjacent_find(cabs.begin(),cabs.end())==cabs.end());
    std::string result;for(const auto& cab:cabs){if(!result.empty())result+=';';result+=cab;}return result;
}
}
