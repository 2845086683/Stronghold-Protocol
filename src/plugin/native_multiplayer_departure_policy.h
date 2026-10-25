#pragma once
#include "native_json.h"
#include <stdexcept>
// Recipient-bound terminal presence. A retained party is not a live scene.
namespace cn_departure {
inline const cnjson::Value* field(const cnjson::Value& v,const char* key){if(v.kind!=cnjson::Value::Object)return nullptr;auto p=v.object.find(key);return p==v.object.end()?nullptr:&p->second;}
inline int64_t number(const cnjson::Value& v,const char* key){auto x=field(v,key);if(!x||x->kind!=cnjson::Value::Integer)throw std::runtime_error("private departure integer missing");return x->integer;}
inline std::string text(const cnjson::Value& v,const char* key){auto x=field(v,key);if(!x||x->kind!=cnjson::Value::String)throw std::runtime_error("private departure identity missing");return x->string;}
inline bool scene_departed(const cnjson::Value& v){
    auto left=field(v,"left");if(left&&left->kind==cnjson::Value::Boolean&&left->boolean)return true;
    bool pending_result=false;
    if(auto state=field(v,"selfScenePresence")){
        const auto seat=number(v,"seat"),match=number(v,"matchSequence");
        if(state->kind!=cnjson::Value::Object||seat<0||seat>3||match<1||number(*state,"seat")!=seat||number(*state,"matchSequence")!=match)
            throw std::runtime_error("private scene presence belongs to another recipient");
        bool terminal=false;for(auto key:{"sceneLeft","quit","returned"}){auto flag=field(*state,key);if(!flag||flag->kind!=cnjson::Value::Boolean)throw std::runtime_error("private scene presence flag missing");terminal=terminal||flag->boolean;}
        // Grace-expired offline members retain one authenticated result to
        // consume after restart. They may resume that SETTLE page, never battle.
        auto pending=field(v,"nativeSettlementPending");
        pending_result=terminal&&pending&&pending->kind==cnjson::Value::Boolean&&pending->boolean&&
           text(v,"phase")=="result"&&field(v,"nativeSettleGameInfo")&&
           field(v,"nativeSettleGameInfo")->kind==cnjson::Value::Object&&
           field(*state,"quit")->boolean&&field(*state,"sceneLeft")->boolean&&!field(*state,"returned")->boolean;
        if(terminal&&!pending_result)return true;
    }
    auto exit=field(v,"nativeSceneExit");
    if(exit&&exit->kind!=cnjson::Value::Null){
        const auto seat=number(v,"seat");
        if(exit->kind!=cnjson::Value::Object||seat<0||seat>3||number(*exit,"seat")!=seat||
           number(*exit,"reason")!=7||(text(*exit,"kind")!="voluntary"&&text(*exit,"kind")!="completed")||(text(*exit,"kind")=="completed"&&text(v,"phase")!="result")||text(*exit,"roomId")!=text(v,"roomId")||
           number(*exit,"matchSequence")!=number(v,"matchSequence"))throw std::runtime_error("private departure belongs to another scene");
        return !pending_result;
    }
    if(pending_result)throw std::runtime_error("private pending settlement lacks its terminal receipt");
    auto party=field(v,"partyReturn");auto returned=party?field(*party,"returnedSeats"):nullptr;
    if(returned){
        if(returned->kind!=cnjson::Value::Array)throw std::runtime_error("private party return seats malformed");
        for(const auto& row:returned->array){if(row.kind!=cnjson::Value::Integer||row.integer<0||row.integer>3)throw std::runtime_error("private party return seat invalid");if(row.integer==number(v,"seat"))return true;}
    }
    return false;
}
}
