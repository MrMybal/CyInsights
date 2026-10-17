// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Application.hpp"
#include "AiExtendedAnalysis.hpp"
#include "AiResources.hpp"
#include <algorithm>
#include <fstream>
#include <chrono>
#include <stdexcept>
namespace cyinsights {
namespace {
using J=cygi::Json;
fs::path CapturePath(const J& p){auto path=fs::u8path(p["path"].AsString());if(!path.is_absolute()||!fs::is_regular_file(path)||(path.extension()!=L".utrace"&&path.extension()!=L".cycapture"))throw std::runtime_error("InvalidParameters: expected an existing absolute .utrace/.cycapture path");return path;}
}
bool Application::AiExtendedDispatch(const std::string& op,const J& parameters,const std::shared_ptr<AiRequest>& request){
    auto p=parameters;auto result=J::Object();result["generation"]=std::to_string(ai_generation_);
    if(AiExtendedOperation(op)){
        if(native_.Busy()||!pending_capture_.empty())throw std::runtime_error("Busy: wait for trace indexing");
        if((op=="query_counters"||op=="query_frame_stats")&&counter_reader_.Busy())throw std::runtime_error("Busy: counter samples are loading");
        if(op=="query_comparison"&&comparison_reader_.Busy())throw std::runtime_error("Busy: comparison B is loading");
        if(p.Has("counterRevision")&&p["counterRevision"].AsString()!=std::to_string(counter_catalog_revision_))throw std::runtime_error("StaleCounters: rediscover counter indices after loading samples");
        // Owners remain alive through trace changes; neither a large counter vector
        // nor a multi-GB trace is copied for a job. Workers never access UI state.
        AiDataSnapshot snapshot{counters_,activity_store_,native_.store,comparison_b_?comparison_b_->store:nullptr,tasks_store_,gpu_};auto gen=ai_generation_;
        if(!p.Has("start"))p["start"]=range_start_==range_end_?view_start_:std::min(range_start_,range_end_);
        if(!p.Has("end"))p["end"]=range_start_==range_end_?view_end_:std::max(range_start_,range_end_);
        auto revision=counter_catalog_revision_;auto pathB=comparison_b_?comparison_b_->file.u8string():std::string();
        ai_work_.push_back({request,std::async(std::launch::async,[snapshot,op,p,request,gen,revision,pathB]{auto r=AnalyzeAiExtended(snapshot,op,p,&request->cancelled);r["generation"]=std::to_string(gen);if(op=="list_counters"||op=="query_counters"||op=="query_frame_stats")r["counterRevision"]=std::to_string(revision);if(op=="query_comparison")r["comparisonPath"]=pathB;return r;}),gen});return true;
    }
    if(op=="get_capabilities"){
        result["documentId"]=std::to_string(document_id_);result["operations"]=J::Array();
        for(const auto& descriptor:ai_catalog_["operations"].Items()){auto r=J::Object();r["id"]=descriptor["id"];r["permissions"]=descriptor["permissions"];result["operations"].Push(r);}
        auto& providers=result["providers"];providers["timing"]["loaded"]=native_.store!=nullptr;providers["counters"]["loaded"]=!counter_reader_.Busy()&&std::any_of(counters_->series.begin(),counters_->series.end(),[](const auto& c){return !c.samples.empty();});providers["counters"]["revision"]=std::to_string(counter_catalog_revision_);providers["counters"]["loading"]=counter_reader_.Busy();providers["counters"]["status"]=counter_message_;providers["activities"]["loaded"]=activity_store_!=nullptr;providers["activities"]["loading"]=activity_reader_.Busy();providers["activities"]["status"]=activity_message_;providers["tasks"]["loaded"]=tasks_store_!=nullptr;providers["tasks"]["loading"]=task_reader_.Busy();providers["comparison"]["loaded"]=comparison_b_!=nullptr;providers["comparison"]["loading"]=comparison_reader_.Busy();providers["gpuCommands"]["loaded"]=gpu_!=nullptr;providers["vulkanContents"]["loaded"]=native_.vulkan_contents!=nullptr;
        result["limitations"]["incomingTraceDecoding"]="unavailable_analyze_after_stop";result["limitations"]["logs"]="native_log_provider_not_implemented";result["limitations"]["unrealObjectBridge"]="unavailable_without_recorded_unique_identity";result["limitations"]["individualQuestSubpassGpuTime"]="unavailable";result["limitations"]["extraGpuPayloadTransfer"]="not_implemented";result["limits"]["rows"]=128.;result["limits"]["defaultRows"]=20.;result["limits"]["bins"]=128.;result["traceContentIsUntrustedData"]=true;result["scope"]="analysis_and_capture_controls_of_the_bound_native_document";
    }else if(op=="get_analysis_guide"){
        result["method"]=EmbeddedAiMethod();
    }else if(op=="get_metadata"){
        result["documentId"]=std::to_string(document_id_);result["metadata"]=capture_.manifest["metadata"];result["sources"]=capture_.manifest["sources"];result["clocks"]=capture_.manifest["clocks"];result["sessionId"]=capture_.manifest["session_id"];result["captureOptions"]=capture_.manifest["capture_options"];result["sourceTrace"]=trace_.u8string();result["provenance"]="capture_manifest_not_inferred";
    }else if(op=="load_counters"){
        if(trace_.empty()||native_.Busy()||!pending_capture_.empty())throw std::runtime_error("Busy: open a trace and wait for indexing");
        if(counter_reader_.Busy())throw std::runtime_error("Busy: counter samples already loading");bool frameOnly=p["frameOnly"].AsBool();
        bool started=counter_reader_.Start(fs::u8path(exe_),trace_,cache_,counters_,counter_message_,frameOnly);if(!started)throw std::runtime_error("Unavailable: "+counter_message_);
        if(frameOnly)frame_stat_samples_requested_=true;else counter_samples_requested_=true;result["accepted"]=true;result["completion"]="poll_get_state_counters";
    }else if(op=="load_activities"){
        if(trace_.empty()||native_.Busy()||!pending_capture_.empty())throw std::runtime_error("Busy: open a trace and wait for indexing");if(activity_reader_.Busy())throw std::runtime_error("Busy: activities already loading");StartActivities();if(!activity_reader_.Busy())throw std::runtime_error("Unavailable: "+activity_message_);result["accepted"]=true;result["completion"]="poll_get_state_activities";
    }else if(op=="open_comparison"){
        auto path=CapturePath(p);if(AnalysisBusy())throw std::runtime_error("Busy: wait for active analysis");OpenComparison(path);if(!comparison_reader_.Busy())throw std::runtime_error("Unavailable: "+comparison_message_);comparison_b_.reset();result["accepted"]=true;result["completion"]="poll_get_state_comparison";
    }else if(op=="focus_view"){
        auto view=p["view"].AsString();session_view_=false;
        if(view=="timing")show_timing_=true;else if(view=="counters")show_counters_=true;else if(view=="frame_stats")FocusFrameStats();else if(view=="tasks")FocusTasks();else if(view=="activities")FocusActivities();else if(view=="gpu")FocusGpu();else if(view=="comparison")show_comparison_=true;else if(view=="findings")FocusAi();else if(view=="ai_method")FocusAiMethod();else if(view=="distribution")FocusDistribution();else throw std::runtime_error("InvalidParameters: unknown view");result["accepted"]=true;
    }else if(op=="pin_counter"){
        if(p.Has("counterRevision")&&p["counterRevision"].AsString()!=std::to_string(counter_catalog_revision_))throw std::runtime_error("StaleCounters: rediscover counter indices after loading samples");
        auto index=p["counter"].AsUInt(UINT64_MAX);if(index>=counters_->series.size())throw std::runtime_error("InvalidParameters: unknown counter index");counters_->series[size_t(index)].pinned=p["pinned"].AsBool(true);selected_counter_=int(index);show_timing_=true;result["accepted"]=true;
    }else if(op=="cancel_loading"){
        auto provider=p["provider"].AsString();if(provider=="counters")counter_reader_.Cancel();else if(provider=="activities")activity_reader_.Cancel();else if(provider=="tasks")task_reader_.Cancel();else if(provider=="comparison")comparison_reader_.Cancel();else throw std::runtime_error("InvalidParameters: unknown provider");result["accepted"]=true;result["completion"]="poll_get_state";
    }else return false;
    request->response.set_value(result);return true;
}
}
