// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsightsRemoteControl.h"
#include "CyInsights.h"
#include "CyInsightsSettings.h"
#include "CyInsightsCaptureJson.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "CoreGlobals.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Policies/CondensedJsonPrintPolicy.h"

static TAutoConsoleVariable<int32> RemoteEnabled(TEXT("CyInsights.RemoteCapture"), 1,
    TEXT("Accept capture-only LAN commands in supported development builds. 0 disables the endpoint."));
static TAutoConsoleVariable<int32> RemotePort(TEXT("CyInsights.RemotePort"), 41855,
    TEXT("TCP port for CyInsights capture control (default 41855)."));
DEFINE_LOG_CATEGORY_STATIC(LogCyInsightsRemote, Log, All);

FCyInsightsRemoteControl::~FCyInsightsRemoteControl() { CloseClient(); CloseListener(); }
void FCyInsightsRemoteControl::CloseListener()
{
    if (Listener) { Listener->Close(); ISocketSubsystem::Get()->DestroySocket(Listener); Listener = nullptr; }
}
void FCyInsightsRemoteControl::CloseClient()
{
    if (!OwnedSession.IsEmpty() && OwnedSession == Owner.SessionId && !Owner.NetworkHost.IsEmpty())
        Owner.StopCapture();
    OwnedSession.Reset();
    if (Client) { Client->Close(); ISocketSubsystem::Get()->DestroySocket(Client); Client = nullptr; }
    Input.Reset(); Output.Reset(); OutputOffset = 0;
}
void FCyInsightsRemoteControl::Tick()
{
#if CYINSIGHTS_ENABLED
    int32 Port = RemotePort.GetValueOnGameThread();
    FParse::Value(FCommandLine::Get(), TEXT("CyInsightsRemotePort="), Port);
    const double Now = FPlatformTime::Seconds();
    if (!RemoteEnabled.GetValueOnGameThread() || Port < 1 || Port > 65535) {
        CloseClient(); CloseListener(); return;
    }
    if (Listener && BoundPort != Port) { CloseClient(); CloseListener(); }
    auto* Sockets = ISocketSubsystem::Get();
    if (!Sockets) return;
    if (!Listener && Now >= RetryAt) {
        RetryAt = Now + 5;
        auto Address = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
        Address->SetAnyAddress(); Address->SetPort(Port);
        Listener = Sockets->CreateSocket(NAME_Stream, TEXT("CyInsights capture control"), FNetworkProtocolTypes::IPv4);
        bool Reusable=true;
#if PLATFORM_ANDROID
        // SO_REUSEADDR permits rebinding after our old TCP connection enters
        // TIME_WAIT. Never enable SO_REUSEPORT or share an active listener.
        Reusable=Listener&&Listener->SetReuseAddr(true);
#endif
        if (!Listener || !Reusable || !Listener->SetNonBlocking() || !Listener->Bind(*Address) || !Listener->Listen(1)) {
            const int32 SocketError=static_cast<int32>(Sockets->GetLastErrorCode());
            CloseListener();
            UE_LOG(LogCyInsightsRemote, Warning, TEXT("Cannot listen for capture commands on port %d (socket error %d); retrying in 5 seconds"), Port,SocketError);
        } else {
            BoundPort = Port;
            UE_LOG(LogCyInsightsRemote, Display, TEXT("Remote capture ready on TCP %d (development only)"), Port);
        }
    }
    bool Pending = false;
    if (Listener && !Client && Listener->HasPendingConnection(Pending) && Pending) {
        auto Peer = Sockets->CreateInternetAddr(FNetworkProtocolTypes::IPv4);
        Client = Listener->Accept(*Peer, TEXT("CyInsights PC"));
        if (Client) {
            Client->SetNonBlocking(); Client->SetNoDelay();
            PeerIp = Peer->ToString(false);
            LastActivity = Now;
        }
    }
    if (!Client) return;
    if (Now - LastActivity > 30) { CloseClient(); return; }
    // One response at a time. Partial writes persist into subsequent ticks.
    if (OutputOffset < Output.Num()) {
        int32 Sent = 0;
        if (Client->Send(Output.GetData() + OutputOffset, Output.Num() - OutputOffset, Sent) && Sent > 0) {
            OutputOffset += Sent; LastActivity = Now;
        } else if (Sockets->GetLastErrorCode() != SE_EWOULDBLOCK) { CloseClient(); return; }
        if (OutputOffset < Output.Num()) return;
        Output.Reset(); OutputOffset = 0;
    }
    uint8 Bytes[1024]; int32 Read = 0;
    if (Client->Recv(Bytes, sizeof(Bytes), Read)) {
        // Unreal streaming Recv reports would-block as success with zero bytes;
        // orderly EOF reports false. This differs from raw BSD/Winsock recv.
        if (Read > 0) { Input.Append(Bytes, Read); LastActivity = Now; }
    } else { CloseClient(); return; }
    if (Input.Num() > 4096 || Now - LastActivity > 30) { CloseClient(); return; }
    int32 End = INDEX_NONE;
    if (Input.Find(uint8('\n'), End)) {
        const FUTF8ToTCHAR Converted(reinterpret_cast<const ANSICHAR*>(Input.GetData()), End);
        const FString Line(Converted.Length(), Converted.Get());
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 5)
        Input.RemoveAt(0, End + 1, EAllowShrinking::No);
#else
        Input.RemoveAt(0, End + 1, false);
#endif
        Dispatch(Line);
    }
