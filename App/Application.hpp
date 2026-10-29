// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "Analysis/FrameTimeline.hpp"
#include "Capture.hpp"
#include "RemoteCapture.hpp"
#include "NativeTrace.hpp"
#include "NativeSetup.hpp"
#include "CounterTrace.hpp"
#include "ComparisonTrace.hpp"
#include "Session/CaptureArchive.hpp"
#include "Session/SessionClient.hpp"
#include "TimingModel.hpp"
#include "PagedTimingStore.hpp"
#include "Distribution.hpp"
#include "TaskTrace.hpp"
#include "ActivityTrace.hpp"
#include "AiRpc.hpp"
#include "OvrCapture.hpp"
#include "QuestTileReplay.hpp"
#include <imgui.h>
#include <map>
#include <memory>
#include <set>
#include <cmath>
#include <unordered_map>
#include <functional>
namespace cyinsights {
class Application {
  public:
    struct NavigationState {
        double start=0,end=0,frames_start=0,frames_end=0;
        float x=0,y=0,width=0,height=0,ruler_x=0,ruler_y=0,ruler_width=0;
        uint64_t model_version=0;bool dragging=false;int visible_rows=0,recorded_rows=0;
        float tracks_x=0,tracks_y=0,tracks_scroll=0,tracks_scroll_max=0;
        float measure_x=0,measure_y=0,measure_width=0,measure_height=0;
    };
    NavigationState NavigationView() const {return {view_start_,view_end_,frames_start_,frames_end_,navigation_x_,navigation_y_,navigation_width_,navigation_height_,ruler_x_,ruler_y_,ruler_width_,model_version_,drag_surface_!=DragSurface::None,visible_rows_,recorded_rows_,tracks_x_,tracks_y_,tracks_scroll_,tracks_scroll_max_,measure_x_,measure_y_,measure_width_,measure_height_};}
    struct InteractionState {
        float scope_x=0,scope_y=0,menu_x=0,menu_y=0,hist_x=0,hist_y=0,hist_width=0;
        float blank_x=0,blank_y=0;
        std::string scope_name,filter_name,function_name;
        double selection_a=0,selection_b=0,hist_a=0,hist_b=0;
        uint64_t count=0,matched=0;int bin=-1;std::array<uint64_t,64> bins{};bool separate_dock=false;
    };
    InteractionState InteractionView() const;
    struct FrameStatsState {uint64_t rows=0;bool selected_interval=false,curve=false;};
    FrameStatsState FrameStatsViewState() const {return {frame_stat_rows_,range_start_!=range_end_,selected_counter_>=0&&static_cast<size_t>(selected_counter_)<counters_->series.size()&&counters_->series[selected_counter_].pinned};}
    struct ActivityState {uint64_t events=0,matches=0,plot_queries=0;double a=0,b=0;};
    ActivityState ActivityView() const {return {activity_store_?activity_store_->count:0,activity_result_.details.lanes.empty()?0:activity_result_.details.lanes.front().matched,activity_plot_queries_,activity_result_.details.start,activity_result_.details.end};}
    Application();
    ~Application();
    void SetBrandLogo(ImTextureID texture){brand_logo_=texture;}
    void FocusSession(){session_view_=true;}
    bool FocusVulkanPassContents();
    void FocusCaptureOptions(){session_view_=true;focus_capture_options_=true;}
    void Draw();
    void DrawMenuBar();
    void DrawHosted(bool visible);
    void DrawInactive();
    void ConfigureDocument(uint64_t id,std::function<void(const fs::path&)> open);
    std::string WindowName(const char* name) const;
    const fs::path& DocumentPath() const {return document_path_;}
    bool CanCloseDocument() const {return !remote_.Snapshot().Busy();}
    bool Open(const fs::path &file);
    bool Connect(uint32_t process_id);
    bool FocusSlowFrame();
    void FocusDistribution(){show_distribution_=true;focus_distribution_=true;}
    void FocusTasks(){show_tasks_=true;focus_tasks_=true;}
    void FocusActivities(){show_activities_=true;focus_activities_=true;activity_dock_pending_=true;}
    void FocusFrameStats(){show_frame_stats_=true;frame_stats_dock_pending_=true;session_view_=false;}
    void FilterFunction(const std::string& name);
    void OpenFunctionHistogram(const std::string& name);
    bool InspectFirstTask();
    void EnableAi(bool control=false,bool capture=false);
    void FocusAi(){show_ai_=true;focus_ai_=true;session_view_=false;}
    void FocusAiMethod(){FocusAi();ai_requested_tab_=3;}
    std::future<cygi::Json> RunLocalAiTool(const std::string& operation,const cygi::Json& parameters);
    cygi::Json StandaloneAiState() const;
    void WriteAnalysisReport(const fs::path& output) const;
    bool AnalysisBusy() const { return native_import_pending_ || !pending_capture_.empty() || native_.Busy() || model_task_.valid() || counter_reader_.Busy() || comparison_reader_.Busy() || distribution_task_.valid() || distribution_pending_ || tasks_pending_ || task_reader_.Busy() || tasks_query_.valid() || links_query_.valid() || activity_reader_.Busy() || activity_query_.valid() || activity_pending_; }
    void FocusGpu() {
        show_gpu_ = true;
        focus_gpu_ = true;
    }
    void FocusGraph() {
        FocusGpu();
        focus_graph_ = true;
    }

