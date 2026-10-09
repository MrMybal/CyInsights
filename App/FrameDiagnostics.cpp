// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "FrameDiagnostics.hpp"
#include "AnalysisWorkers.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
namespace cyinsights {
namespace {
std::string Lower(std::string text){for(auto& c:text)c=static_cast<char>(std::tolower(static_cast<unsigned char>(c)));return text;}
}
FrameDomain DiagnosticDomain(const std::string& thread) {
    auto n=Lower(thread);
    if(n=="gamethread")return FrameDomain::Game;
    if(n=="renderthread"||n=="renderingthread"||n.rfind("renderthread ",0)==0)return FrameDomain::Render;
    if(n=="gpu"||n.rfind("gpu ",0)==0||(n.rfind("gpu",0)==0&&n.size()>3&&std::isdigit(static_cast<unsigned char>(n[3]))))return FrameDomain::Gpu;
    return FrameDomain::Other;
}
bool DiagnosticWait(const std::string& name) {
    auto n=Lower(name);
    // Deliberately restricted to explicit wait/sleep families, not arbitrary
    // substrings (e.g. a shader named AwaitedLighting is not an observed wait).
    auto leaf=n.substr(n.find_last_of(':')==std::string::npos?0:n.find_last_of(':')+1);
    return leaf.rfind("waitfor",0)==0||leaf.rfind("waituntil",0)==0||leaf=="waitframe"||
        leaf=="wait"||leaf=="sleep"||leaf=="sleepnostats"||
        leaf=="framelimit"||leaf=="idle"||leaf=="presentwait";
}
void FrameCoverage::Union::Add(double a,double b) {
    if(a<last)ordered=false;last=a;
    if(ordered&&!intervals.empty()&&a<=intervals.back().second)intervals.back().second=std::max(b,intervals.back().second);
    else intervals.emplace_back(a,b);
}
void FrameCoverage::Union::Finish() {
    if(!ordered) {
        std::sort(intervals.begin(),intervals.end());size_t size=0;
        for(auto v:intervals){if(size&&v.first<=intervals[size-1].second)intervals[size-1].second=std::max(v.second,intervals[size-1].second);else intervals[size++]=v;}
        intervals.resize(size);
    }
    prefix.resize(intervals.size()+1);prefix[0]=0;
    for(size_t i=0;i<intervals.size();++i)prefix[i+1]=prefix[i]+intervals[i].second-intervals[i].first;
}
double FrameCoverage::Union::Before(double t) const {
    auto it=std::lower_bound(intervals.begin(),intervals.end(),t,[](auto v,double x){return v.first<x;});
    auto i=static_cast<size_t>(it-intervals.begin());
    return i?prefix[i-1]+std::clamp(t-intervals[i-1].first,0.,intervals[i-1].second-intervals[i-1].first):0.;
}
double FrameCoverage::Union::Query(double a,double b) const {return std::max(0.,Before(b)-Before(a));}
void FrameCoverage::Advance(double time) {
    while(!stack_.empty()&&cursor_<time) {
        double stop=std::min(time,stack_.back().end);
        if(stack_.back().wait&&stop>cursor_)resolved_wait_.Add(cursor_,stop);
        cursor_=std::max(cursor_,stop);
        if(stack_.back().end<=cursor_)stack_.pop_back();else break;
    }
    cursor_=time;
    while(!stack_.empty()&&stack_.back().end<=time)stack_.pop_back();
}
void FrameCoverage::Add(double a,double b,bool wait,bool unfinished,int depth) {
    if(domain==FrameDomain::Other)return;
    if(unfinished){unfinished_.push_back(a);return;}
    if(!std::isfinite(a)||!std::isfinite(b)||b<=a)return;
    total_.Add(a,b);if(wait)wait_.Add(a,b);
    if(depth<0){nesting_=false;return;}
    if(a<last_start_)nesting_=false;last_start_=a;
    if(!nesting_)return;
    has_depth_=true;Advance(a);
    // Only contained nesting can distinguish a task executed while a parent
    // wait pumps the scheduler. Crossing/malformed scopes fall back to the
    // conservative union of explicitly named wait scopes.
    if(!stack_.empty()&&(depth<=stack_.back().depth||b>stack_.back().end)){nesting_=false;return;}
    stack_.push_back({b,depth,wait});
}
void FrameCoverage::Finish(){
    if(nesting_&&!stack_.empty())Advance(stack_.front().end);
    total_.Finish();wait_.Finish();resolved_wait_.Finish();std::sort(unfinished_.begin(),unfinished_.end());
}
FrameWork FrameCoverage::Query(double a,double b) const {
    FrameWork result;result.total=total_.Query(a,b);
    result.wait=std::min(result.total,(nesting_&&has_depth_?resolved_wait_:wait_).Query(a,b));
    result.active=std::max(0.,result.total-result.wait);result.available=result.total>1e-12;
    // An unfinished scope might still be running: never invent its endpoint.
    result.incomplete=(!unfinished_.empty()&&unfinished_.front()<b)||(has_depth_&&!nesting_);
    return result;
}
std::vector<FrameDiagnostic> BuildFrameDiagnostics(std::vector<FrameCoverage>& coverage,
    const std::vector<std::pair<double,double>>& frames,const std::atomic<bool>* cancel) {
    ParallelAnalysis(coverage.size(),0,[&](size_t i){coverage[i].Finish();},cancel);
    std::vector<FrameDiagnostic> result(frames.size());
    const size_t chunk=512;
    ParallelAnalysis((frames.size()+chunk-1)/chunk,0,[&](size_t job){
        for(size_t i=job*chunk;i<std::min(frames.size(),(job+1)*chunk);++i) {
            for(const auto& track:coverage)if(track.domain!=FrameDomain::Other) {
                auto w=track.Query(frames[i].first,frames[i].second);auto& best=result[i].work[static_cast<size_t>(track.domain)];
                bool incomplete=best.incomplete||w.incomplete;
                if(w.available&&(!best.available||w.active>best.active||(w.active==best.active&&w.total>best.total)))best=w;
                best.incomplete=incomplete;
            }
        }
    },cancel);
    return result;
}
FrameCause DiagnoseFrame(const FrameDiagnostic& frame,double duration,double budget) {
    if(!std::isfinite(budget)||budget<=0||duration<=0)return FrameCause::Unknown;
    if(duration<=budget)return FrameCause::WithinBudget;
    double largest=0,wait=0;size_t index=0;
    for(size_t i=0;i<3;++i) {
        const auto& w=frame.work[i];if(w.incomplete)return FrameCause::Unknown;
        if(w.available&&w.active>largest){largest=w.active;index=i;}
        if(i!=2)wait=std::max(wait,w.wait);
    }
    // Observed work must exceed the target; a slow frame with little observed
    // work is not proof that its longest sampled thread caused the delay.
    if(largest<=budget)return wait>=duration*.25?FrameCause::Wait:FrameCause::Unknown;
    for(size_t i=0;i<3;++i)if(i!=index&&frame.work[i].available&&frame.work[i].active>=largest*.9)return FrameCause::Mixed;
    return index==0?FrameCause::Game:index==1?FrameCause::Render:FrameCause::Gpu;
}
const char* FrameCauseName(FrameCause cause) {
    switch(cause){case FrameCause::WithinBudget:return "Within budget";case FrameCause::Game:return "GameThread candidate";
        case FrameCause::Render:return "RenderThread candidate";case FrameCause::Gpu:return "GPU candidate";
        case FrameCause::Mixed:return "Mixed candidates";case FrameCause::Wait:return "Wait / pacing";default:return "Unresolved / incomplete";}
}
}
