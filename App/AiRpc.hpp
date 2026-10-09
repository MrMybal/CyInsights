// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <CyGPUInspectorCore/Json.hpp>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
namespace cyinsights {
struct AiRequest {
    cygi::Json document;
    std::promise<cygi::Json> response;
    std::atomic<bool> cancelled{false};
};
// Local transport only. CyToolsCore owns external jobs, identities and scheduling.
// Native requests are marshalled to the UI; workers never touch ImGui state.
class AiRpc {
public:
    ~AiRpc();
    bool Start(std::string& error);
    void Stop();
    std::vector<std::shared_ptr<AiRequest>> Take();
    std::string Name() const;
    bool Running() const {return running_.load();}
private:
    void Serve(std::wstring name,std::wstring security);
    std::atomic<bool> running_{false};
    std::mutex mutex_;
    std::vector<std::shared_ptr<AiRequest>> queue_;
    std::vector<std::thread> workers_;
    std::string name_;
};
cygi::Json AiError(const std::string& code,const std::string& message);
}
