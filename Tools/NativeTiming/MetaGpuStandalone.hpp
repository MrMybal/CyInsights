// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include "MetaGpuPerfetto.hpp"
#include "MetaGpuRecords.hpp"
#include <memory>
namespace cymeta {
// GPU-only import: preserve source intervals; display t=0 at the first measured
// GPU event. No CPU clock anchor, Unreal frame identity, or game PID is inferred.
inline Counts ExportStandalone(const std::filesystem::path& input,const std::filesystem::path& output,
                               const std::function<bool()>& cancelled={}) {
    struct Track {
        std::unique_ptr<FILE,decltype(&fclose)> file{nullptr,&fclose};
        uint64_t count=0;
    };
    uint64_t origin=UINT64_MAX;
    std::map<std::string,uint32_t> timers;
    std::vector<std::string> names;
    std::map<std::string,Track> tracks;
    auto key=[](const Stage& s){return "GPU / Meta / "+StageLane(s)+" / "+s.queue+" (GPU only)";};
    auto check=[&]{if(cancelled&&cancelled())throw std::runtime_error("Analysis cancelled");};
    Decoder decoder;
    auto counts=decoder.Read(input,[&](const Stage& s){
        check();origin=std::min(origin,s.mono_ns);
        auto id=timers.try_emplace(s.name,uint32_t(names.size()));if(id.second)names.push_back(s.name);
        tracks.try_emplace(key(s));
    },{},cancelled);
    if(!counts.stages)throw std::runtime_error("No timed GPU stages available in this Perfetto recording");
    for(auto& entry:tracks){entry.second.file.reset(std::tmpfile());if(!entry.second.file)throw std::runtime_error("Cannot create GPU import spool");setvbuf(entry.second.file.get(),nullptr,_IOFBF,1024*1024);}
    decoder.Read(input,[&](const Stage& s){
        check();auto& track=tracks.at(key(s));
        const double a=double(s.mono_ns-origin)*1e-9,b=a+double(s.duration_ns)*1e-9;
        if(!std::isfinite(b)||b<a)throw std::runtime_error("Invalid GPU interval");
        cyvp::WriteValue(track.file.get(),Record{a,b,timers.at(s.name)});++track.count;
    },{},cancelled);
    check();
    std::unique_ptr<FILE,decltype(&fclose)> out(_wfopen(output.c_str(),L"wb"),&fclose);
    if(!out)throw std::runtime_error("Cannot create GPU timing cache");
    if(fwrite(cynt::magic,1,8,out.get())!=8)throw std::runtime_error("Cannot write GPU timing cache");
    cyvp::WriteValue(out.get(),uint32_t(names.size()));
    for(uint32_t i=0;i<names.size();++i){cyvp::WriteValue(out.get(),uint64_t(i));cyvp::WriteValue(out.get(),uint32_t(names[i].size()));if(fwrite(names[i].data(),1,names[i].size(),out.get())!=names[i].size())throw std::runtime_error("Cannot write GPU dictionary");}
    cyvp::WriteValue(out.get(),uint32_t(tracks.size()));uint64_t lane=0;
    for(auto& entry:tracks){check();ExportTrack(out.get(),entry.second.file.get(),entry.first,++lane,entry.second.count,0);}
    cyvp::WriteValue(out.get(),uint64_t(0)); // No measured Unreal frames in a GPU-only file.
    if(fwrite(cynt::footer,1,8,out.get())!=8)throw std::runtime_error("Cannot finalize GPU timing cache");
    cyvp::WriteValue(out.get(),counts.stages);
    if(fflush(out.get())||ferror(out.get()))throw std::runtime_error("GPU timing cache write failed");
    return counts;
}
}
