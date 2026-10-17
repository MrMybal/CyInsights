// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Installed providers decode the trace; only this adapter is compiled here.
#include <Windows.h>
#include <strsafe.h>
#undef Yield
#pragma warning(disable:4996) // ReadTimers remains the common installed 5.5-5.8 API.
#include "CoreMinimal.h"
#include "Misc/CommandLine.h"
#include "Modules/ModuleManager.h"
#include "TraceServices/ITraceServicesModule.h"
#include "Trace/DataStream.h"
#include "TraceServices/AnalysisService.h"
#include "TraceServices/ModuleService.h"
#include "TraceServices/Model/TimingProfiler.h"
#include "TraceServices/Model/Threads.h"
#include "TraceServices/Model/Frames.h"
#include "TraceServices/Model/Counters.h"
#include "Runtime/Launch/Resources/Version.h"
#include "Format.hpp"
#include "GpuScopeName.hpp"
#include <cstdio>
#include <cmath>
#include <cfloat>
#include <vector>
#include <set>
#include <string>
#include <stdexcept>
#include <unordered_map>
#include <filesystem>
using namespace TraceServices;
static std::string Utf8(const TCHAR* s){if(!s)return {};int n=WideCharToMultiByte(CP_UTF8,0,s,-1,nullptr,0,nullptr,nullptr);std::string result(n,'\0');if(n){WideCharToMultiByte(CP_UTF8,0,s,-1,result.data(),n,nullptr,nullptr);result.pop_back();}return result;}
class Input final:public UE::Trace::IInDataStream {
public:
 FILE* file;bool& failed;Input(const wchar_t* path,bool& error):file(_wfopen(path,L"rb")),failed(error){}
 ~Input(){if(file)fclose(file);}
 int32 Read(void* data,uint32 size)override{auto n=fread(data,1,size,file);failed|=ferror(file)!=0;return static_cast<int32>(n);}
};
static void Write(FILE* f,const void* p,size_t n){if(fwrite(p,1,n,f)!=n)throw std::runtime_error("Timing output write failed");}
template<class T>static void Value(FILE* f,const T& v){Write(f,&v,sizeof(v));}
static void String(FILE* f,const std::string& s){if(s.size()>UINT32_MAX)throw std::runtime_error("Timing name too long");Value(f,static_cast<uint32_t>(s.size()));Write(f,s.data(),s.size());}
static void Cell(FILE* f,const std::string& s){fputc('"',f);for(char c:s){if(c=='"')fputc('"',f);fputc(c,f);}fputc('"',f);}
struct Track {uint64 id;uint32 timeline;std::string name;};
#include "FrameStatsAnalyzer.hpp"
#include "VulkanPassAnalyzer.hpp"
#include "MetaGpuAnalyzer.hpp"
int wmain(int argc,wchar_t** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc!=4){fprintf(stderr,"Usage: CyNativeTiming trace.utrace timing.cynt engine-binaries\n");return 2;}
 const std::wstring diagnosticPath=std::wstring(argv[2])+L".log";
 FILE* diagnostics=nullptr;if(_wfreopen_s(&diagnostics,diagnosticPath.c_str(),L"wb",stderr))return 5;
 FILE* out=nullptr;FILE* catalog=nullptr;
 try{
 FCommandLine::Set(TEXT("-NoLog -NoAnalytics -NoTrace"));
 const auto frameStatsPath=std::filesystem::path(argv[2]).parent_path()/L"frame-stats.cystats";
 FrameStatsModule frameStats(frameStatsPath.wstring());
 VulkanPassModule vulkanPasses((std::filesystem::path(argv[2]).parent_path()/L"vulkan-passes.raw").wstring());
 MetaGpuModule metaGpu(std::filesystem::path(argv[2]).parent_path());
 auto& modules=FModuleManager::Get();FString binaries(argv[3]);
 modules.AddModule(TEXT("TraceAnalysis"));modules.SetModuleFilename(TEXT("TraceAnalysis"),binaries/TEXT("UnrealInsights-TraceAnalysis.dll"));
 modules.AddModule(TEXT("TraceServices"));modules.SetModuleFilename(TEXT("TraceServices"),binaries/TEXT("UnrealInsights-TraceServices.dll"));
 auto* module=FModuleManager::LoadModulePtr<ITraceServicesModule>(TEXT("TraceServices"));if(!module)throw std::runtime_error("Cannot load installed TraceServices module");
 auto settings=module->GetModuleService();TArray<FModuleInfo> available;settings->GetAvailableModules(available);
 for(auto& info:available){auto name=info.Name.ToString();settings->SetModuleEnabled(info.Name,name==TEXT("CyInsights_FrameStats")||name==TEXT("CyInsights_VulkanPasses")||name==TEXT("CyInsights_MetaGpu")||name==TEXT("TraceModule_TimingProfiler")||name==TEXT("TraceModule_Counters")||name==TEXT("TraceModule_Stats")||name==TEXT("TraceModule_CsvProfiler"));}
 bool inputFailed=false;auto source=new Input(argv[1],inputFailed);if(!source->file){delete source;return 4;}
 auto session=module->GetAnalysisService()->StartAnalysis(~0u,argv[2],TUniquePtr<UE::Trace::IInDataStream>(source));if(!session.IsValid())return 4;
 session->Wait();if(inputFailed)return 6;
 metaGpu.Read(std::filesystem::path(argv[1]),frameStats);frameStats.Finish();
 for(const auto& message:const_cast<IAnalysisSession*>(session.Get())->DrainPendingMessages())if(message.Severity==EMessageSeverity::Error)return 6;
 out=_wfopen(argv[2],L"wb");if(!out)return 5;setvbuf(out,nullptr,_IOFBF,1024*1024);Write(out,cynt::magic,8);
 uint64 total=0;
 {
 FAnalysisSessionReadScope lock(*session);
 auto* timing=ReadTimingProfilerProvider(*session);if(!timing)throw std::runtime_error("Timing provider unavailable");
 std::vector<Track> tracks;std::set<uint32> seen;
 ReadThreadProvider(*session).EnumerateThreads([&](const FThreadInfo& t){uint32 index;if(timing->GetCpuThreadTimelineIndex(t.Id,index)&&seen.insert(index).second)tracks.push_back({t.Id,index,Utf8(t.Name)});});
 auto gpu=[&](uint32 index,const std::string& name){if(seen.insert(index).second)tracks.push_back({(1ull<<63)|index,index,name});};
 uint32 index;if(timing->GetGpuTimelineIndex(index))gpu(index,"GPU");if(timing->GetGpu2TimelineIndex(index))gpu(index,"GPU2");
#if ENGINE_MINOR_VERSION >= 6
 timing->EnumerateGpuQueues([&](const FGpuQueueInfo& q){uint32 index;if(timing->GetGpuQueueTimelineIndex(q.Id,index))gpu(index,"GPU / "+Utf8(q.Name)+" / queue "+std::to_string(q.Id));});
#endif
 timing->ReadTimers([&](const ITimingProfilerTimerReader& reader){
  const auto nativeCount=reader.GetTimerCount();auto n=nativeCount;
  std::vector<std::string> groupNames;
  std::unordered_map<std::string,uint32> groupIds;
  auto groupName=[&](const FTimingProfilerEvent& e)->std::string {
#if ENGINE_MINOR_VERSION >= 6
   const auto* t=reader.GetTimer(e.TimerIndex);
   if(t&&int32(e.TimerIndex)<0&&FCString::Strcmp(t->Name,TEXT("RDGEvent"))==0) {
    const auto metadata=reader.GetMetadata(e.TimerIndex);
    const auto name=cynt::GpuScopeName(metadata.GetData(),static_cast<size_t>(metadata.Num()));
    return std::string(name);
   }
#endif
   return {};
  };
  // Only visit GPU tracks twice. Store unique names, never a second copy of
  // the event stream or a metadata map growing with the number of frames.
  for(const auto& track:tracks)if(track.id>>63)timing->ReadTimeline(track.timeline,[&](const ITimingProfilerProvider::Timeline& timeline){
   timeline.EnumerateEvents(-DBL_MAX,DBL_MAX,[&](double,double,uint32,const FTimingProfilerEvent& e){
    auto name=groupName(e);
    if(!name.empty()&&groupIds.find(name)==groupIds.end()) {
     if(n==UINT32_MAX)throw std::runtime_error("GPU name dictionary overflow");
     groupIds.emplace(name,n++);groupNames.push_back(std::move(name));
    }
    return EEventEnumerate::Continue;
   });
  });
  fprintf(stdout,"Decoded GPU group labels: %zu\n",groupNames.size());
  const auto passTimerBase=n;
  if(vulkanPasses.names.size()>UINT32_MAX-n)throw std::runtime_error("Vulkan pass dictionary overflow");
  n+=static_cast<uint32>(vulkanPasses.names.size());
  const auto metaTimerBase=n;
  if(metaGpu.names.size()>UINT32_MAX-n)throw std::runtime_error("Meta GPU dictionary overflow");
  n+=static_cast<uint32>(metaGpu.names.size());
  Value(out,n);
  for(uint32 i=0;i<nativeCount;++i){auto* t=reader.GetTimer(i);if(!t)throw std::runtime_error("Missing timer dictionary entry");Value(out,static_cast<uint64>(t->Id));String(out,Utf8(t->Name));}
  for(uint32 i=0;i<groupNames.size();++i){Value(out,static_cast<uint64>(nativeCount+i));String(out,groupNames[i]);}
  for(uint32 i=0;i<vulkanPasses.names.size();++i){Value(out,uint64(passTimerBase+i));String(out,vulkanPasses.names[i]);}
  for(uint32 i=0;i<metaGpu.names.size();++i){Value(out,uint64(metaTimerBase+i));String(out,metaGpu.names[i]);}
  Value(out,static_cast<uint32>(tracks.size()+(vulkanPasses.recorded?1:0)+metaGpu.TrackCount()));
  for(const auto& track:tracks){
   Value(out,track.id);String(out,track.name);
   auto countOffset=_ftelli64(out);uint64 count=0;Value(out,count);
   timing->ReadTimeline(track.timeline,[&](const ITimingProfilerProvider::Timeline& timeline){
    timeline.EnumerateEvents(-DBL_MAX,DBL_MAX,[&](double a,double b,uint32 depth,const FTimingProfilerEvent& e){
     auto* t=reader.GetTimer(e.TimerIndex);if(!t||t->Id>=n)throw std::runtime_error("Invalid timing event dictionary reference");
     uint32 timer=t->Id;
     if(track.id>>63){const auto name=groupName(e);if(!name.empty())timer=groupIds.at(name);}
     cynt::Event record{a,b,timer,depth,(std::isinf(b)&&b>0)?1u:0u,0};Value(out,record);++count;return EEventEnumerate::Continue;
    });
   });
   auto end=_ftelli64(out);if(_fseeki64(out,countOffset,SEEK_SET))throw std::runtime_error("Timing output seek failed");Value(out,count);if(_fseeki64(out,end,SEEK_SET))throw std::runtime_error("Timing output seek failed");total+=count;
  }
  if(vulkanPasses.recorded){vulkanPasses.Export(out,passTimerBase);total+=vulkanPasses.aligned;}
  metaGpu.Export(out,metaTimerBase);total+=metaGpu.recorded;
 });
 const auto& frames=ReadFrameProvider(*session);uint64 n=0;
 for(uint32 type=0;type<2;++type)n+=frames.GetFrameCount(static_cast<ETraceFrameType>(type));Value(out,n);
 for(uint32 type=0;type<2;++type)frames.EnumerateFrames(static_cast<ETraceFrameType>(type),uint64(0),frames.GetFrameCount(static_cast<ETraceFrameType>(type)),[&](const FFrame& frame){cynt::Frame record{frame.StartTime,frame.EndTime,frame.Index,type,0};Value(out,record);++total;});
 // A small catalog preserves the existing on-demand counter value loader.
 FString catalogPath=FString(argv[2]);int slash=-1;catalogPath.FindLastChar(TEXT('/'),slash);int backslash;if(catalogPath.FindLastChar(TEXT('\\'),backslash))slash=FMath::Max(slash,backslash);
 catalogPath=catalogPath.Left(slash+1)+TEXT("counters.csv");catalog=_wfopen(*catalogPath,L"wb");if(!catalog)throw std::runtime_error("Counter catalog write failed");fprintf(catalog,"Id,Type,Name\n");
 ReadCounterProvider(*session).EnumerateCounters([&](uint32 id,const ICounter& c){fprintf(catalog,"%u,%s%s,",id,c.IsFloatingPoint()?"Double":"Int64",c.IsResetEveryFrame()?"|ResetEveryFrame":"");Cell(catalog,Utf8(c.GetName()));fputc('\n',catalog);});
 if(ferror(catalog))throw std::runtime_error("Counter catalog write failed");auto catalogFile=catalog;catalog=nullptr;if(fclose(catalogFile))throw std::runtime_error("Counter catalog close failed");
 }
 Write(out,cynt::footer,8);Value(out,total);auto file=out;out=nullptr;if(fclose(file))throw std::runtime_error("Timing output close failed");
 fprintf(stdout,"Native timing records: %llu\n",total);return 0;
 }catch(const std::exception& e){fprintf(stderr,"%s\n",e.what());if(out)fclose(out);if(catalog)fclose(catalog);return 7;}
}

