// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#include "CyInsightsTrace.h"
#include "Modules/ModuleManager.h"
#include "ProfilingDebugging/TraceAuxiliary.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

#if !CYINSIGHTS_TRACE_HAS_GUID
namespace {
FCriticalSection IdentityMutex;
FGuid IdentityEpoch;
bool IdentityWasConnected = false;
void ResetObservedIdentity()
{
    FScopeLock Lock(&IdentityMutex);
    IdentityEpoch = FGuid::NewGuid();
    IdentityWasConnected = true;
}
}
#endif

#if !CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
#include "LegacyTracePackets.h"
#include "Async/Async.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/Archive.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include <atomic>

namespace {
class FLegacyTraceSink
{
public:
    ~FLegacyTraceSink() { StopRequested = true; if (Worker.IsValid()) Worker.Wait(); }
    bool Start(const FString& File, const FString& Host, uint16 Port)
    {
        FScopeLock Lock(&Lifecycle);
        if (Running || Trace::IsTracing()) return false;
        if (Worker.IsValid()) Worker.Wait();
        { FScopeLock ErrorLock(&ErrorMutex); Error.Reset(); }
        StopRequested = false;
        auto* Sockets = ISocketSubsystem::Get();
        if (!Sockets) return Fail(TEXT("Socket subsystem unavailable"));
        auto Local = Sockets->CreateInternetAddr();
        bool Valid = false; Local->SetIp(TEXT("127.0.0.1"), Valid); Local->SetPort(0);
        Listener = Sockets->CreateSocket(NAME_Stream, TEXT("CyInsights legacy trace receiver"), false);
        if (!Listener || !Listener->SetNonBlocking() || !Listener->Bind(*Local) || !Listener->Listen(1)) {
            Cleanup(); return Fail(TEXT("Cannot bind legacy loopback trace receiver"));
        }
        Listener->GetAddress(*Local);
        const uint16 LocalPort = static_cast<uint16>(Local->GetPort());
        if (!File.IsEmpty()) {
            Writer.Reset(IFileManager::Get().CreateFileWriter(*File));
            if (!Writer) { Cleanup(); return Fail(TEXT("Cannot create native trace file")); }
        } else {
            Output = Sockets->CreateSocket(NAME_Stream, TEXT("CyInsights PC trace stream"), false);
            auto Remote = Sockets->CreateInternetAddr();
            Remote->SetIp(*Host, Valid); Remote->SetPort(Port);
            if (!Valid || !Output || !Output->SetNonBlocking()) { Cleanup(); return Fail(TEXT("Invalid PC trace receiver")); }
            Output->Connect(*Remote);
            const double Until = FPlatformTime::Seconds() + 5;
            while (Output->GetConnectionState() != SCS_Connected && FPlatformTime::Seconds() < Until && !StopRequested)
                FPlatformProcess::Sleep(.005f);
            if (Output->GetConnectionState() != SCS_Connected) { Cleanup(); return Fail(TEXT("Cannot connect legacy trace relay to PC")); }
        }
        Running = true;
        Worker = Async(EAsyncExecution::Thread, [this]() { Run(); });
        if (!Trace::SendTo(TEXT("127.0.0.1"), LocalPort)) {
            StopRequested = true; Worker.Wait(); return Fail(TEXT("Native trace writer rejected capture connection"));
        }
        return true;
    }
    bool Stop() { StopRequested = true; return !Running; }
    FString GetError() { FScopeLock Lock(&ErrorMutex); return Error; }
private:
    bool Fail(const FString& Message) { FScopeLock Lock(&ErrorMutex); Error = Message; return false; }
    void Cleanup()
    {
        auto* Sockets = ISocketSubsystem::Get();
        for (FSocket** Socket : { &Native, &Listener, &Output }) {
            if (*Socket) { (*Socket)->Close(); Sockets->DestroySocket(*Socket); *Socket = nullptr; }
        }
        if (Writer) {
            if (!Writer->Close() || Writer->IsError()) Fail(TEXT("Cannot finalize legacy native trace file"));
            Writer.Reset();
        }
    }
    bool Write(const uint8* Data, size_t Size)
    {
        if (Writer) { Writer->Serialize(const_cast<uint8*>(Data), static_cast<int64>(Size)); return !Writer->IsError(); }
        size_t Offset = 0;
        double Until = FPlatformTime::Seconds() + 5;
        while (Offset < Size && FPlatformTime::Seconds() < Until) {
            int32 Sent = 0;
            if (Output->Send(Data + Offset, static_cast<int32>(Size - Offset), Sent) && Sent > 0) {
                Offset += Sent; Until = FPlatformTime::Seconds() + 5;
            } else if (ISocketSubsystem::Get()->GetLastErrorCode() != SE_EWOULDBLOCK) return false;
            else FPlatformProcess::Sleep(.001f);
        }
        return Offset == Size;
    }
    void Run()
    {
        cyinsights::legacy::PacketStream Packets;
        uint8 Buffer[65536];
        double StopAt = 0;
        const double ConnectUntil = FPlatformTime::Seconds() + 10;
        while (true) {
            const double Now = FPlatformTime::Seconds();
            // UE4 has no public writer-disconnect API. Drain a bounded grace
            // interval, then close ONLY our receiver at a complete packet boundary.
            if (StopRequested && StopAt == 0) StopAt = Now + .1;
            if (StopAt > 0 && Now >= StopAt) break;
            if (!Native) {
                bool Pending = false;
                if (Listener->HasPendingConnection(Pending) && Pending) {
                    Native = Listener->Accept(TEXT("CyInsights native writer"));
                    if (Native) Native->SetNonBlocking();
                }
                if (!Native && Now > ConnectUntil) { Fail(TEXT("Legacy trace writer did not connect")); break; }
            }
            if (Native) {
                int32 Count = 0;
                if (!Native->Recv(Buffer, sizeof(Buffer), Count)) {
                    if (!StopRequested) Fail(TEXT("Legacy native trace stream interrupted"));
                    break;
                }
                if (Count > 0 && !Packets.Feed(Buffer, static_cast<size_t>(Count),
                    [this](const uint8_t* Data, size_t Size) { return Write(Data, Size); })) {
                    Fail(UTF8_TO_TCHAR(Packets.GetError().c_str())); break;
                }
            }
            FPlatformProcess::Sleep(.001f);
        }
        if (!Packets.HasHeader()) Fail(TEXT("Legacy writer supplied no native trace header"));
        Cleanup();
        Running = false;
    }
    FCriticalSection Lifecycle, ErrorMutex;
    FString Error;
    std::atomic<bool> Running{false}, StopRequested{false};
    TFuture<void> Worker;
    FSocket* Listener = nullptr;
    FSocket* Native = nullptr;
    FSocket* Output = nullptr;
    TUniquePtr<FArchive> Writer;
};
TUniquePtr<FLegacyTraceSink> LegacySink;
}
#endif

