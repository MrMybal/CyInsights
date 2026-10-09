// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "AiAnalysis.hpp"
#include "Distribution.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace cyinsights {
namespace {
using J=cygi::Json;
void Check(const std::atomic<bool>* cancel){if(cancel&&cancel->load())throw std::runtime_error("Cancelled");}
J Number(double n){return std::isfinite(n)?J(n):J();}
J Range(double a,double b){auto j=J::Object();j["start"]=a;j["end"]=b;j["unit"]="seconds";return j;}
J Summary(const Distribution& d){auto j=J::Object();j["count"]=std::to_string(d.count);j["unavailable"]=std::to_string(d.unavailable);j["min"]=Number(d.count?d.min:NAN);j["mean"]=Number(d.count?d.mean:NAN);j["max"]=Number(d.count?d.max:NAN);j["unit"]="seconds";j["quantilesExact"]=d.exact_percentiles;
    auto q=J::Array();for(size_t i=0;i<4;++i){auto v=J::Object();v["percentile"]=static_cast<double>(std::array<int,4>{50,90,95,99}[i]);v["value"]=Number(d.count?d.percentiles[i]:NAN);v["lower"]=Number(d.count?d.percentile_bounds[i].first:NAN);v["upper"]=Number(d.count?d.percentile_bounds[i].second:NAN);q.Push(v);}j["quantiles"]=q;return j;}
J Cost(const FrameWork& w){auto j=J::Object();j["available"]=w.available&&!w.incomplete;j["total"]=Number(w.available&&!w.incomplete?w.total:NAN);j["active"]=Number(w.available&&!w.incomplete?w.active:NAN);j["recognizedWaits"]=Number(w.available&&!w.incomplete?w.wait:NAN);j["provenance"]="derived_interval_unions";return j;}
}
bool AiAnalysisOperation(const std::string& op){return op=="list_threads"||op=="list_functions"||op=="hotspots"||op=="histogram"||op=="detect_issues"||op=="query_tasks";}
cygi::Json AnalyzeAi(const std::shared_ptr<PagedTimingStore>& s,const std::shared_ptr<TaskStore>& tasks,const std::string& op,const cygi::Json& p,const std::atomic<bool>* cancel){
    using J=cygi::Json;if(!s)throw std::runtime_error("NoTrace: open a capture and wait for the timing index");Check(cancel);
    double a=p.Has("start")?p["start"].AsNumber(NAN):s->start,b=p.Has("end")?p["end"].AsNumber(NAN):std::nextafter(s->end,INFINITY);
    if(!std::isfinite(a)||!std::isfinite(b)||b<=a)throw std::runtime_error("InvalidParameters: end must exceed start");
    int limit=static_cast<int>(p["limit"].AsNumber(20));if(limit<1||limit>128)throw std::runtime_error("InvalidParameters: limit must be 1..128");
    auto j=J::Object();j["range"]=Range(a,b);j["traceEvents"]=std::to_string(s->count);auto rows=J::Array();
    auto search=p["search"].AsString();
    if(op=="list_threads"||op=="list_functions"){
        uint64_t offset=p["offset"].AsUInt(),matched=0;
        if(op=="list_threads")for(size_t i=0;i<s->tracks.size();++i){Check(cancel);auto& t=s->tracks[i];if(!TimingMatches(t.name,search.c_str()))continue;auto n=matched++;if(n<offset||rows.Size()>=static_cast<size_t>(limit))continue;auto r=J::Object();r["index"]=static_cast<double>(i);r["name"]=t.name;r["threadId"]=t.has_id?J(std::to_string(t.id)):J();r["gpu"]=t.gpu;rows.Push(r);}
        else for(size_t i=0;i<s->timers.size();++i){Check(cancel);auto& t=s->timers[i];if(t.frame_type>=0||!TimingMatches(t.name,search.c_str()))continue;auto n=matched++;if(n<offset||rows.Size()>=static_cast<size_t>(limit))continue;auto r=J::Object();r["index"]=static_cast<double>(i);r["name"]=t.name;r["timerId"]=t.has_id?J(std::to_string(t.id)):J();rows.Push(r);}
        j["matched"]=std::to_string(matched);j["offset"]=std::to_string(offset);j["hasMore"]=offset<matched&&rows.Size()<matched-offset;j["rows"]=rows;return j;
    }
    if(op=="hotspots"){
        auto w=s->Window(a,b,a,b,1,cancel);std::vector<size_t> order;
        for(size_t i=0;i<w.totals.size();++i)if(s->timers[i].frame_type<0&&w.totals[i].count&&TimingMatches(s->timers[i].name,search.c_str()))order.push_back(i);
        bool exclusive=p["metric"].AsString()=="exclusive";std::sort(order.begin(),order.end(),[&](auto x,auto y){return exclusive?w.totals[x].exclusive>w.totals[y].exclusive:w.totals[x].inclusive>w.totals[y].inclusive;});
        for(auto i:order){if(rows.Size()>=static_cast<size_t>(limit))break;auto& t=w.totals[i];auto r=J::Object();r["name"]=s->timers[i].name;r["timerId"]=s->timers[i].has_id?J(std::to_string(s->timers[i].id)):J();r["count"]=std::to_string(t.count);r["inclusive"]=t.inclusive;r["exclusive"]=Number(s->relations_valid?t.exclusive:NAN);r["unfinished"]=std::to_string(t.unfinished);rows.Push(r);}
        j["rows"]=rows;j["matched"]=std::to_string(order.size());j["unit"]="seconds";j["provenance"]="derived_clipped_scope_totals";j["nestedInclusiveValuesAreNotThreadUtilization"]=true;return j;
    }
    if(op=="histogram"){
        auto source=p["source"].AsString();auto function=p["function"].AsString();auto mask=function.empty()?std::vector<uint8_t>{}:s->FunctionMask(function);int track=static_cast<int>(p["track"].AsNumber(-1));if(track<-1||(track>=0&&static_cast<size_t>(track)>=s->tracks.size()))throw std::runtime_error("InvalidParameters: unknown track index");
        int domain=static_cast<int>(p["domain"].AsNumber(0));if(domain<0||domain>2)throw std::runtime_error("InvalidParameters: domain must be 0..2");auto metric=p["metric"].AsString();bool frame=source=="frames";
        SampleVisitor visit=[&](const SampleConsumer& consume){if(frame){for(size_t i=0;i<s->frames.size();++i){Check(cancel);auto f=s->frames[i];if(f.first<a||f.first>=b)continue;auto work=s->frame_diagnostics[i].work[domain];consume({metric=="wait"?work.wait:metric=="total"?work.total:work.active,f.first,f.second,i,0,work.available&&!work.incomplete&&(domain!=2||metric!="wait")});}}
            else s->VisitScopes(track<0?SIZE_MAX:static_cast<size_t>(track),a,b,p["rootsOnly"].AsBool(),[&](const StoredScope& r){consume({r.end-r.start,r.start,r.end,0,r.timer,!r.unfinished});},cancel,function.empty()?nullptr:&mask);};
        auto d=BuildDistribution(visit,frame,-1,0,cancel,p["logDurations"].AsBool(true));j["summary"]=Summary(d);j["function"]=function;j["sampleRule"]="start_in_half_open_range_full_duration";j["provenance"]=frame?"derived_frame_interval_cost":"measured_native_inclusive_scope_duration";
        if(p["includeBins"].AsBool()){auto bins=J::Array();for(auto n:d.bins)bins.Push(std::to_string(n));j["bins"]=bins;}
        for(auto& e:d.examples){if(rows.Size()>=static_cast<size_t>(limit))break;auto r=J::Object();r["start"]=e.start;r["end"]=e.end;r["duration"]=e.value;r["name"]=frame?"Frame interval":s->timers[e.timer].name;rows.Push(r);}j["slowest"]=rows;return j;
    }
    if(op=="detect_issues"){
        double fps=p["targetFps"].AsNumber(60);if(!std::isfinite(fps)||fps<1||fps>1000)throw std::runtime_error("InvalidParameters: targetFps must be 1..1000");double budget=1/fps;std::vector<size_t> slow;uint64_t examined=0;std::array<uint64_t,7> causes{};
        for(size_t i=0;i<s->frames.size();++i){Check(cancel);auto f=s->frames[i];if(f.first<a||f.first>=b)continue;++examined;auto cause=DiagnoseFrame(s->frame_diagnostics[i],f.second-f.first,budget);++causes[static_cast<size_t>(cause)];if(f.second-f.first>budget)slow.push_back(i);}
        std::sort(slow.begin(),slow.end(),[&](auto x,auto y){return s->frames[x].second-s->frames[x].first>s->frames[y].second-s->frames[y].first;});
        for(auto i:slow){if(rows.Size()>=static_cast<size_t>(limit))break;auto f=s->frames[i];auto r=J::Object();r["frameIndex"]=std::to_string(s->frames_native?s->frame_ids[i]:i);r["frameIdentity"]=s->frames_native?"native":"derived_tick_candidate";r["start"]=f.first;r["end"]=f.second;r["duration"]=f.second-f.first;r["probableCause"]=FrameCauseName(DiagnoseFrame(s->frame_diagnostics[i],f.second-f.first,budget));r["provenance"]="heuristic_not_causal_proof";r["game"]=Cost(s->frame_diagnostics[i].work[0]);r["render"]=Cost(s->frame_diagnostics[i].work[1]);r["gpu"]=Cost(s->frame_diagnostics[i].work[2]);rows.Push(r);}
        auto counts=J::Object();for(size_t i=0;i<causes.size();++i)counts[FrameCauseName(static_cast<FrameCause>(i))]=std::to_string(causes[i]);j["causeCounts"]=counts;j["examined"]=std::to_string(examined);j["overBudget"]=std::to_string(slow.size());j["budgetSeconds"]=budget;j["findings"]=rows;j["provenance"]="heuristic_not_causal_proof";return j;
    }
    if(op=="query_tasks"){
        bool candidates=p["candidates"].AsBool();if(!tasks&&!candidates)throw std::runtime_error("Unavailable: load native task data first or request candidates explicitly");
        int64_t thread=static_cast<int64_t>(p["thread"].AsNumber(-1));auto page=p["page"].AsUInt();auto result=candidates?TaskStore{}.ScopeCandidates(*s,a,b,thread,search,page,cancel):tasks->Query(a,b,thread,search,page,cancel);
        for(auto& t:result.rows){if(rows.Size()>=static_cast<size_t>(limit))break;auto r=J::Object();r["id"]=candidates?J():J(std::to_string(t.id));r["name"]=candidates?s->timers[t.name].name:tasks->names[t.name];r["start"]=Number(t.time[3]);r["end"]=Number(t.time[4]);r["execution"]=Number(t.Duration(TaskPhase::Started,TaskPhase::Finished));r["queueDelay"]=candidates?J():Number(t.Duration(TaskPhase::Scheduled,TaskPhase::Started));
            r["threadId"]=candidates?(s->tracks[t.thread[3]].has_id?J(std::to_string(s->tracks[t.thread[3]].id)):J()):(t.thread[3]==UINT32_MAX?J():J(std::to_string(t.thread[3])));rows.Push(r);}
        j["rows"]=rows;j["matched"]=std::to_string(result.matched);j["page"]=std::to_string(page);j["pageSize"]=128.;j["provenance"]=candidates?"heuristic_cpu_scope_candidates":"recorded_native_task_lifecycle";return j;
    }
    throw std::runtime_error("UnknownOperation");
}
}
