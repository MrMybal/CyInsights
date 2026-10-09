// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TimingEvent.hpp"
#include "CounterModel.hpp"
#include <Windows.h>
#include <filesystem>
#include <future>
#include <string>
#include <vector>
#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
namespace cyinsights {
class PagedTimingStore;
struct NativeReadProgress {
    std::atomic<uint64_t> bytes{0}, total{0}, events{0};
    std::atomic<bool> cancel{false};
};
class NativeTrace {
  public:
    ~NativeTrace();
    bool Start(const std::filesystem::path &exe, const std::filesystem::path &trace,
               const std::filesystem::path &cache, std::string &error, bool paged = false);
    bool Poll(std::string &message);
    void Cancel();
    bool Importing() const { return csv_task_.valid(); }
    const std::string& Error() const { return error_; }
    const std::filesystem::path& LogPath() const { return log_; }
    const NativeReadProgress& Progress() const { return progress_; }
    uint64_t ExportBytes() const;
    std::shared_ptr<PagedTimingStore> store;
    // Import an existing export without launching an analyzer process.
    bool ImportCsv(const std::filesystem::path &file, std::string &error);
    bool Busy() const {
        return process_ != nullptr || csv_task_.valid();
    }
    std::vector<TimingEvent> events;
    CounterModel counters;
    static bool ReadCsv(const std::filesystem::path &file, std::vector<TimingEvent> &events,
                        std::string &error, NativeReadProgress* progress = nullptr);
    static bool ScanCsv(const std::filesystem::path& file,
                        const std::function<void(TimingEvent&&)>& sink,std::string& error,
                        NativeReadProgress* progress=nullptr);
    static bool Launch(const std::filesystem::path &exe, const std::wstring &arguments, HANDLE *process,
                       std::string &error,const std::filesystem::path& working_directory = {});
    static std::wstring Quote(const std::wstring &value);
    static std::vector<std::string> CsvFields(const std::string &line);

  private:
    struct CsvResult {
        std::vector<TimingEvent> events;
        CounterModel counters;
        std::shared_ptr<PagedTimingStore> store;
        std::string error;
        bool success = false;
    };
    NativeReadProgress progress_;
    std::string error_;
    std::filesystem::path log_;
    std::chrono::steady_clock::time_point started_;
    std::future<CsvResult> csv_task_;
    HANDLE process_ = nullptr;
    std::filesystem::path csv_;
    bool read_counters_ = false;
    bool paged_ = false;
};
} // namespace cyinsights
