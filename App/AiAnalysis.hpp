// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "PagedTimingStore.hpp"
#include "TaskStore.hpp"
#include <CyGPUInspectorCore/Json.hpp>
namespace cyinsights {
// Immutable native snapshots. All potentially large queries run off the UI.
cygi::Json AnalyzeAi(const std::shared_ptr<PagedTimingStore>& store,const std::shared_ptr<TaskStore>& tasks,
    const std::string& operation,const cygi::Json& parameters,const std::atomic<bool>* cancel=nullptr);
bool AiAnalysisOperation(const std::string& operation);
}
