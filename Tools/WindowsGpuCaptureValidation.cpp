// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later
// Owned rendered-host regression tool: uses the same recorder and writer as CyInsights.
#include "Session/SessionClient.hpp"
#include "Session/CaptureArchive.hpp"
#include <CyGPUInspectorCore/Json.hpp>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <stdexcept>

int wmain(int argc,wchar_t** argv) {
    try {
        if(argc!=3 && argc!=4)throw std::runtime_error("Usage: CyWindowsGpuCaptureValidation <owned-host-pid> <fresh-output-directory> [--plugin-request]");
        const auto pid=static_cast<uint32_t>(std::stoul(argv[1]));const std::filesystem::path output=argv[2];
        if(std::filesystem::exists(output))throw std::runtime_error("Output must be a fresh directory");
        cygi::SessionDirectory directory;cygi::SessionClient client;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);
        while(!client.IsAttached() && std::chrono::steady_clock::now()<deadline) {
            if(directory.Open())for(const auto& entry:directory.List())
                if(entry.process_id==pid && entry.IsFresh() && client.Connect(entry))break;
            if(!client.IsAttached())std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if(!client.IsAttached())throw std::runtime_error("CyInsights collector session not found");
        if(!client.SetLevel(cygi::TrackingLevel::pass_timing) || !client.RequestFullSync())throw std::runtime_error("Cannot configure collector");
        std::string error;
        if(argc==4) {
            if(std::wstring(argv[3])!=L"--plugin-request")throw std::runtime_error("Unknown validation option");
            std::ofstream(output.parent_path()/(output.filename().wstring()+L".ready"))<<"CyInsights recorder connected\n";
        } else if(!client.StartDeepCapture(1,false,false,false,false,error))throw std::runtime_error("Detailed capture refused: "+error);
        std::map<uint64_t,cygi::FrameTimings> timings;cygi::FrameInfo captured;bool has_frame=false;
        auto& model=client.Model();
        while(std::chrono::steady_clock::now()<deadline && client.WriterAlive()) {
            {
                std::lock_guard<std::mutex> lock(model.Mutex());
                for(auto& timing:model.TakeQueuedTimings())timings[timing.frame_index]=std::move(timing);
                while(timings.size()>120)timings.erase(timings.begin());
                const auto& state=model.CaptureState();
                if(state.stage==cygi::CaptureStage::failed)throw std::runtime_error("Collector capture failed");
                if(state.frames_done && !has_frame) {
                    const auto* frame=model.HasDeepFrame() && model.DeepFrame().index==state.first_frame ? &model.DeepFrame() : model.FrameByIndex(state.first_frame);
                    if(frame) {captured=*frame;has_frame=true;}
                }
                const auto measured=timings.find(captured.index);
                if(has_frame && state.stage==cygi::CaptureStage::finished && measured!=timings.end() && measured->second.IsValid()) {
                    if(captured.events.empty() || !captured.draw_count)throw std::runtime_error("No rendered GPU draws recorded");
                    std::vector<cygi::CaptureBufferRecord> buffers;
                    if(model.TakeCaptureBuffers(buffers) && !buffers.empty())throw std::runtime_error("Unexpected default resource readback");
                    cygi::FrameGraph graph;graph.Build(model,captured);
                    if(!cygi::CaptureArchive::Save(model,graph,output,error,&captured,&measured->second))throw std::runtime_error(error);
                    auto result=cygi::Json::Object();result["collector"]="CyInsightsGpu";result["execution_frame"]=std::to_string(captured.index);
                    result["gpu_timing_frame"]=std::to_string(measured->second.frame_index);result["events"]=static_cast<uint32_t>(captured.events.size());
                    result["draws"]=captured.draw_count;result["dispatches"]=captured.dispatch_count;result["readbacks"]=false;
                    result["gpu_ms"]=measured->second.Milliseconds(measured->second.total_ticks);
                    std::ofstream(output/"validation.json")<<result.Write(2);
                    std::cout<<result.Write(-1)<<'\n';return 0;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        throw std::runtime_error("Capture/timestamp timeout");
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
