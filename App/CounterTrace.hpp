// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "CounterModel.hpp"
#include <atomic>
#include <future>
namespace cyinsights {
class CounterTrace {
public:
    ~CounterTrace();
    bool Start(const std::filesystem::path& exe,const std::filesystem::path& trace,
               const std::filesystem::path& cache,CounterModel catalog,std::string& error);
    bool Poll(CounterModel& result,std::string& message);
    bool Busy() const {return task_.valid();}
    void Cancel(){cancel_.store(true);}
private:
    std::future<CounterModel> task_;
    std::atomic<bool> cancel_{false};
};
}
