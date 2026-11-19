// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "AiNative.hpp"
namespace cyinsights {
// Durable host jobs are separate from the SDK's expiring resource spool. A restart
// marks unfinished work interrupted; an action is never replayed automatically.
class AiJobs {
    struct Job { NativeJson metadata; cymcp::CallContext context; bool finished=false; };
    std::filesystem::path root_;
    std::mutex mutex_;
    std::map<std::string,std::shared_ptr<Job>> live_;
    NativeHandler invoke_;
    std::shared_ptr<cymcp::ResourceRegistry> resources_;
    cymcp::Cancellation lifetime_;
    cymcp::Executor workers_{2,32};
    NativeJson Load(const std::string&,const cymcp::CallContext&);
public:
    AiJobs(std::filesystem::path,NativeHandler,std::shared_ptr<cymcp::ResourceRegistry>);
    ~AiJobs();
    void Register(cymcp::Registry&);
    void CancelAll();
};
}
