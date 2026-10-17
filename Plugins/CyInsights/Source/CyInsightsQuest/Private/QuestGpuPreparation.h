// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Android/AndroidPlatformMisc.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"

// Timestamp support is latched by Vulkan at RHI initialization. Setting it when
// the PC requests a capture is too late. This only prepares detected headsets;
// the native GPU trace channel still controls recording during the capture.
inline FString CyInsightsPrepareQuestGpuTiming() {
    const FString Model=FAndroidMisc::GetDeviceModel();
    const FString Make=FAndroidMisc::GetDeviceMake();
    if(!Model.Contains(TEXT("Quest"),ESearchCase::IgnoreCase)&&!Make.Equals(TEXT("Oculus"),ESearchCase::IgnoreCase))return TEXT("Device not recognized as Quest; automatic timestamp preparation not applied");
    bool Enabled=true;
    if(GConfig)GConfig->GetBool(TEXT("/Script/CyInsights.CyInsightsSettings"),TEXT("bPrepareQuestGpuTiming"),Enabled,GGameIni);
    if(!Enabled)return TEXT("Quest timestamp preparation disabled in project settings; restart after enabling it");
    auto& Console=IConsoleManager::Get();
    auto* Timestamp=Console.FindConsoleVariable(TEXT("r.Android.SupportsTimestampQueries"));
    if(!Timestamp) {
        // Older engines register this CVar inside the Vulkan module. Loading
        // that installed module does not initialize or rebuild the engine/RHI.
        FModuleManager::LoadModulePtr<IModuleInterface>(TEXT("VulkanRHI"));
        Timestamp=Console.FindConsoleVariable(TEXT("r.Vulkan.SupportsTimestampQueries"));
    }
    if(!Timestamp)return TEXT("Android/Vulkan timestamp option unavailable in this engine");
    if(Timestamp&&!(Timestamp->GetFlags()&ECVF_ReadOnly)&&Timestamp->GetInt()==0) {
        const uint32 Priority=Timestamp->GetFlags()&ECVF_SetByMask;
        // Preserve an explicit command-line/console/code override.
        if(Priority<ECVF_SetByCode&&Priority!=ECVF_SetByCommandline)Timestamp->Set(1,ECVF_SetByCode);
    }
    return Timestamp->GetInt()!=0?TEXT("Quest timestamp support requested before RHI initialization; hardware support checked by RHI"):
        TEXT("Quest timestamps disabled by a read-only or explicit code/command-line/console override; restart with timestamps enabled");
}
