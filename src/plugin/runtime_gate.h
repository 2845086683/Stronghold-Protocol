#pragma once
#include <atomic>
#include <thread>

namespace cn {
class RuntimeGate {
    std::atomic<bool>& closing;
    std::atomic<unsigned> readers{0};
public:
    explicit RuntimeGate(std::atomic<bool>& flag):closing(flag){}
    bool enter(){if(closing.load())return false;readers.fetch_add(1);if(closing.load()){readers.fetch_sub(1);return false;}return true;}
    void leave(){readers.fetch_sub(1);}
    void close_and_drain(){closing.store(true);while(readers.load())std::this_thread::yield();}
};
class RuntimeLease {
    RuntimeGate& gate;
    bool owned;
public:
    explicit RuntimeLease(RuntimeGate& value):gate(value),owned(value.enter()){}
    ~RuntimeLease(){release();}
    RuntimeLease(const RuntimeLease&)=delete;
    RuntimeLease& operator=(const RuntimeLease&)=delete;
    explicit operator bool() const{return owned;}
    void release(){if(owned){owned=false;gate.leave();}}
};
}
