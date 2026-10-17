// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "Capture.hpp"
#include <fstream>
#include <iostream>
#include <chrono>
#include <cmath>
namespace fs=std::filesystem;
int main(){
    int failures=0,checks=0;auto test=[&](bool ok,const char* name){++checks;if(!ok){++failures;std::cerr<<"FAIL: "<<name<<"\n";}};
    auto dir=fs::temp_directory_path()/("CyCaptureTests_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));fs::create_directories(dir/"input");
    std::ofstream(dir/"input/manifest.json")<<R"({"format":"cycapture","version":1,"session_id":"test","sources":[{"id":"unreal.trace","status":"measured","path":"trace.utrace"}]})";
    std::ofstream(dir/"input/trace.utrace",std::ios::binary)<<"trace payload\0preserved";
    std::string error;cyinsights::Capture capture;
    test(cyinsights::Pack(dir/"input",dir/"a.cycapture",error),"pack");
    test(cyinsights::Inspect(dir/"a.cycapture",capture,error)&&capture.entries.size()==2,"verify archive payloads");
    test(cyinsights::Extract(capture,dir/"extract",error),"extract to new directory");
    test(fs::file_size(dir/"input/trace.utrace")==fs::file_size(dir/"extract/trace.utrace"),"round trip payload size");
    test(!cyinsights::Extract(capture,dir/"extract",error),"refuse overwriting extraction");
    test(!cyinsights::Pack(dir/"input",dir/"a.cycapture",error),"refuse overwriting archive");
    test(!cyinsights::Pack(dir/"input",dir/"input/a.cycapture",error),"refuse writing inside staging");
    fs::copy_file(dir/"a.cycapture",dir/"corrupt.cycapture");{
        std::fstream f(dir/"corrupt.cycapture",std::ios::binary|std::ios::in|std::ios::out);f.seekp(-1,std::ios::end);f.put('X');}
    test(!cyinsights::Inspect(dir/"corrupt.cycapture",capture,error),"checksum catches corruption");
    fs::copy_file(dir/"a.cycapture",dir/"truncated.cycapture");fs::resize_file(dir/"truncated.cycapture",32);
    test(!cyinsights::Inspect(dir/"truncated.cycapture",capture,error),"truncated header");
    fs::create_directory(dir/"invalid");
    std::ofstream(dir/"invalid/manifest.json")<<R"({"format":"cycapture","version":1,"session_id":"test","sources":[{"id":"meta.quest.gpu","status":"invalid"}]})";
    test(!cyinsights::Pack(dir/"invalid",dir/"invalid.cycapture",error)&&error=="Every source needs an id and measured/derived/unavailable status","Packaging retains the manifest validation reason without a dangling error pointer");
    cygi::Json manifest;cygi::Json::Parse(R"({"format":"cycapture","version":2,"session_id":"test","sources":[]})",manifest);
    test(!cyinsights::ValidateManifest(manifest,error),"reject unsupported major version");
    cygi::Json::Parse(R"({"format":"cycapture","version":1,"session_id":"test","sources":[{"id":"gpu","status":"measured","path":"../escape"}]})",manifest);
    test(!cyinsights::ValidateManifest(manifest,error),"reject traversal");
    cygi::Json::Parse(R"({"format":"cycapture","version":1,"session_id":"test","sources":[{"id":"gpu","status":"measured","path":"C:/escape"}]})",manifest);
    test(!cyinsights::ValidateManifest(manifest,error),"reject drive path");
    cygi::Json::Parse(R"({"format":"cycapture","version":1,"session_id":"test","sources":[{"id":"gpu","status":"measured","path":"con.txt"}]})",manifest);
    test(!cyinsights::ValidateManifest(manifest,error),"reject reserved Windows device name");
    double seconds=0;cyinsights::ClockAnchor a{uint64_t(1)<<60,2,1000000,.000001};
    test(cyinsights::MapClock(a.ticks+1,a,seconds)&&std::abs(seconds-2.000001)<1e-12,"preserve integer ticks above 2^53");
    test(cyinsights::MapClock(a.ticks-1,a,seconds)&&std::abs(seconds-1.999999)<1e-12,"map before clock anchor without unsigned wrap");
    a.frequency=0;test(!cyinsights::MapClock(0,a,seconds),"missing frequency is unavailable");
    a.frequency=1;a.uncertainty_seconds=-1;test(!cyinsights::MapClock(0,a,seconds),"invalid clock uncertainty");
    std::error_code ec;fs::remove_all(dir,ec);std::cout<<checks<<" checks, "<<failures<<" failures\n";return failures?1:0;
}
