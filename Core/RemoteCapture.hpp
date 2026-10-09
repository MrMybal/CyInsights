// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "Capture.hpp"
#include <atomic>
#include <mutex>
#include <thread>

namespace cyinsights {
enum class RemoteState { idle, probing, ready, starting, recording, stopping, packaging, complete, failed };
struct RemoteOptions {
    std::string ipv4;
    uint16_t control_port = 41855;
    double seconds = 0; // Zero means manual stop.
    fs::path output_directory;
};
struct RemoteSnapshot {
    RemoteState state = RemoteState::idle;
    std::string message, session_id;
    uint64_t bytes = 0;
    double elapsed = 0;
    cygi::Json manifest;
    fs::path archive, partial_trace;
    bool Busy() const {
        return state == RemoteState::probing || state == RemoteState::starting || state == RemoteState::recording ||
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
    void Stop() { stop_.store(true); }
    RemoteSnapshot Snapshot() const;
private:
    bool Launch(const RemoteOptions&, bool probe);
    void Run(RemoteOptions, bool probe);
    void Publish(const RemoteSnapshot&);
    mutable std::mutex mutex_;
    RemoteSnapshot snapshot_;
    std::thread worker_;
    std::atomic<bool> stop_{false}, cancel_{false};
};
// Validates the native stream preamble without changing a single received byte.
bool NativeTraceHeader(const std::string& prefix, bool eof, std::string& error);
const char* RemoteStateName(RemoteState);
}
