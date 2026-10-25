#pragma once
#include "native_garrison_policy.h"

namespace cngarrison {
// The caller owns the TLS counter and enables this only around the original
// native state transition, never around a model-only projection or an ACK.
class TransitionScope {
    unsigned& depth_;
    bool enabled_;
public:
    explicit TransitionScope(unsigned& depth,bool enabled=true):depth_(depth),enabled_(enabled){if(enabled_)++depth_;}
    ~TransitionScope(){if(enabled_)--depth_;}
    TransitionScope(const TransitionScope&)=delete;
    TransitionScope& operator=(const TransitionScope&)=delete;
};

inline bool capture_scope_with_transition(bool private_scope,bool projecting,bool in_flight,
                                          bool native_transition,bool live_battle){
    return private_scope&&((native_transition&&live_battle)||capture_scope(true,projecting,in_flight));
}

// A callback only queues owned C++ values and marks the journal dirty. Flush
// the whole queue at a safe boundary before its first HTTP send; no filesystem
// or network operation is authorized while any native/projection scope lives.
inline bool journal_flush_allowed(bool projecting,bool in_flight,bool native_transition){
    return !projecting&&!in_flight&&!native_transition;
}
} // namespace cngarrison
