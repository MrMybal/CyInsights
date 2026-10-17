// CyInsights interface, independent of the CyGPUInspector UI.
// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include <unordered_map>
#include "AnalysisWorkers.hpp"
#include "UiMetrics.hpp"
#include <CyGPUInspectorCore/Format.hpp>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <commdlg.h>
#include <shobjidl.h>
#include <cstdio>
#include <fstream>
#include <imgui.h>
#include <imgui_internal.h>
#include <map>
namespace cyinsights {
fs::path ChooseFile(const wchar_t *filter, bool save) {
    wchar_t name[32768]{};
    OPENFILENAMEW of{};
    of.lStructSize = sizeof(of);
    of.lpstrFilter = filter;
    of.lpstrFile = name;
    of.nMaxFile = 32768;
    of.Flags = OFN_EXPLORER | OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    return (save ? GetSaveFileNameW(&of) : GetOpenFileNameW(&of)) ? fs::path(name) : fs::path();
}
fs::path ChooseFolder() {
    const auto initialized=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    fs::path result;IFileOpenDialog* dialog=nullptr;
    if(SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&dialog)))) {
        DWORD options=0;dialog->GetOptions(&options);dialog->SetOptions(options|FOS_PICKFOLDERS|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST);
        dialog->SetTitle(L"Select your Unreal engine root folder");
        if(SUCCEEDED(dialog->Show(GetActiveWindow()))) {
            IShellItem* item=nullptr;
            if(SUCCEEDED(dialog->GetResult(&item))) {
                PWSTR path=nullptr;if(SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH,&path))){result=path;CoTaskMemFree(path);}item->Release();
            }
        }
        dialog->Release();
    }
    if(SUCCEEDED(initialized))CoUninitialize();return result;
}
namespace {
const wchar_t *CaptureFilter = L"Unreal / CyInsights captures\0*.cycapture;*.utrace\0All files\0*.*\0\0";
cygi::Json ReadJson(const fs::path &file) {
    std::ifstream in(file);
    std::string text((std::istreambuf_iterator<char>(in)), {});
    cygi::Json j;
    cygi::Json::Parse(text, j);
    return j;
}
fs::path NewCache() {
    wchar_t temp[32768]{};
    GetTempPathW(32768, temp);
    auto n = std::chrono::steady_clock::now().time_since_epoch().count();
    return fs::path(temp) / "CyInsights" / (std::to_string(GetCurrentProcessId()) + "_" + std::to_string(n));
}
bool Match(const std::string &value, const char *query) {
    std::string a = value, b = query;
    std::transform(a.begin(), a.end(), a.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    std::transform(b.begin(), b.end(), b.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return a.find(b) != std::string::npos;
}
void Columns(std::initializer_list<const char *> cols) {
    for (auto c : cols)
        ImGui::TableSetupColumn(c);
    ImGui::TableHeadersRow();
}
} // namespace
Application::Application() {
    wchar_t configured[32768]{};
    const auto length = GetEnvironmentVariableW(L"CYINSIGHTS_UNREAL_INSIGHTS", configured, 32768);
    std::error_code error;
    if (length > 0 && length < 32768 && fs::is_regular_file(configured, error))
        std::snprintf(exe_, sizeof(exe_), "%s", fs::path(configured).u8string().c_str());
    native_setup_.LoadRegistered(fs::u8path(exe_));
    if(native_setup_.installations.empty())session_view_=true;
}
Application::~Application() {
    native_setup_.Cancel();
    ai_rpc_.Stop();ai_issues_cancel_=true;for(auto& work:ai_work_)if(work.request)work.request->cancelled=true;
    if(ai_issues_task_.valid())ai_issues_task_.wait();for(auto& work:ai_work_)if(work.result.valid())work.result.wait();
    activity_cancel_=true;activity_reader_.Cancel();if(activity_query_.valid())activity_query_.wait();
    distribution_cancel_.store(true);task_reader_.Cancel();tasks_cancel_.store(true);links_cancel_.store(true);
    if(distribution_task_.valid())distribution_task_.wait();if(tasks_query_.valid())tasks_query_.wait();if(links_query_.valid())links_query_.wait();
    model_cancel_.store(true);native_.Cancel();counter_reader_.Cancel();comparison_reader_.Cancel();
    if(model_task_.valid())model_task_.wait();
}
bool Application::Open(const fs::path &file) {
    drag_surface_=DragSurface::None;
    if(AnalysisBusy()){message_="Wait for the active analysis to finish, or cancel it";return false;}
    document_path_=fs::absolute(file);
    pending_capture_=file;pending_shown_=false;load_error_.clear();load_started_=ImGui::GetTime();cancelled_load_=false;
    session_view_=false;show_timing_=true;show_frames_=true;
    return true;
}
bool Application::OpenNow(const fs::path &file) {
    frame_glyph_start_=NAN;
    if (AnalysisBusy()) {
        message_ = "Wait for the active trace analysis to finish";
        return false;
    }
    try {
        ++ai_generation_;ai_issues_cancel_=true;ai_findings_.clear();ai_issues_=cygi::Json::Object();ai_issues_a_=ai_issues_b_=NAN;
        activity_store_.reset();activity_result_={};activity_pinned_.clear();activity_request_.clear();activity_ready_.clear();activity_details_request_.clear();activity_details_ready_.clear();activity_details_observed_.clear();activity_pending_=false;activity_requested_at_=-1;activity_message_.clear();activity_auto_started_=false;activity_search_[0]=0;activity_lane_=0;activity_page_=0;++activity_revision_;
        activity_plot_queries_=activity_detail_queries_=0;
        tasks_store_.reset();tasks_={};selected_task_.reset();task_links_.clear();tasks_message_.clear();distribution_={};distribution_message_.clear();
        analysis_a_=analysis_b_=NAN;analysis_whole_=true;distribution_label_.clear();distribution_dirty_=true;distribution_track_=0;distribution_task_thread_=-1;tasks_thread_=-1;tasks_page_=0;tasks_auto_started_=false;
        timing_exact_name_.clear();loaded_exact_name_.clear();timing_filter_key_.clear();timing_timer_matches_.clear();timing_name_matches_.clear();distribution_function_.clear();distribution_summary_.reset();distribution_summary_key_.clear();
        distribution_pending_=tasks_pending_=histogram_dock_pending_=false;observed_selection_a_=observed_selection_b_=0;
        if (file.extension() == L".utrace") {
            capture_ = Capture{};
            root_.clear();
            trace_ = fs::absolute(file);
            cache_ = NewCache();
            gpu_.reset();
            gpu_frames_.clear();
            metrics_.clear();
        } else {
            Capture c;
            std::string error;
            if (!Inspect(file, c, error)) {
                message_ = error;
                return false;
            }
            auto fresh = NewCache();
            if (!Extract(c, fresh, error)) {
                message_ = error;
                return false;
            }
            capture_ = std::move(c);
            root_ = fresh;
            cache_ = root_ / "analysis";
            trace_.clear();
            gpu_.reset();
            gpu_frames_.clear();
            metrics_.clear();
            for (const auto &source : capture_.manifest["sources"].Items())
                if (source["id"].AsString() == "meta.metrics" && source.Has("path")) {
                    std::ifstream input(root_ / fs::u8path(source["path"].AsString()));
                    std::string line;
                    while (metrics_.size() < 100001 && std::getline(input, line)) {
                        if (metrics_.empty() && line.rfind("\xEF\xBB\xBF", 0) == 0)
                            line.erase(0, 3);
                        metrics_.push_back(NativeTrace::CsvFields(line));
                    }
                }
            for (const auto &s : capture_.manifest["sources"].Items())
                if (s["id"].AsString() == "unreal.trace" && s.Has("path"))
                    trace_ = root_ / fs::u8path(s["path"].AsString());
            for (const auto &item : fs::recursive_directory_iterator(root_))
                if (item.is_regular_file() && item.path().filename() == "capture.json" &&
                    fs::is_regular_file(item.path().parent_path() / "events.bin"))
                    gpu_frames_.push_back(item.path().parent_path());
            std::sort(gpu_frames_.begin(), gpu_frames_.end(), [](const fs::path &a, const fs::path &b) {
                return ReadJson(a / "capture.json")["frame"]["index"].AsUInt() <
                       ReadJson(b / "capture.json")["frame"]["index"].AsUInt();
            });
            if (!gpu_frames_.empty()) {
                selected_frame_ = 0;
                LoadGpu(gpu_frames_.front());
            }
            const auto version = capture_.manifest["metadata"]["engine_version"].AsString();
            if (version.size() >= 3 && exe_[0]) {
                const auto minor = version.substr(0, version.find('.', 2));
                const bool supported = minor == "4.27" ||
                    (minor.size() == 3 && minor[0] == '5' && minor[1] == '.' && minor[2] >= '0' && minor[2] <= '8');
                if(supported&&AnalyzerVersion(fs::u8path(exe_))!=minor)
                    for(const auto& candidate:native_setup_.installations)
                        if(AnalyzerVersion(candidate)==minor){SelectAnalyzer(candidate);break;}
            }
        }
        native_.events.clear();
        native_.store.reset();loaded_start_=loaded_end_=NAN;overview_width_=0;
        window_aggregated_=false;counter_samples_requested_=false;
        frame_stat_samples_requested_=false;
        capture_file_=file;comparison_index_=TimingComparisonIndex{};
        comparison_b_.reset();comparison_rows_.clear();comparison_message_.clear();comparison_a_start_=comparison_a_end_=-1;
        native_.counters=CounterModel{};native_.vulkan_contents.reset();native_.vulkan_contents_error.clear();
        counters_=std::make_shared<CounterModel>();counter_stats_.clear();counter_plots_.clear();selected_counter_=-1;
        counter_stats_start_=counter_stats_end_=-1;counter_message_.clear();
        cpu_frames_.clear();
        timing_search_matches_.clear();
        timing_model_ = TimingModel{};
        selected_timer_ = -1;
        callers_.clear();
        callees_.clear();
        statistics_start_ = -1;
        statistics_timer_ = -2;
        selected_event_ = selected_cpu_ = -1;
        has_diff_ = false;
        view_start_ = 0;
        view_end_ = 1;
        range_start_ = range_end_ = 0;
        message_ = "Opened " + file.u8string();
        if (!trace_.empty())
            ImportNative();
        return true;
    } catch (const std::exception &e) {
        message_ = load_error_ = e.what();
        return false;
    }
}
void Application::LoadGpu(const fs::path &directory) {
    auto m = std::make_unique<cygi::SessionModel>();
    std::string error;
    cygi::CaptureInfo info;
    if (!cygi::CaptureArchive::Load(directory, *m, info, error)) {
        message_ = error;
        return;
    }
    graph_.Build(*m);
    gpu_timeline_.Build(m->Timings(), graph_, m->LastFrame().events);
    gpu_info_ = info;
    gpu_ = std::move(m);
    selected_event_ = -1;
}
void Application::ImportNative() {
    if(native_setup_.Busy()){native_import_pending_=true;message_="Waiting for native reader preparation...";return;}
    if(exe_[0]&&fs::u8path(exe_)!=native_setup_.selected) {
        native_setup_.Prepare(fs::u8path(exe_));setup_dismissed_=false;native_import_pending_=true;return;
    }
    if (AnalysisBusy()) {
        message_ = "Native analysis is already running";
        return;
    }
    std::string error;
    load_error_.clear();load_started_=ImGui::GetTime();model_cancel_.store(false);
    if (!native_.Start(fs::u8path(exe_), trace_, cache_, error,true))
        message_ = load_error_ = error;
    else {
        native_.events.clear();
        timing_model_ = TimingModel{};
        comparison_index_=TimingComparisonIndex{};comparison_rows_.clear();comparison_a_start_=comparison_a_end_=-1;
        timing_search_matches_.clear();
        cpu_frames_.clear();
        callers_.clear();
        callees_.clear();
        selected_timer_ = selected_cpu_ = -1;
        statistics_start_ = -1;
        statistics_timer_ = -2;
        range_start_ = range_end_ = 0;
        message_ = "Analyzing native trace with Epic's installed analyzer...";
    }
}
void Application::Layout(unsigned id) {
    ImGui::DockBuilderRemoveNode(id);
    ImGui::DockBuilderAddNode(id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(id, WorkspaceViewport()->WorkSize);
    unsigned main = id, right, overview, messages, callers, callees, timers;
    ImGui::DockBuilderSplitNode(
        main, ImGuiDir_Right,
        std::clamp(300.f * UiScale() / WorkspaceViewport()->WorkSize.x, .10f, .30f), &right, &main);
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Up, .12f, &overview, &main);
    ImGui::DockBuilderSplitNode(main, ImGuiDir_Down, .075f, &messages, &main);
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, .25f, &timers, &right);
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Up, .5f, &callers, &callees);
    ImGui::DockBuilderDockWindow(WindowName("Frames").c_str(), overview);
    ImGui::DockBuilderDockWindow(WindowName("Timing").c_str(), main);
    ImGui::DockBuilderDockWindow(WindowName("GPU frame").c_str(), main);
    ImGui::DockBuilderDockWindow(WindowName("Capture comparison").c_str(), main);
    ImGui::DockBuilderDockWindow(WindowName("Distributions").c_str(), main);
    ImGui::DockBuilderDockWindow(WindowName("Tasks").c_str(), main);
    ImGui::DockBuilderDockWindow(WindowName("Activities").c_str(), main);
    ImGui::DockBuilderDockWindow(WindowName("AI Inspector").c_str(), main);
    ImGui::DockBuilderDockWindow(WindowName("Timers").c_str(), timers);
    ImGui::DockBuilderDockWindow(WindowName("Counters").c_str(), timers);
    ImGui::DockBuilderDockWindow(WindowName("Selection").c_str(), timers);
    ImGui::DockBuilderDockWindow(WindowName("Platform metrics").c_str(), timers);
    ImGui::DockBuilderDockWindow(WindowName("Callers").c_str(), callers);
    ImGui::DockBuilderDockWindow(WindowName("Callees").c_str(), callees);
    ImGui::DockBuilderDockWindow(WindowName("Messages").c_str(), messages);
    ImGui::DockBuilderFinish(id);
}
void Application::LoadWindow() {
    if(!native_.store||native_.Busy()||!load_error_.empty())return;
    if(cancelled_load_){if(view_start_==requested_start_&&view_end_==requested_end_)return;cancelled_load_=false;}
    double a=range_start_==range_end_?view_start_:std::min(range_start_,range_end_),
           b=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
    // Complete the current read. Navigation queues the latest interval instead
    // of cancelling every pointer movement and repeatedly stealing the grab.
    if(model_task_.valid())return;
    if(view_start_==loaded_start_&&view_end_==loaded_end_&&a==statistics_start_&&b==statistics_end_&&timing_exact_name_==loaded_exact_name_)return;
    if(drag_surface_!=DragSurface::None&&ImGui::GetTime()-model_request_time_<1./60.)return;
    model_request_time_=ImGui::GetTime();
    requested_start_=view_start_;requested_end_=view_end_;requested_stats_start_=a;requested_stats_end_=b;model_cancel_.store(false);
    float pixels=std::max(1.f,WorkspaceViewport()->WorkSize.x-300*UiScale());
    auto store=native_.store;double start=view_start_,end=view_end_;
    model_task_=std::async(std::launch::async,[this,store,start,end,a,b,pixels,name=timing_exact_name_,retired=std::move(retired_timing_)]() mutable {
        retired.reset();
        TimingAnalysis result;
        // Read adjacent scopes too so the old model remains useful while panning.
        double margin=end-start,left=std::max(store->start,start-margin),right=std::min(store->end,end+margin);
        // A task can be recorded before/after the CPU scope domain. Display an
        // empty native timing interval there rather than reverse its bounds.
        if(right<=left){left=start-margin;right=end+margin;}
        auto window=store->Window(left,right,a,b,pixels*static_cast<float>((right-left)/(end-start)),&model_cancel_,0,name);
        result.events=std::move(window.events);result.aggregated=window.aggregated;
        result.model.Build(result.events,false,&model_cancel_);
        result.model.Aggregate(result.events,a,b);store->ApplyTotals(window,result.model);
        result.summary=store->Summary(window,result.model);
        result.model.frames_native=store->frames_native;result.model.frame_ids=store->frame_ids;
        result.window_start=start;result.window_end=end;
        result.exact_name=name;
        return result;
    });
}
void Application::Tick() {
    NativeSetupTick();
    AiTick();
    PollTraceAnalysis();
    AdvanceTimingDrag();
    ActivityTick();
    if(!pending_capture_.empty()&&pending_shown_&&!native_setup_.Busy()) {
        auto file=std::move(pending_capture_);pending_capture_.clear();
        if(!OpenNow(file)&&load_error_.empty())load_error_=message_;
    }
    Live();
    const bool imported=native_.Poll(message_);
    if(imported&&!native_.Error().empty()&&native_.Error()!="Analysis cancelled")load_error_=native_.Error();
    if(imported) {
        counters_=std::make_shared<CounterModel>(std::move(native_.counters));
        ++counter_catalog_revision_;
        counter_message_=counters_->error;
        if(native_.store&&native_.store->count) {
            // The disk index is ready before the window model. Publish events
            // and their matching model together, never a half-built UI state.
            native_.events.clear();
            cpu_frames_=native_.store->frames;timing_model_.start=native_.store->start;timing_model_.end=native_.store->end;
            timing_model_.frames_native=native_.store->frames_native;timing_model_.frame_ids=native_.store->frame_ids;
            if(!cpu_frames_.empty())FitTiming();
            else {view_start_=(timing_model_.start+timing_model_.end)/2;view_end_=std::min(timing_model_.end,view_start_+.05);}
            overview_width_=0;
        }else if(native_.Error().empty())load_error_="The analyzer exported no CPU/GPU timing scopes. Check the capture channels and selected Unreal analyzer.";
    }
    if (imported && !native_.store && !native_.events.empty()) {
        try {
            model_task_ = std::async(std::launch::async, [this,events = std::move(native_.events),retired=std::move(retired_timing_)]() mutable {
                retired.reset();
                TimingAnalysis result;
                result.events = std::move(events);
                result.model.Build(result.events,true,&model_cancel_);
                result.comparison.Build(result.events,result.model,&model_cancel_);
                return result;
            });
            message_ = "Indexing native timing in background...";
        } catch (const std::exception &e) {
            message_ = "Cannot index native timing: " + std::string(e.what());
        }
    }
    CounterModel incoming_counters;
    if(counter_reader_.Poll(incoming_counters,counter_message_)) {
        // Pin/unpin actions made while a background import runs belong to the
        // UI, and must survive publication of its earlier catalogue snapshot.
        std::unordered_map<uint64_t,bool> pinned_by_id;
        auto identity=[](const CounterSeries& c){return (uint64_t(c.frame_stats)<<32)|(c.frame_stats?c.frame_stat_id:c.id);};
        for(const auto& current:counters_->series)pinned_by_id.emplace(identity(current),current.pinned);
        for(auto& next:incoming_counters.series){auto pin=pinned_by_id.find(identity(next));if(pin!=pinned_by_id.end())next.pinned=pin->second;}
        counters_=std::make_shared<CounterModel>(std::move(incoming_counters));
        ++counter_catalog_revision_;
        counter_stats_start_=counter_stats_end_=-1;counter_stats_.clear();counter_plots_.clear();
        size_t pins=0;for(auto& counter:counters_->series)
            if(counter.meta&&!counter.availability&&counter.reason.empty()&&pins<4){counter.pinned=true;++pins;}
        if(native_.events.empty()&&!model_task_.valid())FitCapture();
    }
    if (model_task_.valid() && model_task_.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        try {
            auto result = model_task_.get();
            if(model_cancel_.load())throw std::runtime_error("Analysis cancelled");
            const auto previous_selected=selected_timer_>=0&&static_cast<size_t>(selected_timer_)<timing_model_.names.size()?timing_model_.names[selected_timer_]:std::string();
            for(auto& track:result.model.tracks)for(const auto& old:timing_model_.tracks)
                if(track.has_id&&old.has_id?track.id==old.id:track.name==old.name){track.visible=old.visible;track.collapsed=old.collapsed;break;}
            native_.events.swap(result.events);
            std::swap(timing_model_,result.model);
            ++model_version_;
            timer_colours_.clear();timer_colours_.reserve(timing_model_.names.size());
            for(const auto& name:timing_model_.names)timer_colours_.push_back(TimerColour(name));
            std::swap(comparison_index_,result.comparison);
            comparison_a_summary_.swap(result.summary);
            comparison_a_start_=comparison_a_end_=-1;
            if(native_.store) {
                loaded_start_=result.window_start;loaded_end_=result.window_end;window_aggregated_=result.aggregated;
                loaded_exact_name_=std::move(result.exact_name);
                statistics_start_=timing_model_.aggregate_start;statistics_end_=timing_model_.aggregate_end;
            } else {cpu_frames_ = timing_model_.frames;FitTiming();statistics_start_ = -1;}
            statistics_timer_ = -2;
            selected_timer_ = -1;
            selected_cpu_=-1;
            if(!previous_selected.empty())for(size_t i=0;i<timing_model_.names.size();++i)if(timing_model_.names[i]==previous_selected){selected_timer_=static_cast<int>(i);break;}
            message_ = "Ready: " + std::to_string(native_.store?native_.store->count:native_.events.size()) + " native timing events";
            retired_timing_=std::make_unique<TimingAnalysis>(std::move(result));
        } catch (const std::exception &e) {
            message_ = model_cancel_.load()?"Visible interval loading cancelled":"Native timing indexing failed: " + std::string(e.what());
            if(!model_cancel_.load())load_error_=message_;
        }
    }
    if(comparison_reader_.Poll(comparison_b_,comparison_message_)) {
        comparison_a_start_=comparison_a_end_=-1;
        if(comparison_b_){comparison_b_start_=comparison_b_->start;comparison_b_end_=comparison_b_->end;}
        message_=comparison_message_;
    }
    LoadWindow();
    if (previous_message_ != message_) {
        messages_.push_back(message_);
        previous_message_ = message_;
    }
}
void Application::DrawMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if(brand_logo_)ImGui::Image(brand_logo_,ImVec2(ImGui::GetFontSize(),ImGui::GetFontSize()));
        else ImGui::TextColored(ImVec4(.75f, .85f, .93f, 1), "Cy");
        ImGui::SameLine();
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Open capture...", "Ctrl+O")) {
                ChooseAndOpenCaptures();
            }
            if (ImGui::MenuItem("Pack staging manifest...")) {
                auto m = ChooseFile(L"Capture manifest\0manifest.json\0\0");
                if (!m.empty()) {
                    auto dest = ChooseFile(L"CyInsights capture\0*.cycapture\0\0", true);
                    if (!dest.empty()) {
                        if (dest.extension() != L".cycapture")
                            dest += L".cycapture";
                        std::string error;
                        message_ = Pack(m.parent_path(), dest, error) ? "Saved " + dest.u8string() : error;
                    }
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Frames", nullptr, &show_frames_);
            ImGui::MenuItem("Timing", nullptr, &show_timing_);
            ImGui::MenuItem("Timers", nullptr, &show_timers_);
            ImGui::MenuItem("Callers", nullptr, &show_callers_);
            ImGui::MenuItem("Callees", nullptr, &show_callees_);
            ImGui::MenuItem("GPU frame", nullptr, &show_gpu_);
            ImGui::MenuItem("Selection", nullptr, &show_details_);
            ImGui::MenuItem("Platform metrics", nullptr, &show_metrics_);
            if(ImGui::MenuItem("Frame stats",nullptr,&show_frame_stats_)&&show_frame_stats_)frame_stats_dock_pending_=true;
            ImGui::MenuItem("Counters", nullptr, &show_counters_);
            ImGui::MenuItem("Capture comparison", nullptr, &show_comparison_);
            ImGui::MenuItem("Distributions", nullptr, &show_distribution_);
            ImGui::MenuItem("Tasks", nullptr, &show_tasks_);
            if(ImGui::MenuItem("Network / storage / memory", nullptr, &show_activities_)&&show_activities_)FocusActivities();
            ImGui::MenuItem("AI Inspector", nullptr, &show_ai_);
            ImGui::MenuItem("Messages", nullptr, &show_messages_);
            if (ImGui::MenuItem("Reset Insights layout"))
                reset_layout_ = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Analysis")) {
            if(ImGui::MenuItem("Compare native capture...",nullptr,false,!native_.events.empty()&&!AnalysisBusy())) {
                auto file=ChooseFile(CaptureFilter);if(!file.empty())OpenComparison(file);
            }
            if (ImGui::MenuItem("Analyze native timing", nullptr, false, !trace_.empty() && !AnalysisBusy()))
                ImportNative();
            if (ImGui::MenuItem("Open in Unreal Insights", nullptr, false, !trace_.empty())) {
                std::string error;
                if (!NativeTrace::Launch(fs::u8path(exe_),
                                         L"-OpenTraceFile=" + NativeTrace::Quote(trace_.wstring()), nullptr,
                                         error))
                    message_ = error;
            }
            if (ImGui::MenuItem("Compare GPU capture...", nullptr, false, gpu_ != nullptr)) {
                auto file = ChooseFile(CaptureFilter);
                if (!file.empty()) {
                    Capture c;
                    std::string error;
                    auto dir = NewCache();
                    if (Inspect(file, c, error) && Extract(c, dir, error)) {
                        fs::path gpuDir;
                        for (const auto &e : c.entries)
                            if (fs::u8path(e.name).filename() == "capture.json") {
                                gpuDir = (dir / fs::u8path(e.name)).parent_path();
                                break;
                            }
                        auto model = std::make_unique<cygi::SessionModel>();
                        cygi::CaptureInfo info;
                        if (!gpuDir.empty() && cygi::CaptureArchive::Load(gpuDir, *model, info, error)) {
                            comparison_timed_ = gpu_->Timings().IsValid() && model->Timings().IsValid();
                            diff_ = cygi::CompareCaptures(*gpu_, *model);
                            has_diff_ = true;
                            message_ = "Compared selected frame against first GPU frame of capture B";
                        } else
                            message_ = error.empty() ? "Capture B has no GPU commands" : error;
                    } else
                        message_ = error;
                }
            }
            ImGui::EndMenu();
        }
        ImGui::SameLine();
        auto project = capture_.manifest["metadata"]["project"].AsString();
        ImGui::TextDisabled("  %s  —  CyInsights", project.empty() ? "Unreal capture" : project.c_str());
        ImGui::EndMainMenuBar();
    }
}
void Application::Draw() {
    Tick();
    DrawMenuBar();
    Toolbar();
    float navigation_height = NavigationHeight();
    const auto *viewport = WorkspaceViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + navigation_height));
    ImGui::SetNextWindowSize(
        ImVec2(viewport->WorkSize.x, std::max(1.f, viewport->WorkSize.y - navigation_height)));
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    BeginView("##InsightsWorkspace", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoDocking);
    unsigned dock = ImGui::GetID("InsightsLayoutV2");
    ImGui::DockSpace(dock, ImVec2(0, 0), session_view_ ? ImGuiDockNodeFlags_KeepAliveOnly : 0);
    if (first_ || reset_layout_) {
        Layout(dock);
        first_ = reset_layout_ = false;
    }
    ImGui::End();
    ImGui::PopStyleVar(2);
    if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
        ChooseAndOpenCaptures();
    }
    if (session_view_) {
        FollowTimelineSelection();
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + navigation_height));
        ImGui::SetNextWindowSize(
            ImVec2(viewport->WorkSize.x, std::max(1.f, viewport->WorkSize.y - navigation_height)));
        Session();
        if(show_ai_)AiView();
        LoadStatus();
        NativeSetupView();
        return;
    }
    DrawPanels();
}
void Application::DrawPanels() {
    UpdateTiming();
    DockFunctionHistogram();
    DockActivities();
    if (show_frames_)
        Frames();
    if (show_timing_)
        Timing();
    FollowTimelineSelection();
    if (show_gpu_)
        Gpu();
    if (show_timers_)
        Statistics();
    if(show_counters_)Counters();
    if(show_frame_stats_)FrameStatsView();
    if(show_comparison_)Comparison();
    if(show_distribution_)DistributionView();
    if(show_tasks_)TasksView();
    if(show_activities_)ActivitiesView();
    if(show_ai_)AiView();
    if (show_callers_)
        Relations(true);
    if (show_callees_)
        Relations(false);
    if (show_details_)
        Details();
    if (show_metrics_)
        PlatformMetrics();
    if (show_messages_)
        Messages();
    if(document_visible_){LoadStatus();NativeSetupView();}
}
void Application::Session() {
    if(hosted_)ImGui::BeginChild("Session",ImVec2(0,0));
    else BeginView("Session", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking);
    if(brand_logo_){ImGui::Image(brand_logo_,ImVec2(80*UiScale(),80*UiScale()));ImGui::SameLine();}
    ImGui::BeginGroup();
    ImGui::TextColored(ImVec4(.4f, .8f, 1.f, 1), "CyInsights");
    ImGui::TextUnformatted("Unreal & GPU profiling");
    ImGui::EndGroup();
    ImGui::Separator();
    ImGui::TextUnformatted("Capture session");
    ImGui::TextWrapped("%s", message_.c_str());
    if (AnalysisBusy())
        ImGui::TextDisabled("Native analysis in progress");
    ImGui::SeparatorText("Remote capture - PC / Quest");
    const auto remote = remote_.Snapshot();
    ImGui::BeginDisabled(remote.Busy());
    ImGui::SetNextItemWidth(250);
    ImGui::InputTextWithHint("##remoteip", "Device IPv4 address", remote_ip_, sizeof(remote_ip_));
    RemoteOptions options;
    options.ipv4 = remote_ip_;
    options.control_port = static_cast<uint16_t>(remote_port_);
    options.seconds = remote_seconds_;
    options.override_capture_options=!remote_use_defaults_;
    options.capture=remote_capture_;
    options.meta_gpu=remote_meta_;options.meta_gpu.adb=fs::u8path(remote_adb_);options.meta_gpu.device=remote_adb_device_;options.meta_gpu.package=remote_android_package_;
    wchar_t app_path[32768]{};
    GetModuleFileNameW(nullptr, app_path, 32768);
    options.output_directory = fs::path(app_path).parent_path() / "Captures";
    if (ImGui::Button("Connect to device")) remote_.Probe(options);
    ImGui::SameLine();
    if (ImGui::Button("Start remote trace")) remote_.Start(options);
    if(focus_capture_options_){ImGui::SetNextItemOpen(true);focus_capture_options_=false;}
    if (ImGui::TreeNode("Capture options")) {
        ImGui::Checkbox("Use plugin project defaults",&remote_use_defaults_);
        if(remote_use_defaults_&&!remote.capture_defaults.IsNull()){
            std::string error;if(ReadCaptureOptions(remote.capture_defaults,remote_capture_,error))std::snprintf(remote_stat_groups_,sizeof(remote_stat_groups_),"%s",remote_capture_.stat_groups.c_str());
        }
        if(!remote.capture_defaults.IsNull()&&ImGui::Button("Copy plugin defaults for this capture")){
            std::string error;if(ReadCaptureOptions(remote.capture_defaults,remote_capture_,error)){
                remote_use_defaults_=false;std::snprintf(remote_stat_groups_,sizeof(remote_stat_groups_),"%s",remote_capture_.stat_groups.c_str());
            }
        }
        ImGui::BeginDisabled(remote_use_defaults_);
        if(ImGui::Button("Basic profile")){remote_capture_=CaptureOptions{};std::snprintf(remote_stat_groups_,sizeof(remote_stat_groups_),"%s",remote_capture_.stat_groups.c_str());}
        ImGui::Checkbox("CPU timing",&remote_capture_.cpu);ImGui::SameLine();ImGui::Checkbox("GPU groups each frame (default)",&remote_capture_.gpu);
        if(!remote_capture_.gpu)remote_capture_.gpu_passes=false;
        ImGui::BeginDisabled(!remote_capture_.gpu);ImGui::Checkbox("Extra individual GPU passes (each frame)",&remote_capture_.gpu_passes);ImGui::EndDisabled();
        ImGui::Checkbox("Basic frame statistics",&remote_capture_.frame_stats);ImGui::SameLine();ImGui::Checkbox("UObject counts",&remote_capture_.object_counts);
        if(!remote_capture_.frame_stats)remote_capture_.stat_groups_enabled=false;
        ImGui::Checkbox("Process memory each frame",&remote_capture_.memory_stats);
        ImGui::BeginDisabled(!remote_capture_.frame_stats);ImGui::Checkbox("Additional Unreal stat groups",&remote_capture_.stat_groups_enabled);
        ImGui::BeginDisabled(!remote_capture_.stat_groups_enabled);ImGui::SetNextItemWidth(300);
        if(ImGui::InputText("Groups (* = all active)",remote_stat_groups_,sizeof(remote_stat_groups_)))remote_capture_.stat_groups=remote_stat_groups_;
        ImGui::EndDisabled();ImGui::EndDisabled();
        ImGui::Checkbox("Tasks",&remote_capture_.tasks);ImGui::SameLine();ImGui::Checkbox("Network",&remote_capture_.network);
        ImGui::Checkbox("File I/O",&remote_capture_.file_io);ImGui::SameLine();ImGui::Checkbox("Asset loading",&remote_capture_.loadtime);
        ImGui::Checkbox("Native logs",&remote_capture_.logs);ImGui::SameLine();ImGui::Checkbox("Memory tags (LLM)",&remote_capture_.memtags);
        ImGui::Checkbox("Quest GPU completion / Vulkan passes",&remote_capture_.quest_gpu_completion);
        ImGui::TextDisabled("Two extra queries per Vulkan render pass; may affect scheduling. No pass splitting or resource readbacks.");
        ImGui::Checkbox("Quest runtime metrics",&remote_capture_.quest_metrics);ImGui::SameLine();ImGui::SetNextItemWidth(110);ImGui::InputInt("Meta Hz (0 = each frame)",&remote_capture_.quest_hz);remote_capture_.quest_hz=std::clamp(remote_capture_.quest_hz,0,120);
        ImGui::TextWrapped("Default GPU: native rendering groups each render frame. Quest completion mode also measures Vulkan render-pass boundaries; native subgroups inside tile passes may remain unresolved. Individual RDG names are optional. Meta execution-frame identity is unknown. No image/buffer copies.");
        ImGui::Checkbox("Detailed GPU snapshot on start",&remote_capture_.gpu_details);
        ImGui::BeginDisabled(!remote_capture_.gpu_details);
        ImGui::SetNextItemWidth(110);ImGui::InputInt("GPU snapshot frames",&remote_capture_.gpu_frames);remote_capture_.gpu_frames=std::clamp(remote_capture_.gpu_frames,1,8);
        ImGui::Checkbox("GPU descriptors",&remote_capture_.gpu_bindings);ImGui::SameLine();ImGui::Checkbox("GPU barriers",&remote_capture_.gpu_barriers);
        ImGui::Checkbox("Timestamp every GPU command",&remote_capture_.gpu_per_command);
        ImGui::Checkbox("Copy GPU images / textures (expensive)",&remote_capture_.gpu_readbacks);
        ImGui::EndDisabled();ImGui::EndDisabled();
        ImGui::TextWrapped("Basic: CPU/GPU scopes, frame times and object counts. Extra groups and readbacks are off. GPU snapshots require a Windows collector and connected recorder; detailed payloads still use the collector export workflow.");
        ImGui::SeparatorText("Meta hardware GPU capture (Quest)");
        ImGui::Checkbox("Record Meta render stages and counters (test default)",&remote_meta_.enabled);
        ImGui::BeginDisabled(!remote_meta_.enabled);
        ImGui::Checkbox("Restart the Quest game before capture (optional)",&remote_meta_.restart_game);
        ImGui::SetNextItemWidth(430);ImGui::InputTextWithHint("ADB executable","Automatic, or path to adb.exe",remote_adb_,sizeof(remote_adb_));
        ImGui::SetNextItemWidth(300);ImGui::InputTextWithHint("ADB device","Automatic match by game IP",remote_adb_device_,sizeof(remote_adb_device_));
        ImGui::SetNextItemWidth(300);ImGui::InputTextWithHint("Android package","Automatic from updated plugin",remote_android_package_,sizeof(remote_android_package_));
        ImGui::TextWrapped("Detailed Meta GPU profiling is enabled before launching the game. Restart resets the current game session. An authorized ADB connection is required; USB or ADB over Wi-Fi. Meta hardware events are saved in the same .cycapture. No texture or buffer copies.");
        ImGui::EndDisabled();
        ImGui::SetNextItemWidth(150);
        ImGui::InputInt("Control port", &remote_port_);
        remote_port_ = std::clamp(remote_port_, 1, 65535);
        ImGui::SetNextItemWidth(150);
        ImGui::InputFloat("Duration (seconds, 0 = manual)", &remote_seconds_);
        if (!std::isfinite(remote_seconds_)) remote_seconds_ = 0;
        remote_seconds_ = std::clamp(remote_seconds_, 0.f, 3600.f);
        ImGui::TextWrapped("Saved on PC: %s", options.output_directory.u8string().c_str());
        ImGui::TreePop();
    }
    ImGui::EndDisabled();
    if (remote.state == RemoteState::starting || remote.state == RemoteState::recording) {
        if (ImGui::Button("Stop remote trace")) remote_.Stop();
    }
    if(remote.state==RemoteState::recording&&ImGui::TreeNode("Request extra GPU snapshot")){
        ImGui::SetNextItemWidth(110);ImGui::InputInt("Snapshot frames",&remote_gpu_options_.gpu_frames);remote_gpu_options_.gpu_frames=std::clamp(remote_gpu_options_.gpu_frames,1,8);
        ImGui::Checkbox("Descriptors",&remote_gpu_options_.gpu_bindings);ImGui::SameLine();ImGui::Checkbox("Barriers",&remote_gpu_options_.gpu_barriers);
        ImGui::Checkbox("Per-command GPU timestamps",&remote_gpu_options_.gpu_per_command);
        ImGui::Checkbox("Include GPU image / texture readbacks",&remote_gpu_options_.gpu_readbacks);
        ImGui::BeginDisabled(!remote.capture_capabilities["gpu_request"].AsBool());
        if(ImGui::Button("Capture GPU snapshot now"))remote_.RequestGpu(remote_gpu_options_);
        ImGui::EndDisabled();
        ImGui::TextWrapped("Windows collector + connected recorder required. Exported GPU files are not transferred by the native trace receiver. Quest provides runtime metrics; detailed Vulkan commands remain unavailable.");
        ImGui::TreePop();
    }
    if(!remote.gpu_message.empty())ImGui::TextWrapped("GPU: %s",remote.gpu_message.c_str());
    if(!remote.meta_message.empty())ImGui::TextWrapped("Meta GPU: %s",remote.meta_message.c_str());
    for(const auto& source:remote.manifest["sources"].Items())if(source["id"].AsString()=="unreal.gpu.scopes"&&source.Has("timestamp_queries_available")) {
        const bool ready=source["trace_compiled"].AsBool()&&source["timestamp_queries_available"].AsBool();
        ImGui::TextColored(ready?ImVec4(.45f,.85f,.6f,1):ImVec4(1,.65f,.3f,1),"Native GPU groups: %s",ready?"timestamps supported (events verified after analysis)":"unavailable on this running target");
        if(source.Has("vulkan_timestamp_stage")) {
            ImGui::TextWrapped("GPU timestamp mode: %s",source["vulkan_timestamp_stage_meaning"].AsString().c_str());
            if(source["completion_timestamps_requested"].AsBool()&&source["vulkan_timestamp_stage"].AsNumber()!=1)
                ImGui::TextWrapped("GPU completion mode was not applied: %s",source["completion_timestamp_path"].AsString().c_str());
        } else if(remote.manifest["metadata"]["platform"].AsString()=="Android")
            ImGui::TextWrapped("This Quest capture does not report its timestamp stage. Update the target plugin to record GPU completion timestamps.");
        if(!ready||(source["groups_requested"].AsBool()&&!source["group_recording_enabled"].AsBool()))ImGui::TextWrapped("%s",source["reason"].AsString().c_str());
        if(source["groups_requested"].AsBool()&&source["applied_rdg_event_level"].AsNumber()<source["requested_rdg_event_level"].AsNumber())
            ImGui::TextWrapped("Nested RDG group level not applied: engine instrumentation or an explicit console override limits coverage.");
        const auto preparation=source["timestamp_preparation"].AsString();
        if(!preparation.empty())ImGui::TextWrapped("%s",preparation.c_str());
    }
    for(const auto& source:remote.manifest["sources"].Items())if(source["id"].AsString()=="cyinsights.vulkan.passes") {
        ImGui::TextWrapped("Vulkan passes: %s resolved, %s pending, %s unavailable",source["resolved_pass_count"].AsString().c_str(),source["pending_pass_count"].AsString().c_str(),source["unresolved_pass_count"].AsString().c_str());
        ImGui::TextWrapped("%s",source["reason"].AsString().c_str());
    }
    ImGui::Text("%s | %.1f MiB | %.1f s", RemoteStateName(remote.state),
                double(remote.bytes) / (1024 * 1024), remote.state==RemoteState::starting||remote.state==RemoteState::probing?remote.startup_elapsed:remote.elapsed);
    if((remote.state==RemoteState::starting||remote.state==RemoteState::probing)&&remote.session_id.empty())ImGui::TextWrapped("Waiting for capture startup; no trace data has been received yet.");
    if (!remote.message.empty()) ImGui::TextWrapped("%s", remote.message.c_str());
    if(!remote.diagnostic_log.empty())ImGui::TextWrapped("Capture diagnostics: %s",remote.diagnostic_log.u8string().c_str());
    if (!remote.archive.empty()) {
        ImGui::BeginDisabled(AnalysisBusy());
        if (ImGui::Button("Open received capture")) RequestOpen(remote.archive);
        ImGui::EndDisabled();
        ImGui::TextWrapped("%s", remote.archive.u8string().c_str());
    }
    if (remote.state == RemoteState::failed && !remote.partial_trace.empty())
        ImGui::TextWrapped("Partial data retained: %s", remote.partial_trace.u8string().c_str());
    ImGui::TextWrapped("Launch the game with the plugin in Development/Debug. Enter its LAN IPv4 address; "
                       "Unreal and available Quest GPU metrics travel in the same trace. Analyze after stop.");
    ImGui::SeparatorText("Capture");
    if(capture_.file.empty())ImGui::TextDisabled("No capture loaded.");
    if(!capture_.file.empty())for (const auto &kv : capture_.manifest["metadata"].Members())
        ImGui::TextWrapped("%s: %s", kv.first.c_str(), kv.second.Write().c_str());
    ImGui::SeparatorText("Sources");
    for (const auto &s : capture_.manifest["sources"].Items()) {
        ImGui::TextWrapped("%s  [%s]", s["id"].AsString().c_str(), s["status"].AsString().c_str());
        if (s.Has("reason"))
            ImGui::TextDisabled("%s", s["reason"].AsString().c_str());
    }
    ImGui::SeparatorText("Native analyzer");
    auto worker_setting=AnalysisWorkerSetting();
    char worker_label[64];
    if(worker_setting)std::snprintf(worker_label,sizeof(worker_label),"%u workers",worker_setting);
    else std::snprintf(worker_label,sizeof(worker_label),"Automatic (up to %u)",AnalysisWorkerLimit());
    if(ImGui::BeginCombo("Analysis workers",worker_label)) {
        if(ImGui::Selectable("Automatic",worker_setting==0))SetAnalysisWorkerCount(0);
        for(unsigned workers:{1u,2u,4u,8u})if(workers<=AnalysisWorkerLimit()) {
            char label[32];std::snprintf(label,sizeof(label),"%u workers",workers);
            if(ImGui::Selectable(label,worker_setting==workers))SetAnalysisWorkerCount(workers);
        }
        ImGui::EndCombo();
    }
    if(ImGui::IsItemHovered())ImGui::SetTooltip("Automatic adapts concurrency to the visible workload.\nReduce workers to leave more CPU time for other applications.\nThis setting applies to this application session.");
    ImGui::TextWrapped("%s",native_setup_.message.c_str());
    ImGui::BeginDisabled(native_setup_.Busy()||AnalysisBusy());
    ImGui::TextWrapped("Add the root of your Unreal installation (or its Engine folder). Registered engines are saved and their readers are prepared automatically.");
    bool add_engine=ImGui::InputTextWithHint("Engine root","C:/Unreal/UE_5.8",engine_root_input_,sizeof(engine_root_input_),ImGuiInputTextFlags_EnterReturnsTrue);
    if(ImGui::Button("Browse engine folder")) {
        auto p=ChooseFolder();if(!p.empty()){std::snprintf(engine_root_input_,sizeof(engine_root_input_),"%s",p.u8string().c_str());add_engine=true;}
    }
    ImGui::SameLine();add_engine|=ImGui::Button("Add engine");
    if(add_engine)AddEngineRoot(fs::u8path(engine_root_input_));
    if(ImGui::BeginCombo("Registered Unreal engines",exe_[0]?NativeSetup::EngineRoot(fs::u8path(exe_)).u8string().c_str():"Add an engine")) {
        for(const auto& path:native_setup_.installations) {
            const auto label=NativeSetup::EngineRoot(path).u8string();
            if(ImGui::Selectable(label.c_str(),path==fs::u8path(exe_)))SelectAnalyzer(path);
        }
        ImGui::EndCombo();
    }
    ImGui::BeginDisabled(!exe_[0]||native_setup_.Busy());
    if(ImGui::Button("Prepare native readers"))SelectAnalyzer(fs::u8path(exe_));
    ImGui::SameLine();if(ImGui::Button("Remove selected engine")) {
        if(native_setup_.RemoveEngine(fs::u8path(exe_),engine_registration_error_)) {
            std::snprintf(exe_,sizeof(exe_),"%s",native_setup_.selected.u8string().c_str());
            if(exe_[0])SelectAnalyzer(native_setup_.selected);
        }
    }
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if(!engine_registration_error_.empty())ImGui::TextColored(ImVec4(1,.45f,.4f,1),"%s",engine_registration_error_.c_str());
    if(exe_[0])ImGui::TextWrapped("Analyzer: %s",exe_);
    ImGui::SeparatorText("Windows collector");
    ImGui::TextWrapped("Optional CyInsights collector for extra command detail. Native .utrace GPU groups do not require it.");
    if (ImGui::Button("Discover sessions")) {
        sessions_.Open();
    }
    if (sessions_.IsValid())
        for (const auto &entry : sessions_.List())
            if (ImGui::Button(entry.Describe().c_str()))
                Connect(entry.process_id);
    if (client_) {
        ImGui::Text("PID %u / %s", client_->Entry().process_id,
                    client_->WriterAlive() ? "connected" : "disconnected");
        if (ImGui::Button("Capture detailed GPU frame")) {
            client_->SetLevel(cygi::TrackingLevel::pass_timing);
            client_->StartDeepCapture(remote_gpu_options_.gpu_frames, remote_gpu_options_.gpu_bindings,
                remote_gpu_options_.gpu_barriers, remote_gpu_options_.gpu_per_command, remote_gpu_options_.gpu_readbacks, message_);
        }
        ImGui::TextWrapped("Export directory: %s", live_root_.u8string().c_str());
        ImGui::Checkbox("Extra GPU bindings",&remote_gpu_options_.gpu_bindings);
        ImGui::SameLine();ImGui::Checkbox("Extra GPU barriers",&remote_gpu_options_.gpu_barriers);
        ImGui::Checkbox("Extra per-command timestamps",&remote_gpu_options_.gpu_per_command);
        ImGui::Checkbox("Extra texture / buffer readbacks",&remote_gpu_options_.gpu_readbacks);
    }
    ImGui::Separator();
    ImGui::TextWrapped("Search applies to timing events, shaders and resources.");
    ImGui::InputTextWithHint("##search", "Search...", search_, sizeof(search_));
    ImGui::TextWrapped("Measured: solid bars. Derived: reconstructed passes/frame candidates. Unavailable: "
                       "missing channel, collector or calibration.");
    if(hosted_)ImGui::EndChild();else ImGui::End();
}
bool Application::Connect(uint32_t pid) {
    if (!sessions_.Open()) {
        message_ = "No Windows collector session";
        return false;
    }
    for (const auto &entry : sessions_.List())
        if (entry.process_id == pid) {
            auto client = std::make_unique<cygi::SessionClient>();
            if (!client->Connect(entry)) {
                message_ = client->LastError();
                return false;
            }
            client_ = std::move(client);
            wchar_t exe[32768]{};
            GetModuleFileNameW(nullptr, exe, 32768);
            live_root_ = fs::path(exe).parent_path() / "Captures" / NewCache().filename();
            fs::create_directories(live_root_);
            std::ofstream(live_root_ / "receiver.log") << "Connected to PID " << pid << "\n";
            client_->SetLevel(cygi::TrackingLevel::pass_timing);
            client_->RequestFullSync();
            delayed_timings_.clear();
            saved_frames_.clear();
            pending_frames_.clear();
            live_state_.clear();
            active_first_frame_ = 0;
            finished_at_ = 0;
            message_ =
                "Collector connected; detailed captures will be saved after their GPU timestamps arrive";
            return true;
        }
    message_ = "Requested collector process is not available";
    return false;
}
void Application::Live() {
    if (!client_)
        return;
    auto &model = client_->Model();
    std::vector<fs::path> completed;
    {
        std::lock_guard<std::mutex> lock(model.Mutex());
        for (auto &t : model.TakeQueuedTimings())
            delayed_timings_[t.frame_index] = std::move(t);
        while (delayed_timings_.size() > 120)
            delayed_timings_.erase(delayed_timings_.begin());
        const auto &state = model.CaptureState();
        const auto status = std::to_string(static_cast<unsigned>(state.mode)) + ":" +
                            std::to_string(static_cast<unsigned>(state.stage)) + ":" +
                            std::to_string(state.first_frame) + ":" + std::to_string(state.frames_done);
        if (status != live_state_) {
            live_state_ = status;
            std::ofstream(live_root_ / "receiver.log", std::ios::app)
                << "capture " << status << " last_frame=" << model.LastFrame().index
                << " deep_frame=" << model.DeepFrame().index << " timings=" << delayed_timings_.size()
                << "\n";
        }
        if (state.mode == cygi::CaptureMode::deep && state.frames_done) {
            if (active_first_frame_ != state.first_frame) {
                active_first_frame_ = state.first_frame;
                finished_at_ = 0;
            }
            for (uint32_t i = 0; i < state.frames_done; ++i) {
                auto index = state.first_frame + i;
                if (saved_frames_.count(index) || pending_frames_.count(index))
                    continue;
                const auto *frame = model.HasDeepFrame() && model.DeepFrame().index == index
                                        ? &model.DeepFrame()
                                        : model.FrameByIndex(index);
                if (frame)
                    pending_frames_[index] = *frame;
            }
        }
        if (state.stage == cygi::CaptureStage::finished) {
            const double now = ImGui::GetTime();
            if (finished_at_ == 0)
                finished_at_ = now;
            for (auto it = pending_frames_.begin(); it != pending_frames_.end();) {
                auto index = it->first;
                auto timing = delayed_timings_.find(index);
                bool missing = timing == delayed_timings_.end();
                bool timeout = now - finished_at_ > 60.;
                bool unsupported = model.HasSession() &&
                                   (model.Session().capability_flags & cygi::kCapTimestampQueries) == 0;
                if (missing && !timeout && !unsupported) {
                    ++it;
                    continue;
                }
                cygi::FrameTimings empty;
                empty.frame_index = index;
                const auto &result = missing ? empty : timing->second;
                cygi::FrameGraph graph;
                graph.Build(model, it->second);
                auto directory = live_root_ / std::to_string(index);
                std::string error;
                if (cygi::CaptureArchive::Save(model, graph, directory, error, &it->second, &result)) {
                    std::ofstream(live_root_ / "receiver.log", std::ios::app)
                        << "saved " << index << " timing=" << (!missing)
                        << " events=" << it->second.events.size() << "\n";
                    auto metadata = ReadJson(directory / "capture.json");
                    metadata["timing_status"] = cygi::Json(missing ? "unavailable" : "measured");
                    metadata["timing_reason"] =
                        cygi::Json(missing ? (unsupported ? "unsupported" : "timeout after 60 seconds") : "");
                    metadata["collector_process_id"] = cygi::Json(client_->Entry().process_id);
                    metadata["collector_device_index"] = cygi::Json(client_->Entry().device_index);
                    metadata["ipc_dropped_bytes"] = cygi::Json(std::to_string(client_->DroppedBytes()));
                    std::ofstream(directory / "capture.json") << metadata.Write(2);
                    saved_frames_.insert(index);
                    completed.push_back(directory);
                    it = pending_frames_.erase(it);
                } else {
                    message_ = error;
                    ++it;
                }
            }
        }
    }
    for (auto &path : completed) {
        gpu_frames_.push_back(path);
        selected_frame_ = gpu_frames_.size() - 1;
        LoadGpu(path);
        message_ = "Saved execution frame " + std::to_string(gpu_info_.frame_index) + "; GPU timing " +
                   ReadJson(path / "capture.json")["timing_status"].AsString();
    }
}
void Application::Gpu() {
    bool visible = BeginView("GPU frame");
    if (focus_gpu_) {
        auto *window = ImGui::GetCurrentWindow();
        if (window->DockNode && window->DockNode->TabBar) {
            window->DockNode->SelectedTabId = window->TabId;
            window->DockNode->TabBar->NextSelectedTabId = window->TabId;
            focus_gpu_ = false;
        }
    }
    if (!visible) {
        ImGui::End();
        return;
    }
    if (!gpu_) {
        ImGui::TextWrapped("No command collector data in this capture. Unreal native GPU timing remains in "
                           "Timing when its channel is available.");
        ImGui::End();
        return;
    }
    const auto &frame = gpu_->LastFrame();
    ImGui::Text("Frame %llu  |  %u draws  |  %u dispatches", frame.index, frame.draw_count,
                frame.dispatch_count);
    ImGui::TextWrapped("GPU clock is separate: CPU alignment unavailable without calibration. Pass names and "
                       "graph edges are derived.");
    auto p = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;
    auto *draw = ImGui::GetWindowDrawList();
    double length = std::max(1e-9, gpu_timeline_.Length());
    ImGui::InvisibleButton("gpu_passes", ImVec2(width, 38));
    for (size_t i = 0; i < graph_.Passes().size() && i < gpu_timeline_.Spans().size(); ++i) {
        auto &s = gpu_timeline_.Spans()[i];
        auto &pass = graph_.Passes()[i];
        float a = p.x + width * static_cast<float>(s.start / length),
              b = p.x + width * static_cast<float>(s.end / length);
        draw->AddRectFilled(ImVec2(a, p.y), ImVec2(b, p.y + 30),
                            s.measured ? IM_COL32(111, 148, 70, 255) : IM_COL32(125, 103, 61, 255));
        if (b - a > 50) {
            draw->PushClipRect(ImVec2(a, p.y), ImVec2(b, p.y + 30), true);
            draw->AddText(ImVec2(a + 3, p.y + 5), IM_COL32(255, 255, 255, 255), pass.name.c_str());
            draw->PopClipRect();
        }
        if (ImGui::IsMouseHoveringRect(ImVec2(a, p.y), ImVec2(b, p.y + 30)))
            ImGui::SetTooltip("%s (derived name, %.0f%%)\n%.4f %s; position %s", pass.name.c_str(),
                              pass.confidence * 100, s.Length(), gpu_timeline_.Timed() ? "ms" : "commands",
                              s.measured ? "measured" : "derived");
    }
    ImGui::TextDisabled("%s", gpu_timeline_.Timed()
                                  ? (gpu_timeline_.MeasuredPositions()
                                         ? "Measured GPU offsets where available; brown bars are derived"
                                         : "Durations measured; positions reconstructed sequentially")
                                  : "Command order only; GPU timestamps unavailable");
    if (ImGui::BeginTabBar("GPU data")) {
        if (ImGui::BeginTabItem("Commands")) {
            if (ImGui::BeginTable("commands", 5,
                                  ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                                      ImGuiTableFlags_ScrollY)) {
                Columns({"Command", "Type", "Pipeline", "Resource", "GPU time"});
                ImGuiListClipper clip;
                clip.Begin(static_cast<int>(frame.events.size()));
                while (clip.Step())
                    for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                        auto &e = frame.events[i];
                        ImGui::TableNextRow();
                        ImGui::TableNextColumn();
                        auto label = std::to_string(e.index);
                        if (ImGui::Selectable(label.c_str(), selected_event_ == i,
                                              ImGuiSelectableFlags_SpanAllColumns)) {
                            selected_event_ = i;
                            selected_cpu_ = -1;
                        }
                        ImGui::TableNextColumn();
                        ImGui::TextUnformatted(cygi::EventKindName(e.kind));
                        ImGui::TableNextColumn();
                        ImGui::Text("%u", e.pipeline_id);
                        ImGui::TableNextColumn();
                        ImGui::Text("%u", e.primary_resource);
                        ImGui::TableNextColumn();
                        auto t = gpu_->Timings().by_event.find(e.index);
                        if (t != gpu_->Timings().by_event.end() && gpu_->Timings().frequency)
                            ImGui::Text("%.4f ms", gpu_->Timings().Milliseconds(t->second));
                        else
                            ImGui::TextDisabled("Unavailable");
                    }
                ImGui::EndTable();
            }
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Passes / graph", nullptr,
                                focus_graph_ ? ImGuiTabItemFlags_SetSelected : 0)) {
            ImGui::TextWrapped("Reconstructed resource graph. Click a pass to inspect its first command. "
                               "Names and dependency edges are derived.");
            ImGui::BeginChild("pass_graph", ImVec2(0, 250), true, ImGuiWindowFlags_HorizontalScrollbar);
            const auto origin = ImGui::GetCursorScreenPos();
            const float nodeW = 220, nodeH = 68, strideX = 285, strideY = 110;
            auto *canvas = ImGui::GetWindowDrawList();
            std::map<uint32_t, ImVec2> positions;
            for (size_t i = 0; i < graph_.Passes().size(); ++i)
                positions[graph_.Passes()[i].id] =
                    ImVec2(origin.x + 20 + (i % 3) * strideX, origin.y + 20 + (i / 3) * strideY);
            for (const auto &edge : graph_.Edges()) {
                auto a = positions.find(edge.from_pass), b = positions.find(edge.to_pass);
                if (a == positions.end() || b == positions.end())
                    continue;
                ImVec2 start(a->second.x + nodeW, a->second.y + nodeH / 2),
                    end(b->second.x, b->second.y + nodeH / 2);
                canvas->AddBezierCubic(start, ImVec2(start.x + 35, start.y), ImVec2(end.x - 35, end.y), end,
                                       IM_COL32(101, 130, 144, 150), 1.5f);
            }
            for (const auto &pass : graph_.Passes()) {
                auto pos = positions[pass.id];
                canvas->AddRectFilled(pos, ImVec2(pos.x + nodeW, pos.y + nodeH), IM_COL32(39, 50, 59, 255),
                                      4);
                canvas->AddRect(pos, ImVec2(pos.x + nodeW, pos.y + nodeH), IM_COL32(87, 124, 147, 255), 4);
                canvas->PushClipRect(pos, ImVec2(pos.x + nodeW, pos.y + nodeH), true);
                canvas->AddText(ImVec2(pos.x + 7, pos.y + 7), IM_COL32(220, 225, 230, 255),
                                pass.name.c_str());
                char label[128];
                std::snprintf(label, sizeof(label), "%u draws / %u dispatches\nDerived, confidence %.0f%%",
                              pass.draw_count, pass.dispatch_count, pass.confidence * 100);
                canvas->AddText(ImVec2(pos.x + 7, pos.y + 27), IM_COL32(160, 175, 185, 255), label);
                canvas->PopClipRect();
                if (ImGui::IsWindowHovered() &&
                    ImGui::IsMouseHoveringRect(pos, ImVec2(pos.x + nodeW, pos.y + nodeH)) &&
                    ImGui::IsMouseClicked(0)) {
                    for (size_t i = 0; i < frame.events.size(); ++i)
                        if (frame.events[i].index == pass.first_event) {
                            selected_event_ = static_cast<int>(i);
                            selected_cpu_ = -1;
                            break;
                        }
                }
            }
            ImGui::Dummy(ImVec2(strideX * 3, strideY * ((graph_.Passes().size() + 2) / 3) + 20));
            ImGui::EndChild();
            ImGui::SeparatorText("Resource dependencies (derived)");
            for (auto &edge : graph_.Edges())
                ImGui::Text("Pass %u -> Pass %u  /  Resource %u", edge.from_pass, edge.to_pass,
                            edge.resource_id);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Shaders")) {
            for (auto &s : gpu_->Shaders())
                if (s.id && Match(s.signature.ToHex(), search_))
                    ImGui::Text("%u  %s  %s  %u bytes  %u calls", s.id, cygi::ShaderStageName(s.stage),
                                s.signature.ToShortHex(20).c_str(), s.code_size,
                                s.draws_this_frame + s.dispatches_this_frame);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Resources")) {
            for (auto &r : gpu_->Resources())
                if (r.id && Match(r.Describe(), search_))
                    ImGui::TextWrapped("%s", r.Describe().c_str());
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Pipelines")) {
            for (auto &q : gpu_->Pipelines())
                if (q.id) {
                    ImGui::Text("%u  %s  shaders:", q.id, q.is_compute ? "Compute" : "Graphics");
                    for (auto id : q.shader_ids) {
                        ImGui::SameLine();
                        ImGui::Text("%u", id);
                    }
                }
            ImGui::EndTabItem();
        }
        focus_graph_ = false;
        ImGui::EndTabBar();
    }
    ImGui::End();
}
bool Application::FocusVulkanPassContents(){
    if(!native_.vulkan_contents)return false;
    for(size_t i=0;i<native_.events.size();++i){const auto& e=native_.events[i];if(e.thread!="GPU / Vulkan render passes (approximate alignment)")continue;bool ambiguous=false;if(native_.vulkan_contents->Find(e,ambiguous).empty())continue;selected_cpu_=int(i);show_details_=true;session_view_=false;ImGui::SetWindowFocus(WindowName("Selection").c_str());return true;}
    return false;
}
void Application::Details() {
    BeginView("Selection");
    if (selected_cpu_ >= 0 && selected_cpu_ < static_cast<int>(native_.events.size())) {
        auto &e = native_.events[selected_cpu_];
        ImGui::TextWrapped("%s", e.name.c_str());
        if(e.frame_type>=0)ImGui::Text("Native %s frame index: %llu / separate from TimerId",
            e.frame_type==0?"game":"rendering",static_cast<unsigned long long>(e.frame_index));
        ImGui::Text("Thread: %s\nStart: %.6f ms\nDepth: %d", e.thread.c_str(), e.start * 1000, e.depth);
        if (e.open_ended)
            ImGui::TextWrapped("Unfinished scope: measured start, unavailable end/duration. Display clipped "
                               "to the last native event.");
        else
            ImGui::Text("End: %.6f ms\nInclusive: %.6f ms (measured)", e.end * 1000,
                        (e.end - e.start) * 1000);
        if(e.thread=="GPU / Vulkan render passes (approximate alignment)"){
            ImGui::SeparatorText("Vulkan subpass contents");bool ambiguous=false;
            auto rows=native_.vulkan_contents?native_.vulkan_contents->Find(e,ambiguous):std::vector<cyvpd::Record>{};
            if(rows.empty())ImGui::TextWrapped("%s",ambiguous?"Pass association ambiguous; no unique contents.":native_.vulkan_contents_error.empty()?"Contents unavailable in this capture. Record a new trace with the updated plugin and native reader.":native_.vulkan_contents_error.c_str());
            else{ImGui::Text("Capture-local pass: %llu",rows[0].pass);ImGui::Text("Submission: %llu",rows[0].submission);
                for(const auto& r:rows){ImGui::PushID(int(r.subpass));if(ImGui::TreeNodeEx("subpass",ImGuiTreeNodeFlags_DefaultOpen,"Subpass %u",r.subpass)){
                    ImGui::Text("Direct draws: %llu",r.direct);ImGui::Text("Indexed draws: %llu",r.indexed);
                    ImGui::Text("Indirect calls: %llu",r.indirect);ImGui::Text("Requested indirect commands: %llu",r.indirect_commands);
                    ImGui::Text("Requested vertices: %llu",r.vertices);ImGui::Text("Requested indices: %llu",r.indices);
                    ImGui::Text("Direct instance arguments: %llu",r.instances);
                    if(r.flags&1)ImGui::TextWrapped("Partial coverage: some secondary command buffers were recorded before capture.");
                    if(r.flags&2)ImGui::TextWrapped("Indirect vertex, index and instance arguments unavailable; GPU buffers were not read.");
                    ImGui::TreePop();}ImGui::PopID();}
                ImGui::TextWrapped("Observed Vulkan command arguments. Requested vertices/indices are not measured shader invocations. Only intercepted draw APIs are counted.");
            }
            ImGui::TextWrapped("Individual subpass GPU costs and Unreal group names unavailable. The parent pass duration is measured; no duration is assigned to its subpasses.");
        }
    } else if (gpu_ && selected_event_ >= 0 &&
               selected_event_ < static_cast<int>(gpu_->LastFrame().events.size())) {
        const auto &f = gpu_->LastFrame();
        auto &e = f.events[selected_event_];
        ImGui::Text("Command %u: %s", e.index, cygi::EventKindName(e.kind));
        ImGui::Text("Pipeline %u\nResources %u / %u\nArguments %u / %u / %u / %u", e.pipeline_id,
                    e.primary_resource, e.secondary_resource, e.a, e.b, e.c, e.d);
        ImGui::SeparatorText("Descriptors");
        if (auto state = f.DrawStateOfEvent(e.index)) {
            for (auto &binding : state->bindings)
                ImGui::Text("Slot %u / resource %u", binding.slot, binding.resource_id);
            if (state->TablesUnresolved())
                ImGui::TextDisabled("Descriptor tables unresolved");
        } else
            ImGui::TextDisabled("Unavailable: no deep bindings");
        ImGui::SeparatorText("Barriers");
        if (auto barriers = f.BarriersOfEvent(e.index))
            ImGui::Text("%zu recorded transitions", barriers->entries.size());
        else
            ImGui::TextDisabled("Unavailable for this command");
        ImGui::SeparatorText("Unreal object");
        ImGui::TextWrapped(
            "No measured object identity. Instancing and Nanite may produce multiple candidates. The editor "
            "bridge requires an explicit session-scoped object mapping.");
    } else
        ImGui::TextWrapped("Select a native timing event or a GPU command.");
    if (range_start_ != range_end_)
        ImGui::Text("Selected interval: %.4f ms", std::abs(range_end_ - range_start_) * 1000);
    ImGui::End();
}
void Application::PlatformMetrics() {
    BeginView("Platform metrics");
    ImGui::TextWrapped("Raw Meta report values. Column names are retained from the report; units and clock "
                       "alignment have not been normalized. Up to 100,000 records are displayed; the "
                       "complete report remains in the archive.");
    if (metrics_.empty())
        ImGui::TextDisabled("Unavailable: no Meta CSV in this capture");
    else if (metrics_[0].size() > 0 && metrics_[0].size() <= 64 &&
             ImGui::BeginTable("meta_report", static_cast<int>(metrics_[0].size()),
                               ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY)) {
        for (const auto &column : metrics_[0])
            ImGui::TableSetupColumn(column.c_str(), ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableHeadersRow();
        ImGuiListClipper clip;
        clip.Begin(static_cast<int>(metrics_.size() - 1));
        while (clip.Step())
            for (int i = clip.DisplayStart; i < clip.DisplayEnd; ++i) {
                ImGui::TableNextRow();
                const auto &row = metrics_[i + 1];
                for (size_t j = 0; j < metrics_[0].size(); ++j) {
                    ImGui::TableNextColumn();
                    if (j < row.size())
                        ImGui::TextUnformatted(row[j].c_str());
                    else
                        ImGui::TextDisabled("Unavailable");
                }
            }
        ImGui::EndTable();
    } else
        ImGui::TextDisabled("Unsupported report column count; raw file retained");
    ImGui::End();
}
} // namespace cyinsights