  private:
    void Tick();
    void DrawPanels();
    void OvrHeatmap();QuestTileReplay quest_tiles_;
    std::shared_ptr<cyovr::Capture> ovr_capture_;
    bool show_ovr_=false;
    int ovr_surface_=0,ovr_tile_=-1,ovr_stage_=0;
    void RequestOpen(const fs::path& file);
    void ChooseAndOpenCaptures();
    bool BeginView(const char* name,bool* open=nullptr,ImGuiWindowFlags flags=0);
    // Geometry is a value; ImGuiViewport owns backend bookkeeping and must
    // never be copied into a document (its destructor asserts on those pointers).
    struct WorkspaceGeometry {ImGuiID ID=0;ImVec2 WorkPos{},WorkSize{};};
    WorkspaceGeometry WorkspaceViewport() const {
        if(hosted_)return workspace_viewport_;
        const auto* viewport=ImGui::GetMainViewport();return {viewport->ID,viewport->WorkPos,viewport->WorkSize};
    }
    uint64_t document_id_=0;
    fs::path document_path_;
    std::function<void(const fs::path&)> open_document_;
    bool hosted_=false,document_visible_=true;
    WorkspaceGeometry workspace_viewport_{};
    ImGuiID document_dock_=0;
    ImTextureID brand_logo_=0;
    void AiTick();
    void AiView();
    void AiMethodView();
    void PrepareAiTool(const std::string& operation);
    void AiDispatch(const std::shared_ptr<AiRequest>& request,bool external=true);
    bool AiExtendedDispatch(const std::string& operation,const cygi::Json& parameters,const std::shared_ptr<AiRequest>& request);
    cygi::Json AiState() const;
    void StartAiIssues();
    AiRpc ai_rpc_;
    struct AiWork {std::shared_ptr<AiRequest> request;std::future<cygi::Json> result;uint64_t generation=0;};
    std::vector<AiWork> ai_work_;
    std::future<cygi::Json> ai_issues_task_;
    std::atomic<bool> ai_issues_cancel_{false};
    cygi::Json ai_issues_=cygi::Json::Object(),ai_catalog_;
    std::vector<cygi::Json> ai_findings_;
    uint64_t ai_generation_=0,ai_issues_generation_=0;
    double ai_issues_a_=NAN,ai_issues_b_=NAN,ai_selection_changed_=0;
    bool show_ai_=false,focus_ai_=false,ai_control_=false,ai_capture_=false,ai_auto_=true,ai_french_=false;
    float ai_fps_=60;
    std::string ai_message_,ai_result_;
    char ai_parameters_[16384]="{}";
    int ai_operation_=0;
    int ai_requested_tab_=-1;
    void Layout(unsigned id);
    void Timing();
    void Frames();
    void FrameGlyphs(float width);
    FrameCause Cause(size_t frame) const;
    void FrameBackground(float x,float y,float height,FrameDomain domain);
    double frame_glyph_start_=NAN,frame_glyph_end_=NAN;
    float frame_glyph_width_=-1;
    float frame_target_fps_=60;
    bool show_frame_causes_=true;
    void Gpu();
    void Details();
    void Statistics();
    void Session();
    void PlatformMetrics();
    void Counters();
    void CounterTracks(float width);
    void FrameStatsView();
    uint64_t frame_stat_rows_=0;
    bool show_frame_stats_=false,frame_stats_dock_pending_=false;
    bool frame_stat_samples_requested_=false;
    char frame_stats_search_[256]{};
    int frame_stats_group_=0;
    std::vector<std::string> frame_stat_groups_;
    std::vector<size_t> frame_stat_metrics_;
    std::string frame_stat_filter_key_;
    uint64_t frame_stat_catalog_revision_=UINT64_MAX;
    size_t frame_stat_catalog_size_=0;
    void ActivitiesView();
    void ActivityTracks(float width);
    void ActivityTick();
    void StartActivities();
    void DockActivities();
    ActivityTrace activity_reader_;std::shared_ptr<ActivityStore> activity_store_;
    struct ActivityResult {ActivityWindow visible,details;bool has_visible=false,has_details=false;};
    std::future<ActivityResult> activity_query_;std::atomic<bool> activity_cancel_{false};
    ActivityResult activity_result_;std::vector<uint8_t> activity_pinned_;
    std::string activity_message_,activity_request_,activity_ready_;
    std::string activity_details_request_,activity_details_ready_,activity_details_observed_;
    double activity_details_changed_at_=0,activity_requested_at_=-1;bool activity_pending_=false;
    uint64_t activity_plot_queries_=0,activity_detail_queries_=0;
    char activity_search_[256]{};int activity_lane_=0;uint64_t activity_page_=0,activity_revision_=0;
    bool show_activities_=false,activity_auto_started_=false,focus_activities_=false,activity_dock_pending_=false;float activity_width_=1200;
    void UpdateCounters();
    void FitCapture();
    void Comparison();
    void DistributionView();
    void TasksView();
    void StartDistribution(int bin=-1,uint64_t page=0);
    void StartTasks(uint64_t page=0);
    void StartNativeTasks();
    void PollTraceAnalysis();
    void FollowTimelineSelection();
    void DockFunctionHistogram();
    void FocusInterval(double start,double end,uint32_t thread,bool native_identity);
    bool show_distribution_=false,show_tasks_=false,focus_distribution_=false,focus_tasks_=false;
    bool tasks_auto_started_=false;
    int distribution_source_=0,distribution_domain_=0,distribution_metric_=1,distribution_track_=0;
    bool distribution_roots_=true,distribution_log_y_=true,distribution_log_x_=true,distribution_dirty_=true;
    double analysis_a_=NAN,analysis_b_=NAN,distribution_a_=0,distribution_b_=0;
    bool analysis_whole_=true;
    bool follow_timeline_selection_=true,distribution_pending_=false,tasks_pending_=false,histogram_dock_pending_=false;
    double observed_selection_a_=NAN,observed_selection_b_=NAN,selection_changed_at_=0;
    std::string distribution_function_,distribution_result_function_;
    std::optional<Distribution> distribution_summary_;
    std::string distribution_summary_key_;
    std::string distribution_request_key_;
    std::string distribution_message_,distribution_label_;
    int distribution_result_source_=0,distribution_result_track_=0;
    int64_t distribution_task_thread_=-1;
    Distribution distribution_;
    std::future<Distribution> distribution_task_;std::atomic<bool> distribution_cancel_{false};
    TaskTrace task_reader_;std::shared_ptr<TaskStore> tasks_store_;
    std::string tasks_message_;bool tasks_candidates_=false,tasks_result_candidates_=false;
    int64_t tasks_thread_=-1;uint64_t tasks_page_=0;
    char tasks_search_[256]{};
    TaskPage tasks_;std::future<TaskPage> tasks_query_;std::atomic<bool> tasks_cancel_{false};
    std::optional<TaskRecord> selected_task_;std::vector<TaskLink> task_links_;
    std::future<std::vector<TaskLink>> links_query_;std::atomic<bool> links_cancel_{false};
    uint64_t links_request_id_=UINT64_MAX;
    void OpenComparison(const fs::path& file);
    void Toolbar();
    void PanelMenu();
    void LoadStatus();
    void NativeSetupTick();
    void NativeSetupView();
    void SelectAnalyzer(const fs::path& path);
    bool AddEngineRoot(const fs::path& root);
    NativeSetup native_setup_;
    bool native_import_pending_=false,setup_dismissed_=false;
    char engine_root_input_[4096]{};
    std::string engine_registration_error_;
    bool OpenNow(const fs::path& file);
    void LoadWindow();
    enum class DragSurface {None,Frames,Ruler,Tracks,Measurement};
    void BeginTimingDrag(DragSurface surface,int button,float width,bool zoom=true);
    void AdvanceTimingDrag();
    void MeasurementRuler(float width);
    void Relations(bool callers);
    void Messages();
    void UpdateTiming();
    void TimerTable(const char *id, const std::vector<TimerTotal> &rows, bool selectable);
    struct TimerTableCache {
        std::vector<size_t> filtered;
        std::string query;
        uint64_t model_version=UINT64_MAX,statistics_version=UINT64_MAX;
        int selected_timer=-2,column=-1,direction=-1;
    };
    std::map<std::string,TimerTableCache> timer_tables_;
    uint64_t statistics_version_=0,timing_search_version_=UINT64_MAX;
    std::vector<uint32_t> timer_colours_;
    void FitTiming();
    void StepFrame(int direction);
    void FocusTimer(uint32_t timer);
    TimingModel timing_model_;
    std::shared_ptr<CounterModel> counters_=std::make_shared<CounterModel>();
    CounterTrace counter_reader_;
    int selected_counter_ = -1;
    bool show_counters_ = true, meta_counters_only_ = false, show_availability_ = false;
    bool hide_empty_counters_ = true;
    char counter_search_[256]{};
    std::string counter_message_;
    double counter_stats_start_ = -1, counter_stats_end_ = -1;
    std::vector<CounterStats> counter_stats_;
    uint64_t counter_stats_version_=0,counter_catalog_revision_=0;std::string counter_filter_key_,counter_order_key_;std::vector<size_t> counter_visible_;
    struct CounterPlotCache {double a=-1,b=-1;int width=0;std::vector<CounterBucket> buckets;};
    std::vector<CounterPlotCache> counter_plots_;
    struct TimingAnalysis {
        std::vector<TimingEvent> events;
        TimingModel model;
        TimingComparisonIndex comparison;
        std::vector<ComparisonValue> summary;
        double window_start=0,window_end=0;
        bool aggregated=false;
        std::string exact_name;
    };
    std::atomic<bool> model_cancel_{false};
    std::future<TimingAnalysis> model_task_;
    // Retain one old display snapshot until the next preparation worker can
    // reclaim it, keeping string/vector destruction out of a drag frame.
    std::unique_ptr<TimingAnalysis> retired_timing_;
    fs::path pending_capture_;
    bool pending_shown_=false,window_aggregated_=false,counter_samples_requested_=false,cancelled_load_=false;
    double load_started_=0,loaded_start_=NAN,loaded_end_=NAN;
    double requested_start_=NAN,requested_end_=NAN,requested_stats_start_=NAN,requested_stats_end_=NAN;
    std::string load_error_;
    double overview_drag_start_=0,overview_drag_end_=1;
    float overview_drag_x_=0,overview_drag_y_=0;
    DragSurface drag_surface_=DragSurface::None;
    DragSurface drag_released_surface_=DragSurface::None;
    int drag_button_=0;bool drag_moved_=false,drag_zoom_=true;
    float drag_width_=1;
    float drag_scroll_start_=0,tracks_scroll_=0,tracks_scroll_max_=0;
    std::string drag_context_name_;
    float timing_blank_x_=0,timing_blank_y_=0;
    double model_request_time_=-1,frames_start_=0,frames_end_=1;
    uint64_t model_version_=0;
    float ruler_x_=0,ruler_y_=0,ruler_width_=0;
    float measure_x_=0,measure_y_=0,measure_width_=0,measure_height_=0,measure_drag_origin_=0;
    int visible_rows_=0,recorded_rows_=0;
    float tracks_x_=0,tracks_y_=0;
    int overview_width_=0;
    float navigation_x_=0,navigation_y_=0,navigation_width_=0,navigation_height_=0;
    std::vector<FrameGlyph> frame_glyphs_;
    TimingComparisonIndex comparison_index_;
    std::vector<ComparisonValue> comparison_a_summary_;
    ComparisonTrace comparison_reader_;
    std::shared_ptr<ComparisonDataset> comparison_b_;
    std::vector<ComparisonRow> comparison_rows_;
    std::string comparison_message_;
    double comparison_a_start_=-1,comparison_a_end_=-1,comparison_b_start_=0,comparison_b_end_=1;
    bool show_comparison_=false;
    char comparison_search_[256]{};
    int comparison_mode_=0,comparison_domain_=0,comparison_frame_b_=0;
    std::vector<TimerTotal> callers_, callees_;
    int selected_timer_ = -1;
    double statistics_start_ = -1, statistics_end_ = -1;
    int statistics_timer_ = -2;
    bool show_frames_ = true, show_timing_ = true, show_timers_ = true, show_callers_ = true,
         show_callees_ = true, show_gpu_ = false, show_messages_ = true;
    bool session_view_ = false, reset_layout_ = false;
    RemoteCapture remote_;
    bool remote_auto_open_=true;
    fs::path remote_opened_archive_;
    char remote_ip_[64]{};
    int remote_port_ = 41855;
    float remote_seconds_ = 0;
    bool remote_use_defaults_=true;
    bool focus_capture_options_=false;
    CaptureOptions remote_capture_,remote_gpu_options_;
    MetaQuestOptions remote_meta_;bool remote_tiles_=false,remote_tile_images_=false;float remote_tile_hz_=1;std::future<std::string> remote_share_;std::atomic<bool> remote_share_cancel_{false};std::string remote_share_message_;
    char remote_adb_[4096]{},remote_adb_device_[256]{},remote_android_package_[256]{};
    char remote_stat_groups_[513]="Game,UObjects,GPU";
    bool show_details_ = false, show_metrics_ = false;
    int track_filter_ = 0;
    bool show_tick_guides_ = true, hide_inactive_tracks_ = false;
    std::vector<std::string> messages_;
    std::string previous_message_;
    std::string timing_search_query_;
    std::string timing_exact_name_,timing_scope_context_,timing_filter_key_;
    std::vector<uint8_t> timing_timer_matches_;
    std::unordered_map<std::string,uint8_t> timing_name_matches_;
    std::string loaded_exact_name_;
    float scope_probe_x_=0,scope_probe_y_=0,context_hist_x_=0,context_hist_y_=0,hist_probe_x_=0,hist_probe_y_=0,hist_probe_width_=0;
    std::string scope_probe_name_;
    std::vector<uint8_t> timing_search_matches_;
    std::vector<std::vector<std::string>> metrics_;
    void LoadGpu(const fs::path &directory);
    void ImportNative();
    void Live();
    std::unique_ptr<cygi::SessionClient> client_;
    cygi::SessionDirectory sessions_;
    fs::path live_root_;
    std::map<uint64_t, cygi::FrameTimings> delayed_timings_;
    std::set<uint64_t> saved_frames_;
    std::map<uint64_t, cygi::FrameInfo> pending_frames_;
    uint64_t active_first_frame_ = 0;
    double finished_at_ = 0;
    std::string live_state_;
    Capture capture_;
    fs::path root_, trace_, cache_,capture_file_;
    std::string message_ = "Open a .cycapture or a native .utrace to begin";
    NativeTrace native_;
    std::shared_ptr<cygi::SessionModel> gpu_;
    cygi::CaptureInfo gpu_info_;
    cygi::FrameGraph graph_;
    cygi::FrameTimeline gpu_timeline_;
    std::vector<fs::path> gpu_frames_;
    size_t selected_frame_ = 0;
    std::vector<std::pair<double, double>> cpu_frames_;
    cygi::CaptureDiff diff_;
    bool has_diff_ = false;
    bool comparison_timed_ = false;
    int selected_event_ = -1, selected_cpu_ = -1;
    double view_start_ = 0, view_end_ = 1, range_start_ = 0, range_end_ = 0;
    char search_[256]{}, exe_[1024]{};
    bool first_ = true;
    bool focus_gpu_ = false;
    bool focus_graph_ = false;
    int depth_levels_ = 0; // All recorded depths; a limit is an explicit view setting.
};
fs::path ChooseFile(const wchar_t *filter, bool save = false);
fs::path ChooseFolder();
std::vector<fs::path> ChooseCaptures();
} // namespace cyinsights
