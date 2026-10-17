// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Core/MetaQuestCapture.hpp"
#include "../Core/RemoteCapture.hpp"
#include "NativeTiming/MetaGpuPerfetto.hpp"
#include <iostream>
#include <thread>
int main(int argc,char** argv){try{
    if(argc==3&&std::string(argv[1])=="--write-config"){auto config=cymeta::Config(10000);std::ofstream out(std::filesystem::u8path(argv[2]),std::ios::binary);out.write(config.data(),config.size());return out?0:1;}
    if(argc==3&&std::string(argv[1])=="--decode"){cymeta::Decoder d;int printed=0;auto c=d.Read(std::filesystem::u8path(argv[2]),[&](const cymeta::Stage& s){if(printed++<5)std::cout<<s.name<<" | "<<s.queue<<" | pid "<<s.pid<<" | "<<s.mono_ns<<" ns | "<<double(s.duration_ns)*1e-6<<" ms\n";});std::cout<<"Stages "<<c.stages<<"; counters "<<c.counters<<"; unavailable "<<c.unavailable<<"; stage packets "<<c.stage_packets<<"; timed stage packets "<<c.timed_stage_packets<<"; unaligned stages "<<c.unaligned_stages<<'\n';return 0;}
    if(argc<6)throw std::runtime_error("--capture IP package seconds output [--no-restart], or --decode file");
    if(std::string(argv[1])=="--remote"){
        cyinsights::RemoteOptions o;o.ipv4=argv[2];o.seconds=std::stod(argv[4]);o.output_directory=std::filesystem::u8path(argv[5]);o.meta_gpu.package=argv[3];o.meta_gpu.restart_game=argc<7||std::string(argv[6])!="--no-restart";
        cyinsights::RemoteCapture capture;if(!capture.Start(o))return 2;auto begin=std::chrono::steady_clock::now();std::string last;
        while(std::chrono::steady_clock::now()-begin<std::chrono::seconds(120)){auto s=capture.Snapshot();auto status=s.message+" | "+s.meta_message;if(status!=last){std::cout<<status<<std::endl;last=status;}if(s.state==cyinsights::RemoteState::complete){std::cout<<s.archive.u8string()<<'\n'<<s.manifest.Write(2)<<'\n';return 0;}if(s.state==cyinsights::RemoteState::failed)return 3;std::this_thread::sleep_for(std::chrono::milliseconds(50));}return 4;
    }
    std::atomic<bool> cancel{false};cyinsights::MetaQuestCapture capture(cancel);cyinsights::MetaQuestOptions options;options.package=argv[3];options.restart_game=argc<7||std::string(argv[6])!="--no-restart";
    const double seconds=std::stod(argv[4]);if(!std::isfinite(seconds)||seconds<.1||seconds>300)throw std::runtime_error("Probe duration 0.1..300 seconds");
    auto meta=cygi::Json::Object();if(!capture.Prepare(options,argv[2],meta))throw std::runtime_error(capture.Message());std::cout<<capture.Message()<<'\n';
    std::filesystem::create_directories(std::filesystem::u8path(argv[5]));
    if(!capture.Begin(std::filesystem::u8path(argv[5]),seconds))throw std::runtime_error(capture.Message());std::cout<<capture.Message()<<'\n';std::this_thread::sleep_for(std::chrono::milliseconds(int(seconds*1000)));capture.End();std::cout<<capture.Source().Write(2)<<'\n';return capture.Source()["status"].AsString()=="measured"?0:3;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
