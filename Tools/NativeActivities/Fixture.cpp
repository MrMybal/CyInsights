// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Synthetic provider integration fixture; it does not measure device traffic.
#include <Windows.h>
#include <strsafe.h>
#include "HAL/PlatformTime.h"
#include "ProfilingDebugging/PlatformFileTrace.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "ProfilingDebugging/MiscTrace.h"
#include "Trace/Trace.h"
#include "Trace/Trace.inl"
UE_TRACE_CHANNEL_DEFINE(CyActivityFixtureChannel);
UE_TRACE_EVENT_BEGIN(NetTrace, InitEvent)
 UE_TRACE_EVENT_FIELD(uint64, Timestamp) UE_TRACE_EVENT_FIELD(uint32, NetTraceVersion) UE_TRACE_EVENT_FIELD(uint32, NetTraceReporterVersion)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(NetTrace, ConnectionCreatedEvent)
 UE_TRACE_EVENT_FIELD(uint16, ConnectionId) UE_TRACE_EVENT_FIELD(uint8, GameInstanceId)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(NetTrace, PacketEvent)
 UE_TRACE_EVENT_FIELD(uint64, Timestamp) UE_TRACE_EVENT_FIELD(uint32, PacketBits) UE_TRACE_EVENT_FIELD(uint32, SequenceNumber)
 UE_TRACE_EVENT_FIELD(uint16, ConnectionId) UE_TRACE_EVENT_FIELD(uint8, GameInstanceId) UE_TRACE_EVENT_FIELD(uint8, PacketType)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(LLM, TrackerSpec, Important|NoSync)
 UE_TRACE_EVENT_FIELD(uint8, TrackerId) UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Name)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(LLM, TagsSpec, Important|NoSync)
 UE_TRACE_EVENT_FIELD(uint64, TagId) UE_TRACE_EVENT_FIELD(uint64, ParentId) UE_TRACE_EVENT_FIELD(uint8, TagSetId) UE_TRACE_EVENT_FIELD(UE::Trace::AnsiString, Name)
UE_TRACE_EVENT_END()
UE_TRACE_EVENT_BEGIN(LLM, TagValue)
 UE_TRACE_EVENT_FIELD(uint8, TrackerId) UE_TRACE_EVENT_FIELD(uint64, Cycle) UE_TRACE_EVENT_FIELD(uint64[], Tags) UE_TRACE_EVENT_FIELD(int64[], Values)
UE_TRACE_EVENT_END()
int wmain(int argc,wchar_t** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);if(argc!=2)return 2;FPlatformTime::InitTiming();
 UE::Trace::FInitializeDesc desc;desc.bUseWorkerThread=false;UE::Trace::Initialize(desc);UE::Trace::ToggleChannel(TEXT("CyActivityFixture"),true);UE::Trace::ToggleChannel(TEXT("file"),true);UE::Trace::ToggleChannel(TEXT("cpu"),true);UE::Trace::ToggleChannel(TEXT("frame"),true);
 if(!UE::Trace::WriteTo(argv[1]))return 3;UE::Trace::Update();
 UE_TRACE_LOG(NetTrace,InitEvent,CyActivityFixtureChannel)<<InitEvent.Timestamp(FPlatformTime::Cycles64())<<InitEvent.NetTraceVersion(5)<<InitEvent.NetTraceReporterVersion(1);
 UE_TRACE_LOG(NetTrace,ConnectionCreatedEvent,CyActivityFixtureChannel)<<ConnectionCreatedEvent.ConnectionId(7)<<ConnectionCreatedEvent.GameInstanceId(1);
 UE_TRACE_LOG(LLM,TrackerSpec,CyActivityFixtureChannel,sizeof("Fixture tracker")-1)<<TrackerSpec.TrackerId(0)<<TrackerSpec.Name("Fixture tracker");
 UE_TRACE_LOG(LLM,TagsSpec,CyActivityFixtureChannel,sizeof("Fixture allocation state")-1)<<TagsSpec.TagId(42)<<TagsSpec.ParentId(0)<<TagsSpec.TagSetId(0)<<TagsSpec.Name("Fixture allocation state");
 FPlatformFileTrace::BeginOpen(TEXT("Synthetic provider fixture.bin"));Sleep(2);FPlatformFileTrace::EndOpen(123);
 for(int i=0;i<3;++i){FMiscTrace::OutputBeginFrame(TraceFrameType_Game);FCpuProfilerTrace::OutputBeginDynamicEvent(TEXT("Synthetic CPU scope / provider fixture"));FPlatformFileTrace::BeginRead(100+i,123,128*i,128);Sleep(2);FPlatformFileTrace::EndRead(100+i,128);
  UE_TRACE_LOG(NetTrace,PacketEvent,CyActivityFixtureChannel)<<PacketEvent.Timestamp(FPlatformTime::Cycles64())<<PacketEvent.PacketBits(800+8*i)<<PacketEvent.SequenceNumber(i)<<PacketEvent.ConnectionId(7)<<PacketEvent.GameInstanceId(1)<<PacketEvent.PacketType(i==1?1:0);
  uint64 tags[]={42};int64 values[]={1024+512*i};UE_TRACE_LOG(LLM,TagValue,CyActivityFixtureChannel)<<TagValue.TrackerId(0)<<TagValue.Cycle(FPlatformTime::Cycles64())<<TagValue.Tags(tags,1)<<TagValue.Values(values,1);
  Sleep(5);FCpuProfilerTrace::OutputEndEvent();FMiscTrace::OutputEndFrame(TraceFrameType_Game);FCpuProfilerTrace::FlushThreadBuffer();UE::Trace::Update();
 }
 FPlatformFileTrace::BeginClose(123);FPlatformFileTrace::EndClose(123);UE::Trace::Update();UE::Trace::Stop();UE::Trace::Shutdown();return 0;
}
