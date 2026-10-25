#pragma once
// Research only. No hooks or managed runtime calls in this policy.
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace cnmulti_own_start {
enum class Owner { Normal, Boss, Help, Unknown };
enum class Decision { Proceed, YieldNull, Stop };
struct Stamp {
    std::string room,run;
    int64_t match=0;
    int32_t round=0,seat=-1;
    bool privateMulti=false;
    // Native game state: BATTLE=4, HELP=5, BOSS=6. Not a guessed enum index.
    int32_t nativeState=0;
};
inline bool same_stamp(const Stamp& a,const Stamp& b) {
    return a.room==b.room&&a.run==b.run&&a.match==b.match&&a.round==b.round&&
        a.seat==b.seat&&a.privateMulti==b.privateMulti&&a.nativeState==b.nativeState;
}
struct World {
    void* iterator=nullptr;
    void* task=nullptr;
    void* controller=nullptr;
    void* map=nullptr; // Map object, not mutable LevelData/MapData.
    void* manager=nullptr;
    void* bridge=nullptr;
    void* level=nullptr; // Diagnostic only. Intentionally excluded from the lease.
};
struct Host {
    std::function<bool()> mayStartOwnBattle;
    std::function<uint32_t(void*)> retain;
    std::function<void*(uint32_t)> resolve;
    std::function<void(uint32_t)> release;
    std::function<uint32_t(void*)> retainWeak;
};
class Gate {
    Host host;
    Stamp stamp;
    Owner owner=Owner::Unknown;
    std::string stopReason;
    std::vector<uint32_t> handles;
    uint32_t pending=0;Stamp pendingStamp;Owner pendingOwner=Owner::Unknown;
    void clear_pending(){if(pending)host.release(pending);pending=0;pendingStamp={};pendingOwner=Owner::Unknown;}
    // A parent coroutine can still be yielded in StartMainBattle while the
    // service advances to PREPARE/the next round. Keep a weak iterator root
    // until cancellation/collection, so a late MoveNext cannot poison a new lease.
    std::vector<uint32_t> retired;
    static std::vector<void*> physical(const World& w) {
        return {w.iterator,w.task,w.controller,w.map,w.manager,w.bridge};
    }
    bool matches(const World& world) const {
        const auto values=physical(world);
        if(handles.size()!=values.size())return false;
        for(size_t i=0;i<values.size();++i)
            if(!values[i]||host.resolve(handles[i])!=values[i])return false;
        return true;
    }
    void acquire(const World& world) {
        handles.reserve(6); // Allocate before acquiring any managed root.
        for(auto value:physical(world)) {
            const auto handle=host.retain(value);
            if(!handle)throw std::runtime_error("own start strong root unavailable");
            handles.push_back(handle);
            if(host.resolve(handle)!=value)throw std::runtime_error("own start root changed");
        }
    }
public:
    explicit Gate(Host value):host(std::move(value)){}
    Gate(const Gate&)=delete;
    Gate& operator=(const Gate&)=delete;
    ~Gate(){reset();}
    void reset_active() {
        clear_pending();
        for(auto i=handles.rbegin();i!=handles.rend();++i)host.release(*i);
        handles.clear();stamp={};owner=Owner::Unknown;
    }
    void reset() {
        reset_active();
        for(auto i=retired.rbegin();i!=retired.rend();++i)host.release(*i);
        retired.clear();
    }
    void reap_retired() {
        for(auto i=retired.begin();i!=retired.end();)if(!host.resolve(*i)){auto h=*i;i=retired.erase(i);host.release(h);}else ++i;
    }
    void retire() {
        reap_retired();if(pending){if(retired.size()>=16)throw std::runtime_error("own start retirement bound exceeded");retired.push_back(pending);pending=0;pendingStamp={};pendingOwner=Owner::Unknown;}if(handles.empty())return;
        // A weak root recognizes an iterator still held by Unity without
        // retaining a disposed battlefield through that iterator's references.
        if(retired.size()>=16)throw std::runtime_error("own start retirement bound exceeded");
        auto value=host.resolve(handles.front());auto weak=host.retainWeak(value);
        if(!weak||host.resolve(weak)!=value){if(weak)host.release(weak);throw std::runtime_error("own start retired weak root unavailable");}
        try{retired.push_back(weak);}catch(...){host.release(weak);throw;}
        reset_active();
    }
    bool retired_iterator(void* iterator) const {
        for(auto h:retired)if(host.resolve(h)==iterator)return true;
        return false;
    }
    bool complete_retired(void* iterator) {
        for(auto i=retired.begin();i!=retired.end();++i)if(host.resolve(*i)==iterator){auto h=*i;retired.erase(i);host.release(h);return true;}
        return false;
    }
    size_t retired_count() const {return retired.size();}
    // Called only by the authenticated UI projection, before its native state
    // notification. It revokes an old start; it never grants a new start.
    bool observe(const Stamp& current,bool ownBattle,bool pendingAllowed=true) {
        if(pending){const bool same=pendingStamp.room==current.room&&pendingStamp.run==current.run&&pendingStamp.match==current.match&&pendingStamp.round==current.round&&pendingStamp.seat==current.seat&&pendingStamp.privateMulti==current.privateMulti;
            if(!same||!pendingAllowed){retire();return true;}}
        if(handles.empty())return false;
        const bool sameActor=stamp.room==current.room&&stamp.run==current.run&&
            stamp.match==current.match&&stamp.seat==current.seat&&stamp.privateMulti==current.privateMulti;
        if(sameActor&&current.round<stamp.round)throw std::runtime_error("own start projection round regressed");
        if(!sameActor||stamp.round!=current.round||!ownBattle||stamp.nativeState!=current.nativeState){retire();return true;}
        return false;
    }
    bool leased() const {return !handles.empty();}
    bool waiting() const {return pending!=0;}
    bool owns(void* iterator) const {return (!handles.empty()&&host.resolve(handles[0])==iterator)||(pending&&host.resolve(pending)==iterator);}
    const std::string& failure() const {return stopReason;}
    Decision stop(const char* reason){reset_active();stopReason=reason;return Decision::Stop;}
    void wait(Owner kind,const Stamp& current,void* iterator){
        if(!iterator)throw std::runtime_error("own start pending iterator missing");
        if(pending){if(host.resolve(pending)!=iterator||pendingOwner!=kind||!same_stamp(pendingStamp,current))throw std::runtime_error("own start pending room/run/round or iterator changed");return;}
        auto weak=host.retainWeak(iterator);if(!weak||host.resolve(weak)!=iterator){if(weak)host.release(weak);throw std::runtime_error("own start pending weak root unavailable");}
        pending=weak;pendingStamp=current;pendingOwner=kind;
    }
    Decision before(Owner kind,int32_t state,const Stamp& current,const World& world) {
        stopReason.clear();
        // Only the exact registered Help class gets this exception. Its own NFF
        // loading barrier remains in force outside this independent own gate.
        if(kind==Owner::Help){retire();return Decision::Proceed;}
        if(kind!=Owner::Normal&&kind!=Owner::Boss)
            throw std::runtime_error("unregistered owner of shared battle iterator");
        const int32_t expectedState=kind==Owner::Boss?6:4;
        if(!current.privateMulti||current.room.empty()||current.run.empty()||current.match<1||
           current.round<1||current.round>15||current.seat<0||current.seat>3||
           current.nativeState!=expectedState)return stop("actor or native battle state changed");
        for(auto p:physical(world))if(!p)return stop("native battle owner is missing");
        if(state==0) {
            if(!handles.empty()) {
                if(owner!=kind)return stop("iterator owner changed before first yield");
                if(!same_stamp(stamp,current))return stop("room/run/round changed before first yield");
                if(!matches(world))return stop("Unity world changed before first yield");
                return Decision::Proceed;
            }
            if(pending)wait(kind,current,world.iterator);
            if(!host.mayStartOwnBattle()){wait(kind,current,world.iterator);return Decision::YieldNull;}
            clear_pending();
            try{acquire(world);}catch(...){reset_active();throw;}
            stamp=current;owner=kind;
            return Decision::Proceed;
        }
        if(state>=1&&state<=3) {
            if(owner!=kind)return stop("resumed iterator has no matching start lease");
            if(!same_stamp(stamp,current))return stop("room/run/round changed during native start");
            if(!matches(world))return stop("Unity world changed during native start");
            return Decision::Proceed;
        }
        return stop("native start iterator state is outside the pinned coroutine");
    }
    void after(uint8_t hasNext){if(!hasNext)reset_active();}
};
} // namespace cnmulti_own_start
