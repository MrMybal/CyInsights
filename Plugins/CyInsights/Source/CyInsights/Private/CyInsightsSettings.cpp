// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsightsSettings.h"
#include "HAL/IConsoleManager.h"
cycapture::Options UCyInsightsSettings::CaptureDefaults() {
    const auto* S=GetDefault<UCyInsightsSettings>();cycapture::Options O;
    O.cpu=S->bCpuTiming;O.gpu=S->bGpuTiming;O.gpu_passes=S->bGpuPasses;O.frame_stats=S->bFrameStats;O.object_counts=S->bObjectCounts;O.memory_stats=S->bMemoryStats;
    O.stat_groups_enabled=S->bRecordStatGroups;O.stat_groups=TCHAR_TO_UTF8(*S->StatGroups);
    O.tasks=S->bTasks;O.network=S->bNetwork;O.file_io=S->bFileIo;O.loadtime=S->bAssetLoading;O.logs=S->bLogs;O.memtags=S->bMemoryTags;
    O.quest_metrics=S->bQuestMetrics;O.quest_hz=S->QuestMetricsHz;O.quest_gpu_completion=S->bQuestGpuCompletion;
    O.gpu_details=S->bDetailedGpuOnStart;O.gpu_frames=S->GpuFrames;O.gpu_bindings=S->bGpuDescriptors;O.gpu_barriers=S->bGpuBarriers;O.gpu_per_command=S->bGpuCommandTimestamps;O.gpu_readbacks=S->bGpuReadbacks;
    // Existing console/launch overrides remain explicit choices. Constructor
    // defaults must not override the project profile or a PC capture override.
    auto Explicit=[](const TCHAR* Name){auto* C=IConsoleManager::Get().FindConsoleVariable(Name);return C&&(C->GetFlags()&ECVF_SetByMask)!=ECVF_SetByConstructor?C:nullptr;};
    if(auto* C=Explicit(TEXT("CyInsights.RecordStats")))O.frame_stats=C->GetInt()!=0;
    if(auto* C=Explicit(TEXT("CyInsights.RecordStatGroups")))O.stat_groups_enabled=C->GetInt()!=0;
    if(auto* C=Explicit(TEXT("CyInsights.StatsGroups")))O.stat_groups=TCHAR_TO_UTF8(*C->GetString());
    if(auto* C=Explicit(TEXT("CyInsights.QuestGpuMetrics")))O.quest_metrics=C->GetInt()!=0;
    if(auto* C=Explicit(TEXT("CyInsights.QuestGpuMetricsHz"))){const float Rate=C->GetFloat();O.quest_hz=FMath::IsFinite(Rate)?FMath::RoundToInt(FMath::Clamp(Rate,0.f,120.f)):0;}
    if(!O.frame_stats)O.stat_groups_enabled=false;
    return O;
}
