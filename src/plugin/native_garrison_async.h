#pragma once
#include <atomic>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>

namespace cngarrison {
// One owned, bounded IO job at a time. Inputs/results contain C++ values only.
// No wait, filesystem call or network call occurs in start()/try_take() while
// the job is running. The host joins after its runtime gate has drained.
template<class Input,class Output> class AsyncJob {
    std::thread thread_;
    std::atomic<bool> done_{false};
    Output output_;
    std::exception_ptr exception_;
public:
    AsyncJob()=default;
    AsyncJob(const AsyncJob&)=delete;
    AsyncJob& operator=(const AsyncJob&)=delete;
    ~AsyncJob(){shutdown();}
    bool active()const{return thread_.joinable();}
    bool completed()const{return active()&&done_.load(std::memory_order_acquire);}
    void start(Input input,Output(*execute)(const Input&)){
        if(active())throw std::runtime_error("private garrison IO job already active");
        done_.store(false,std::memory_order_relaxed);exception_=nullptr;
        thread_=std::thread([this,input=std::move(input),execute](){
            try{output_=execute(input);}catch(...){exception_=std::current_exception();}
            done_.store(true,std::memory_order_release);
        });
    }
    bool try_take(Output& output){
        if(!completed())return false;
        thread_.join();if(exception_)std::rethrow_exception(exception_);
        output=std::move(output_);return true;
    }
    void shutdown(){if(thread_.joinable())thread_.join();}
};
} // namespace cngarrison
