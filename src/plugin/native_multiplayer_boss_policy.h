#pragma once
#include "native_multiplayer_dispatch_policy.h"

namespace cnmulti_boss {
struct Journal {
    int64_t epoch=0,seat=-1,group=0,report_group=0,authority=-1,available=-1;
    std::string room,timeline;
    std::vector<std::string> committed;
};
struct Window {Journal next;std::vector<cnjson::Value> steps;std::vector<int32_t> acknowledged;bool active=false;};
inline Window accept(const Journal& old,const cnjson::Value& view){
    using namespace cnmulti_dispatch;Window out;out.next=old;
    if(string(view,"phase")!="boss"){out.next={};return out;}
    auto reconstruction=view.object.find("selfReconstruction");
    if(reconstruction!=view.object.end()&&reconstruction->second.kind==cnjson::Value::Object){out.next={};return out;}
    const auto& value=at(view,"bossNative");
    if(value.kind==cnjson::Value::Null)throw std::runtime_error("private native boss clock absent");
    const auto epoch=number(value,"epoch"),seat=number(view,"seat"),group=number(value,"group"),report=number(value,"reportGroup"),authority=number(value,"reportAuthority");
    const auto room=string(view,"roomId"),timeline=string(value,"timelineId");
    if(room.empty()||room.size()>128||timeline.size()!=64||timeline.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::runtime_error("private native boss receipt identity invalid");
    if(number(value,"version")!=1||number(value,"durationMs")!=100||epoch<1||seat<0||seat>3||group<1||group>2||report<0||report>2||authority< -1||authority>3)
        throw std::runtime_error("private native boss policy mismatch");
    if(old.epoch&&epoch<old.epoch)return out;
    if(epoch==old.epoch&&(room!=old.room||timeline!=old.timeline))throw std::runtime_error("private native boss epoch identity changed");
    if(epoch!=old.epoch)out.next={};
    out.active=true;out.next.epoch=epoch;out.next.seat=seat;out.next.group=group;out.next.report_group=report;out.next.authority=authority;out.next.room=room;out.next.timeline=timeline;
    const auto first=number(value,"firstSeq"),available=number(value,"availableThrough");const auto& steps=at(value,"steps");
    if(first<0||available<0||available>=18000||first>available+1||steps.kind!=cnjson::Value::Array||steps.array.size()>128)
        throw std::runtime_error("private native boss window bounds");
    if(available<old.available&&epoch==old.epoch){out.next=old;out.active=false;return out;}
    out.next.available=available;
    const auto& receipt=at(value,"damageAcknowledgements");const auto& acknowledged=at(receipt,"seqs");
    if(string(receipt,"roomId")!=room||string(receipt,"timelineId")!=timeline||number(receipt,"epoch")!=epoch||
        number(receipt,"round")!=number(value,"round")||number(value,"round")!=number(view,"round")||
        number(receipt,"group")!=group||acknowledged.kind!=cnjson::Value::Array||acknowledged.array.size()>128)
        throw std::runtime_error("private native boss receipt owner mismatch");
    int64_t prior=-1;
    for(const auto& seq:acknowledged.array){
        if(seq.kind!=cnjson::Value::Integer||seq.integer<=prior||seq.integer>available)
            throw std::runtime_error("private native boss receipt sequence invalid");
        prior=seq.integer;out.acknowledged.push_back(int32_t(seq.integer));
    }
    auto expected=first;
    for(const auto& step:steps.array){
        const auto seq=number(step,"seq");const auto& actions=at(step,"actions");
        if(seq!=expected++||seq>available||number(step,"duration")!=100||actions.kind!=cnjson::Value::Array||actions.array.size()>16384)
            throw std::runtime_error("private native boss frame shape");
        for(const auto& action:actions.array){
            const auto operation=number(action,"operate");const auto& params=at(action,"paramList");
            if(number(action,"playerUidIndex")!=0||operation<5||operation>10||params.kind!=cnjson::Value::Array||params.array.size()>4098)
                throw std::runtime_error("private native boss down action shape");
            for(const auto& param:params.array)if(param.kind!=cnjson::Value::Integer||param.integer<INT32_MIN||param.integer>INT32_MAX)throw std::runtime_error("private native boss action integer bounds");
            const auto count=params.array.size();
            if(((operation==5||operation==6)&&count!=3)||(operation==7&&count!=1)||(operation==8&&count<2)||((operation==9||operation==10)&&count!=2))
                throw std::runtime_error("private native boss action parameters");
        }
        const auto encoded=cnjson::dump(step);
        if(size_t(seq)<out.next.committed.size()){
            if(out.next.committed[size_t(seq)]!=encoded)throw std::runtime_error("private native boss committed frame changed");
        }else{
            if(size_t(seq)!=out.next.committed.size()+out.steps.size())throw std::runtime_error("private native boss missing frame window");
            out.steps.push_back(step);
        }
    }
    return out;
}
inline void commit(Journal& journal,const cnjson::Value& step){
    const auto seq=cnmulti_dispatch::number(step,"seq");
    if(seq!=int64_t(journal.committed.size()))throw std::runtime_error("private native boss commit sequence mismatch");
    journal.committed.push_back(cnjson::dump(step));
}
inline bool reports(const Journal& value){return value.epoch&&value.report_group==value.group&&value.seat==value.authority;}
}
