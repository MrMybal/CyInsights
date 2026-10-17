// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "UiMetrics.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <imgui.h>
#include <imgui_internal.h>
namespace cyinsights {
namespace {
void Activate(bool& focus){if(!focus)return;auto* w=ImGui::GetCurrentWindow();if(w->DockNode&&w->DockNode->TabBar){w->DockNode->SelectedTabId=w->TabId;w->DockNode->TabBar->NextSelectedTabId=w->TabId;}focus=false;}
void Milliseconds(double seconds){if(std::isfinite(seconds))ImGui::Text("%.4f",seconds*1000);else ImGui::TextDisabled("Unavailable");}
const char* domains[]={"GameThread","RenderThread","GPU"};
}
Application::InteractionState Application::InteractionView() const {
    InteractionState state;state.scope_x=scope_probe_x_;state.scope_y=scope_probe_y_;state.scope_name=scope_probe_name_;
    state.menu_x=context_hist_x_;state.menu_y=context_hist_y_;state.hist_x=hist_probe_x_;state.hist_y=hist_probe_y_;state.hist_width=hist_probe_width_;
    state.blank_x=timing_blank_x_;state.blank_y=timing_blank_y_;
    state.filter_name=timing_exact_name_;state.function_name=distribution_result_function_;state.selection_a=range_start_;state.selection_b=range_end_;
    state.hist_a=distribution_a_;state.hist_b=distribution_b_;state.count=distribution_.count;state.matched=distribution_.matched;state.bin=distribution_.selected_bin;state.bins=distribution_.bins;
    auto* timing=ImGui::FindWindowByName(WindowName("Timing").c_str());auto* distribution=ImGui::FindWindowByName(WindowName("Distributions").c_str());
    state.separate_dock=timing&&distribution&&timing->DockId&&distribution->DockId&&timing->DockId!=distribution->DockId;return state;
}
void Application::FilterFunction(const std::string& name) {
    timing_exact_name_=name;timing_search_version_=UINT64_MAX;
    // A second click is a selection, not a zoom or a new drag.
    drag_surface_=DragSurface::None;drag_released_surface_=DragSurface::None;
}
void Application::OpenFunctionHistogram(const std::string& name) {
    if(!native_.store||name.empty())return;
    distribution_source_=1;distribution_track_=-1;distribution_roots_=false;distribution_function_=name;
    follow_timeline_selection_=true;analysis_whole_=range_start_==range_end_;
    analysis_a_=analysis_whole_?native_.store->start:std::min(range_start_,range_end_);
    analysis_b_=analysis_whole_?native_.store->end:std::max(range_start_,range_end_);
    observed_selection_a_=range_start_;observed_selection_b_=range_end_;
    distribution_cancel_.store(true);distribution_pending_=true;distribution_dirty_=true;selection_changed_at_=ImGui::GetTime()-.2;
    histogram_dock_pending_=true;FocusDistribution();
}
void Application::DockFunctionHistogram() {
    if(!histogram_dock_pending_)return;histogram_dock_pending_=false;
    auto* timing=ImGui::FindWindowByName(WindowName("Timing").c_str());auto* distribution=ImGui::FindWindowByName(WindowName("Distributions").c_str());
    if(!timing||!timing->DockId||(distribution&&distribution->DockId&&distribution->DockId!=timing->DockId))return;
    auto* node=ImGui::DockBuilderGetNode(timing->DockId);if(!node||!node->IsLeafNode())return;
    auto* root_node=node;while(root_node->ParentNode)root_node=root_node->ParentNode;auto root=root_node->ID;ImGuiID bottom=0,top=0;
    ImGui::DockBuilderSplitNode(node->ID,ImGuiDir_Down,.42f,&bottom,&top);
    ImGui::DockBuilderDockWindow(WindowName("Distributions").c_str(),bottom);ImGui::DockBuilderFinish(root);
}
void Application::FollowTimelineSelection() {
    if(!native_.store)return;
    if(follow_timeline_selection_&&(observed_selection_a_!=range_start_||observed_selection_b_!=range_end_)){
        observed_selection_a_=range_start_;observed_selection_b_=range_end_;
        analysis_whole_=range_start_==range_end_;analysis_a_=analysis_whole_?native_.store->start:std::min(range_start_,range_end_);
        analysis_b_=analysis_whole_?native_.store->end:std::max(range_start_,range_end_);
        selection_changed_at_=ImGui::GetTime();distribution_dirty_=true;
        distribution_pending_=show_distribution_;tasks_pending_=show_tasks_&&tasks_auto_started_;
    }
    // Keep the grab and old picture during a range drag. Only the settled
    // selection schedules work; a completed obsolete result is discarded.
    if(ImGui::IsMouseDown(0)||ImGui::GetTime()-selection_changed_at_<.12)return;
    if(distribution_pending_&&!distribution_task_.valid()){distribution_pending_=false;StartDistribution();}
    if(tasks_pending_&&!tasks_query_.valid()&&!task_reader_.Busy()){tasks_pending_=false;StartTasks();}
}
void Application::FocusInterval(double a,double b,uint32_t thread,bool native_identity) {
    if(!std::isfinite(a)||!std::isfinite(b)||b<a)return;
    if(b==a)b=a+.000001;range_start_=a;range_end_=b;
    double pad=std::max(.00001,(b-a)*.15);view_start_=std::max(std::min(timing_model_.start,a),a-pad);view_end_=std::min(std::max(timing_model_.end,b),b+pad);
    selected_cpu_=selected_timer_=-1;show_timing_=true;
    for(auto& track:timing_model_.tracks)if(native_identity&&track.has_id&&track.id==thread){track.visible=true;track.collapsed=false;}
    auto* w=ImGui::FindWindowByName(WindowName("Timing").c_str());if(w&&w->DockNode&&w->DockNode->TabBar){w->DockNode->SelectedTabId=w->TabId;w->DockNode->TabBar->NextSelectedTabId=w->TabId;}
    ImGui::SetWindowFocus(WindowName("Timing").c_str());
}
bool Application::InspectFirstTask() {
    if(!tasks_store_||tasks_result_candidates_||links_query_.valid())return false;
    for(const auto& record:tasks_.rows)if(std::isfinite(record.Duration(TaskPhase::Started,TaskPhase::Finished))){
        selected_task_=record;auto store=tasks_store_;auto id=record.id;links_request_id_=id;links_cancel_.store(false);
        links_query_=std::async(std::launch::async,[this,store,id]{return store->Links(id,&links_cancel_);});return true;
    }
    return false;
}
void Application::WriteAnalysisReport(const fs::path& output) const {
    size_t limited_gpu_tracks=0;
    for(const auto& track:timing_model_.tracks)if(track.gpu&&track.gpu_detail.Limited())++limited_gpu_tracks;
    uint64_t stat_metrics=0,stat_samples=0,stat_bounds=0,stat_delayed=0,stat_execution_unknown=0;
    for(const auto& metric:counters_->series)if(metric.frame_stats){++stat_metrics;stat_samples+=metric.frame_samples.size();for(const auto& sample:metric.frame_samples){stat_bounds+=(sample.flags&cyfs::FrameBounds)!=0;stat_delayed+=(sample.flags&cyfs::Delayed)!=0;stat_execution_unknown+=(sample.flags&cyfs::ExecutionUnknown)!=0;}}
    std::ofstream out(output);out<<std::setprecision(17)<<std::boolalpha;
    out<<"{\"timing_events\":"<<(native_.store?native_.store->count:0)
       <<",\"native_setup_ready\":"<<(native_setup_.state==NativeSetup::State::Ready)
       <<",\"native_setup_status\":"<<cygi::Json(native_setup_.message).Write(-1)
       <<",\"native_timing_backend\":\""<<(native_.DirectTiming()?"direct_binary":"csv_bridge")<<"\""
       <<",\"native_decode_seconds\":"<<native_.DecodeSeconds()<<",\"native_index_seconds\":"<<native_.IndexSeconds()
       <<",\"native_timing_bytes\":"<<native_.ExportBytes()<<",\"native_counter_catalog\":"<<counters_->series.size()
       <<",\"gpu_timing_detail_limited_tracks\":"<<limited_gpu_tracks
       <<",\"vulkan_subpass_records\":"<<(native_.vulkan_contents?native_.vulkan_contents->count:0)
       <<",\"frame_stat_metrics\":"<<stat_metrics<<",\"frame_stat_samples\":"<<stat_samples<<",\"frame_stat_source_bounds\":"<<stat_bounds
       <<",\"frame_stat_delayed_samples\":"<<stat_delayed<<",\"frame_stat_unknown_execution_frame\":"<<stat_execution_unknown
       <<",\"selection_start\":"<<range_start_<<",\"selection_end\":"<<range_end_<<",\"view_start\":"<<view_start_<<",\"view_end\":"<<view_end_
       <<",\"activity_events\":"<<(activity_store_?activity_store_->count:0)
       <<",\"activity_lanes\":"<<(activity_store_?activity_store_->lanes.size():0)
       <<",\"activity_status\":"<<cygi::Json(activity_message_).Write(-1)
       <<",\"activity_visible_lanes\":"<<activity_result_.visible.lanes.size()
       <<",\"activity_plot_queries\":"<<activity_plot_queries_<<",\"activity_detail_queries\":"<<activity_detail_queries_
       <<",\"activity_range_start\":"<<activity_result_.details.start<<",\"activity_range_end\":"<<activity_result_.details.end
       <<",\"activity_matches\":"<<(activity_result_.details.lanes.empty()?0:activity_result_.details.lanes.front().matched)
       <<",\"exact_function_filter\":"<<cygi::Json(timing_exact_name_).Write(-1)<<",\"histogram_function\":"<<cygi::Json(distribution_result_function_).Write(-1)
       <<",\"histogram_range_start\":"<<distribution_a_<<",\"histogram_range_end\":"<<distribution_b_
       <<",\"distribution_count\":"<<distribution_.count<<",\"distribution_unavailable\":"<<distribution_.unavailable
       <<",\"distribution_exact_quantiles\":"<<distribution_.exact_percentiles<<",\"bins\":[";
    for(size_t i=0;i<Distribution::bin_count;++i){if(i)out<<',';out<<distribution_.bins[i];}
    out<<"],\"percentiles\":[";for(size_t i=0;i<4;++i){if(i)out<<',';out<<distribution_.percentiles[i];}
    out<<"],\"native_tasks\":"<<(tasks_store_?tasks_store_->count:0)<<",\"task_events\":"<<(tasks_store_?tasks_store_->events:0)
       <<",\"task_links\":"<<(tasks_store_?tasks_store_->relations:0)<<",\"task_candidates\":"<<tasks_result_candidates_
       <<",\"task_matches\":"<<tasks_.matched<<",\"task_rows\":"<<tasks_.rows.size()<<",\"selected_task\":";
    if(selected_task_){out<<"{\"id\":"<<selected_task_->id<<",\"phases\":[";for(size_t i=0;i<7;++i){if(i)out<<',';auto t=selected_task_->time[i];if(std::isfinite(t))out<<t;else out<<"null";}out<<"],\"links\":"<<task_links_.size()<<'}';}else out<<"null";out<<'}';
}
void Application::StartDistribution(int bin,uint64_t page) {
    if(!native_.store||distribution_task_.valid())return;
    if(!std::isfinite(analysis_a_)||!std::isfinite(analysis_b_)){analysis_a_=native_.store->start;analysis_b_=native_.store->end;}
    auto store=native_.store;auto tasks=tasks_store_;const int source=distribution_source_,domain=distribution_domain_,metric=distribution_metric_,track=distribution_track_;
    if(analysis_whole_&&source==2&&tasks){analysis_a_=std::isfinite(tasks->start)?std::min(store->start,tasks->start):store->start;analysis_b_=std::nextafter(std::max(store->end,tasks->end),INFINITY);}
    if(source==2&&!tasks){distribution_message_="Load native tasks in Tasks first";return;}
    if(source==1&&(track<-1||(track>=0&&static_cast<size_t>(track)>=store->tracks.size())))return;
    const double a=analysis_a_,b=analysis_b_;const bool roots=distribution_roots_,log_x=distribution_log_x_;const auto task_thread=distribution_task_thread_;
    const auto function=distribution_function_;distribution_result_function_=function;
    std::ostringstream key;key<<std::setprecision(17)<<source<<','<<domain<<','<<metric<<','<<track<<','<<a<<','<<b<<','<<roots<<','<<log_x<<','<<task_thread<<','<<function;
    auto summary=distribution_summary_key_==key.str()?distribution_summary_:std::nullopt;
    if(bin<0&&summary&&!distribution_dirty_&&summary->selected_bin<0){distribution_=*summary;distribution_message_="Ready; reused complete analysis";return;}
    distribution_request_key_=key.str();
    distribution_cancel_.store(false);distribution_result_source_=source;distribution_result_track_=track;distribution_a_=a;distribution_b_=b;
    distribution_label_=source==0?std::string(domains[domain])+" per game interval / "+(metric==0?"observed total":metric==1?"excluding recognized waits":"recognized waits"):
        source==1?(track<0?std::string("All threads"):store->tracks[track].name)+(function.empty()?"":" / "+function)+(roots?" / depth 0 scopes":" / all scopes (inclusive samples)"):
        std::string("Native tasks / ")+(metric==0?"execution":metric==1?"scheduled-to-start queue delay":"launched-to-scheduled dependency delay");
    distribution_message_="Preparing all samples in background...";distribution_dirty_=false;distribution_={};
    if(source==2&&task_thread>=0)distribution_label_+=" / thread "+std::to_string(task_thread);
    distribution_task_=std::async(std::launch::async,[this,store,tasks,source,domain,metric,track,a,b,roots,bin,page,task_thread,log_x,function,summary]{
        std::vector<uint8_t> matches;if(source==1&&!function.empty())matches=store->FunctionMask(function);
        SampleVisitor visitor=[&](const SampleConsumer& consume){
            if(source==0){for(size_t i=0;i<store->frames.size();++i){auto f=store->frames[i];if(f.first<a||f.first>=b)continue;
                auto w=store->frame_diagnostics[i].work[domain];double value=metric==0?w.total:metric==1?w.active:w.wait;
                bool valid=w.available&&!w.incomplete&&(domain!=2||metric!=2);consume({value,f.first,f.second,i,0,valid});}}
            else if(source==1)store->VisitScopes(track<0?SIZE_MAX:static_cast<size_t>(track),a,b,roots,[&](const StoredScope& s){consume({s.end-s.start,s.start,s.end,0,s.timer,!s.unfinished});},&distribution_cancel_,matches.empty()?nullptr:&matches);
            else tasks->Visit([&](const TaskRecord& r){
                if(task_thread>=0&&r.thread[3]!=static_cast<uint32_t>(task_thread))return;
                auto first=metric==0?TaskPhase::Started:metric==1?TaskPhase::Scheduled:TaskPhase::Launched;
                auto last=metric==0?TaskPhase::Finished:metric==1?TaskPhase::Started:TaskPhase::Scheduled;
                double start=r.time[static_cast<size_t>(first)],end=r.time[static_cast<size_t>(last)];if(!std::isfinite(start)||start<a||start>=b)return;
                auto value=r.Duration(first,last);consume({value,start,end,r.id,r.name,std::isfinite(value)&&!(r.flags&2)});
            },&distribution_cancel_);
        };
        return bin>=0&&summary?SelectDistributionSamples(visitor,*summary,bin,page,&distribution_cancel_):BuildDistribution(visitor,source==0,bin,page,&distribution_cancel_,log_x);
    });
}
void Application::StartNativeTasks() {
    if(task_reader_.Busy()||trace_.empty()||!native_.store)return;
    tasks_auto_started_=true;
    try {
        auto analyzer=fs::u8path(exe_);auto binaries=analyzer.parent_path();std::ifstream version(binaries.parent_path().parent_path()/"Build/Build.version");
        std::string text((std::istreambuf_iterator<char>(version)),{});cygi::Json metadata;if(!cygi::Json::Parse(text,metadata))throw std::runtime_error("Selected analyzer installation has no readable Build.version");
        int minor=static_cast<int>(metadata["MinorVersion"].AsNumber());if(metadata["MajorVersion"].AsNumber()!=5||minor<5||minor>8)throw std::runtime_error("Native task adapter requires an installed UE 5.5-5.8 analyzer");
        auto helper=FindNativeReader("Tasks",analyzer);
        if(task_reader_.Start(helper,binaries,trace_,cache_/"Tasks",native_.store->end,tasks_message_))tasks_message_="Decoding native task lifecycle...";
    }catch(const std::exception& e){tasks_message_=e.what();}
}
void Application::StartTasks(uint64_t page) {
    if(!native_.store||tasks_query_.valid())return;
    if(!std::isfinite(analysis_a_)||!std::isfinite(analysis_b_)){analysis_a_=native_.store->start;analysis_b_=native_.store->end;}
    auto native=native_.store;auto tasks=tasks_store_;bool candidates=tasks_candidates_||!tasks||!tasks->count;
    if(analysis_whole_&&!candidates){analysis_a_=std::isfinite(tasks->start)?std::min(native->start,tasks->start):native->start;analysis_b_=std::nextafter(std::max(native->end,tasks->end),INFINITY);}
    auto search=std::string(tasks_search_);double a=analysis_a_,b=analysis_b_;auto thread=tasks_thread_;
    tasks_page_=page;tasks_result_candidates_=candidates;tasks_cancel_.store(false);tasks_={};selected_task_.reset();task_links_.clear();
    tasks_query_=std::async(std::launch::async,[this,native,tasks,candidates,search,a,b,thread,page]{
        return candidates?TaskStore{}.ScopeCandidates(*native,a,b,thread,search,page,&tasks_cancel_):tasks->Query(a,b,thread,search,page,&tasks_cancel_);
    });
}
void Application::PollTraceAnalysis() {
    if(distribution_task_.valid()&&distribution_task_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
        try{auto result=distribution_task_.get();if(!distribution_pending_){distribution_=std::move(result);if(distribution_.selected_bin<0){distribution_summary_=distribution_;distribution_summary_key_=distribution_request_key_;}distribution_message_=distribution_.count?"Ready; all valid samples counted":"No valid samples; missing data is not measured zero";}}catch(const std::exception& e){distribution_message_=e.what();}}
    if(task_reader_.Poll(tasks_store_,tasks_message_)) {
        if(tasks_store_){tasks_candidates_=tasks_store_->count==0;tasks_thread_=-1;selected_task_.reset();task_links_.clear();if(!tasks_query_.valid())StartTasks();}
    }
    if(tasks_query_.valid()&&tasks_query_.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto result=tasks_query_.get();if(!tasks_pending_)tasks_=std::move(result);}catch(const std::exception& e){tasks_message_=e.what();}}
    if(links_query_.valid()&&links_query_.wait_for(std::chrono::seconds(0))==std::future_status::ready){try{auto links=links_query_.get();if(selected_task_&&selected_task_->id==links_request_id_&&!tasks_result_candidates_)task_links_=std::move(links);}catch(const std::exception& e){tasks_message_=e.what();}}
}
void Application::DistributionView() {
    bool visible=BeginView("Distributions",&show_distribution_);Activate(focus_distribution_);if(!visible){ImGui::End();return;}
    if(!native_.store){ImGui::TextWrapped("Open a trace and wait for its native timing index.");ImGui::End();return;}
    if(distribution_label_.empty()&&!distribution_task_.valid())StartDistribution();
    float scale=UiScale();ImGui::BeginDisabled(distribution_task_.valid());
    const char* sources[]={"Per-frame thread cost","Native scope durations","Native task durations"};
    ImGui::SetNextItemWidth(220*scale);if(ImGui::Combo("Samples",&distribution_source_,sources,3)){distribution_dirty_=true;distribution_metric_=distribution_source_==2?0:1;distribution_function_.clear();}
    if(distribution_source_==0) {
        ImGui::SameLine();ImGui::SetNextItemWidth(135*scale);distribution_dirty_|=ImGui::Combo("Thread domain",&distribution_domain_,domains,3);
        const char* metrics[]={"Observed total","Excluding recognized waits","Recognized waits"};ImGui::SameLine();ImGui::SetNextItemWidth(205*scale);distribution_dirty_|=ImGui::Combo("Cost",&distribution_metric_,metrics,3);
    }else if(distribution_source_==1) {
        if(native_.store->tracks.empty()){ImGui::TextDisabled("No native tracks available");ImGui::EndDisabled();ImGui::End();return;}
        if(distribution_track_>=static_cast<int>(native_.store->tracks.size()))distribution_track_=0;
        ImGui::SameLine();ImGui::SetNextItemWidth(240*scale);
        if(ImGui::BeginCombo("Thread",distribution_track_<0?"All threads":native_.store->tracks[distribution_track_].name.c_str())){if(ImGui::Selectable("All threads",distribution_track_<0)){distribution_track_=-1;distribution_dirty_=true;}for(size_t i=0;i<native_.store->tracks.size();++i){auto& t=native_.store->tracks[i];auto label=t.name+(t.has_id?" ["+std::to_string(t.id)+"]":"");if(ImGui::Selectable(label.c_str(),static_cast<int>(i)==distribution_track_)){distribution_track_=static_cast<int>(i);distribution_dirty_=true;}}ImGui::EndCombo();}
        ImGui::SameLine();distribution_dirty_|=ImGui::Checkbox("Depth 0 only",&distribution_roots_);
    }else {
        const char* metrics[]={"Execution","Queue delay","Dependency delay"};ImGui::SameLine();distribution_dirty_|=ImGui::Combo("Phase",&distribution_metric_,metrics,3);
        if(tasks_store_){ImGui::SameLine();ImGui::SetNextItemWidth(200*scale);auto label=distribution_task_thread_<0?"All execution threads":std::to_string(distribution_task_thread_);if(ImGui::BeginCombo("Task thread",label.c_str())){if(ImGui::Selectable("All execution threads",distribution_task_thread_<0)){distribution_task_thread_=-1;distribution_dirty_=true;}for(const auto& t:tasks_store_->threads){auto name=t.second+" ["+std::to_string(t.first)+"]";if(ImGui::Selectable(name.c_str(),distribution_task_thread_==t.first)){distribution_task_thread_=t.first;distribution_dirty_=true;}}ImGui::EndCombo();}}
        if(!tasks_store_){ImGui::SameLine();if(ImGui::Button("Open Tasks"))FocusTasks();}
    }
    if(!distribution_function_.empty()){ImGui::TextWrapped("Exact function: %s",distribution_function_.c_str());ImGui::SameLine();if(ImGui::SmallButton("Clear function")){distribution_function_.clear();distribution_dirty_=true;}}
    if(ImGui::Button("Whole capture")){follow_timeline_selection_=false;analysis_whole_=true;analysis_a_=native_.store->start;analysis_b_=native_.store->end;distribution_dirty_=true;}
    ImGui::SameLine();if(ImGui::Button("Use selected interval")){follow_timeline_selection_=true;observed_selection_a_=range_start_;observed_selection_b_=range_end_;analysis_whole_=false;analysis_a_=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);analysis_b_=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);distribution_dirty_=true;}
    ImGui::SameLine();if(ImGui::Checkbox("Follow timeline selection",&follow_timeline_selection_)){observed_selection_a_=observed_selection_b_=NAN;}
    ImGui::SameLine();distribution_dirty_|=ImGui::Checkbox("Log durations",&distribution_log_x_);
    ImGui::SameLine();if(ImGui::Button("Analyze"))StartDistribution();ImGui::EndDisabled();
    ImGui::SameLine();ImGui::Checkbox("Log counts",&distribution_log_y_);
    if(distribution_task_.valid()){ImGui::SameLine();if(ImGui::Button("Cancel"))distribution_cancel_.store(true);ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()),ImVec2(-1,12*scale),"Preparing distribution...");}
    ImGui::TextWrapped("%s",distribution_message_.c_str());if(distribution_dirty_)ImGui::TextColored(ImVec4(1,.7f,.3f,1),distribution_pending_?"Updating from timeline selection... Previous result keeps its original interval.":"Settings changed; press Analyze. Existing result keeps its original interval.");
    ImGui::TextWrapped("%s | %.6f - %.6f s",distribution_label_.c_str(),distribution_a_,distribution_b_);
    if(!distribution_.count){ImGui::Text("Unavailable/incomplete samples: %llu",distribution_.unavailable);ImGui::End();return;}
    ImGui::Text("%llu samples | %llu unavailable/incomplete | min %.3f | mean %.3f | max %.3f ms",distribution_.count,distribution_.unavailable,distribution_.min*1000,distribution_.mean*1000,distribution_.max*1000);
    ImGui::Text("P50%s %.3f   P90%s %.3f   P95%s %.3f   P99%s %.3f ms",distribution_.exact_percentiles?"":"~",distribution_.percentiles[0]*1000,distribution_.exact_percentiles?"":"~",distribution_.percentiles[1]*1000,distribution_.exact_percentiles?"":"~",distribution_.percentiles[2]*1000,distribution_.exact_percentiles?"":"~",distribution_.percentiles[3]*1000);
    if(ImGui::IsItemHovered()){ImGui::BeginTooltip();ImGui::TextUnformatted(distribution_.exact_percentiles?"Exact nearest-rank quantiles of all valid frame samples":"Estimated quantiles; the exact value is bounded by the histogram bin:");for(size_t q=0;q<4;++q)ImGui::Text("P%d: [%.6f, %.6f] ms",q==0?50:q==1?90:q==2?95:99,distribution_.percentile_bounds[q].first*1000,distribution_.percentile_bounds[q].second*1000);ImGui::EndTooltip();}
    auto p=ImGui::GetCursorScreenPos();float width=std::max(1.f,ImGui::GetContentRegionAvail().x),height=175*scale;
    hist_probe_x_=p.x;hist_probe_y_=p.y;hist_probe_width_=width;
    ImGui::InvisibleButton("histogram",ImVec2(width,height));bool hover=ImGui::IsItemHovered();auto* draw=ImGui::GetWindowDrawList();draw->AddRectFilled(p,ImVec2(p.x+width,p.y+height),IM_COL32(48,48,48,255));
    uint64_t largest=*std::max_element(distribution_.bins.begin(),distribution_.bins.end()),cumulative=0;ImVec2 previous(p.x,p.y+height-22*scale);int clicked=-1;
    for(size_t bin=0;bin<Distribution::bin_count;++bin) {
        auto n=distribution_.bins[bin];float left=p.x+width*static_cast<float>(bin)/Distribution::bin_count,right=p.x+width*static_cast<float>(bin+1)/Distribution::bin_count;
        double fraction=distribution_log_y_?std::log1p(double(n))/std::log1p(double(largest)):double(n)/double(largest);
        float base=p.y+height-22*scale;draw->AddRectFilled(ImVec2(left,base-(height-38*scale)*static_cast<float>(fraction)),ImVec2(std::max(left+1,right-1),base),distribution_.selected_bin==static_cast<int>(bin)?IM_COL32(226,177,93,255):IM_COL32(104,169,227,255));
        cumulative+=n;ImVec2 current(right,base-(height-38*scale)*static_cast<float>(double(cumulative)/distribution_.count));draw->AddLine(previous,current,IM_COL32(233,210,128,255),1.5f*scale);previous=current;
        if(hover&&ImGui::IsMouseHoveringRect(ImVec2(left,p.y),ImVec2(right,p.y+height))) {auto bounds=distribution_.Bounds(bin);ImGui::SetTooltip("%.6f - %.6f ms%s\n%llu samples | %.2f%% | cumulative %.2f%%\nClick to browse every sample in this bin",bounds.first*1000,bounds.second*1000,(distribution_.min==distribution_.max||bin+1==Distribution::bin_count)?" (upper inclusive)":" (upper exclusive)",n,double(n)*100/distribution_.count,double(cumulative)*100/distribution_.count);
            if(ImGui::IsMouseClicked(0)&&!distribution_task_.valid()&&!distribution_dirty_)clicked=static_cast<int>(bin);}
    }
    char label[80];std::snprintf(label,sizeof(label),"%.3f ms",distribution_.min*1000);draw->AddText(ImVec2(p.x+4*scale,p.y+height-18*scale),IM_COL32(220,220,220,255),label);
    std::snprintf(label,sizeof(label),"%.3f ms | CDF 100%%",distribution_.max*1000);draw->AddText(ImVec2(p.x+width-ImGui::CalcTextSize(label).x-4*scale,p.y+height-18*scale),IM_COL32(233,210,128,255),label);
    ImGui::TextDisabled("Each sample starts in the analysis interval; its complete duration is used. Missing endpoints are excluded. Scope samples are inclusive, not summed thread utilization.");
    if(distribution_.selected_bin<0)ImGui::Text("Slowest %zu of %llu samples; click a bin for all its values",distribution_.examples.size(),distribution_.count);
    else {ImGui::Text("Bin %d | %llu values | page %llu",distribution_.selected_bin,distribution_.matched,distribution_.page+1);
        ImGui::SameLine();ImGui::BeginDisabled(distribution_task_.valid());if(ImGui::Button("Slowest values"))StartDistribution();ImGui::SameLine();if(ImGui::Button("Previous")&&distribution_.page)StartDistribution(distribution_.selected_bin,distribution_.page-1);ImGui::SameLine();if(ImGui::Button("Next")&&distribution_.page+1<(distribution_.matched+127)/128)StartDistribution(distribution_.selected_bin,distribution_.page+1);ImGui::EndDisabled();}
    if(ImGui::BeginTable("distribution_values",4,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_ScrollY,ImVec2(0,std::max(70.f*scale,ImGui::GetContentRegionAvail().y)))) {
        for(auto title:{"Sample","Start (s)","End (s)","Duration (ms)"})ImGui::TableSetupColumn(title);ImGui::TableHeadersRow();
        for(size_t i=0;i<distribution_.examples.size();++i){auto s=distribution_.examples[i];std::string name;
            if(distribution_result_source_==0)name=(native_.store->frames_native?"Frame ":"Tick candidate ")+std::to_string(native_.store->frames_native?native_.store->frame_ids[s.index]:s.index);
            else if(distribution_result_source_==1)name=native_.store->timers[s.timer].name;
            else name="Task "+std::to_string(s.index)+(tasks_store_?" / "+tasks_store_->names[s.timer]:"");
            ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableNextColumn();if(ImGui::Selectable(name.c_str(),false,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)&&ImGui::IsMouseDoubleClicked(0))FocusInterval(s.start,s.end,0,false);
            ImGui::TableNextColumn();ImGui::Text("%.6f",s.start);ImGui::TableNextColumn();ImGui::Text("%.6f",s.end);ImGui::TableNextColumn();Milliseconds(s.value);ImGui::PopID();}
        ImGui::EndTable();}
    if(clicked>=0)StartDistribution(clicked);
    ImGui::End();
}
void Application::TasksView() {
    bool visible=BeginView("Tasks",&show_tasks_);Activate(focus_tasks_);if(!visible){ImGui::End();return;}
    if(!native_.store){ImGui::TextWrapped("Open a trace first.");ImGui::End();return;}
    if(!tasks_auto_started_)StartNativeTasks();
    if(!tasks_store_&&!task_reader_.Busy()&&tasks_message_.empty())tasks_message_="Native tasks unavailable; CPU scopes can be inspected as candidates";
    ImGui::BeginDisabled(task_reader_.Busy()||tasks_query_.valid()||links_query_.valid());
    if(ImGui::Button("Load native task data"))StartNativeTasks();ImGui::SameLine();if(ImGui::Checkbox("CPU task scopes (candidates)",&tasks_candidates_)){tasks_thread_=-1;selected_task_.reset();task_links_.clear();}
    ImGui::SetNextItemWidth(220*UiScale());ImGui::InputTextWithHint("##task_search","Search task name / native ID",tasks_search_,sizeof(tasks_search_));ImGui::SameLine();
    std::string preview="All execution threads";
    if(tasks_thread_>=0){if(tasks_candidates_||!tasks_store_||!tasks_store_->count){if(static_cast<size_t>(tasks_thread_)<native_.store->tracks.size())preview=native_.store->tracks[static_cast<size_t>(tasks_thread_)].name;}
        else {auto t=tasks_store_->threads.find(static_cast<uint32_t>(tasks_thread_));preview=t!=tasks_store_->threads.end()?t->second:std::to_string(tasks_thread_);}}
    if(ImGui::BeginCombo("Thread",preview.c_str())) {if(ImGui::Selectable("All execution threads",tasks_thread_<0))tasks_thread_=-1;
        if(tasks_candidates_||!tasks_store_||!tasks_store_->count){for(size_t i=0;i<native_.store->tracks.size();++i)if(!native_.store->tracks[i].gpu){auto label=native_.store->tracks[i].name+" ["+std::to_string(i)+"]";if(ImGui::Selectable(label.c_str(),tasks_thread_==static_cast<int64_t>(i)))tasks_thread_=static_cast<int64_t>(i);}}
        else for(const auto& t:tasks_store_->threads){auto label=t.second+" ["+std::to_string(t.first)+"]";if(ImGui::Selectable(label.c_str(),tasks_thread_==t.first))tasks_thread_=t.first;}
        ImGui::EndCombo();}
    if(ImGui::Button("Whole capture")){follow_timeline_selection_=false;analysis_whole_=true;analysis_a_=native_.store->start;analysis_b_=native_.store->end;}
    ImGui::SameLine();if(ImGui::Button("Use selected interval")){follow_timeline_selection_=true;observed_selection_a_=range_start_;observed_selection_b_=range_end_;analysis_whole_=false;analysis_a_=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);analysis_b_=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);}
    ImGui::SameLine();if(ImGui::Checkbox("Follow timeline selection",&follow_timeline_selection_))observed_selection_a_=observed_selection_b_=NAN;
    ImGui::SameLine();if(ImGui::Button("Find tasks"))StartTasks();ImGui::EndDisabled();
    if(task_reader_.Busy()||tasks_query_.valid()||links_query_.valid()){ImGui::ProgressBar(-static_cast<float>(ImGui::GetTime()),ImVec2(-1,12*UiScale()),task_reader_.Busy()?task_reader_.Phase()==1?"Decoding native task events...":"Indexing tasks on disk...":"Querying tasks...");if(ImGui::Button("Cancel task analysis")){task_reader_.Cancel();tasks_cancel_.store(true);links_cancel_.store(true);}}
    ImGui::TextWrapped("%s",tasks_message_.c_str());
    if(tasks_pending_)ImGui::TextDisabled("Updating from timeline selection after the drag...");
    ImGui::TextDisabled(tasks_result_candidates_?"CPU scope candidates: measured timings; native task ID, queue delays and dependencies unavailable.":"Native IDs are capture-local. Execution/pending overlap is filtered; missing lifecycle endpoints stay unavailable.");
    ImGui::Text("%llu matches | page %llu",tasks_.matched,tasks_page_+1);ImGui::SameLine();ImGui::BeginDisabled(tasks_query_.valid());if(ImGui::Button("Previous")&&tasks_page_)StartTasks(tasks_page_-1);ImGui::SameLine();if(ImGui::Button("Next")&&tasks_page_+1<(tasks_.matched+127)/128)StartTasks(tasks_page_+1);ImGui::EndDisabled();
    float detail=selected_task_?std::min(220.f*UiScale(),ImGui::GetContentRegionAvail().y*.4f):0;
    if(ImGui::BeginTable("task_table",6,ImGuiTableFlags_RowBg|ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_ScrollY,ImVec2(0,std::max(80.f*UiScale(),ImGui::GetContentRegionAvail().y-detail)))) {
        for(auto title:{"Identity / name","Execution thread","Start (s)","Execute (ms)","Queue (ms)","Dependency (ms)"})ImGui::TableSetupColumn(title);ImGui::TableHeadersRow();
        for(size_t i=0;i<tasks_.rows.size();++i){const auto& r=tasks_.rows[i];std::string name=tasks_result_candidates_?"Scope candidate / "+native_.store->timers[r.name].name:"Task "+std::to_string(r.id)+" / "+tasks_store_->names[r.name];
            ImGui::PushID(static_cast<int>(i));ImGui::TableNextRow();ImGui::TableNextColumn();if(ImGui::Selectable(name.c_str(),selected_task_&&selected_task_->id==r.id,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)){selected_task_=r;task_links_.clear();if(ImGui::IsMouseDoubleClicked(0))FocusInterval(r.time[3],r.time[4],r.thread[3],!tasks_result_candidates_);}
            ImGui::TableNextColumn();auto tid=r.thread[3];if(tid==UINT32_MAX)ImGui::TextDisabled("Unavailable");else if(tasks_result_candidates_)ImGui::TextUnformatted(native_.store->tracks[tid].name.c_str());else {auto t=tasks_store_->threads.find(tid);ImGui::Text("%s [%u]",t==tasks_store_->threads.end()?"Unknown":t->second.c_str(),tid);}
            ImGui::TableNextColumn();if(std::isfinite(r.time[3]))ImGui::Text("%.6f",r.time[3]);else ImGui::TextDisabled("Pending / missing");
            ImGui::TableNextColumn();Milliseconds(r.Duration(TaskPhase::Started,TaskPhase::Finished));ImGui::TableNextColumn();Milliseconds(r.Duration(TaskPhase::Scheduled,TaskPhase::Started));ImGui::TableNextColumn();Milliseconds(r.Duration(TaskPhase::Launched,TaskPhase::Scheduled));ImGui::PopID();}
        ImGui::EndTable();}
    if(selected_task_) {
        const auto& r=*selected_task_;ImGui::Separator();
        if(ImGui::Button("Show execution in Timing"))FocusInterval(r.time[3],r.time[4],r.thread[3],!tasks_result_candidates_);
        if(!tasks_result_candidates_){ImGui::SameLine();ImGui::BeginDisabled(links_query_.valid());if(ImGui::Button("Load recorded links")){auto store=tasks_store_;auto id=r.id;links_request_id_=id;links_cancel_.store(false);links_query_=std::async(std::launch::async,[this,store,id]{return store->Links(id,&links_cancel_);});}ImGui::EndDisabled();
            ImGui::Text("Task %llu | size %s | %s%s",r.id,(r.flags&8)?(std::to_string(r.size)+" bytes").c_str():"unavailable",(r.flags&1)?"awaitable":(r.flags&4)?"not awaitable":"tracking unavailable",(r.flags&2)?" / conflicting observations":"");
            const char* phase[]={"Created","Launched","Scheduled","Started","Finished","Completed","Destroyed"};
            if(ImGui::BeginTable("lifecycle",7,ImGuiTableFlags_BordersInnerV|ImGuiTableFlags_RowBg)){for(auto title:phase)ImGui::TableSetupColumn(title);ImGui::TableHeadersRow();ImGui::TableNextRow();for(double t:r.time){ImGui::TableNextColumn();if(std::isfinite(t))ImGui::Text("%.6f s",t);else ImGui::TextDisabled("Unavailable");}ImGui::EndTable();}
            if(ImGui::BeginChild("task_links",ImVec2(0,0))) {ImGui::TextDisabled("Recorded links from -> to; execution/completion prerequisite kind not inferred. Wait-for lists / critical path not decoded.");for(const auto& link:task_links_)ImGui::Text("%llu -> %llu @ %.6f s",link.from,link.to,link.time);}ImGui::EndChild();}
    }
    ImGui::End();
}
}
