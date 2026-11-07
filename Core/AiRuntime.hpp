// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <string>
namespace cyinsights::ai_runtime {
namespace fs=std::filesystem;
enum class State {Missing,Checking,Installing,Ready,Failed,Cancelled};
struct Result {State state=State::Missing;fs::path python;std::string error;};
class Setup {
public:
    Setup(fs::path source,fs::path storage);
    ~Setup();
    Setup(const Setup&)=delete;Setup& operator=(const Setup&)=delete;
    bool Check();
    bool Install();
    void Cancel(){cancel_=true;}
    void Poll();
    bool Busy()const{return state==State::Checking||state==State::Installing;}
    State state=State::Missing;
    fs::path source,storage,python,log;
    std::string error,log_tail;
    std::atomic<unsigned> phase{0};
    std::atomic<uint64_t> received{0},total{0};
    static std::string Fingerprint(const fs::path& source);
    static fs::path ReceiptPython(const fs::path& storage,const std::string& fingerprint);
    static void ValidateExtracted(const fs::path& directory);
private:
    bool Start(bool install);
    Result Work(bool install,const fs::path& journal);
    std::future<Result> task_;
    std::atomic<bool> cancel_{false};
    std::chrono::steady_clock::time_point last_log_{};
};
}