#endif
}
void FCyInsightsRemoteControl::Dispatch(const FString& Line)
{
    auto Reply = MakeShared<FJsonObject>();
    Reply->SetNumberField(TEXT("version"), 1);
    bool Ok = false;
    FString Error;
    TSharedPtr<FJsonObject> Request;
    double Version = 0;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Line), Request) || !Request ||
        !Request->TryGetNumberField(TEXT("version"), Version) || Version != 1) {
        Error = TEXT("Invalid capture protocol/version");
    } else {
        FString Command;
        Request->TryGetStringField(TEXT("command"), Command);
        if (Command == TEXT("hello") || Command == TEXT("status")) {
            Ok = true;
        } else if (Command == TEXT("start")) {
            auto Options=UCyInsightsSettings::CaptureDefaults();
            const bool HasOptions=Request->HasField(TEXT("capture_options"));
            bool ValidOptions=true;
            if(HasOptions){
                const auto Field=Request->TryGetField(TEXT("capture_options"));
                if(!Field.IsValid()||Field->Type!=EJson::Object){ValidOptions=false;Error=TEXT("Capture options must be an object");}
                else ValidOptions=CyInsightsOptions::Read(Field->AsObject(),Options,Error);
            }
            double ReceiverPort = 0, Seconds = 0;
            Request->TryGetNumberField(TEXT("receiver_port"), ReceiverPort);
            Request->TryGetNumberField(TEXT("seconds"), Seconds);
            if(!ValidOptions){}
            else if (!FMath::IsFinite(ReceiverPort) || ReceiverPort < 1 || ReceiverPort > 65535 ||
                ReceiverPort != FMath::FloorToDouble(ReceiverPort) || !FMath::IsFinite(Seconds) || Seconds < 0 || Seconds > 3600) {
                Error = TEXT("Invalid receiver port or duration");
            } else if (Owner.StartCapture(PeerIp, static_cast<uint16>(ReceiverPort),HasOptions?&Options:nullptr)) {
                OwnedSession = Owner.SessionId;
                Owner.StopAtSeconds = Seconds > 0 ? FPlatformTime::Seconds() + Seconds : 0;
                Ok = true;
            } else Error = TEXT("Capture unavailable or another native trace is active");
        } else if(Command==TEXT("gpu")) {
            FString Session;Request->TryGetStringField(TEXT("session_id"),Session);
            if(Session.IsEmpty()||Session!=OwnedSession||Session!=Owner.SessionId||!Owner.bCapturing)Error=TEXT("This connection does not own an active capture");
            else {
                auto Options=Owner.CaptureOptions;
                bool ValidOptions=true;
                if(Request->HasField(TEXT("capture_options"))){const auto Field=Request->TryGetField(TEXT("capture_options"));
                    if(!Field.IsValid()||Field->Type!=EJson::Object){ValidOptions=false;Error=TEXT("Capture options must be an object");}
                    else ValidOptions=CyInsightsOptions::Read(Field->AsObject(),Options,Error);
                }
                if(ValidOptions){Ok=Owner.CaptureGpu(Options);if(!Ok)Error=Owner.GpuCaptureMessage;}
            }
        } else if (Command == TEXT("stop")) {
            FString Session;
            Request->TryGetStringField(TEXT("session_id"), Session);
            if (Session.IsEmpty() || Session != OwnedSession || Session != Owner.SessionId) {
                Error = TEXT("This connection does not own the requested capture");
            } else { Owner.StopCapture(); Ok = true; }
        } else Error = TEXT("Unknown capture command");
    }
    Reply->SetBoolField(TEXT("ok"), Ok);
    if (!Error.IsEmpty()) Reply->SetStringField(TEXT("error"), Error);
    Reply->SetStringField(TEXT("session_id"), Owner.SessionId);
    Reply->SetStringField(TEXT("state"), Owner.bNetworkPending ? TEXT("starting") : Owner.bCapturing ? TEXT("recording") : TEXT("idle"));
    Reply->SetBoolField(TEXT("native_trace_active"), CyInsightsTrace::IsConnected());
    Reply->SetBoolField(TEXT("runtime_ready"),GFrameCounter>0);
    Reply->SetStringField(TEXT("game_frame"),LexToString(GFrameCounter));
    Reply->SetObjectField(TEXT("capture_defaults"),CyInsightsOptions::Write(UCyInsightsSettings::CaptureDefaults()));
    auto Capabilities=MakeShared<FJsonObject>();Capabilities->SetNumberField(TEXT("capture_options_version"),1);
    Capabilities->SetBoolField(TEXT("gpu_request"),Owner.CanCaptureGpu());Capabilities->SetBoolField(TEXT("gpu_payload_transfer"),false);
    Capabilities->SetBoolField(TEXT("native_gpu_passes"),true);
    Capabilities->SetBoolField(TEXT("quest_per_frame_observations"),true);
    Capabilities->SetBoolField(TEXT("quest_gpu_completion"),true);
    Capabilities->SetBoolField(TEXT("quest_vulkan_passes"),CYINSIGHTS_VULKAN_PASSES!=0);
    Capabilities->SetBoolField(TEXT("remote_startup_status"),true);
    Reply->SetObjectField(TEXT("capture_capabilities"),Capabilities);
    if(!Owner.GpuCaptureMessage.IsEmpty())Reply->SetStringField(TEXT("gpu_capture_message"),Owner.GpuCaptureMessage);
    if (!Owner.NetworkError.IsEmpty()) Reply->SetStringField(TEXT("capture_error"), Owner.NetworkError);
    // Start/status snapshots; the finalized manifest is retained after native EOF.
    TSharedPtr<FJsonObject> Manifest;
    FString Saved;
    if (!Owner.bCapturing && !Owner.bNetworkPending) Manifest = Owner.FinalManifest;
    if (!Manifest && !Owner.bCapturing && !Owner.bNetworkPending && !Owner.Directory.IsEmpty() &&
        FFileHelper::LoadFileToString(Saved, *(Owner.Directory / TEXT("manifest.json"))))
        FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Saved), Manifest);
    if (!Manifest) Manifest = Owner.CaptureManifest();
    Reply->SetObjectField(TEXT("manifest"), Manifest);
    FString Text;
    FJsonSerializer::Serialize(Reply, TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
    Text += TEXT("\n");
    const FTCHARToUTF8 Utf8(*Text);
    if (Utf8.Length() > 65536) { CloseClient(); return; }
    Output.Append(reinterpret_cast<const uint8*>(Utf8.Get()), Utf8.Length());
}
