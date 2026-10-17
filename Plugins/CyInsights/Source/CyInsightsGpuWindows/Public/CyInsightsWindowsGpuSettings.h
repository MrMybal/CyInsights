// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "CyInsightsWindowsGpuSettings.generated.h"

// Local paths, stored in the user's Engine.ini; the loader reads them before UObject startup.
UCLASS(config=Engine, meta=(DisplayName="CyInsights - Windows GPU collector (optional)"))
class CYINSIGHTSGPUWINDOWS_API UCyInsightsWindowsGpuSettings : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
    virtual FName GetSectionName() const override { return TEXT("CyInsightsWindowsGpu"); }
    UPROPERTY(config,EditAnywhere,Category="Optional collector",meta=(ConfigRestartRequired=true,
        ToolTip="Load the extra Windows GPU collector before RHI initialization. Disabled by default; native utrace GPU groups work without it."))
    bool bLoadInEditor=false;
    UPROPERTY(config,EditAnywhere,Category="Optional collector",meta=(ConfigRestartRequired=true))
    bool bLoadInGame=false;
    UPROPERTY(config,EditAnywhere,Category="Optional collector",meta=(ConfigRestartRequired=true,FilePathFilter="dll",
        ToolTip="Explicit path to an add-on-capable ReShade runtime. Empty uses this plugin's Binaries/ThirdParty/CyInsightsGpu/Win64 folder."))
    FFilePath ReShadePath;
    UPROPERTY(config,EditAnywhere,Category="Optional collector",meta=(ConfigRestartRequired=true,FilePathFilter="addon64",
        ToolTip="CyInsightsGpu.addon64, built from this repository. No CyGPUInspector application or plugin is used."))
    FFilePath AddonPath;
};
