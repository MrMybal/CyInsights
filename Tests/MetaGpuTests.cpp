// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Core/MetaQuestCapture.hpp"
#include "../Tools/NativeTiming/MetaGpuPerfetto.hpp"
#include <Windows.h>
#include <iostream>
#include <fstream>
#include "../Core/RemoteCapture.hpp"
#include "FakeRemoteRestart.hpp"
using namespace cymeta;
namespace fs=std::filesystem;
std::string Read(const fs::path& p){std::ifstream in(p,std::ios::binary);return {(std::istreambuf_iterator<char>(in)),{}};}
void Write(const fs::path& p,const std::string& text){std::ofstream out(p,std::ios::binary);out.write(text.data(),text.size());}
std::string Fixture(){
    std::string trace;auto packet=[&](const std::string& body){Message(trace,1,body);};
    std::string specs,context,queue,stage;
    Integer(context,1,43);Integer(context,2,123);Message(specs,1,context);Message(queue,1,"graphics");Message(specs,2,queue);
    Message(stage,1,"Binning");Message(specs,3,stage);stage.clear();Message(stage,1,"Render");Message(specs,3,stage);
    for(int i=0;i<2;++i){std::string event,p;Integer(event,1,i+1);Integer(event,2,200000);Integer(event,3,0);Integer(event,4,i);Integer(event,5,43);Integer(event,8,22);Integer(event,10,77);if(!i)Message(event,7,specs);Integer(p,8,1000000000000ull+i*300000);Integer(p,58,1);Integer(p,10,1);Message(p,53,event);packet(p);}
    // Clock snapshot deliberately arrives after the events it describes.
    std::string snap;for(auto id:{1,3}){std::string c;Integer(c,1,id);Integer(c,2,id==1?1000000000000ull:5000000000ull);Message(snap,1,c);}std::string p;Message(p,6,snap);packet(p);
    std::string desc,metric;Integer(metric,1,9);Message(metric,2,"GPU Busy %");Message(desc,1,metric);
    std::string counters;Message(counters,1,desc);for(double value:{42.,-1.}){std::string c;Integer(c,1,9);Var(c,(3<<3)|1);c.append(reinterpret_cast<const char*>(&value),8);Message(counters,2,c);}p.clear();Integer(p,8,1000000200000ull);Integer(p,58,1);Integer(p,10,1);Message(p,52,counters);packet(p);
    p.clear();std::string e;Integer(e,2,100);Integer(e,5,43);Integer(p,8,1);Integer(p,58,99);Message(p,53,e);packet(p);return trace;
}
int Fake(int argc,char** argv,const fs::path& dir){
    std::vector<std::string> a;for(int i=1;i<argc;++i)a.emplace_back(argv[i]);
    if(a==std::vector<std::string>{"devices"}){std::cout<<"List of devices attached\nfakeQuest\tdevice\n";return 0;}
    if(a.size()<3||a[0]!="-s"||a[1]!="fakeQuest")return 2;a.erase(a.begin(),a.begin()+2);
    if(a[0]=="push"){Write(dir/"remote-config",a.at(2));return 0;}
    if(a[0]=="pull"){fs::copy_file(dir/"fixture.pftrace",fs::u8path(a.at(2)),fs::copy_options::overwrite_existing);return 0;}
    if(a[0]!="shell"||a.size()<2)return 2;a.erase(a.begin());
    if(a[0]=="ip"){std::cout<<"inet 192.0.2.5/24";return 0;}
    if(a[0]=="ovrgpuprofiler"){
        if(a.at(1)=="-i"){if(fs::exists(dir/"query-failed"))return 2;std::cout<<(fs::exists(dir/"disabled")?"Detailed GPU profiling is disabled.":"Detailed GPU profiling is enabled.");return 0;}
        Write(dir/"enabled",a.at(2));std::cout<<"Detailed GPU profiling enabled";return 0;
    }
    if(a[0]=="cmd"){std::cout<<"com.test.Game/com.epicgames.unreal.GameActivity\n";return 0;}
    if(a[0]=="am"){if(a.at(1)=="force-stop")Write(dir/"restarted",a.back());else Write(dir/"launch-component",a.at(3));return 0;}
    if(a[0]=="pidof"){std::cout<<"123\n";return 0;}
    if(a[0]=="perfetto"){if(a[1]=="--query")std::cout<<"gpu.renderstages.oculus\ngpu.counters.oculus\n";else std::cout<<"987\n";return 0;}
    if(a[0]=="cat"){std::cout<<"perfetto "<<Read(dir/"remote-config");return 0;}
    if(a[0]=="kill"){Write(dir/"stopped",a.back());return 0;}
    if(a[0]=="test")return 1;
    if(a[0]=="sh"){std::cout<<"gone\n";return 0;}
    if(a[0]=="rm")return 0;return 2;
}
int main(int argc,char** argv){
    wchar_t env[32768]{};if(GetEnvironmentVariableW(L"CYINSIGHTS_TEST_META_ROOT",env,32768)&&argc>1)return Fake(argc,argv,fs::path(env));
    auto dir=fs::temp_directory_path()/("cyinsights-meta-test-"+std::to_string(GetCurrentProcessId()));int checks=0;
    auto check=[&](bool ok,const char* name){if(!ok)throw std::runtime_error(name);++checks;};
    try{fs::create_directories(dir);Write(dir/"fixture.pftrace",Fixture());
        WSADATA sockets{};check(WSAStartup(MAKEWORD(2,2),&sockets)==0,"Local restart test Winsock available");
        Decoder decoder;std::vector<Stage> events;std::vector<Counter> samples;auto counts=decoder.Read(dir/"fixture.pftrace",[&](const Stage& s){events.push_back(s);},[&](const Counter& c){samples.push_back(c);});
        check(counts.stages==2&&counts.counters==1&&counts.unavailable==2,"Measured stage/counter counts and unavailable data");
        check(counts.stage_packets==3&&counts.timed_stage_packets==3&&counts.unaligned_stages==1,"Missing hardware events are distinct from rejected clock alignment");
        check(events[0].mono_ns==5000000000ull&&events[1].mono_ns==5000300000ull,"Late realtime/monotonic clock snapshot alignment");
        check(events[0].name=="Binning"&&events[1].name=="Render"&&events[0].queue=="graphics","Hardware stage and queue dictionary");
        check(events[0].pid==123&&events[0].context==43&&events[0].surface==22&&events[0].submission==77,"Process/context/surface provenance");
        check(samples[0].value==42&&samples[0].name=="GPU Busy %","Invalid hardware values are excluded");
        {auto interrupted=Fixture();std::string packet,event;Integer(packet,10,1);Integer(packet,13,2);Integer(packet,42,1);Integer(packet,8,5001000000ull);Integer(packet,58,3);Integer(event,2,100);Message(packet,53,event);Message(interrupted,1,packet);Write(dir/"loss.pftrace",interrupted);auto loss=decoder.Read(dir/"loss.pftrace");check(loss.stages==2&&loss.unavailable==3,"Lost incremental dictionaries never assign stale stage identities");}
        Write(dir/"bad.pftrace",std::string("\x0a\x10\x00",3));bool rejected=false;try{decoder.Read(dir/"bad.pftrace");}catch(...){rejected=true;}check(rejected,"Truncated trace rejected");
        auto config=Config(1000);check(Number(config,8)==1&&Number(config,9)==250,"Streaming file configuration");
        {Wire wire(config);Field field;bool found=false;while(wire.Next(field))if(field.id==2){auto source=Find(field.bytes,1);if(Text(source.bytes,1)=="gpu.renderstages.oculus"){auto stages=Find(source.bytes,133);found=Number(stages.bytes,1)==1&&Number(stages.bytes,2,1)==0&&!Find(stages.bytes,4).id;}}check(found,"Full load/store with low-overhead and per-draw tracing disabled");}
        wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);SetEnvironmentVariableW(L"CYINSIGHTS_TEST_META_ROOT",dir.c_str());
        std::atomic<bool> cancel{false};cyinsights::MetaQuestOptions options;check(!options.restart_game,"Running-game capture does not restart by default");options.restart_game=true;options.adb=exe;options.package="com.test.Game";auto metadata=cygi::Json::Object();
        {cyinsights::MetaQuestCapture capture(cancel);check(capture.Prepare(options,"192.0.2.5",metadata),capture.Message().c_str());check(capture.Restarted(),"Selected game restarted before capture");auto stage=dir/"capture_test";fs::create_directory(stage);check(capture.Begin(stage,1),capture.Message().c_str());capture.BindSession("session-test");capture.End();auto source=capture.Source();check(Read(stage/"meta-gpu.info.json").find("session-test")!=std::string::npos,"Native and Meta session identity preserved");check(source["stage_count"].AsString()=="2"&&source["status"].AsString()=="measured","Collected file inspected and source manifest measured");check(source["profiling_driver_status"].AsString()=="enabled"&&source["timed_stage_packet_count"].AsString()=="3"&&source["stage_source_advertised"].AsBool(),"Driver verification and actual packet diagnostics retained");check(Read(dir/"enabled")=="com.test.Game"&&Read(dir/"stopped")=="987","Scoped activation and owned Perfetto cleanup");}
        check(Read(dir/"launch-component")=="com.test.Game/com.epicgames.unreal.GameActivity","Restart launches the resolved target activity rather than random input injection");
        options.restart_game=false;
        Write(dir/"disabled","test");{cyinsights::MetaQuestCapture capture(cancel);check(!capture.Prepare(options,"192.0.2.5",metadata)&&capture.Source()["profiling_driver_status"].AsString()=="disabled","Activation without enabled driver cannot report success");}fs::remove(dir/"disabled");
        Write(dir/"query-failed","test");{cyinsights::MetaQuestCapture capture(cancel);check(capture.Prepare(options,"192.0.2.5",metadata)&&capture.Source()["profiling_driver_status"].AsString()=="query_failed","Older driver query failure remains explicitly unverified");}fs::remove(dir/"query-failed");
        options.package="com.test.Game;echo";{cyinsights::MetaQuestCapture capture(cancel);check(!capture.Prepare(options,"192.0.2.5",metadata),"Unsafe remote package rejected");}
        for(int scenario:{0,1,2,3}){
            const bool stall=scenario==1,preparation_failed=scenario==2,running_game=scenario==3;
            fs::remove(dir/"restarted");if(preparation_failed)Write(dir/"disabled","test");if(running_game){Write(dir/"restarted","already running fixture");fs::remove(dir/"enabled");fs::remove(dir/"launch-component");}FakeRemoteRestart game(dir,stall);cyinsights::RemoteCapture capture;cyinsights::RemoteOptions remote;
            remote.ipv4="127.0.0.1";remote.control_port=game.port;remote.output_directory=dir/(stall?"stalled":preparation_failed?"native_fallback":running_game?"running_game":"restarted_ok");remote.seconds=.5;
            remote.meta_gpu.adb=exe;remote.meta_gpu.device="fakeQuest";remote.meta_gpu.package="com.test.Game";remote.meta_gpu.restart_game=!running_game;
            check(capture.Start(remote),"Combined native/Meta startup accepted");const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
            while(capture.Snapshot().Busy()&&std::chrono::steady_clock::now()<deadline){
                if(stall&&game.restart_hello){check(capture.Snapshot().startup_elapsed>0,"Startup timer continues while the restarted game is silent");capture.Stop();break;}
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            const auto stopped=std::chrono::steady_clock::now();while(capture.Snapshot().Busy()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(std::chrono::milliseconds(10));
            auto result=capture.Snapshot();
            if(stall){check(result.state==cyinsights::RemoteState::failed&&result.archive.empty()&&game.start_count==0,"Stop during restart cannot start an unattended trace");check(std::chrono::steady_clock::now()-stopped<std::chrono::seconds(1),"Stop interrupts the pending restarted-game hello promptly");}
            else{check(result.state==cyinsights::RemoteState::complete&&game.start_count==1&&(preparation_failed||game.ready_polls>=3),"Reconnect to current process waits for readiness and records once");cyinsights::Capture archived;std::string why;check(cyinsights::Inspect(result.archive,archived,why),"Combined native/Meta archive validates after startup");check(result.manifest["metadata"]["process_id"].AsUInt()==(preparation_failed?122u:123u)&&result.bytes==264,"Final identity and native bytes come from current process");if(preparation_failed)check(result.meta_message.find("not prepared")!=std::string::npos,"Failed Meta preparation preserves native recording and reports unavailable GPU source");}
            check(fs::exists(result.diagnostic_log)&&Read(result.diagnostic_log).find(stall?"Capture stopped":"Capture saved")!=std::string::npos,"Durable capture phase/error journal available");
            if(running_game)check(!fs::exists(dir/"enabled")&&!fs::exists(dir/"launch-component")&&result.meta_message.find("2 hardware stages")!=std::string::npos,"Default running-game capture records native/Meta sources without reactivation, force-stop or relaunch");
            if(preparation_failed)fs::remove(dir/"disabled");
        }
        WSACleanup();
        SetEnvironmentVariableW(L"CYINSIGHTS_TEST_META_ROOT",nullptr);fs::remove_all(dir);std::cout<<checks<<" Meta GPU checks passed\n";return 0;
    }catch(const std::exception& e){SetEnvironmentVariableW(L"CYINSIGHTS_TEST_META_ROOT",nullptr);std::cerr<<e.what()<<'\n';return 1;}
}
