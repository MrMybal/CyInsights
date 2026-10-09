// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Trace/Trace.h"

#if ENGINE_MAJOR_VERSION >= 5
using FCyInsightsTicker = FTSTicker;
using FCyInsightsTickerHandle = FTSTicker::FDelegateHandle;
namespace CyInsightsNativeTrace = UE::Trace;
#else
using FCyInsightsTicker = FTicker;
using FCyInsightsTickerHandle = FDelegateHandle;
namespace CyInsightsNativeTrace = Trace;
#endif

// Shared by the coordinator and Quest collector. Synthetic identities on old
// engines are observation epochs, explicitly distinguished from native GUIDs.
namespace CyInsightsTrace
{
    CYINSIGHTSTRACE_API bool IsConnected();
    CYINSIGHTSTRACE_API bool GetIdentity(FGuid& Session, FGuid& Trace);
    CYINSIGHTSTRACE_API bool StartFile(const FString& Path, const FString& Channels);
    CYINSIGHTSTRACE_API void ConfigureChannels(const FString& Channels);
    CYINSIGHTSTRACE_API bool SendTo(const FString& Host, uint16 Port);
    CYINSIGHTSTRACE_API bool Stop();
    CYINSIGHTSTRACE_API FString GetError();
}
