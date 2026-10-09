// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <atomic>
#include <cstddef>
#include <functional>
namespace cyinsights {
// Shared analysis workers, never ImGui/D3D workers. The caller participates.
unsigned AnalysisWorkerLimit();
unsigned AnalysisWorkerCount();
unsigned AnalysisWorkerSetting();
void SetAnalysisWorkerCount(unsigned count); // 0 = automatic; 1 = reference mode.
void ParallelAnalysis(size_t jobs,unsigned workers,const std::function<void(size_t)>& work,
                      const std::atomic<bool>* cancel=nullptr);
}
