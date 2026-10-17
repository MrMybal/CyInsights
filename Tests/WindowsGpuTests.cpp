// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: AGPL-3.0-or-later
#include <CyGPUInspectorCore/SessionDirectory.hpp>
#include <Windows.h>
#include <cstring>
#include <iostream>
#include <stdexcept>

void Check(bool value,const char* message) { if(!value)throw std::runtime_error(message); }
int wmain(int argc,wchar_t** argv) {
    try {
        char ring[256]{},signal[256]{},pipe[256]{};
        const auto pid=GetCurrentProcessId();constexpr uint32_t device=0x7ffffffe;
        cygi::MakeRingName(pid,device,ring,sizeof(ring));cygi::MakeSignalName(pid,device,signal,sizeof(signal));
        cygi::MakePipeName(pid,device,pipe,sizeof(pipe));
        Check(std::strstr(ring,"CyInsights.Gpu") && !std::strstr(ring,"CyGPUInspector"),"Event ring is not isolated");
        Check(std::strstr(signal,"CyInsights.Gpu") && !std::strstr(signal,"CyGPUInspector"),"Signal is not isolated");
        Check(std::strstr(pipe,"CyInsights.Gpu") && !std::strstr(pipe,"CyGPUInspector"),"Control pipe is not isolated");
        Check(!std::strcmp(cygi::kSessionDirectoryName,"Local\\CyInsights.Gpu.Sessions.v1"),"Wrong session directory");
        cygi::SessionPublisher writer;
        Check(writer.Claim(pid,device,cygi::GraphicsApi::d3d12,"CyInsights-test",ring,signal,pipe,4096,0),"Cannot publish test session");
        writer.Heartbeat(42,cygi::TrackingLevel::tracking);
        cygi::SessionDirectory reader;Check(reader.Open(),"Cannot discover our collector");
        bool found=false;
        for(const auto& entry:reader.List())if(entry.process_id==pid && entry.device_index==device) {
            found=entry.ring_name==ring && entry.pipe_name==pipe && entry.frame_index==42;
        }
        Check(found,"CyInsights recorder cannot discover its own collector");
        writer.Release();
        for(const auto& entry:reader.List())Check(entry.process_id!=pid || entry.device_index!=device,"Session survives release");
        if(argc==2) {
            const auto module=LoadLibraryExW(argv[1],nullptr,DONT_RESOLVE_DLL_REFERENCES);
            Check(module!=nullptr,"Cannot inspect collector binary");
            bool valid=true;
            for(const auto* symbol:{"CyInsightsGpu_RequestCapture","CyInsightsGpu_GetStatus","CyInsightsGpu_GetApiVersion","CyInsightsGpu_SetViewportScope"})
                valid=valid && GetProcAddress(module,symbol)!=nullptr;
            valid=valid && GetProcAddress(module,"CyGPUInspectorRS_RequestCapture")==nullptr;
            FreeLibrary(module);Check(valid,"Collector exports do not match the integrated plugin ABI");
        }
        std::cout<<"CyInsights GPU session isolation and optional binary ABI passed\n";return 0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
