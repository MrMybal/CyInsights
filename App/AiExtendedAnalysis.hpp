// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "AiAnalysis.hpp"
#include "ActivityStore.hpp"
#include "CounterModel.hpp"
#include "Session/SessionModel.hpp"
namespace cyinsights {
struct AiDataSnapshot {
    std::shared_ptr<const CounterModel> counters;
    std::shared_ptr<ActivityStore> activities;
    std::shared_ptr<PagedTimingStore> timing, comparison;
    std::shared_ptr<TaskStore> tasks;
    std::shared_ptr<cygi::SessionModel> gpu;
};
bool AiExtendedOperation(const std::string& operation);
cygi::Json AnalyzeAiExtended(const AiDataSnapshot&,const std::string&,const cygi::Json&,
    const std::atomic<bool>* cancel=nullptr);
}
