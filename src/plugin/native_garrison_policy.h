#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
#include "native_garrison_async.h"

namespace cngarrison {
struct Event {
    std::string run, request;
    std::int32_t round=0, source=0, count=0;
    std::vector<std::int32_t> bonds;
    unsigned attempts=0;
    std::uint64_t last_attempt=0;
};
inline void validate(Event& event) {
    if(event.run.empty()||event.run.size()>128||event.round<1||event.round>15||event.source<=0||event.count<=0||event.count>999)
        throw std::runtime_error("private garrison event identity/count invalid");
    if(event.bonds.empty()||event.bonds.size()>64)throw std::runtime_error("private garrison target count invalid");
    auto canonical=event.bonds;std::sort(canonical.begin(),canonical.end());
    if(canonical.front()<0||canonical.back()>255||std::adjacent_find(canonical.begin(),canonical.end())!=canonical.end())
        throw std::runtime_error("private garrison target identifier invalid or repeated");
    event.bonds=std::move(canonical);
}
// Own immutable C++ data only. Every emitted native event remains a separate
// transaction, including two identical triggers in the same engine frame.
class Queue {
    std::deque<Event> events_;
    std::uint64_t sequence_=0;
public:
    void push(Event event,const std::string& incarnation) {
        validate(event);
        if(events_.size()>=4096)throw std::runtime_error("private garrison pending queue overflow");
        event.request="native-bond-"+event.run+"-"+std::to_string(event.round)+"-"+incarnation+"-"+std::to_string(++sequence_);
        events_.push_back(std::move(event));
    }
    bool empty()const{return events_.empty();}
    std::size_t size()const{return events_.size();}
    Event& front(){if(empty())throw std::runtime_error("private garrison queue empty");return events_.front();}
    const std::deque<Event>& events()const{return events_;}
    void restore(Event event){
        validate(event);const auto prefix="native-bond-"+event.run+"-"+std::to_string(event.round)+"-";
        if(event.request.rfind(prefix,0)!=0||event.request.size()>128||event.request.size()<=prefix.size())throw std::runtime_error("private garrison persisted request identity invalid");
        for(char c:event.request.substr(prefix.size()))if((c<'0'||c>'9')&&c!='-')throw std::runtime_error("private garrison persisted request identity invalid");
        if(events_.size()>=4096)throw std::runtime_error("private garrison persisted queue overflow");
        for(const auto& old:events_)if(old.request==event.request)throw std::runtime_error("private garrison persisted request repeated");
        events_.push_back(std::move(event));
    }
    void acknowledge(const std::string& request){if(front().request!=request)throw std::runtime_error("private garrison acknowledgement order mismatch");events_.pop_front();}
    void clear(){events_.clear();} // Preserve process sequence across sessions.
};
inline bool retry_due(const Event& event,std::uint64_t now){return event.attempts<3&&(!event.last_attempt||now-event.last_attempt>=5000);}
inline bool capture_scope(bool private_scope,bool projecting,bool in_flight){return private_scope&&!projecting&&!in_flight;}
} // namespace cngarrison
