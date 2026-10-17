// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "Capture.hpp"
#include <atomic>
namespace cyinsights {
struct MetaQuestOptions {
    bool enabled=true,restart_game=false; // Capture the running game; restart is explicit.
    fs::path adb;
    std::string device,package;
};
class MetaQuestCapture {
public:
    explicit MetaQuestCapture(const std::atomic<bool>& cancel,const std::atomic<bool>* stop=nullptr):cancel_(cancel),stop_(stop){}
    ~MetaQuestCapture();
    // Preparation occurs before the app-owned Unreal capture. Never interrupts
    // an active native trace; the caller verifies idle ownership first.
    bool Prepare(const MetaQuestOptions&,const std::string& ipv4,const cygi::Json& metadata);
    bool Begin(const fs::path& staging,double seconds,const cygi::Json& metadata=cygi::Json());
    void BindSession(const std::string& session){session_=session;}
    void End();
    cygi::Json Source() const;
    const std::string& Message()const{return message_;}
    bool Restarted()const{return restarted_;}
    static fs::path FindAdb(const fs::path& explicit_path={});
private:
    std::string Call(const std::vector<std::wstring>& arguments,unsigned timeout=10000,bool cleanup=false);
    std::string Shell(const std::vector<std::wstring>& arguments,unsigned timeout=10000,bool cleanup=false);
    const std::atomic<bool>& cancel_;
    const std::atomic<bool>* stop_=nullptr;
    bool Stopping()const{return cancel_.load()||(stop_&&stop_->load());}
    fs::path adb_,staging_,journal_;
    std::string device_,package_,message_,remote_config_,remote_trace_;
    std::string session_;
    std::vector<uint32_t> game_pids_;
    uint32_t pid_=0,perfetto_pid_=0;bool restarted_=false,ended_=false,prepared_=false;
    uint64_t stages_=0,counters_=0;std::string result_="unavailable";
    double cpu_frequency_=0;
    std::string activation_,profiling_state_,profiling_verification_="not_queried";
    std::string launch_component_;
    bool stage_source_advertised_=false,counter_source_advertised_=false;
    uint64_t stage_packets_=0,timed_stage_packets_=0,unaligned_stages_=0;
};
}
