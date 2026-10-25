#pragma once
#include "native_json.h"
#include <set>

// This is the private HTTP envelope planner. It never changes a native DTO and
// never treats a repeated room read as a newly received battle scene.
namespace cnmulti_dispatch {
// Presentation catchup is deliberately finite. The authoritative server
// outbox and its economic effects remain complete; a returning UI plays only
// the newest announcements instead of blocking scene recovery on old text.
inline constexpr size_t broadcast_catchup_limit=128;
inline void append_broadcast_tail(std::vector<cnjson::Value>& pending,const std::vector<cnjson::Value>& incoming){
    if(incoming.size()>=broadcast_catchup_limit){pending.assign(incoming.end()-broadcast_catchup_limit,incoming.end());return;}
    const auto retained=broadcast_catchup_limit-incoming.size();
    if(pending.size()>retained)pending.erase(pending.begin(),pending.end()-retained);
    pending.insert(pending.end(),incoming.begin(),incoming.end());
}
inline const cnjson::Value& at(const cnjson::Value& value,const char* key){
    auto item=value.object.find(key);
    if(value.kind!=cnjson::Value::Object||item==value.object.end())throw std::runtime_error(std::string("multiplayer dispatch field missing: ")+key);
    return item->second;
}
inline int64_t number(const cnjson::Value& value,const char* key){const auto& item=at(value,key);if(item.kind!=cnjson::Value::Integer)throw std::runtime_error("multiplayer dispatch integer required");return item.integer;}
inline std::string string(const cnjson::Value& value,const char* key){const auto& item=at(value,key);if(item.kind!=cnjson::Value::String)throw std::runtime_error("multiplayer dispatch string required");return item.string;}
inline cnjson::Value object(){cnjson::Value value;value.kind=cnjson::Value::Object;return value;}
struct Snapshot {
    std::string room,immutable,scene,preparation,players;
    int64_t generation=0,revision=-1,event_seq=0;
    bool seeded=false;
};
struct Plan {
    Snapshot next;
    bool available=false,stale=false,full=false,scene=false,preparation=false,players=false,board=false;
    cnjson::Value board_status;
    std::vector<cnjson::Value> special_updates;
    std::vector<cnjson::Value> broadcasts;
    std::vector<cnjson::Value> chats;
};
inline bool board_delta(const std::string& previous,const cnjson::Value& preparation,cnjson::Value& result){
    if(previous.empty()||preparation.kind!=cnjson::Value::Object)return false;
    auto old=cnjson::parse(previous);if(old.kind!=cnjson::Value::Object)return false;
    auto oldRest=old,newRest=preparation;
    for(const auto* key:{"positions","bonds"}){if(!oldRest.object.count(key)||!newRest.object.count(key))return false;oldRest.object.erase(key);newRest.object.erase(key);}
    if(cnjson::dump(oldRest)!=cnjson::dump(newRest))return false;
    const auto& oldBonds=at(old,"bonds");const auto& newBonds=at(preparation,"bonds");
    if(oldBonds.kind!=cnjson::Value::Array||newBonds.kind!=cnjson::Value::Array||at(preparation,"positions").kind!=cnjson::Value::Array)return false;
    std::map<int64_t,std::string> prior;
    for(const auto& bond:oldBonds.array)if(!prior.emplace(number(bond,"bondIdentifier"),cnjson::dump(bond)).second)return false;
    cnjson::Value updates;updates.kind=cnjson::Value::Array;std::set<int64_t> seen;
    for(const auto& bond:newBonds.array){auto id=number(bond,"bondIdentifier");auto found=prior.find(id);if(found==prior.end()||!seen.insert(id).second)return false;if(found->second!=cnjson::dump(bond))updates.array.push_back(bond);}
    if(seen.size()!=prior.size())return false; // BoardStatus updates cannot remove a bond entry.
    result=object();result.object["uidIndex"]=at(preparation,"uidIndex");result.object["positions"]=at(preparation,"positions");result.object["updatedBonds"]=std::move(updates);return true;
}
inline bool special_delta(const std::string& previous,const cnjson::Value& scene,std::vector<cnjson::Value>& updates){
    if(previous.empty()||number(scene,"state")!=2)return false;
    auto old=cnjson::parse(previous);if(number(old,"state")!=2)return false;
    auto oldRest=old,newRest=scene;newRest.object.erase("runtimePlayers");newRest.object.erase("preparationStatus");newRest.object.erase("obUidIndex");
    // SP effects are selected here and deployed by the later PREPARE task.
    // They do not justify a fresh scene-state/board initialization per pick.
    for(const auto* key:{"specialPreparation","specialEffects","forceEndTs"}){oldRest.object.erase(key);newRest.object.erase(key);}
    if(cnjson::dump(oldRest)!=cnjson::dump(newRest))return false;
    auto before=old.object.find("specialPreparation");auto after=scene.object.find("specialPreparation");
    if(before==old.object.end()||after==scene.object.end()||before->second.kind!=cnjson::Value::Object||after->second.kind!=cnjson::Value::Object)return false;
    auto oldSp=before->second,newSp=after->second;const auto oldQueue=at(oldSp,"playersQueue"),queue=at(newSp,"playersQueue");
    if(oldQueue.kind!=cnjson::Value::Array||queue.kind!=cnjson::Value::Array||oldQueue.array.size()!=queue.array.size()||queue.array.size()>4)return false;
    for(const auto* key:{"playersQueue","finTs","currQueueIndex"}){oldSp.object.erase(key);newSp.object.erase(key);}
    if(cnjson::dump(oldSp)!=cnjson::dump(newSp))return false;
    const auto index=number(after->second,"currQueueIndex"),oldIndex=number(before->second,"currQueueIndex"),deadline=number(after->second,"finTs");
    if(index<oldIndex||index<0||size_t(index)>queue.array.size()||deadline<0)return false;
    std::vector<cnjson::Value> next;
    for(size_t i=0;i<queue.array.size();++i){const auto& from=oldQueue.array[i];const auto& to=queue.array[i];if(number(from,"uidIndex")!=number(to,"uidIndex"))return false;
        if(cnjson::dump(from)!=cnjson::dump(to)){auto value=object();value.object["playerChoice"]=to;value.object["finTs"]=at(after->second,"finTs");value.object["currQueueIndex"]=at(after->second,"currQueueIndex");next.push_back(std::move(value));}}
    if(next.empty())return false;updates=std::move(next);return true;
}
inline void special_receipt(const cnjson::Value& row,int64_t sequence,int64_t after,int64_t cursor,int64_t round,std::map<int64_t,cnjson::Value>& ordered){
    if(sequence<=after)return;if(sequence>cursor)throw std::runtime_error("multiplayer special update cursor exceeds its envelope");
    if(number(row,"round")!=round)return; // Historical rounds are not current native selection models.
    const auto& update=at(row,"updateInfo");const auto& choice=at(update,"playerChoice");
    const auto seat=number(choice,"uidIndex"),slot=number(choice,"choice"),index=number(update,"currQueueIndex"),deadline=number(update,"finTs");
    if(seat<0||seat>3||slot<0||slot>1024||index<0||index>4||deadline<0)throw std::runtime_error("multiplayer special update contract outside pinned native bounds");
    auto old=ordered.find(sequence);if(old!=ordered.end()&&cnjson::dump(old->second)!=cnjson::dump(update))throw std::runtime_error("multiplayer special update sequence conflicts");ordered[sequence]=update;
}
inline Plan plan(const Snapshot& old,const cnjson::Value& view){
    Plan result;
    if(string(view,"activityId")!="act2autochess")throw std::runtime_error("multiplayer dispatch activity mismatch");
    const auto room=string(view,"roomId");const auto mode=string(view,"modeId");
    if(room.empty()||room.size()>128||mode.rfind("mode_multi_",0)!=0)throw std::runtime_error("multiplayer dispatch room/mode mismatch");
    const auto seat=number(view,"seat"),generation=number(view,"generation"),event_seq=number(view,"eventSeq");
    if(seat<0||seat>3||generation<1||event_seq<0)throw std::runtime_error("multiplayer dispatch envelope bound");
    if(!old.room.empty()&&old.room!=room)throw std::runtime_error("multiplayer dispatch room changed without reset");
    const auto& self=at(view,"selfState");int64_t revision=self.kind==cnjson::Value::Null?-1:number(self,"revision");
    if(generation<old.generation||(generation==old.generation&&(revision<old.revision||event_seq<old.event_seq))){result.stale=true;return result;}
    result.next=old;result.next.room=room;result.next.generation=generation;result.next.revision=revision;
    const auto& projection=at(view,"dtoProjection");const auto& packet=at(projection,"allStateSync");
    if(packet.kind!=cnjson::Value::Null){
        const auto& all=at(at(packet,"payload"),"allStateSyncData");
        if(number(all,"uidIndex")!=seat||string(all,"modeId")!=mode)throw std::runtime_error("multiplayer dispatch recipient mismatch");
        const auto& scene=at(all,"sceneDetail");auto immutable=all;immutable.object.erase("sceneDetail");
        auto core=scene;core.object.erase("runtimePlayers");core.object.erase("preparationStatus");
        // Rest observation is delivered as PreparationStatusDn, not a scene
        // state transition (official events-001:735..780). The private plan
        // owns the target/view switch. Do not re-dirty all native scene modules
        // merely because its envelope describes that target in obUidIndex.
        const auto state=number(scene,"state");if(state==2||state==3)core.object.erase("obUidIndex");
        result.next.immutable=cnjson::dump(immutable);result.next.scene=cnjson::dump(core);
        result.next.preparation=cnjson::dump(at(scene,"preparationStatus"));result.next.players=cnjson::dump(at(scene,"runtimePlayers"));
        result.available=true;result.full=!old.seeded||old.immutable!=result.next.immutable;
        result.scene=!result.full&&old.scene!=result.next.scene;
        if(result.scene&&special_delta(old.scene,scene,result.special_updates))result.scene=false;
        result.preparation=!result.full&&!result.scene&&old.preparation!=result.next.preparation;
        // SELF_BATTLE is owned by the original local engine. Server layer
        // receipts update durable authority, never replay preparation modules
        // into a running field. The following phase's Scene Fill imports the
        // complete committed board/layers as usual. Peer finish status remains
        // a RuntimePlayers delta and is delivered immediately.
        if(state==4&&old.generation==generation&&!result.full&&!result.scene)result.preparation=false;
        result.players=!result.full&&!result.scene&&old.players!=result.next.players;
        // Official movement is BoardStatusDn / FillWithBoardStatus. Updating
        // the complete preparation here also re-dirties shop/choice modules.
        // Retain the broad fill whenever any non-board value really changed.
        if(result.preparation&&board_delta(old.preparation,at(scene,"preparationStatus"),result.board_status)){result.board=true;result.preparation=false;}
        result.next.seeded=true;
    }
    const auto& events=at(view,"events");if(events.kind!=cnjson::Value::Array)throw std::runtime_error("multiplayer dispatch events array required");
    std::map<int64_t,cnjson::Value> special;
    const auto priorScene=old.scene.empty()?cnjson::Value{}:cnjson::parse(old.scene);
    const bool mayCompleteSpecial=result.available&&!result.full&&old.seeded&&priorScene.kind==cnjson::Value::Object&&number(priorScene,"state")==2;
    int64_t previous=old.event_seq;
    for(const auto& event:events.array){
        const auto seq=number(event,"seq");if(seq<=old.event_seq)continue;
        if(seq<=previous||seq>event_seq)throw std::runtime_error("multiplayer dispatch events order mismatch");previous=seq;
        if(string(event,"kind")=="broadcast"){
            if(result.broadcasts.size()==broadcast_catchup_limit)result.broadcasts.erase(result.broadcasts.begin());
            result.broadcasts.push_back(at(event,"payload"));
        }
        // Ephemeral native speech bubbles: no history playback on cold rejoin.
        if(old.seeded&&string(event,"kind")=="chat"){
            const auto& chat=at(event,"payload");
            if(number(chat,"expiresTs")>number(view,"serverTime")){
                if(result.chats.size()==16)result.chats.erase(result.chats.begin());
                result.chats.push_back(chat);
            }
        }
        if(mayCompleteSpecial&&string(event,"kind")=="special-preparation-update")special_receipt(at(event,"payload"),seq,old.event_seq,event_seq,number(priorScene,"round"),special);
    }
    auto responseUpdates=view.object.find("nativeSpPreparationUpdates");
    if(mayCompleteSpecial&&responseUpdates!=view.object.end()){
        if(responseUpdates->second.kind!=cnjson::Value::Array||responseUpdates->second.array.size()>16)throw std::runtime_error("multiplayer special command receipt bound");
        for(const auto& row:responseUpdates->second.array)special_receipt(row,number(row,"eventSeq"),old.event_seq,event_seq,number(priorScene,"round"),special);
    }
    if(!special.empty()){result.special_updates.clear();for(auto& row:special)result.special_updates.push_back(std::move(row.second));}
    result.next.event_seq=event_seq;return result;
}
inline std::string short_type(const std::string& value){auto at=value.rfind('+');return at==std::string::npos?value:value.substr(at+1);}
struct Upstream {std::string op="protocol";cnjson::Value payload=object();};
inline Upstream upstream(const std::string& type,const cnjson::Value& dto){
    if(dto.kind!=cnjson::Value::Object)throw std::runtime_error("multiplayer upstream DTO object required");
    Upstream result;const auto name=short_type(type);
    if(name=="AutoChessBattleSceneObserveUp"){
        const auto seat=number(dto,"obIndex");if(seat<0||seat>3)throw std::runtime_error("multiplayer observation seat invalid");
        result.op="watch";result.payload.object["seat"]=at(dto,"obIndex");return result;
    }
    if(name=="AutoChessBattleSceneCancelObserveUp"){result.op="watch";return result;}
    if(name!="AutoChessBattleSceneChatReq"&&(name.rfind("AutoChessBattleScene",0)!=0||name.size()<2||name.substr(name.size()-2)!="Up"))throw std::runtime_error("multiplayer upstream declaration outside battle scope");
    cnjson::Value text;text.kind=cnjson::Value::String;text.string=type;result.payload.object["type"]=text;result.payload.object["payload"]=dto;return result;
}
}
