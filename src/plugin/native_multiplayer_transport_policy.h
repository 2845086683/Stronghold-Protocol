#pragma once
#include "native_json.h"
#include "native_multiplayer_departure_policy.h"
#include <deque>
#include <stdexcept>
#include <string>
#include <vector>

// This is a private bridge envelope. Native DTOs remain unchanged inside it.
// In particular, a pending mutation never acquires a newer phase generation
// after a timeout or a process restart.
namespace cnmp {
inline cnjson::Value object(){cnjson::Value v;v.kind=cnjson::Value::Object;return v;}
inline cnjson::Value array(){cnjson::Value v;v.kind=cnjson::Value::Array;return v;}
inline cnjson::Value string(const std::string& s){cnjson::Value v;v.kind=cnjson::Value::String;v.string=s;return v;}
inline cnjson::Value number(int64_t n){cnjson::Value v;v.kind=cnjson::Value::Integer;v.integer=n;return v;}
inline const cnjson::Value& at(const cnjson::Value& v,const char* name){auto i=v.object.find(name);if(v.kind!=cnjson::Value::Object||i==v.object.end())throw std::runtime_error("multiplayer envelope field missing");return i->second;}
inline std::string text(const cnjson::Value& v,const char* name){const auto& x=at(v,name);if(x.kind!=cnjson::Value::String)throw std::runtime_error("multiplayer envelope text required");return x.string;}
inline int64_t integer(const cnjson::Value& v,const char* name){const auto& x=at(v,name);if(x.kind!=cnjson::Value::Integer||x.integer<0)throw std::runtime_error("multiplayer envelope unsigned number required");return x.integer;}
inline bool hex_id(const std::string& v){return v.size()==32&&v.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline bool token_valid(const std::string& v){return v.size()>=32&&v.size()<=128&&v.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_")==std::string::npos;}
inline bool mode(const std::string& v){return v=="mode_multi_funny"||v=="mode_multi_normal"||v=="mode_multi_hard"||v=="mode_multi_abyss";}
inline bool code(const std::string& v){return v.size()==6&&v.find_first_not_of("0123456789")==std::string::npos;}
inline bool business_rejection(const std::string& value){
    for(const auto* allowed:{"empty_shop_slot","insufficient_funds","stock_exhausted","max_shop_level",
        "hand_full","trait_reward_selection_required","no_trait_reward_offer","equipment_target_not_allowed",
        "stale_multiplayer_generation","preparation_view_stale_ack","preparation_view_no_pending_plan",
        "preparation_view_not_in_rest","preparation_view_loading_expired","preparation_return_locked",
        "preparation_return_expired","wrong_multiplayer_phase"})if(value==allowed)return true;
    return false;
}
// A newer room snapshot supplies presentation, never another request's result.
inline cnjson::Value command_reply(const cnjson::Value& latest,const cnjson::Value& receipt){
    auto out=latest;out.object.erase("commandRejected");
    // Keep this command's final SP choice even when presentation has already
    // coalesced to PREPARE. The native receiver deduplicates its durable seq.
    auto special=receipt.object.find("nativeSpPreparationUpdates");
    if(special!=receipt.object.end()&&integer(latest,"matchSequence")==integer(receipt,"matchSequence"))out.object["nativeSpPreparationUpdates"]=special->second;
    auto projection=out.object.find("dtoProjection");
    if(projection!=out.object.end()){
        auto packet=projection->second.object.find("preparation");
        if(packet!=projection->second.object.end()&&packet->second.kind==cnjson::Value::Object)
            packet->second.object.at("payload").object["code"]=number(0);
    }
    auto rejected=receipt.object.find("commandRejected");if(rejected==receipt.object.end())return out;
    if(rejected->second.kind!=cnjson::Value::Object||text(rejected->second,"requestId").empty()||
       !business_rejection(text(rejected->second,"error")))
        throw std::runtime_error("multiplayer native command rejection invalid");
    out.object["commandRejected"]=rejected->second;
    out.object.at("dtoProjection").object.at("preparation").object.at("payload").object["code"]=number(1);
    return out;
}
struct Pending {
    std::string operation,route;
    cnjson::Value body;
};
inline bool battle_batchable(const Pending& command){
    if(command.operation!="protocol")return false;
    const auto& payload=at(command.body,"payload");const auto name=text(payload,"type");
    const auto plus=name.find_last_of('+');const auto short_name=plus==std::string::npos?name:name.substr(plus+1);
    for(const auto* allowed:{"SelfBattleAddBond","SelfBattleEnemyEscape","SelfBattleKillEnemy","SelfBattleInfo","SelfBattleFinish","HelpBattleKillEnemy","HelpBattleEnemyEscape","HelpBattleFinish","Action"})
        if(short_name==std::string("AutoChessBattleScene")+allowed+"Up")return true;
    return false;
}
inline size_t batch_acknowledged(const cnjson::Value& reply,const std::vector<Pending>& sent){
    const auto& results=at(reply,"commandBatchResults");
    if(results.kind!=cnjson::Value::Array||results.array.empty()||results.array.size()>sent.size())throw std::runtime_error("multiplayer batch acknowledgement count invalid");
    for(size_t i=0;i<results.array.size();++i){const auto& item=results.array[i];const auto& accepted=at(item,"accepted");
        if(text(item,"requestId")!=text(sent[i].body,"requestId")||accepted.kind!=cnjson::Value::Boolean)throw std::runtime_error("multiplayer batch acknowledgement identity invalid");
        if(!accepted.boolean&&(i+1!=results.array.size()||text(item,"error").empty()))throw std::runtime_error("multiplayer batch rejection prefix invalid");
    }return results.array.size();
}
struct State {
    std::string origin,participant,token,room;
    std::deque<Pending> pending;
    cnjson::Value latest;
    int64_t visible_generation=0,visible_event=0;
    bool enrolled()const{return !participant.empty()&&!token.empty();}
    static int64_t revision(const cnjson::Value& view){auto self=view.object.find("selfState");if(self==view.object.end()||self->second.kind==cnjson::Value::Null)return -1;return integer(self->second,"revision");}
    static std::string run(const cnjson::Value& view){auto self=view.object.find("selfState");if(self==view.object.end()||self->second.kind==cnjson::Value::Null)return {};auto id=self->second.object.find("runId");return id==self->second.object.end()?std::string{}:text(self->second,"runId");}
    bool room_active()const{return !room.empty();}
    void registered(const cnjson::Value& reply){
        const auto id=text(reply,"participantId"),credential=text(reply,"token");
        if(at(reply,"ok").kind!=cnjson::Value::Boolean||!at(reply,"ok").boolean||at(reply,"officialFallback").kind!=cnjson::Value::Boolean||at(reply,"officialFallback").boolean||!hex_id(id)||!token_valid(credential)||enrolled())throw std::runtime_error("multiplayer enrollment identity invalid");
        participant=id;token=credential;
    }
    bool accept(const cnjson::Value& view){
        if(at(view,"ok").kind!=cnjson::Value::Boolean||!at(view,"ok").boolean||
           at(view,"officialFallback").kind!=cnjson::Value::Boolean||at(view,"officialFallback").boolean)
            throw std::runtime_error("multiplayer acknowledgement invalid");
        const auto id=text(view,"roomId");if(!hex_id(id)||(!room.empty()&&room!=id))throw std::runtime_error("multiplayer response room changed");
        const auto left=view.object.find("left");
        if(left!=view.object.end()&&left->second.kind==cnjson::Value::Boolean&&left->second.boolean){room.clear();latest={};visible_generation=visible_event=0;return true;}
        const auto generation=integer(view,"generation"),event=integer(view,"eventSeq");
        if(generation<1||integer(view,"seat")>3||!mode(text(view,"modeId")))throw std::runtime_error("multiplayer snapshot identity invalid");
        const auto& roster=at(view,"roster");if(roster.kind!=cnjson::Value::Array||roster.array.empty()||roster.array.size()>4)throw std::runtime_error("multiplayer roster invalid");
        unsigned own_seats=0;for(const auto& row:roster.array)if(integer(row,"seat")==integer(view,"seat")&&text(row,"participantId")==participant)++own_seats;
        if(own_seats!=1)throw std::runtime_error("multiplayer response belongs to a different participant seat");
        if(latest.kind==cnjson::Value::Object){const auto oldgen=integer(latest,"generation"),oldseq=integer(latest,"eventSeq");
            if(generation<oldgen||event<oldseq)return false;
            const auto oldrun=run(latest),newrun=run(view);
            if(oldrun==newrun){if(revision(view)<revision(latest))return false;}
            else if(generation==oldgen)return false;
            auto oldboss=latest.object.find("bossNative");auto newboss=view.object.find("bossNative");
            if(oldrun==newrun&&oldboss!=latest.object.end()&&newboss!=view.object.end()&&oldboss->second.kind==cnjson::Value::Object&&newboss->second.kind==cnjson::Value::Object){
                const auto oldepoch=integer(oldboss->second,"epoch"),newepoch=integer(newboss->second,"epoch");
                if(newepoch<oldepoch||(newepoch==oldepoch&&integer(newboss->second,"availableThrough")<integer(oldboss->second,"availableThrough")))return false;
            }}
        room=id;latest=view;return true;
    }
    void presented(const cnjson::Value& view){
        if(room.empty()||text(view,"roomId")!=room)return;
        if(integer(view,"generation")<visible_generation||integer(view,"eventSeq")<visible_event)return;
        visible_generation=integer(view,"generation");visible_event=integer(view,"eventSeq");
    }
    void enqueue(const std::string& operation,const std::string& request,const cnjson::Value& payload){
        if(!hex_id(request)||payload.kind!=cnjson::Value::Object||pending.size()>=256)throw std::runtime_error("multiplayer outgoing queue invalid or full");
        auto body=object();body.object["requestId"]=string(request);
        std::string route="/bridge/act2autochess/multiplayer/";
        if((operation=="create"||operation=="join")&&pending.empty()&&latest.kind==cnjson::Value::Object&&text(latest,"phase")=="result"){
            room.clear();latest={};visible_generation=visible_event=0;
        }
        if(operation=="create"){
            if(room_active()||!pending.empty()||!mode(text(payload,"modeId")))throw std::runtime_error("multiplayer create requires an idle custom-room client");
            route+="rooms";body.object["modeId"]=at(payload,"modeId");
        }else if(operation=="join"){
            const auto value=text(payload,"roomCode");if(room_active()||!pending.empty()||!code(value))throw std::runtime_error("multiplayer join requires a six-digit room code and idle client");
            route+="rooms/"+value+"/join";
        }else{
            if(!room_active()||(visible_generation<1&&operation!="reconnect"))throw std::runtime_error("multiplayer command has no presented room");
            route+="rooms/"+room+"/commands";body.object["generation"]=number(visible_generation<1?1:visible_generation);body.object["op"]=string(operation);body.object["payload"]=payload;
        }
        auto squad=payload.object.find("chessSquad");if((operation=="create"||operation=="join")&&squad!=payload.object.end()){if(squad->second.kind!=cnjson::Value::Object)throw std::runtime_error("multiplayer roster snapshot malformed");body.object["profileSquad"]=squad->second;}
        auto display=payload.object.find("displayProfile");if((operation=="create"||operation=="join")&&display!=payload.object.end()){if(display->second.kind!=cnjson::Value::Object)throw std::runtime_error("multiplayer display snapshot malformed");body.object["displayProfile"]=display->second;}
        pending.push_back({operation,route,body});
    }
    cnjson::Value journal()const{
        auto out=object();out.object["version"]=number(1);out.object["origin"]=string(origin);out.object["participantId"]=string(participant);out.object["token"]=string(token);out.object["roomId"]=string(room);
        auto queue=array();for(const auto& row:pending){auto item=object();item.object["operation"]=string(row.operation);item.object["route"]=string(row.route);item.object["body"]=row.body;queue.array.push_back(std::move(item));}out.object["pending"]=std::move(queue);return out;
    }
    void restore(const cnjson::Value& data,const std::string& expected_origin){
        State restored;restored.origin=expected_origin;
        if(integer(data,"version")!=1||text(data,"origin")!=expected_origin)throw std::runtime_error("multiplayer journal origin/version mismatch");
        restored.participant=text(data,"participantId");restored.token=text(data,"token");restored.room=text(data,"roomId");
        if((!restored.participant.empty()||!restored.token.empty())&&(!hex_id(restored.participant)||!token_valid(restored.token)))throw std::runtime_error("multiplayer journal participant invalid");
        if(!restored.room.empty()&&(!hex_id(restored.room)||!restored.enrolled()))throw std::runtime_error("multiplayer journal room invalid");
        const auto& queue=at(data,"pending");if(queue.kind!=cnjson::Value::Array||queue.array.size()>256)throw std::runtime_error("multiplayer journal queue invalid");
        for(const auto& row:queue.array){Pending item{text(row,"operation"),text(row,"route"),at(row,"body")};
            if(!hex_id(text(item.body,"requestId")))throw std::runtime_error("multiplayer journal request invalid");
            const auto prefix=std::string("/bridge/act2autochess/multiplayer/rooms");
            bool valid=item.operation=="create"&&item.route==prefix&&mode(text(item.body,"modeId"));
            if(item.operation=="join")valid=item.route.size()==prefix.size()+12&&item.route.compare(0,prefix.size()+1,prefix+"/")==0&&code(item.route.substr(prefix.size()+1,6))&&item.route.substr(prefix.size()+7)=="/join";
            if(item.operation!="create"&&item.operation!="join")valid=!restored.room.empty()&&item.route==prefix+"/"+restored.room+"/commands"&&integer(item.body,"generation")>=1&&text(item.body,"op")==item.operation&&at(item.body,"payload").kind==cnjson::Value::Object;
            if(!valid)throw std::runtime_error("multiplayer journal route invalid");restored.pending.push_back(std::move(item));
        }
        *this=std::move(restored);
    }
};
}
