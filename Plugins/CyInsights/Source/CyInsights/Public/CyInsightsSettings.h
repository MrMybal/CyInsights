// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CyInsightsCapturePolicy.h"
#include "CyInsightsSettings.generated.h"

// Project defaults apply at the next capture. A PC override is session-local.
UCLASS(config=Game, defaultconfig, meta=(DisplayName="CyInsights"))
class CYINSIGHTS_API UCyInsightsSettings : public UDeveloperSettings {
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override {return TEXT("Plugins");}
    static cycapture::Options CaptureDefaults();
    UPROPERTY(config,EditAnywhere,Category="Basic") bool bCpuTiming=true;
    UPROPERTY(config,EditAnywhere,Category="Basic",meta=(DisplayName="GPU groups each frame",ToolTip="Capture instrumented GPU groups with native timestamps each render frame. Includes nested RDG groups; individual passes and resource copies are separate options.")) bool bGpuTiming=true;
    UPROPERTY(config,EditAnywhere,Category="Basic") bool bFrameStats=true;
    UPROPERTY(config,EditAnywhere,Category="Basic") bool bObjectCounts=true;
    UPROPERTY(config,EditAnywhere,Category="Statistics",meta=(ToolTip="Read process physical/virtual memory each frame. Optional OS-query overhead.")) bool bMemoryStats=false;
    UPROPERTY(config,EditAnywhere,Category="Statistics",meta=(ToolTip="Enable stats aggregation for the selected compiled/active Unreal groups. More capture overhead.")) bool bRecordStatGroups=false;
    UPROPERTY(config,EditAnywhere,Category="Statistics",meta=(EditCondition="bRecordStatGroups",ToolTip="Comma-separated active groups, or * for all. Does not enable disabled groups.")) FString StatGroups=TEXT("Game,UObjects,GPU");
    UPROPERTY(config,EditAnywhere,Category="Extra native sources") bool bTasks=false;
    UPROPERTY(config,EditAnywhere,Category="Extra native sources") bool bNetwork=false;
    UPROPERTY(config,EditAnywhere,Category="Extra native sources") bool bFileIo=false;
    UPROPERTY(config,EditAnywhere,Category="Extra native sources") bool bAssetLoading=false;
    UPROPERTY(config,EditAnywhere,Category="Extra native sources") bool bLogs=false;
    UPROPERTY(config,EditAnywhere,Category="Extra native sources",meta=(ToolTip="Requires active LLM. Full allocation tracing still needs startup flags.")) bool bMemoryTags=false;
    UPROPERTY(config,EditAnywhere,Category="Quest",meta=(ToolTip="Prepare hardware timestamp queries before RHI initialization on detected Quest devices. Restart the game after changing this setting.")) bool bPrepareQuestGpuTiming=true;
    UPROPERTY(config,EditAnywhere,Category="Basic",meta=(DisplayName="Individual GPU passes",EditCondition="bGpuTiming",ToolTip="Add individual RDG pass names beyond the default GPU groups. Coverage depends on engine instrumentation; no texture/buffer copies.")) bool bGpuPasses=false;
    UPROPERTY(config,EditAnywhere,Category="Quest") bool bQuestMetrics=true;
    UPROPERTY(config,EditAnywhere,Category="Quest",meta=(DisplayName="GPU completion / Vulkan passes",ToolTip="During Quest Vulkan captures, request completed native timestamps and add two queries outside each Vulkan render pass. May affect scheduling. No render-pass splits or resource readbacks. Tile subpass costs can remain unresolved. Explicit native console/launch stage overrides are preserved.")) bool bQuestGpuCompletion=true;
    UPROPERTY(config,EditAnywhere,Category="Quest",meta=(EditCondition="bQuestMetrics",ClampMin=0,ClampMax=120,ToolTip="0 observes Meta values every game frame. 1..120 optionally throttles observations. Runtime values do not expose their original GPU execution frame.")) int32 QuestMetricsHz=0;
    UPROPERTY(config,EditAnywhere,Category="GPU snapshot",meta=(ToolTip="Request a bounded Windows collector capture on start. Requires a loaded collector and connected recorder. Native GPU scopes do not need this.")) bool bDetailedGpuOnStart=false;
    UPROPERTY(config,EditAnywhere,Category="GPU snapshot",meta=(ClampMin=1,ClampMax=8)) int32 GpuFrames=1;
    UPROPERTY(config,EditAnywhere,Category="GPU snapshot") bool bGpuDescriptors=false;
    UPROPERTY(config,EditAnywhere,Category="GPU snapshot") bool bGpuBarriers=false;
    UPROPERTY(config,EditAnywhere,Category="GPU snapshot") bool bGpuCommandTimestamps=false;
    UPROPERTY(config,EditAnywhere,Category="GPU snapshot",meta=(ToolTip="Explicit opt-in to image/texture readbacks. High GPU/memory cost. Off by default.")) bool bGpuReadbacks=false;
};
