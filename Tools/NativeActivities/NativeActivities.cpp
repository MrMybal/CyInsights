// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Only this adapter is compiled. Epic providers remain in the licensed installation.
#include <Windows.h>
#include <strsafe.h>
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "TraceServices/ITraceServicesModule.h"
#include "Trace/DataStream.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ModuleService.h"
#include "TraceServices/Model/NetProfiler.h"
#include "TraceServices/Model/LoadTimeProfiler.h"
#include "TraceServices/Model/Memory.h"
#include "TraceServices/Model/AllocationsProvider.h"
#include "Runtime/Launch/Resources/Version.h"
#include <cstdio>
#include <cmath>
#include <string>
using namespace TraceServices;
static std::string Utf8(const TCHAR* text) {if(!text)return {};int n=WideCharToMultiByte(CP_UTF8,0,text,-1,nullptr,0,nullptr,nullptr);std::string s(n,'\0');WideCharToMultiByte(CP_UTF8,0,text,-1,s.data(),n,nullptr,nullptr);if(n)s.pop_back();return s;}
static void Cell(FILE* f,const std::string& s){fputc('"',f);for(char c:s){if(c=='"')fputc('"',f);fputc(c=='\r'||c=='\n'?' ':c,f);}fputc('"',f);}
class Input final:public UE::Trace::IInDataStream {
 FILE* file=nullptr;
public:
 explicit Input(const wchar_t* path){file=_wfopen(path,L"rb");}
 ~Input(){if(file)fclose(file);}
 bool Valid() const{return file!=nullptr;}
 int32 Read(void* data,uint32 size)override{return static_cast<int32>(fread(data,1,size,file));}
};
struct Journal {
 FILE* out;uint64 count=0;uint32 lanes=0;
 uint32 Lane(const char* kind,const std::string& label,const char* unit){auto id=lanes++;fprintf(out,"Lane,%u,%s,",id,kind);Cell(out,label);fprintf(out,",%s\n",unit);return id;}
 void Event(uint32 lane,double a,double b,uint64 value,uint64 identity,uint32 thread,uint32 flags,const std::string& name){
  if(!std::isfinite(a))return;
  if(!std::isfinite(b)||b<a){b=a;flags|=2;} // Incomplete: no fabricated end/duration.
  fprintf(out,"Event,%u,%.17g,%.17g,%llu,%llu,%u,%u,",lane,a,b,value,identity,thread,flags);Cell(out,name);fputc('\n',out);++count;
 }
};
int wmain(int argc,wchar_t** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc!=4){fprintf(stderr,"Usage: CyNativeActivities trace.utrace activity.csv engine-binaries\n");return 2;}
 FCommandLine::Set(TEXT("-NoLog -NoAnalytics -NoTrace"));
 auto& modules=FModuleManager::Get();FString binaries(argv[3]);
 modules.AddModule(TEXT("TraceAnalysis"));modules.SetModuleFilename(TEXT("TraceAnalysis"),binaries/TEXT("UnrealInsights-TraceAnalysis.dll"));
 modules.AddModule(TEXT("TraceServices"));modules.SetModuleFilename(TEXT("TraceServices"),binaries/TEXT("UnrealInsights-TraceServices.dll"));
 auto* module=FModuleManager::LoadModulePtr<ITraceServicesModule>(TEXT("TraceServices"));if(!module)return 3;
 auto settings=module->GetModuleService();TArray<FModuleInfo> available;settings->GetAvailableModules(available);
 for(auto& info:available){auto name=info.Name.ToString();settings->SetModuleEnabled(info.Name,name==TEXT("TraceModule_NetProfiler")||name==TEXT("TraceModule_LoadTimeProfiler")||name==TEXT("TraceModule_Memory"));}
 auto service=module->GetAnalysisService();auto source=new Input(argv[1]);if(!source->Valid()){delete source;return 4;}
 TUniquePtr<UE::Trace::IInDataStream> input(source);
 auto session=service->StartAnalysis(~0u,argv[2],MoveTemp(input));if(!session.IsValid())return 4;session->Wait();
 for(const auto& message:const_cast<IAnalysisSession*>(session.Get())->DrainPendingMessages())if(message.Severity==EMessageSeverity::Error)return 6;
 FILE* out=_wfopen(argv[2],L"wb");if(!out)return 5;fprintf(out,"CYACTIVITY1\n");Journal j{out};
 {
 FAnalysisSessionReadScope scope(*session);double end=session->GetDurationSeconds();
 if(auto* net=ReadNetProfilerProvider(*session);net&&net->GetNetTraceVersion()){
  net->ReadGameInstances([&](const FNetProfilerGameInstance& game){net->ReadConnections(game.GameInstanceIndex,[&](const FNetProfilerConnection& c){
   for(auto mode:{Outgoing,Incoming}){auto n=net->GetPacketCount(c.ConnectionIndex,mode);if(!n)continue;
    auto label="Network / instance "+std::to_string(game.GameInstanceIndex)+" / connection "+std::to_string(c.ConnectionId)+(mode==Outgoing?" / send":" / receive");
    auto lane=j.Lane("network",label,"bytes");
    net->EnumeratePackets(c.ConnectionIndex,mode,0,n-1,[&](const FNetProfilerPacket& p){
     auto name=std::string(mode==Outgoing?"Send packet":"Receive packet")+(p.DeliveryStatus==ENetProfilerDeliveryStatus::Dropped?" [dropped]":"");
     j.Event(lane,p.TimeStamp,p.TimeStamp,p.TotalPacketSizeInBytes,p.SequenceNumber,UINT32_MAX,1,name);
     // Contents have bit offsets, not execution times. Keep them associated
     // with the packet timestamp; never turn bit positions into milliseconds.
     if(p.EventCount)net->EnumeratePacketContentEventsByIndex(c.ConnectionIndex,mode,p.StartEventIndex,p.StartEventIndex+p.EventCount-1,[&](const FNetProfilerContentEvent& e){
      std::string name;if(e.NameIndex<net->GetNameCount())net->ReadName(e.NameIndex,[&](const FNetProfilerName& n){name=Utf8(n.Name);});
      j.Event(lane,p.TimeStamp,p.TimeStamp,e.EndPos-e.StartPos,p.SequenceNumber,UINT32_MAX,1|8,name.empty()?"Packet content":name);
     });
    });
   }
  });});
 }
 if(auto* file=ReadFileActivityProvider(*session)){
  uint32 lane=UINT32_MAX;file->EnumerateFileActivity([&](const FFileInfo& info,const IFileActivityProvider::Timeline& timeline){
   if(timeline.GetEventCount()&&lane==UINT32_MAX)lane=j.Lane("storage","Storage / Unreal file operations","bytes");
   timeline.EnumerateEvents(0,end,[&](double a,double b,uint32,const FFileActivity* e){if(e){auto name=Utf8(GetFileActivityTypeString(e->ActivityType))+" / "+Utf8(info.Path);
    j.Event(lane,a,b,e->ActualSize,e->Offset,e->ThreadId,e->Failed?4:0,name);}return EEventEnumerate::Continue;});return true;
  });
 }
 if(auto* load=ReadLoadTimeProfilerProvider(*session))for(uint32 i=0;i<load->GetTimelineCount();++i){
  load->ReadTimeline(i,[&](const ILoadTimeProfilerProvider::CpuTimeline& t){if(!t.GetEventCount())return;auto lane=j.Lane("loading","Asset loading / timeline "+std::to_string(i),"bytes");
   t.EnumerateEvents(0,end,[&](double a,double b,uint32,const FLoadTimeProfilerCpuEvent& e){
    auto name=Utf8(GetLoadTimeProfilerObjectEventTypeString(e.EventType))+" / "+Utf8(e.Package?e.Package->Name:nullptr);
    j.Event(lane,a,b,e.Export?e.Export->SerialSize:0,e.Package?e.Package->Id:0,UINT32_MAX,0,name);return EEventEnumerate::Continue;});
  });
 }
 if(auto* memory=ReadMemoryProvider(*session)){
#if ENGINE_MINOR_VERSION >= 6
  memory->BeginRead();
#endif
  memory->EnumerateTrackers([&](const FMemoryTrackerInfo& tracker){memory->EnumerateTags([&](const FMemoryTagInfo& tag){
   if(!memory->GetTagSampleCount(tracker.Id,tag.Id))return;
   auto lane=j.Lane("memory","Memory / "+Utf8(*tracker.Name)+" / "+Utf8(*tag.Name),"bytes");
   double finalTime=-1;
   memory->EnumerateTagSamples(tracker.Id,tag.Id,0,end,false,[&](double t,double d,const FMemoryTagSample& s){
    if(d==0){finalTime=t;return;}j.Event(lane,t,t+d,static_cast<uint64>(s.Value),tag.Id,UINT32_MAX,16|64,"LLM sampled value");
   });
   // Query the final point alone: some installed providers return the first
   // sample value for the final zero-duration callback of a multi-point range.
   if(finalTime>=0)memory->EnumerateTagSamples(tracker.Id,tag.Id,finalTime,finalTime,false,[&](double t,double,const FMemoryTagSample& s){j.Event(lane,t,t,static_cast<uint64>(s.Value),tag.Id,UINT32_MAX,16|64,"LLM sampled value");});
  });});
#if ENGINE_MINOR_VERSION >= 6
  memory->EndRead();
#endif
 }
 if(auto* alloc=ReadAllocationsProvider(*session)){
  alloc->BeginRead();auto points=alloc->GetTimelineNumPoints();
  if(points){auto bytes=j.Lane("memory","Memory / total allocated min","bytes");auto maximum=j.Lane("memory","Memory / total allocated max","bytes");
   auto allocations=j.Lane("memory","Memory / allocation events","events");auto frees=j.Lane("memory","Memory / free events","events");
#if ENGINE_MINOR_VERSION >= 7
   alloc->EnumerateTimeline(IAllocationsProvider::ETimelineU64::MinTotalAllocatedMemory,0,points-1,[&](double t,double d,uint64 v){j.Event(bytes,t,t+d,v,0,UINT32_MAX,16,"Native minimum in sample interval");});
#else
   alloc->EnumerateMinTotalAllocatedMemoryTimeline(0,points-1,[&](double t,double d,uint64 v){j.Event(bytes,t,t+d,v,0,UINT32_MAX,16,"Native minimum in sample interval");});
#endif
#if ENGINE_MINOR_VERSION >= 7
   alloc->EnumerateTimeline(IAllocationsProvider::ETimelineU64::MaxTotalAllocatedMemory,0,points-1,[&](double t,double d,uint64 v){j.Event(maximum,t,t+d,v,0,UINT32_MAX,16,"Native maximum in sample interval");});
#else
   alloc->EnumerateMaxTotalAllocatedMemoryTimeline(0,points-1,[&](double t,double d,uint64 v){j.Event(maximum,t,t+d,v,0,UINT32_MAX,16,"Native maximum in sample interval");});
#endif
#if ENGINE_MINOR_VERSION >= 7
   alloc->EnumerateTimeline(IAllocationsProvider::ETimelineU32::AllocEvents,0,points-1,[&](double t,double d,uint32 v){j.Event(allocations,t,t+d,v,0,UINT32_MAX,32,"Allocation count in sample interval");});
#else
   alloc->EnumerateAllocEventsTimeline(0,points-1,[&](double t,double d,uint32 v){j.Event(allocations,t,t+d,v,0,UINT32_MAX,32,"Allocation count in sample interval");});
#endif
#if ENGINE_MINOR_VERSION >= 7
   alloc->EnumerateTimeline(IAllocationsProvider::ETimelineU32::FreeEvents,0,points-1,[&](double t,double d,uint32 v){j.Event(frees,t,t+d,v,0,UINT32_MAX,32,"Free count in sample interval");});
#else
   alloc->EnumerateFreeEventsTimeline(0,points-1,[&](double t,double d,uint32 v){j.Event(frees,t,t+d,v,0,UINT32_MAX,32,"Free count in sample interval");});
#endif
  }alloc->EndRead();
 }
 }
 bool failed=ferror(out)!=0; if(!failed)fprintf(out,"End,%llu\n",j.count);failed|=fclose(out)!=0;
 // The process owns the providers and installed DLLs until exit.
 fprintf(stdout,"Native activity events: %llu\n",j.count);return failed?6:0;
}
