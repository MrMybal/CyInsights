// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "NativeReaders.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <vector>
namespace cyinsights {
class NativeSetup {
public:
    enum class State {Idle,Discovering,Preparing,Ready,Unavailable,Failed,Cancelled};
    ~NativeSetup();
    void Discover(const std::filesystem::path& preferred={});
    void Prepare(const std::filesystem::path& analyzer,const std::filesystem::path& app=ApplicationDirectory());
    void Poll();
    void Cancel();
    bool Busy() const {return state==State::Discovering||state==State::Preparing;}
    double Elapsed() const {return std::chrono::duration<double>(std::chrono::steady_clock::now()-started_).count();}
    State state=State::Idle;
    std::filesystem::path selected,log;
    std::vector<std::filesystem::path> installations;
    std::string message,log_tail;
    std::atomic<unsigned> phase{0};
    static std::vector<std::filesystem::path> Detect(const std::filesystem::path& preferred={});
    static std::string SourceStamp(const std::filesystem::path& app);
    static bool ReadersReady(const std::filesystem::path& analyzer,const std::filesystem::path& app);
private:
    struct Result {State state=State::Failed;std::string message;};
    std::future<std::vector<std::filesystem::path>> discover_;
    std::future<Result> prepare_;
    std::atomic<bool> cancel_{false};
    std::chrono::steady_clock::time_point started_=std::chrono::steady_clock::now(),last_log_=started_;
    Result Build(const std::filesystem::path& analyzer,const std::filesystem::path& app,
                 const std::filesystem::path& destination,const std::filesystem::path& journal);
};
}
