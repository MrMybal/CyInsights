// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <string>
#include <vector>
namespace cyinsights {
enum class Provenance { Measured, Derived, Unavailable };
struct Capability {
    std::string id; Provenance provenance=Provenance::Unavailable;
    std::string reason; // e.g. unsupported API, missing permission, capture loss
};
struct CollectorSession {
    std::string session_id, collector_id, collector_version, platform, graphics_api;
    std::vector<Capability> capabilities;
};
struct FrameIdentity {
    std::string session_id, device_id, queue_id;
    uint64_t execution_frame=0;
    uint64_t received_frame=0; // transport latency; never used as execution identity
};
// Adapter contract. The current Windows path drives ReShade IPC directly;
// wrapping it in this lifecycle remains to be done. Meta reports, Vulkan and
// licensed console SDKs can implement the same lifecycle.
class ICollector {
public:
    virtual ~ICollector()=default;
    virtual CollectorSession Describe() const=0;
    virtual bool Begin(const std::string& common_session_id,std::string& error)=0;
    virtual bool RequestFrames(uint32_t count,std::string& error)=0;
    virtual void StopRequests()=0;
    virtual bool DrainComplete() const=0; // producer fence/results, not a fixed frame delay
    virtual bool WritePayloads(const std::string& staging_directory,std::string& error)=0;
};
}
