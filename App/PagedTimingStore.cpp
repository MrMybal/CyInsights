// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "PagedTimingStore.hpp"
#include "NativeTrace.hpp"
#include "AnalysisWorkers.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <tuple>
#include <unordered_map>
#include <stdexcept>
#include <set>
#include <list>
#include <mutex>
#include <chrono>
namespace cyinsights {
namespace {
struct Parent {double start=0,end=0;int depth=0;uint32_t timer=0;bool unfinished=false;};
void Check(const std::atomic<bool>* cancel){if(cancel&&cancel->load())throw std::runtime_error("Analysis cancelled");}
template<class Record> void Read(std::ifstream& in,uint64_t offset,std::vector<Record>& records,uint32_t count) {
    records.resize(count);in.clear();in.seekg(static_cast<std::streamoff>(offset));
    in.read(reinterpret_cast<char*>(records.data()),static_cast<std::streamsize>(count*sizeof(Record)));
    if(!in)throw std::runtime_error("Cannot read timing cache; reopen the capture");
}
}
// This budget controls only reusable disk blocks. Eviction never discards a
// source event or limits which captures/intervals can be read.
struct PagedTimingStore::Cache {
    struct Entry {std::shared_ptr<const std::vector<Record>> records;std::list<uint32_t>::iterator position;};
    static constexpr size_t budget=32*1024*1024;
    std::mutex mutex;
    std::unordered_map<uint32_t,Entry> entries;
    std::list<uint32_t> recent;
    size_t bytes=0;
};
std::vector<uint8_t> PagedTimingStore::FunctionMask(const std::string& name) const {
    std::vector<uint8_t> result(timers.size());for(size_t i=0;i<result.size();++i)result[i]=timers[i].frame_type<0&&timers[i].name==name?1:0;return result;
}
void PagedTimingStore::VisitScopes(size_t track,double a,double b,bool roots,const std::function<void(const StoredScope&)>& sink,const std::atomic<bool>* cancel,const std::vector<uint8_t>* timer_matches,ScopeVisitMetrics* metrics_out) const {
    Check(cancel);if((track!=SIZE_MAX&&track>=tracks.size())||!std::isfinite(a)||!std::isfinite(b)||b<=a||(timer_matches&&timer_matches->size()!=timers.size()))throw std::runtime_error("Invalid scope distribution interval");
    std::ifstream input;TimingWindow metrics;
    std::vector<uint32_t> candidates;
    if(timer_matches){for(size_t t=0;t<timer_matches->size();++t)if((*timer_matches)[t])candidates.insert(candidates.end(),timer_blocks_[t].begin(),timer_blocks_[t].end());
        std::sort(candidates.begin(),candidates.end());candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());}
    else {for(const auto& lane:lanes_){if(lane.blocks.empty())continue;const auto& first_block=blocks_[lane.blocks.front()];if((track!=SIZE_MAX&&first_block.track!=track)||(roots&&first_block.depth!=0))continue;
        auto first=std::lower_bound(lane.prefix_end.begin(),lane.prefix_end.end(),a)-lane.prefix_end.begin();
        for(size_t n=static_cast<size_t>(first);n<lane.blocks.size();++n){auto i=lane.blocks[n];if(blocks_[i].start>=b)break;candidates.push_back(i);}}
        // Keep the original physical ordering for stable pages and sequential I/O.
        std::sort(candidates.begin(),candidates.end());}
    ScopeVisitMetrics result;
    for(auto i:candidates) {
        const auto& block=blocks_[i];if((track!=SIZE_MAX&&block.track!=track)||(roots&&block.depth!=0)||block.start>=b||block.end<a)continue;
        ++result.block_candidates;Check(cancel);auto records=Records(i,input,metrics);result.records_scanned+=records->size();
        for(const auto& r:*records)if(r.start>=a&&r.start<b&&timers[r.timer].frame_type<0&&(!timer_matches||(*timer_matches)[r.timer])){
            ++result.samples;sink({r.start,r.end,r.timer,block.depth,r.unfinished!=0,block.track});}
    }
    result.blocks_read=metrics.blocks_read;result.cache_hits=metrics.cache_hits;if(metrics_out)*metrics_out=result;
}
std::vector<StoredRelation> PagedTimingStore::Relations(double a,double b,const std::atomic<bool>* cancel) const {
    Check(cancel);if(!relations_valid)throw std::runtime_error("Unavailable: source nesting does not support unique direct relations");
    if(!std::isfinite(a)||!std::isfinite(b)||b<=a)throw std::runtime_error("Invalid relation range");
    std::map<std::tuple<uint32_t,uint32_t,bool>,StoredRelation> totals;std::ifstream input;TimingWindow metrics;
    for(uint32_t i=0;i<blocks_.size();++i){const auto& block=blocks_[i];if(block.start>=b||(!block.unfinished&&block.end<=a))continue;Check(cancel);
        auto records=Records(i,input,metrics);for(const auto& r:*records){if(r.parent<0||timers[r.timer].frame_type>=0||timers[size_t(r.parent)].frame_type>=0)continue;double cost=std::max(0.,std::min(r.unfinished?end:r.end,b)-std::max(r.start,a));if(cost<=0)continue;
            bool gpu=tracks[block.track].gpu;auto key=std::make_tuple(uint32_t(r.parent),r.timer,gpu);auto& v=totals[key];v.parent=uint32_t(r.parent);v.child=r.timer;v.gpu=gpu;if(r.unfinished)++v.unfinished;else{++v.count;v.inclusive+=cost;}}
    }std::vector<StoredRelation> result;for(const auto& entry:totals)result.push_back(entry.second);return result;
}
void PagedTimingStore::ClearReadCache() const {
    std::lock_guard<std::mutex> guard(cache_->mutex);
    cache_->entries.clear();cache_->recent.clear();cache_->bytes=0;
}
std::shared_ptr<const std::vector<PagedTimingStore::Record>> PagedTimingStore::Records(uint32_t index,std::ifstream& input,TimingWindow& metrics) const {
    {
        std::lock_guard<std::mutex> guard(cache_->mutex);
        auto found=cache_->entries.find(index);
        if(found!=cache_->entries.end()) {
            cache_->recent.splice(cache_->recent.begin(),cache_->recent,found->second.position);
            ++metrics.cache_hits;return found->second.records;
        }
    }
    auto records=std::make_shared<std::vector<Record>>();const auto& block=blocks_[index];
    if(!input.is_open())input.open(file_,std::ios::binary);
    if(!input)throw std::runtime_error("Timing cache missing; reopen the capture");
    Read(input,block.offset,*records,block.count);++metrics.blocks_read;
    const size_t bytes=records->size()*sizeof(Record);
    std::lock_guard<std::mutex> guard(cache_->mutex);
    if(cache_->entries.count(index))return records; // Another independent analysis populated it.
    while(cache_->bytes+bytes>Cache::budget&&!cache_->recent.empty()) {
        auto oldest=cache_->recent.back();auto found=cache_->entries.find(oldest);
        cache_->bytes-=found->second.records->size()*sizeof(Record);
        cache_->entries.erase(found);cache_->recent.pop_back();
    }
    if(bytes<=Cache::budget) {
        cache_->recent.push_front(index);cache_->entries.emplace(index,Cache::Entry{records,cache_->recent.begin()});cache_->bytes+=bytes;
    }
    return records;
}
std::shared_ptr<PagedTimingStore> PagedTimingStore::Build(const std::filesystem::path& csv,
    const std::filesystem::path& binary,NativeReadProgress& progress,std::string& error,bool binary_source) {
    auto result=std::make_shared<PagedTimingStore>();result->file_=binary;result->cache_=std::make_shared<Cache>();
    auto partial=binary;partial+=L".partial";
    try {
        std::ofstream out(partial,std::ios::binary|std::ios::trunc);
        if(!out)throw std::runtime_error("Cannot create timing cache; check free disk space and permissions");
        out.write("CYTIME01",8);
        std::unordered_map<uint64_t,uint32_t> thread_ids,timer_ids;
        std::unordered_map<std::string,uint32_t> thread_names,timer_names;
        std::map<std::pair<int,uint64_t>,uint32_t> frame_timers;
        std::map<std::pair<uint32_t,int>,std::vector<Record>> buffers;
        std::vector<std::vector<Parent>> stacks;
        std::vector<double> last_start;
        std::vector<FrameCoverage> coverage;
        std::vector<uint8_t> waits;
        std::vector<std::pair<std::pair<double,double>,uint64_t>> native_frames;
        size_t buffered=0;
        auto flush=[&] {
            Check(&progress.cancel);
            for(auto& entry:buffers) {
                auto& records=entry.second;if(records.empty())continue;
                Block block;block.offset=static_cast<uint64_t>(out.tellp());block.count=static_cast<uint32_t>(records.size());
                block.track=entry.first.first;block.depth=entry.first.second;
                block.start=records.front().start;block.end=records.front().start;
                for(const auto& r:records){block.start=std::min(block.start,r.start);block.end=std::max(block.end,r.unfinished?r.start:r.end);block.unfinished|=r.unfinished!=0;}
                out.write(reinterpret_cast<const char*>(records.data()),static_cast<std::streamsize>(records.size()*sizeof(Record)));
                if(!out)throw std::runtime_error("Timing cache write failed; check free disk space");
                const auto index=static_cast<uint32_t>(result->blocks_.size());
                result->timer_blocks_.resize(result->timers.size());
                std::vector<uint32_t> membership;membership.reserve(records.size());for(const auto& r:records)membership.push_back(r.timer);
                std::sort(membership.begin(),membership.end());membership.erase(std::unique(membership.begin(),membership.end()),membership.end());
                for(auto t:membership)result->timer_blocks_[t].push_back(index);
                result->blocks_.push_back(block);records.clear();
            }
            buffered=0;
        };
        auto consume=[&](TimingEvent&& e) {
            uint32_t track;
            if(e.has_thread_id) {
                auto n=thread_ids.try_emplace(e.thread_id,static_cast<uint32_t>(result->tracks.size()));track=n.first->second;
            } else {auto n=thread_names.try_emplace(e.thread,static_cast<uint32_t>(result->tracks.size()));track=n.first->second;}
            if(track==result->tracks.size()){result->tracks.push_back({e.thread,e.thread_id,e.has_thread_id,e.depth,TimingMatches(e.thread,"gpu")});stacks.emplace_back();last_start.push_back(-INFINITY);coverage.emplace_back(DiagnosticDomain(e.thread));}
            result->tracks[track].max_depth=std::max(result->tracks[track].max_depth,e.depth);
            uint32_t timer;
            if(e.frame_type>=0){auto n=frame_timers.try_emplace(std::make_pair(e.frame_type,e.frame_index),static_cast<uint32_t>(result->timers.size()));timer=n.first->second;}
            else if(e.has_timer_id){auto n=timer_ids.try_emplace(e.timer_id,static_cast<uint32_t>(result->timers.size()));timer=n.first->second;}
            else {auto n=timer_names.try_emplace(e.name,static_cast<uint32_t>(result->timers.size()));timer=n.first->second;}
            if(timer==result->timers.size()){result->timers.push_back({e.name,e.timer_id,e.has_timer_id,e.frame_type,e.frame_index});result->domains_.push_back(0);waits.push_back(DiagnosticWait(e.name)?1:0);}
            if(e.frame_type<0)coverage[track].Add(e.start,e.end,coverage[track].domain!=FrameDomain::Gpu&&waits[timer]!=0,e.open_ended,e.depth);
            result->domains_[timer]|=result->tracks[track].gpu?2:1;
            Record record{e.start,e.open_ended?e.start:e.end,timer,-1,e.open_ended?1u:0u};
            auto& stack=stacks[track];
            if(e.start<last_start[track]){result->relations_valid=false;stack.clear();}
            last_start[track]=e.start;
            while(!stack.empty()&&(stack.back().end<=e.start||stack.back().depth>=e.depth))stack.pop_back();
            if(!e.open_ended&&!stack.empty()) {
                const auto& parent=stack.back();
                if(!parent.unfinished&&parent.depth+1==e.depth&&parent.start<=e.start&&parent.end>=e.end)
                    record.parent=static_cast<int32_t>(parent.timer);
            }
            stack.push_back({e.start,e.end,e.depth,timer,e.open_ended});
            if(result->count==0)result->start=result->end=e.start;
            result->start=std::min(result->start,e.start);result->end=std::max(result->end,e.open_ended?e.start:e.end);++result->count;
            if(!e.open_ended&&e.end>e.start) {
                if(e.frame_type==0)native_frames.push_back({{e.start,e.end},e.frame_index});
                else if(e.thread=="GameThread"&&(e.name=="FEngineLoop::Tick"||e.name=="EngineLoopTick"))result->frames.emplace_back(e.start,e.end);
            }
            auto& buffer=buffers[{track,e.depth}];buffer.push_back(record);++buffered;
            // Working-buffer size affects only flushing, never capture acceptance.
            if(buffer.size()>=2048||buffered>=65536)flush();
        };
        if(!(binary_source?NativeTrace::ScanBinary(csv,consume,error,&progress):NativeTrace::ScanCsv(csv,consume,error,&progress)))throw std::runtime_error(error);
        flush();out.close();if(!out)throw std::runtime_error("Cannot finish timing cache");
        Check(&progress.cancel);
        if(result->end<=result->start)result->end=result->start+.001;
        std::sort(result->frames.begin(),result->frames.end());
        if(!native_frames.empty()) {
            std::sort(native_frames.begin(),native_frames.end());result->frames.clear();result->frames_native=true;
            for(const auto& frame:native_frames){result->frames.push_back(frame.first);result->frame_ids.push_back(frame.second);}
        }
        auto frame_begin=std::chrono::steady_clock::now();
        result->frame_diagnostics=BuildFrameDiagnostics(coverage,result->frames,&progress.cancel);
        result->frame_preparation_ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-frame_begin).count();
        coverage.clear();coverage.shrink_to_fit();
        std::map<std::pair<uint32_t,int>,size_t> lane_ids;
        for(uint32_t i=0;i<result->blocks_.size();++i) {
            const auto& block=result->blocks_[i];auto entry=lane_ids.emplace(std::make_pair(block.track,block.depth),result->lanes_.size());
            if(entry.second)result->lanes_.emplace_back();
            result->lanes_[entry.first->second].blocks.push_back(i);
        }
        for(auto& lane:result->lanes_) {
            Check(&progress.cancel);
            std::sort(lane.blocks.begin(),lane.blocks.end(),[&](auto x,auto y){return result->blocks_[x].start<result->blocks_[y].start;});
            double stop=-INFINITY;
            for(auto i:lane.blocks){const auto& block=result->blocks_[i];stop=std::max(stop,block.unfinished?result->end:block.end);lane.prefix_end.push_back(stop);}
        }
        std::map<ComparisonKey,SummaryGroup> summary_groups;
        for(size_t i=0;i<result->timers.size();++i)if(result->timers[i].frame_type<0)
            for(int domain=0;domain<2;++domain)if(result->domains_[i]&(domain?2:1)) {
                const auto& timer=result->timers[i];ComparisonKey key{timer.name,domain!=0};auto& group=summary_groups[key];
                group.identity.key=key;group.timers.push_back(i);
                if(timer.has_id)group.identity.native_ids.push_back(timer.id);else group.identity.identity_missing=true;
            }
        for(auto& entry:summary_groups) {
            auto& group=entry.second;auto& ids=group.identity.native_ids;
            std::sort(ids.begin(),ids.end());ids.erase(std::unique(ids.begin(),ids.end()),ids.end());
            result->summary_groups_.push_back(std::move(group));
        }
        Check(&progress.cancel);
        // Publish only after the interval/catalog metadata is complete too.
        std::error_code ec;std::filesystem::remove(binary,ec);std::filesystem::rename(partial,binary);
        return result;
    }catch(const std::exception& e){error=e.what();std::error_code ec;std::filesystem::remove(partial,ec);return {};}
}
TimingWindow PagedTimingStore::Window(double a,double b,double stats_a,double stats_b,float pixels,
                                     const std::atomic<bool>* cancel,unsigned workers,const std::string& exact_name) const {
    Check(cancel);if(!std::isfinite(a)||!std::isfinite(b)||b<=a||!std::isfinite(pixels)||pixels<=0)throw std::runtime_error("Invalid timing window");
    TimingWindow window;window.start=a;window.end=b;window.stats_start=stats_a;window.stats_end=stats_b;window.totals.resize(timers.size());
    window.cpu_totals.resize(timers.size());window.gpu_totals.resize(timers.size());
    window.gpu_details.resize(tracks.size());
    std::ifstream in(file_,std::ios::binary);if(!in)throw std::runtime_error("Timing cache missing; reopen the capture");
    auto name_matches=exact_name.empty()?std::vector<uint8_t>{}:FunctionMask(exact_name);
    struct Group {int column;Record first;double stop;uint64_t represented=1;bool unfinished=false,name_match=false;};
    struct Job {
        uint32_t track;int depth;uint64_t records=0;
        std::vector<size_t> blocks;
        std::unordered_map<uint32_t,StoredTotal> totals;
        std::vector<TimingEvent> grouped;
        TimingWindow metrics;
        GpuTimingDetail gpu_detail;
    };
    std::vector<uint32_t> candidates;
    auto find_blocks=[&](double left,double right) {
        for(const auto& lane:lanes_) {
            Check(cancel);
            auto first=std::lower_bound(lane.prefix_end.begin(),lane.prefix_end.end(),left)-lane.prefix_end.begin();
            for(size_t i=static_cast<size_t>(first);i<lane.blocks.size();++i) {
                auto index=lane.blocks[i];const auto& block=blocks_[index];if(block.start>right)break;
                if((block.unfinished?end:block.end)>=left)candidates.push_back(index);
            }
        }
    };
    find_blocks(a,b);
    if(stats_a!=a||stats_b!=b)find_blocks(stats_a,stats_b);
    std::sort(candidates.begin(),candidates.end());candidates.erase(std::unique(candidates.begin(),candidates.end()),candidates.end());
    window.block_candidates=candidates.size();
    std::map<std::pair<uint32_t,int>,size_t> job_ids;
    std::vector<Job> jobs;uint64_t records_to_scan=0;
    for(size_t i=0;i<candidates.size();++i) {
        const auto& block=blocks_[candidates[i]];
        auto key=std::make_pair(block.track,block.depth);auto found=job_ids.emplace(key,jobs.size());
        if(found.second){Job job;job.track=block.track;job.depth=block.depth;jobs.push_back(std::move(job));}
        auto& job=jobs[found.first->second];job.blocks.push_back(i);job.records+=block.count;records_to_scan+=block.count;
    }
    // Fixed partitions and ordered reduction make results independent of job
    // scheduling. Only the independent lanes are processed concurrently.
    std::vector<size_t> order;for(const auto& entry:job_ids)order.push_back(entry.second);
    auto scheduled=order;std::stable_sort(scheduled.begin(),scheduled.end(),[&](auto x,auto y){return jobs[x].records>jobs[y].records;});
    const unsigned automatic=AnalysisWorkerSetting()==0&&records_to_scan<131072?std::min(4u,AnalysisWorkerCount()):AnalysisWorkerCount();
    const unsigned requested=workers?workers:(records_to_scan>=32768?automatic:1);
    window.workers=static_cast<unsigned>(std::max<size_t>(1,std::min<size_t>(jobs.size(),std::min(requested,AnalysisWorkerLimit()))));
    std::vector<std::vector<TimingEvent>> individuals(candidates.size());
    // Dense pixel lookup has a working-memory budget, with an unbounded sparse
    // fallback. Neither path limits the input or drops any source records.
    const size_t columns=pixels<=65536?static_cast<size_t>(std::ceil(std::max(1.f,pixels)))+1:0;
    const bool dense=columns&&jobs.size()<=8*1024*1024/(columns*sizeof(size_t));
    const double span=b-a;
    ParallelAnalysis(jobs.size(),window.workers,[&](size_t task) {
        auto& job=jobs[scheduled[task]];const auto& track=tracks[job.track];
        std::ifstream input;std::vector<Group> groups;
        std::vector<size_t> slots;if(dense)slots.assign(columns,SIZE_MAX);
        std::unordered_map<int,size_t> sparse;
        for(auto position:job.blocks) {
        Check(cancel);auto index=candidates[position];const auto& block=blocks_[index];
        double block_end=block.unfinished?end:block.end;
        bool display=block.start<=b&&block_end>=a,stats=block.start<=stats_b&&block_end>=stats_a;
        if(!display&&!stats)continue;
        auto records=Records(index,input,job.metrics);job.metrics.records_scanned+=records->size();
        for(const auto& r:*records) {
            const auto& timer=timers[r.timer];double stop=r.unfinished?end:r.end;
            if(stats&&timer.frame_type<0) {
                double cost=std::max(0.,std::min(stop,stats_b)-std::max(r.start,stats_a));
                if(cost>0&&r.unfinished)++job.totals[r.timer].unfinished;
                else if(cost>0){auto& total=job.totals[r.timer];++total.count;total.inclusive+=cost;total.exclusive+=cost;
                    if(r.parent>=0&&timers[static_cast<size_t>(r.parent)].frame_type<0)job.totals[static_cast<uint32_t>(r.parent)].exclusive-=cost;}
            }
            if(!display||r.start>b||stop<a)continue;
            if(track.gpu)job.gpu_detail.AddInterval(r.start,r.end,block.depth,r.unfinished!=0,timer.frame_type);
            // Construct names only for actual render records, not every scope
            // folded into an existing pixel. Statistics above use every record.
            auto event=[&] {
                TimingEvent e{track.name,timer.name,r.start,stop,block.depth};
                e.thread_id=track.id;e.has_thread_id=track.has_id;e.timer_id=timer.id;e.has_timer_id=timer.has_id;
                e.open_ended=r.unfinished!=0;e.frame_type=timer.frame_type;e.frame_index=timer.frame_index;return e;
            };
            if((std::min(stop,b)-std::max(r.start,a))/span*pixels<1.) {
                int column=static_cast<int>(std::clamp((r.start-a)/span,0.,1.)*std::max(1.f,pixels-1));
                size_t group_index;
                if(dense)group_index=slots[static_cast<size_t>(column)];
                else {auto found=sparse.find(column);group_index=found==sparse.end()?SIZE_MAX:found->second;}
                if(group_index==SIZE_MAX) {
                    if(dense)slots[static_cast<size_t>(column)]=groups.size();else sparse.emplace(column,groups.size());
                    groups.push_back({column,r,stop,1,r.unfinished!=0,!name_matches.empty()&&name_matches[r.timer]!=0});
                }
                else {
                    auto& group=groups[group_index];
                    group.first.start=std::min(group.first.start,r.start);group.stop=std::max(group.stop,stop);
                    group.unfinished|=r.unfinished!=0;++group.represented;
                    group.name_match|=!name_matches.empty()&&name_matches[r.timer]!=0;
                }
            } else individuals[position].push_back(event());
        }
        }
    std::sort(groups.begin(),groups.end(),[](const auto& x,const auto& y){return x.column<y.column;});
    job.grouped.reserve(groups.size());
    for(auto& group:groups) {
        const auto& timer=timers[group.first.timer];
        const bool aggregated=group.represented>1;
        TimingEvent event{track.name,aggregated?std::to_string(group.represented)+" native scopes (display aggregate)":timer.name,
            group.first.start,group.stop,job.depth};
        event.thread_id=track.id;event.has_thread_id=track.has_id;event.timer_id=timer.id;event.has_timer_id=!aggregated&&timer.has_id;
        event.open_ended=group.unfinished;event.frame_type=aggregated?-1:timer.frame_type;event.frame_index=timer.frame_index;
        event.display_aggregate=aggregated;event.represented=group.represented;job.metrics.aggregated|=aggregated;
        event.exact_name_match=group.name_match;
        job.grouped.push_back(std::move(event));
    }
    },cancel);
    size_t display_count=0;for(const auto& records:individuals)display_count+=records.size();
    for(const auto& job:jobs)display_count+=job.grouped.size();window.events.reserve(display_count);
    for(auto& records:individuals)for(auto& event:records)window.events.push_back(std::move(event));
    for(auto i:order) {
        Check(cancel);auto& job=jobs[i];auto& domain=tracks[job.track].gpu?window.gpu_totals:window.cpu_totals;
        window.gpu_details[job.track].Merge(job.gpu_detail);
        for(const auto& entry:job.totals) {
            auto add=[&](StoredTotal& total){total.count+=entry.second.count;total.unfinished+=entry.second.unfinished;
                total.inclusive+=entry.second.inclusive;total.exclusive+=entry.second.exclusive;};
            add(window.totals[entry.first]);add(domain[entry.first]);
        }
        window.blocks_read+=job.metrics.blocks_read;window.cache_hits+=job.metrics.cache_hits;
        window.records_scanned+=job.metrics.records_scanned;window.aggregated|=job.metrics.aggregated;
        for(auto& event:job.grouped)window.events.push_back(std::move(event));
    }
    return window;
}
std::vector<ComparisonValue> PagedTimingStore::Summary(const TimingWindow& window,const TimingModel& model) const {
    std::unordered_map<uint64_t,uint32_t> ids;std::unordered_map<std::string,uint32_t> names;
    for(uint32_t i=0;i<model.names.size();++i){if(model.timer_has_id[i])ids.emplace(model.timer_ids[i],i);else names.emplace(model.names[i],i);}
    std::vector<ComparisonValue> result;result.reserve(summary_groups_.size());
    for(const auto& group:summary_groups_) {
        ComparisonValue value;value.group=group.identity;value.interval=std::max(0.,window.stats_end-window.stats_start);
        const auto& totals=group.identity.key.gpu?window.gpu_totals:window.cpu_totals;
        for(auto i:group.timers) {
            const auto& timer=timers[i];const auto& total=totals[i];
            value.count+=static_cast<size_t>(total.count);value.unfinished+=static_cast<size_t>(total.unfinished);value.inclusive+=total.inclusive;
            value.exclusive=relations_valid?value.exclusive+std::max(0.,total.exclusive):NAN;
            if(timer.has_id){auto found=ids.find(timer.id);if(found!=ids.end())value.group.timers.push_back(found->second);}
            else {auto found=names.find(timer.name);if(found!=names.end())value.group.timers.push_back(found->second);}
        }
        std::sort(value.group.timers.begin(),value.group.timers.end());value.group.timers.erase(std::unique(value.group.timers.begin(),value.group.timers.end()),value.group.timers.end());
        result.push_back(std::move(value));}
    return result;
}
void PagedTimingStore::ApplyTotals(const TimingWindow& window,TimingModel& model) const {
    std::unordered_map<uint64_t,uint32_t> ids;std::unordered_map<std::string,uint32_t> names;
    for(uint32_t i=0;i<model.names.size();++i){if(model.timer_has_id[i])ids.emplace(model.timer_ids[i],i);else names.emplace(model.names[i],i);}
    model.totals.clear();model.aggregate_start=window.stats_start;model.aggregate_end=window.stats_end;
    for(size_t i=0;i<timers.size();++i) {
        const auto& timer=timers[i];const auto& total=window.totals[i];if(!total.count||timer.frame_type>=0)continue;
        uint32_t index=static_cast<uint32_t>(model.names.size());
        if(timer.has_id){auto n=ids.emplace(timer.id,index);index=n.first->second;}
        else {auto n=names.emplace(timer.name,index);index=n.first->second;}
        if(index==model.names.size()){model.names.push_back(timer.name);model.timer_ids.push_back(timer.id);model.timer_has_id.push_back(timer.has_id);}
        model.totals.push_back({index,static_cast<size_t>(total.count),total.inclusive,relations_valid?std::max(0.,total.exclusive):NAN});
    }
    std::sort(model.totals.begin(),model.totals.end(),[](const auto& x,const auto& y){return x.inclusive>y.inclusive;});
    for(const auto& track:tracks) {
        auto found=std::find_if(model.tracks.begin(),model.tracks.end(),[&](const auto& t){return track.has_id?t.has_id&&t.id==track.id:!t.has_id&&t.name==track.name;});
        if(found==model.tracks.end()){TimingTrack lane;lane.name=track.name;lane.id=track.id;lane.has_id=track.has_id;lane.gpu=TimingMatches(track.name,"gpu");lane.depths.resize(static_cast<size_t>(track.max_depth)+1);model.tracks.push_back(std::move(lane));}
        else found->depths.resize(std::max(found->depths.size(),static_cast<size_t>(track.max_depth)+1));
    }
    std::unordered_map<uint64_t,size_t> track_ids;std::unordered_map<std::string,size_t> track_names;
    for(size_t i=0;i<tracks.size();++i) {
        if(tracks[i].has_id)track_ids.emplace(tracks[i].id,i);else track_names.emplace(tracks[i].name,i);
    }
    auto ordinal=[&](const TimingTrack& track){return track.has_id?track_ids.at(track.id):track_names.at(track.name);};
    for(auto& track:model.tracks)if(ordinal(track)<window.gpu_details.size())track.gpu_detail_source=window.gpu_details[ordinal(track)];
    std::stable_sort(model.tracks.begin(),model.tracks.end(),[&](const auto& x,const auto& y){const auto a=TimingTrackRank(x),b=TimingTrackRank(y);return a!=b?a<b:ordinal(x)<ordinal(y);});
    model.start=start;model.end=end;
}
std::optional<TimingEvent> PagedTimingStore::Nearest(uint64_t id,bool has_id,const std::string& name,double anchor) const {
    std::optional<TimingEvent> best;double distance=INFINITY;
    std::vector<uint32_t> order;for(size_t t=0;t<timers.size();++t){const auto& timer=timers[t];if(timer.frame_type<0&&(has_id?(timer.has_id&&timer.id==id):timer.name==name))order.insert(order.end(),timer_blocks_[t].begin(),timer_blocks_[t].end());}
    std::sort(order.begin(),order.end());order.erase(std::unique(order.begin(),order.end()),order.end());
    auto gap=[&](uint32_t index){const auto& block=blocks_[index];return anchor<block.start?block.start-anchor:anchor>(block.unfinished?end:block.end)?anchor-(block.unfinished?end:block.end):0.;};
    std::sort(order.begin(),order.end(),[&](auto x,auto y){auto a=gap(x),b=gap(y);return a!=b?a<b:x<y;});
    std::ifstream in;TimingWindow metrics;
    for(auto index:order){auto* block=&blocks_[index];if(gap(index)>distance)break;auto records=Records(index,in,metrics);
        for(const auto& r:*records){const auto& timer=timers[r.timer];if(r.unfinished||timer.frame_type>=0||(has_id?(!timer.has_id||timer.id!=id):timer.name!=name))continue;
            double delta=anchor<r.start?r.start-anchor:anchor>r.end?anchor-r.end:0.;
            if(delta<distance){const auto& track=tracks[block->track];TimingEvent e{track.name,timer.name,r.start,r.end,block->depth};e.thread_id=track.id;e.has_thread_id=track.has_id;e.timer_id=timer.id;e.has_timer_id=timer.has_id;best=e;distance=delta;}}
    }return best;
}
}