namespace CyInsightsTrace {
namespace {
// Only called after the coordinator has refused an existing native recording.
const TCHAR* ManagedChannels[]={TEXT("cpu"),TEXT("gpu"),TEXT("frame"),TEXT("bookmark"),TEXT("counters"),TEXT("cyinsightssync"),TEXT("cyinsightsstats"),TEXT("cyinsightsvulkan"),TEXT("stats"),TEXT("task"),TEXT("net"),TEXT("file"),TEXT("loadtime"),TEXT("log"),TEXT("memtag")};
TMap<FString,bool> PreviousChannels;
bool ChannelsOwned=false;
void PrepareChannels() {
    if(ChannelsOwned)return;
    ChannelsOwned=true;PreviousChannels.Reset();
#if CYINSIGHTS_TRACE_HAS_ENUM_CHANNELS
    CyInsightsNativeTrace::EnumerateChannels([](const ANSICHAR* Name,bool Enabled,void*){
        FString Channel(ANSI_TO_TCHAR(Name));Channel.RemoveFromEnd(TEXT("Channel"));
        for(const TCHAR* Managed:ManagedChannels)if(Channel.Equals(Managed,ESearchCase::IgnoreCase))PreviousChannels.Add(Managed,Enabled);
    },nullptr);
#endif
    for(const TCHAR* Name:ManagedChannels)CyInsightsNativeTrace::ToggleChannel(Name,false);
}
}
void RestoreChannels() {
    if(!ChannelsOwned)return;
    for(const TCHAR* Name:ManagedChannels){const bool* Previous=PreviousChannels.Find(Name);CyInsightsNativeTrace::ToggleChannel(Name,Previous&&*Previous);}
    PreviousChannels.Reset();ChannelsOwned=false;
}
bool IsConnected()
{
#if CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
    return FTraceAuxiliary::IsConnected();
#else
    return CyInsightsNativeTrace::IsTracing();
#endif
}
bool GetIdentity(FGuid& Session, FGuid& TraceGuid)
{
#if CYINSIGHTS_TRACE_HAS_GUID
    return FTraceAuxiliary::IsConnected(Session, TraceGuid);
#else
    FScopeLock Lock(&IdentityMutex);
    const bool Connected = IsConnected();
    if (Connected && !IdentityWasConnected) IdentityEpoch = FGuid::NewGuid();
    IdentityWasConnected = Connected;
    Session = IdentityEpoch; TraceGuid = IdentityEpoch;
    return Connected;
#endif
}
void ConfigureChannels(const FString& Channels)
{
    PrepareChannels();
#if CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
    FTraceAuxiliary::FOptions Options;
#if CYINSIGHTS_TRACE_HAS_EXCLUDE_TAIL
    Options.bExcludeTail = true;
#endif
    FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::None, nullptr, *Channels, &Options);
#else
    TArray<FString> Names;
    Channels.ParseIntoArray(Names, TEXT(","), true);
    for (const FString& Name : Names) {
        FString Preset;
        if (GConfig && GConfig->GetString(TEXT("Trace.ChannelPresets"), *Name, Preset, GEngineIni)) {
            TArray<FString> Members; Preset.ParseIntoArray(Members, TEXT(","), true);
            for (const FString& Member : Members) CyInsightsNativeTrace::ToggleChannel(*Member, true);
        } else CyInsightsNativeTrace::ToggleChannel(*Name, true);
    }
#endif
}
bool StartFile(const FString& Path, const FString& Channels)
{
    PrepareChannels();
#if CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
    FTraceAuxiliary::FOptions Options;
#if CYINSIGHTS_TRACE_HAS_EXCLUDE_TAIL
    Options.bExcludeTail = true;
#endif
    const bool Started = FTraceAuxiliary::Start(FTraceAuxiliary::EConnectionType::File, *Path, *Channels, &Options);
#else
    ConfigureChannels(Channels);
    const bool Started = LegacySink && LegacySink->Start(Path, FString(), 0);
#endif
#if !CYINSIGHTS_TRACE_HAS_GUID
    if (Started) ResetObservedIdentity();
#endif
    if(!Started)RestoreChannels();
    return Started;
}
bool SendTo(const FString& Host, uint16 Port)
{
#if CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
#if CYINSIGHTS_TRACE_HAS_SEND_FLAGS
    const bool Started = CyInsightsNativeTrace::SendTo(*Host, Port, CyInsightsNativeTrace::FSendFlags::ExcludeTail);
#else
    const bool Started = CyInsightsNativeTrace::SendTo(*Host, Port);
#endif
#else
    const bool Started = LegacySink && LegacySink->Start(FString(), Host, Port);
#endif
#if !CYINSIGHTS_TRACE_HAS_GUID
    if (Started) ResetObservedIdentity();
#endif
    return Started;
}
bool Stop()
{
#if CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
    const bool Stopped=FTraceAuxiliary::Stop() || !FTraceAuxiliary::IsConnected();
#else
    const bool Stopped=LegacySink && LegacySink->Stop();
#endif
    if(Stopped)RestoreChannels();
    return Stopped;
}
FString GetError()
{
#if CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
    return FString();
#else
    return LegacySink ? LegacySink->GetError() : TEXT("Legacy trace module unavailable");
#endif
}
}

class FCyInsightsTraceModule final : public IModuleInterface
{
public:
    void StartupModule() override {
#if !CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
        LegacySink = MakeUnique<FLegacyTraceSink>();
#endif
    }
    void ShutdownModule() override {
#if !CYINSIGHTS_TRACE_HAS_AUX_CAPTURE
        LegacySink.Reset();
#endif
    }
};
IMPLEMENT_MODULE(FCyInsightsTraceModule, CyInsightsTrace)
