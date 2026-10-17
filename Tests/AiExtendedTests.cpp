// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiExtendedAnalysis.hpp"
#include "NativeTrace.hpp"
#include <fstream>
#include <iostream>
#include <cmath>
#include <cstring>
using namespace cyinsights;
using J=cygi::Json;
int main(){
    int checks=0,failed=0;auto check=[&](bool ok,const char* message){++checks;if(!ok){++failed;std::cerr<<message<<'\n';}};
    auto root=std::filesystem::temp_directory_path()/("CyAiExtended_"+std::to_string(GetCurrentProcessId()));std::filesystem::create_directories(root);
    auto csv=root/"timing.csv";std::ofstream(csv)<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n10,GameThread,1,FEngineLoop::Tick,0,1,0\n10,GameThread,2,Work,0.1,0.5,1\n10,GameThread,1,FEngineLoop::Tick,1,2,0\n10,GameThread,2,Work,1.1,1.9,1\n";
    NativeReadProgress progress;std::string error;auto timing=PagedTimingStore::Build(csv,root/"timing.bin",progress,error);check(timing!=nullptr,"Native fixture index built");if(!timing)return 1;
    auto counters=std::make_shared<CounterModel>();CounterSeries count;count.name="Objects";count.id=91;count.group="UObjects";count.unit="count";count.samples={{0.2,0,9007199254740993LL,true},{1.2,0,9007199254740994LL,false}};counters->series.push_back(count);
    CounterSeries cpu;cpu.name="CPU";cpu.id=92;cpu.floating=true;cpu.unit="ms";cpu.group="Unit";cpu.samples={{0.1,2,0,true},{0.6,4,0,true},{1.1,8,0,true}};counters->series.push_back(cpu);counters->start=0;counters->end=2;counters->Index();
    AiDataSnapshot s;s.timing=timing;s.counters=counters;auto run=[&](const char* op,const J& p=J::Object()){return AnalyzeAiExtended(s,op,p);};
    auto p=J::Object();p["limit"]=1.;auto r=run("list_counters",p);check(r["matched"].AsString()=="2"&&r["hasMore"].AsBool()&&r["rows"].Size()==1,"Counter paging counts full catalogue");
    p["offset"]=1.;r=run("list_counters",p);check(r["rows"].Items()[0]["name"].AsString()=="CPU"&&!r["hasMore"].AsBool(),"Counter second page stable");
    p=J::Object();p["counter"]=0.;r=run("query_counters",p);auto v=r["rows"].Items()[0];check(v["minimum"].AsString()=="9007199254740993"&&v["maximum"].AsString()=="9007199254740993","Exact int64 values survive JSON beyond double precision");check(v["count"].AsString()=="1"&&v["unavailable"].AsString()=="1","Invalid samples excluded, counted unavailable");check(!v.Has("bins")&&!v.Has("sampleCount"),"Large sample payloads omitted by default");
    p["includeSamples"]=true;p["bins"]=2.;r=run("query_counters",p);v=r["rows"].Items()[0];check(v["samples"].Size()==2&&v["samples"].Items()[1]["value"].IsNull(),"Unavailable individual value stays null");check(v["bins"].Size()==2&&v["bins"].Items()[1]["min"].IsNull(),"Empty or invalid bins are null, not zero");
    p=J::Object();p["counter"]=1.;p["start"]=0.;p["end"]=0.6;r=run("query_counters",p);check(r["rows"].Items()[0]["count"].AsString()=="1","Counter selection excludes upper endpoint");
    p=J::Object();p["counter"]=0.;r=run("query_frame_stats",p);check(r["rows"].Size()==2&&r["rows"].Items()[1]["value"].IsNull(),"Frames do not carry a stale counter from previous frame");
    counters->series[0].frame_stats=true;counters->series[0].frame_samples={{0,cyfs::FrameBounds|cyfs::Delayed|cyfs::Valid,0,0.4,1.5,7,8,9007199254740993LL,0},{0,cyfs::FrameBounds|cyfs::Delayed,1,1.4,2.5,9,10,0,0}};
    r=run("query_frame_stats",p);check(r["rows"].Items()[0]["observed"].AsNumber()==1.5&&r["rows"].Items()[0]["sampleTime"].AsNumber()==0.2,"Delayed GPU/stat receipt clock differs from execution placement");check(r["rows"].Items()[1]["observed"].AsNumber()==2.5&&!r["rows"].Items()[1]["available"].AsBool(),"Invalid observations retain recorded source identity without a fabricated value");
    p=J::Object();p["group"]="UObjects";r=run("list_counters",p);check(r["matched"].AsString()=="1","Counter group filter applied");
    p=J::Object();p["function"]="Work";p["limit"]=1.;r=run("query_scopes",p);check(r["matched"].AsString()=="2"&&r["rows"].Size()==1&&r["hasMore"].AsBool(),"Individual scopes bounded without capping population");
    p["offset"]=1.;r=run("query_scopes",p);check(r["rows"].Items()[0]["start"].AsNumber()==1.1,"Scope second page returns next native instance");
    p=J::Object();p["function"]="Work";p["direction"]="callers";p["start"]=0.3;p["end"]=0.4;r=run("query_relations",p);check(r["rows"].Size()==1&&r["rows"].Items()[0]["parent"].AsString()=="FEngineLoop::Tick"&&std::abs(r["rows"].Items()[0]["inclusive"].AsNumber()-0.1)<1e-9,"Caller relation includes crossing child clipped to range");
    p=J::Object();r=run("query_frames",p);check(r["rows"].Size()==2&&r["rows"].Items()[0]["gpu"]["total"].IsNull(),"Frame costs do not fabricate GPU observations");
    auto journal=root/"activity.csv";std::ofstream(journal)<<"CYACTIVITY1\nLane,0,network,Packets,bytes\nLane,1,storage,Reads,bytes\nLane,2,memory,Allocated,bytes\nLane,3,loading,Assets,events\nEvent,0,0.1,0.2,128,9007199254740993,10,0,packet\nEvent,0,0.3,0.4,256,2,10,0,packet\nEvent,1,0.2,0.8,4096,3,10,0,asset.bin\nEvent,2,0.5,0.5,18446744073709551615,4,10,81,memory\nEnd,4\n";
    s.activities=ActivityStore::Build(journal,root/"activity.bin");r=run("list_activity_lanes");check(r["matched"].AsString()=="4","All native activity kinds discovered");
    p=J::Object();p["kind"]="network";r=run("query_activities",p);v=r["rows"].Items()[0];check(v["matched"].AsString()=="2"&&v["nonGaugeValueSum"].AsString()=="384"&&v["events"].Size()==0,"Network summary compact and complete");
    p["includeEvents"]=true;p["bins"]=4.;r=run("query_activities",p);v=r["rows"].Items()[0];check(v["events"].Items()[0]["id"].AsString()=="9007199254740993"&&v["bins"].Size()==4,"Activity event identities exact, curve opt-in");
    p["kind"]="memory";r=run("query_activities",p);v=r["rows"].Items()[0];check(v["minimum"].AsString()=="-1"&&v["nonGaugeValueSum"].AsString()=="0","Signed memory gauge is not summed as traffic");
    p["kind"]="storage";p["start"]=0.4;p["end"]=0.5;r=run("query_activities",p);v=r["rows"].Items()[0];check(std::abs(v["inclusiveSeconds"].AsNumber()-0.1)<1e-9&&v["nonGaugeValueSum"].AsString()=="0","Crossing storage duration clipped; bytes not counted as starting in range");
    p=J::Object();p["kind"]="loading";r=run("query_activities",p);check(r["rows"].Items()[0]["minimum"].IsNull(),"Empty loading lane remains unavailable");
    p=J::Object();s.comparison=timing;r=run("query_comparison",p);check(!r["clocksAligned"].AsBool()&&r["rows"].Size()==2,"Comparison uses session-local clocks and native aggregates");for(auto& row:r["rows"].Items())check(std::abs(row["delta"].AsNumber())<1e-9,"Identical capture has zero delta");
    p["start"]=0.;p["end"]=1.;p["startB"]=1.;p["endB"]=2.;p["metric"]="inclusive";p["search"]="Work";r=run("query_comparison",p);check(std::abs(r["rows"].Items()[0]["delta"].AsNumber()-0.4)<1e-9,"Independent A/B ranges change normalized result");
    auto taskfile=root/"tasks.csv";std::ofstream(taskfile)<<"Event,Time,TaskId,ThreadId,RelatedId,Name,Tracked,TargetThread,Size\nLaunched,0.1,9007199254740993,10,0,Task,1,0,1\nScheduled,0.2,9007199254740993,10,0,,0,0,0\nStarted,0.3,9007199254740993,10,0,,0,0,0\nFinished,0.4,9007199254740993,10,0,,0,0,0\nSubsequentAdded,0.2,9007199254740993,10,8,,0,0,0\nEnd,0,5,0,0,,0,0,0\n";
    s.tasks=TaskStore::Build(taskfile,root/"tasks.bin",2);p=J::Object();p["taskId"]="9007199254740993";r=run("query_task_links",p);check(r["phases"].Size()==7&&r["phases"].Items()[0]["time"].IsNull()&&r["rows"].Items()[0]["to"].AsString()=="8","Task phases and dependency IDs preserved, missing created phase null");
    p=J::Object();r=run("query_gpu",p);check(r["availability"].AsString()=="unavailable"&&r["rows"].Size()==0,"No optional GPU payload invented");
    s.gpu=std::make_shared<cygi::SessionModel>();
    auto apply=[&](cygi::RecordType type,const auto& record){cygi::RecordHeader header{};header.type=type;s.gpu->ApplyRecord(header,reinterpret_cast<const uint8_t*>(&record),uint32_t(sizeof(record)));};
    cygi::PipelineInfoRecord pipeline{};pipeline.pipeline_id=1;apply(cygi::RecordType::pipeline_info,pipeline);
    cygi::ResourceInfoRecord resource{};resource.resource_id=1;resource.kind=cygi::ResourceKind::buffer;resource.buffer_size=9007199254740993ULL;apply(cygi::RecordType::resource_info,resource);
    cygi::ShaderCodeRecord shader{};shader.shader_id=1;apply(cygi::RecordType::shader_code,shader);
    cygi::FrameBeginRecord begin{7,0};apply(cygi::RecordType::frame_begin,begin);
    struct Events {cygi::FrameEventsRecord header;cygi::FrameEvent rows[2];} events{};events.header.frame_index=7;events.header.event_count=2;events.rows[0].index=0;events.rows[0].kind=cygi::EventKind::draw;events.rows[0].pipeline_id=1;events.rows[0].primary_resource=1;events.rows[0].a=300;events.rows[1].index=1;events.rows[1].kind=cygi::EventKind::dispatch;events.rows[1].pipeline_id=1;
    apply(cygi::RecordType::frame_events,events);cygi::FrameEndRecord ended{};ended.frame_index=7;ended.draw_count=1;ended.dispatch_count=1;ended.event_count=2;apply(cygi::RecordType::frame_end,ended);
    p=J::Object();p["section"]="commands";p["limit"]=1.;r=run("query_gpu",p);check(r["frameIndex"].AsString()=="7"&&r["matched"].AsString()=="2"&&r["hasMore"].AsBool(),"GPU command pagination bound to original capture frame");check(r["rows"].Items()[0]["gpuSeconds"].IsNull(),"Untimed command is not zero GPU time");
    p["event"]=1.;p["includeDetails"]=true;r=run("query_gpu",p);check(r["matched"].AsString()=="1"&&r["rows"].Items()[0]["kind"].AsString()==cygi::EventKindName(cygi::EventKind::dispatch),"GPU command index filter applied");
    cygi::DrawStateRecord state{};state.frame_index=7;state.event_index=0;state.pipeline_id=1;state.binding_count=2;cygi::DrawBinding binding[2]{};binding[0].slot=3;binding[1].slot=8;
    std::vector<uint8_t> payload(sizeof(state)+sizeof(binding));std::memcpy(payload.data(),&state,sizeof(state));std::memcpy(payload.data()+sizeof(state),binding,sizeof(binding));cygi::RecordHeader record{};record.type=cygi::RecordType::draw_state;s.gpu->ApplyRecord(record,payload.data(),uint32_t(payload.size()));
    p["event"]=0.;p["includeBindings"]=true;p["detailOffset"]=1.;r=run("query_gpu",p);v=r["rows"].Items()[0];check(v["bindings"].Size()==1&&v["bindings"].Items()[0]["slot"].AsNumber()==8&&!v["bindingsTruncated"].AsBool(),"Binding detail page can inspect entries beyond the first response");check(v["drawState"]["viewport"].IsNull()&&v["drawState"]["scissor"].IsNull(),"Unrecorded viewport/scissor are unavailable, not zero geometry");
    p=J::Object();p["section"]="resources";r=run("query_gpu",p);check(r["rows"].Items()[0]["bytes"].AsString()=="9007199254740993","Resource byte size exact, no buffer content returned");
    p["section"]="pipelines";r=run("query_gpu",p);check(r["rows"].Size()==1,"Recorded pipeline catalogue available");p["section"]="shaders";r=run("query_gpu",p);check(r["rows"].Size()==1&&!r["rows"].Items()[0].Has("code"),"Shader identities provided without bytecode token dump");
    p["section"]="passes";r=run("query_gpu",p);check(r["rows"].Size()>0&&r["provenance"].AsString()=="derived_command_clustering_and_resource_dependencies","Reconstructed GPU passes clearly derived");
    auto rejects=[&](const char* op,J p){try{run(op,p);return false;}catch(...){return true;}};
    p=J::Object();p["counter"]=99.;check(rejects("query_counters",p),"Unknown counter rejected");p=J::Object();p["limit"]=129.;check(rejects("query_frames",p),"Response limits enforced by native layer");p=J::Object();p["start"]=1.;p["end"]=0.;check(rejects("query_scopes",p),"Invalid range rejected");
    std::atomic<bool> cancel{true};bool cancelled=false;try{AnalyzeAiExtended(s,"query_activities",J::Object(),&cancel);}catch(...){cancelled=true;}check(cancelled,"Activity work cancellation checked");
    auto held=s.counters;counters=std::make_shared<CounterModel>();check(held->series.size()==2,"Worker snapshot retains original counter owner after document swaps");
    std::filesystem::remove_all(root);std::cout<<checks<<" checks, "<<failed<<" failures\n";return failed?1:0;
}
