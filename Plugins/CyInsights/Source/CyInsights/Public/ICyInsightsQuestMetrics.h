// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Features/IModularFeature.h"

struct FCyInsightsQuestMetricsStatus
{
    FString Reason;
    FString GpuPreparation;
    FString TraceGuid;
    uint64 ValidSamples = 0;
    uint64 ObservedFrames = 0;
    int32 CounterCount = 0;
};

// Header-only modular feature: the early OpenXR module does not load the
// capture coordinator or depend on its Windows collector.
class ICyInsightsQuestMetrics : public IModularFeature
{
public:
    static FName FeatureName() { return TEXT("CyInsightsQuestMetrics"); }
    virtual FCyInsightsQuestMetricsStatus GetMetricsStatus() const = 0;
    virtual void StopMetrics() = 0;
    virtual void ConfigureCapture(bool Enabled,int32 Hertz) {}
    virtual void ClearCaptureConfiguration() {}
};
