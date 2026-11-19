// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <CyGPUInspectorCore/Json.hpp>
#include <atomic>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <functional>
#include <cymcp/core.hpp>
namespace cyinsights {
struct AiRequest {
    cygi::Json document;
    std::promise<cygi::Json> response;
    std::atomic<bool> cancelled{false};
    bool external=true;
};
// Windows-user transport and UI-thread mailbox. CyMCPSDK owns MCP framing.
class AiRpc {
public:
    ~AiRpc();
    bool Start(std::string& error,uint64_t document=0);
    void Stop();
    std::vector<std::shared_ptr<AiRequest>> Take();
    std::string Name() const;
    bool Running() const {return running_.load();}
    std::function<cygi::Json(const cygi::Json&,const cymcp::CallContext&)> dispatch;
    cygi::Json Submit(cygi::Json,const cymcp::CallContext&,bool external=true);
private:
    void Serve(std::wstring name,std::wstring security);
    std::atomic<bool> running_{false};
    cymcp::Cancellation transport_;
    std::mutex mutex_;
    std::vector<std::shared_ptr<AiRequest>> queue_;
    std::vector<std::thread> workers_;
    std::string name_;
};
cygi::Json AiError(const std::string& code,const std::string& message);
}
