// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "TaskStore.hpp"
#include <future>
namespace cyinsights {
class TaskTrace {
public:
    ~TaskTrace();
    bool Start(const std::filesystem::path& helper,const std::filesystem::path& engine_binaries,
        const std::filesystem::path& trace,const std::filesystem::path& output,double capture_end,std::string& error);
    bool Poll(std::shared_ptr<TaskStore>& store,std::string& message);
    bool Busy() const{return task_.valid();}
    void Cancel(){cancel_.store(true);}
    int Phase() const{return phase_.load();}
    std::filesystem::path Journal() const{return journal_;}
private:
    struct Result {std::shared_ptr<TaskStore> store;std::string error;};
    std::future<Result> task_;std::atomic<bool> cancel_{false};std::atomic<int> phase_{0};std::filesystem::path journal_;
};
}
