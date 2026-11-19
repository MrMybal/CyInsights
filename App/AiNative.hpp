// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <cymcp/ai.hpp>
#include <cymcp/mcp.hpp>
#include <cymcp/resources.hpp>
#include <cymcp/changes.hpp>
#include <cymcp/performance.hpp>
#include <cymcp/http.hpp>
#include <functional>
namespace cyinsights {
using NativeJson=cymcp::Json;
using NativeHandler=std::function<NativeJson(const std::string&,const NativeJson&,const cymcp::CallContext&)>;
// All adapters use this catalogue and the same business callback. No transport can
// manufacture a trusted UI/manual identity in its JSON arguments.
std::shared_ptr<cymcp::Registry> MakeNativeRegistry(const NativeJson&,NativeHandler);
NativeJson NativeDefaults(const NativeJson& schema,NativeJson arguments);
std::shared_ptr<cymcp::Registry> ConnectNativeRemote(std::shared_ptr<const cymcp::Registry>,const NativeJson&,const cymcp::Cancellation&,int cache_seconds=30,bool notifications=true);
void WritePrivateJson(const std::filesystem::path&,const NativeJson&);
NativeJson ReadPrivateJson(const std::filesystem::path&,size_t limit=1024*1024);
struct NativeAiOptions {
    bool compact=true,disk_spool=true,notifications=true,performance=false;
    int cache_seconds=30,retention_seconds=300,chunk_kib=32,spool_mib=256;
    size_t context_kib=128;
    NativeJson Json() const;
    static NativeAiOptions From(const NativeJson&);
};
class AiJobs;
class NativeAi {
    std::filesystem::path directory_;
    cymcp::Cancellation lifetime_,transport_;
    std::unique_ptr<cymcp::McpServer> mcp_;
    std::unique_ptr<cymcp::LocalHttpServer> http_;
    std::unique_ptr<AiJobs> jobs_;
    std::string token_;
    NativeJson connection_=NativeJson::object();
    std::filesystem::path connection_path_;
    NativeHandler native_;
public:
    NativeAi(const NativeJson& catalog,const NativeJson& method,const std::filesystem::path&,NativeHandler);
    ~NativeAi();
    NativeAi(const NativeAi&)=delete;
    NativeAiOptions options;
    std::shared_ptr<cymcp::Registry> registry;
    std::shared_ptr<cymcp::ResourceRegistry> resources;
    std::shared_ptr<cymcp::ChangeFeed> changes;
    std::shared_ptr<cymcp::PerformanceMonitor> performance;
    std::unique_ptr<cymcp::ToolGateway> gateway;
    std::shared_ptr<const cymcp::Registry> Tools() const;
    void Start(uint64_t document);
    void Stop();
    void Shutdown();
    void Cancel();
    bool Running() const {return bool(http_);}
    void DrainPerformance();
    void UpdateState(const NativeJson&);
    const std::filesystem::path& Directory() const {return directory_;}
    NativeJson Connection() const;
    NativeJson Invoke(const std::string&,const NativeJson&,cymcp::CallContext={});
};
// Owner-only Windows pipe client; never interprets a shell command.
NativeJson CallNativeInstance(uint32_t pid,uint64_t document,const std::string&,const NativeJson&,const cymcp::CallContext& context={});
}
