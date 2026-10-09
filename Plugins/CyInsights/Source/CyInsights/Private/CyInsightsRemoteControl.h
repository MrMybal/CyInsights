// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "CoreMinimal.h"
class FSocket;
class FCyInsightsModule;

// Bounded, nonblocking command endpoint. Only trace start/stop/status are
// exposed; never forward input to the console, filesystem, or command shell.
class FCyInsightsRemoteControl
{
public:
    explicit FCyInsightsRemoteControl(FCyInsightsModule& InOwner) : Owner(InOwner) {}
    ~FCyInsightsRemoteControl();
    void Tick();
private:
    void CloseClient();
    void CloseListener();
    void Dispatch(const FString& Line);
    FCyInsightsModule& Owner;
    FSocket* Listener = nullptr;
    FSocket* Client = nullptr;
    FString PeerIp, OwnedSession;
    TArray<uint8> Input, Output;
    int32 OutputOffset = 0, BoundPort = 0;
    double LastActivity = 0, RetryAt = 0;
};
