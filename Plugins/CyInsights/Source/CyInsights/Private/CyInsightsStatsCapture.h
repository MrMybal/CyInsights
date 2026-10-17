// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "CyInsightsCapturePolicy.h"

// Owned by the capture coordinator. Stats history is consumed on Unreal's stats
// execution context; UObject and platform observations stay on the game thread.
class FCyInsightsStatsCapture {
public:
    FCyInsightsStatsCapture();
    ~FCyInsightsStatsCapture();
    void Start(const cycapture::Options& Options);
    void Stop();
    uint64 SampleCount() const;
private:
    struct FFrame {int64 Stats=-1;uint64 Game=0,Begin=0,End=0;};
    struct FMetric {uint32 Id=0;uint16 Counter=0;bool Floating=false,Declared=false;};
    void BeginFrame();
    void EndFrame();
    void StatsFrame(int64 Frame);
    void Emit(const FString& Group,const FString& Name,const TCHAR* Unit,
              bool Floating,int64 Integer,double Real,const FFrame& Frame,uint32 Flags);
    mutable FCriticalSection Mutex;
    bool Recording=false;
    bool RecordGroups=false,RecordObjects=true,RecordMemory=false;
    uint64 Samples=0,BeginCycles=0;
    int64 FirstStatsFrame=0;
    bool AllGroups=true;
    TSet<FName> SelectedGroups;
    TArray<FFrame> Frames;
    TMap<FString,FMetric> Metrics;
    FDelegateHandle BeginHandle,EndHandle,StatsHandle;
};
