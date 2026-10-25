#pragma once
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

// Private envelopes include fractional level-action timing as well as integer
// DTO fields. Keep their kinds distinct and retain real-number lexemes without
// rounding. Bounds and duplicate-key rejection apply before native projection.
namespace cnjson {
struct Value {
    enum Kind { Null, Boolean, Integer, String, Array, Object, Real } kind=Null;
    bool boolean=false;
    int64_t integer=0;
    std::string string;
    std::string number;
    std::vector<Value> array;
    std::map<std::string,Value> object;
};
class Parser {
    const std::string& source;
    size_t at=0,nodes=0;
    [[noreturn]] void fail() const { throw std::runtime_error("invalid bounded private-profile JSON at "+std::to_string(at)); }
    void space(){while(at<source.size()&&(source[at]==' '||source[at]=='\t'||source[at]=='\r'||source[at]=='\n'))++at;}
    bool take(char c){space();if(at<source.size()&&source[at]==c){++at;return true;}return false;}
    uint32_t hex(){uint32_t out=0;for(unsigned i=0;i<4;++i){if(at==source.size())fail();char c=source[at++];out<<=4;if(c>='0'&&c<='9')out+=c-'0';else if(c>='a'&&c<='f')out+=c-'a'+10;else if(c>='A'&&c<='F')out+=c-'A'+10;else fail();}return out;}
    static void unicode(std::string& s,uint32_t c){
        if(c<0x80)s+=char(c);
        else if(c<0x800){s+=char(0xc0|(c>>6));s+=char(0x80|(c&63));}
        else if(c<0x10000){s+=char(0xe0|(c>>12));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
        else{s+=char(0xf0|(c>>18));s+=char(0x80|((c>>12)&63));s+=char(0x80|((c>>6)&63));s+=char(0x80|(c&63));}
    }
    std::string text(){
        if(!take('"'))fail();
        std::string out;
        while(at<source.size()){
            unsigned char c=source[at++];if(c=='"')return out;if(c<32)fail();
            if(c!='\\'){out+=char(c);continue;}if(at==source.size())fail();char escaped=source[at++];
            switch(escaped){case '"':out+='"';break;case '\\':out+='\\';break;case '/':out+='/';break;
                case 'b':out+='\b';break;case 'f':out+='\f';break;case 'n':out+='\n';break;case 'r':out+='\r';break;case 't':out+='\t';break;
                case 'u':{uint32_t code=hex();if(code>=0xd800&&code<=0xdbff){if(at+2>source.size()||source[at++]!='\\'||source[at++]!='u')fail();uint32_t low=hex();if(low<0xdc00||low>0xdfff)fail();code=0x10000+((code-0xd800)<<10)+(low-0xdc00);}else if(code>=0xdc00&&code<=0xdfff)fail();unicode(out,code);break;}
                default:fail();}
        }fail();
    }
    Value value(unsigned depth){
        if(depth>64||++nodes>100000)fail();
        space();if(at==source.size())fail();Value v;char c=source[at];
        if(c=='"'){v.kind=Value::String;v.string=text();return v;}
        if(c=='{'){
            ++at;v.kind=Value::Object;if(take('}'))return v;
            do{auto key=text();if(!take(':'))fail();auto child=value(depth+1);if(!v.object.emplace(std::move(key),std::move(child)).second)fail();if(take('}'))return v;}while(take(','));fail();
        }
        if(c=='['){++at;v.kind=Value::Array;if(take(']'))return v;do{v.array.push_back(value(depth+1));if(take(']'))return v;}while(take(','));fail();}
        for(const auto* word:{"null","true","false"}){std::string literal(word);if(source.compare(at,literal.size(),literal)==0){at+=literal.size();if(literal!="null"){v.kind=Value::Boolean;v.boolean=literal=="true";}return v;}}
        const auto begin=at;auto digit=[&]{return at<source.size()&&source[at]>='0'&&source[at]<='9';};
        if(c=='-')++at;
        if(!digit())fail();
        if(source[at]=='0'){++at;if(digit())fail();}else while(digit())++at;
        bool real=false;
        if(at<source.size()&&source[at]=='.'){real=true;++at;if(!digit())fail();while(digit())++at;}
        if(at<source.size()&&(source[at]=='e'||source[at]=='E')){
            real=true;++at;if(at<source.size()&&(source[at]=='+'||source[at]=='-'))++at;
            if(!digit())fail();
            while(digit())++at;
        }
        if(at-begin>128)fail();
        const char* first=source.data()+begin;const char* last=source.data()+at;
        if(real){double finite=0;auto result=std::from_chars(first,last,finite,std::chars_format::general);
            if(result.ec!=std::errc{}||result.ptr!=last||!std::isfinite(finite))fail();
            v.kind=Value::Real;v.number.assign(first,last);
        }else{auto result=std::from_chars(first,last,v.integer);if(result.ec!=std::errc{}||result.ptr!=last)fail();v.kind=Value::Integer;}
        return v;
    }
public:
    explicit Parser(const std::string& input):source(input){if(source.size()>8*1024*1024)fail();}
    Value parse(){auto result=value(0);space();if(at!=source.size())fail();return result;}
};
inline Value parse(const std::string& input){return Parser(input).parse();}
inline std::string dump(const Value& value){
    std::string out;size_t nodes=0;
    auto quoted=[&](const std::string& text){out+='"';const char* hex="0123456789abcdef";for(unsigned char c:text){if(c=='"'||c=='\\'){out+='\\';out+=char(c);}else if(c<32){out+="\\u00";out+=hex[c>>4];out+=hex[c&15];}else out+=char(c);}out+='"';};
    auto write=[&](auto&& self,const Value& v,unsigned depth)->void{
        if(depth>64||++nodes>100000)throw std::runtime_error("private JSON output bound");
        switch(v.kind){
            case Value::Null:out+="null";break;case Value::Boolean:out+=v.boolean?"true":"false";break;
            case Value::Integer:out+=std::to_string(v.integer);break;case Value::String:quoted(v.string);break;
            case Value::Real:{auto checked=Parser(v.number).parse();if(checked.kind!=Value::Real)throw std::runtime_error("private JSON real required");out+=v.number;break;}
            case Value::Array:{out+='[';bool first=true;for(const auto& item:v.array){if(!first)out+=',';first=false;self(self,item,depth+1);}out+=']';break;}
            case Value::Object:{out+='{';bool first=true;for(const auto& item:v.object){if(!first)out+=',';first=false;quoted(item.first);out+=':';self(self,item.second,depth+1);}out+='}';break;}
        }
        if(out.size()>8*1024*1024)throw std::runtime_error("private JSON output size bound");
    };write(write,value,0);return out;
}
}
