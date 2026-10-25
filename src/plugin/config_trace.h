#pragma once
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

// Keep a failed save distinguishable from a later successful UI operation.
struct ConfigTraceEntry {
    uint64_t sequence=0;
    std::string operation,phase,error;
    bool backendAccepted=false,privateDtoApplied=false,completed=false;
};
class ConfigTrace {
    std::mutex mutex_;
    uint64_t sequence_=0;
    std::vector<ConfigTraceEntry> entries_;
    ConfigTraceEntry last_failure_;
public:
    static constexpr size_t capacity=32;
    uint64_t begin(const char* operation) {
        std::lock_guard<std::mutex> lock(mutex_);
        if(entries_.size()==capacity)entries_.erase(entries_.begin());
        ConfigTraceEntry entry;entry.sequence=++sequence_;entry.operation=operation;entry.phase="identify-activity";
        entries_.push_back(entry);return entry.sequence;
    }
    void update(uint64_t sequence,const char* phase,bool accepted=false,bool applied=false,bool complete=false,const std::string& error={}) {
        std::lock_guard<std::mutex> lock(mutex_);
        for(auto& entry:entries_)if(entry.sequence==sequence){
            entry.phase=phase;entry.backendAccepted|=accepted;entry.privateDtoApplied|=applied;entry.completed|=complete;
            if(!error.empty()){entry.error=error.substr(0,4096);last_failure_=entry;}
            return;
        }
    }
    std::vector<ConfigTraceEntry> snapshot(ConfigTraceEntry& last_failure) {
        std::lock_guard<std::mutex> lock(mutex_);last_failure=last_failure_;return entries_;
    }
};
