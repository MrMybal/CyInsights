// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "Capture.hpp"
#include "CaptureOptions.hpp"
#include "MetaQuestCapture.hpp"
#include <optional>
#include <atomic>
#include <chrono>
#include <mutex>
#include <thread>

namespace cyinsights {
enum class RemoteState { idle, probing, ready, starting, recording, stopping, packaging, complete, failed };
struct RemoteOptions {
    std::string ipv4;
    uint16_t control_port = 41855;
    double seconds = 0; // Zero means manual stop.
    fs::path output_directory;
    bool override_capture_options=false;
    CaptureOptions capture;
    MetaQuestOptions meta_gpu;
};
struct RemoteSnapshot {
    RemoteState state = RemoteState::idle;
    std::string message, session_id;
    uint64_t bytes = 0;
    double elapsed = 0;
    double startup_elapsed = 0;
    std::string phase;
    fs::path diagnostic_log;
    bool finishing=false;
    cygi::Json manifest;
    fs::path archive, partial_trace;
    cygi::Json capture_defaults,capture_capabilities;
    std::string gpu_message;
    std::string meta_message;
    bool Busy() const {
        return finishing || state == RemoteState::probing || state == RemoteState::starting || state == RemoteState::recording ||
               state == RemoteState::stopping || state == RemoteState::packaging;
    }
};
// All network/file work runs off the UI thread. Only Snapshot/Stop touch shared state.
class RemoteCapture {
public:
    RemoteCapture() = default;
    ~RemoteCapture();
    RemoteCapture(const RemoteCapture&) = delete;
    RemoteCapture& operator=(const RemoteCapture&) = delete;
    bool Probe(const RemoteOptions&);
    bool Start(const RemoteOptions&);
    bool RequestGpu(const CaptureOptions&);
    void Stop() { stop_.store(true); }
    RemoteSnapshot Snapshot() const;
private:
    bool Launch(const RemoteOptions&, bool probe);
    void Run(RemoteOptions, bool probe);
    void Publish(const RemoteSnapshot&);
    mutable std::mutex mutex_;
    RemoteSnapshot snapshot_;
    std::optional<CaptureOptions> gpu_request_;
    std::thread worker_;
    std::atomic<bool> stop_{false}, cancel_{false};
    std::chrono::steady_clock::time_point launched_at_{};
};
// Validates the native stream preamble without changing a single received byte.
bool NativeTraceHeader(const std::string& prefix, bool eof, std::string& error);
const char* RemoteStateName(RemoteState);
}
