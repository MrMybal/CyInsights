// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "RemoteCapture.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>

using namespace cyinsights;
using namespace std::chrono_literals;
namespace {
cygi::Json Manifest() {
    cygi::Json j;
    cygi::Json::Parse(R"({"format":"cycapture","version":1,"session_id":"mock-session","metadata":{"platform":"Android"},"sources":[{"id":"unreal.trace","status":"measured","path":"unreal.utrace"},{"id":"unreal.sync","status":"measured","path":"unreal.utrace"},{"id":"meta.openxr.metrics","status":"measured","path":"unreal.utrace"}]})",j);
    return j;
}
enum class Mode { good, old_gpu_stage, old_options, legacy, bad_header, reset, wrong_session, missing_payload, bad_response, auto_stop, busy, oversized_response };
struct Game {
    SOCKET listener=INVALID_SOCKET;
    uint16_t port=0;
    std::thread control;
    Mode mode;
    std::atomic<bool> finish{false};
    std::atomic<bool> got_options{false},got_gpu{false},gpu_owned{false};
    cygi::Json applied;
    std::string payload;
    explicit Game(Mode m):mode(m) {
        listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
        sockaddr_in addr{};addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(bind(listener,reinterpret_cast<sockaddr*>(&addr),sizeof(addr))||listen(listener,1))throw std::runtime_error("mock bind failed");
        int len=sizeof(addr);getsockname(listener,reinterpret_cast<sockaddr*>(&addr),&len);port=ntohs(addr.sin_port);
        payload=std::string("2CRT\0\0\4\7",8)+std::string(70000,'X');
        if(m==Mode::legacy)payload=std::string("ECRT\3\4",6)+std::string(70002,'X');
        if(m==Mode::bad_header)payload.replace(0,4,"FAIL");
        control=std::thread([this](){Serve();});
    }
    ~Game(){finish.store(true);closesocket(listener);if(control.joinable())control.join();}
    void Serve() {
        SOCKET client=accept(listener,nullptr,nullptr);if(client==INVALID_SOCKET)return;
        DWORD timeout=200;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));
        std::string pending;bool recording=false;std::atomic<bool> stop{false};std::thread data;
        auto start=std::chrono::steady_clock::now();
        while(!finish.load()) {
            char c[1024];int n=recv(client,c,sizeof(c),0);
            if(n==SOCKET_ERROR&&WSAGetLastError()==WSAETIMEDOUT)continue;
            if(n<=0)break;pending.append(c,n);
            size_t e;
            while((e=pending.find('\n'))!=std::string::npos) {
                cygi::Json q;cygi::Json::Parse(pending.substr(0,e),q);pending.erase(0,e+1);
                auto cmd=q["command"].AsString();
                if(cmd=="start") {
                    got_options=q.Has("capture_options");applied=q["capture_options"];
                    recording=true;start=std::chrono::steady_clock::now();
                    auto receiver=q["receiver_port"].AsUInt();
                    data=std::thread([this,receiver,&stop](){
                        SOCKET stream=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
                        sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(static_cast<uint16_t>(receiver));
                        if(!connect(stream,reinterpret_cast<sockaddr*>(&a),sizeof(a))) {
                            // Fragment header, then body; final bytes arrive AFTER stop acknowledgement.
                            for(size_t i=0;i<8;++i){send(stream,payload.data()+i,1,0);std::this_thread::sleep_for(2ms);}
                            send(stream,payload.data()+8,65000,0);
                            if(mode==Mode::reset) {
                                linger reset{1,0};setsockopt(stream,SOL_SOCKET,SO_LINGER,reinterpret_cast<char*>(&reset),sizeof(reset));
                            } else {
                                while(!finish.load()&&!stop.load())std::this_thread::sleep_for(10ms);
                                std::this_thread::sleep_for(150ms);
                                send(stream,payload.data()+65008,static_cast<int>(payload.size()-65008),0);
                                shutdown(stream,SD_SEND);
                            }
                        }closesocket(stream);
                    });
                }
                if(cmd=="stop"||(mode==Mode::auto_stop&&recording&&std::chrono::steady_clock::now()-start>200ms)) {
                    recording=false;stop.store(true);
                }
                cygi::Json r=cygi::Json::Object();r["version"]=1;r["ok"]=true;
                if(mode!=Mode::legacy){r["capture_defaults"]=CaptureOptionsJson(CaptureOptions{});r["capture_capabilities"]["capture_options_version"]=1.;r["capture_capabilities"]["gpu_request"]=false;if(mode!=Mode::old_options){r["capture_capabilities"]["native_gpu_passes"]=true;r["capture_capabilities"]["quest_per_frame_observations"]=true;if(mode!=Mode::old_gpu_stage)r["capture_capabilities"]["quest_gpu_completion"]=true;}}
                if(cmd=="gpu"){got_gpu=true;gpu_owned=q["session_id"].AsString()=="mock-session";r["ok"]=false;r["error"]="No recorder connected";}
                r["state"]=(recording||mode==Mode::busy)?"recording":"idle";r["native_trace_active"]=recording||mode==Mode::busy;
                r["session_id"]=(mode==Mode::wrong_session&&cmd=="status")?"wrong-session":"mock-session";
                r["manifest"]=Manifest();if(mode==Mode::old_gpu_stage)r["manifest"]["metadata"]["platform"]="Android";
                if(mode==Mode::missing_payload)r["manifest"]["sources"].Push([](){cygi::Json s=cygi::Json::Object();s["id"]="bad";s["status"]="measured";s["path"]="missing.bin";return s;}());
                auto wire=mode==Mode::bad_response?std::string("not json\n"):r.Write(-1)+"\n";
                if(mode==Mode::oversized_response)wire=std::string(65537,'X')+"\n";
                // Fragment the control response independently from the trace.
                send(client,wire.data(),3,0);send(client,wire.data()+3,static_cast<int>(wire.size()-3),0);
            }
        }
        stop.store(true);if(data.joinable())data.join();closesocket(client);
    }
};
RemoteSnapshot Wait(RemoteCapture& capture,double seconds=8) {
    auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(int(seconds*1000));
    while(capture.Snapshot().Busy()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(20ms);
    return capture.Snapshot();
}
}
int main(int argc,char** argv) {
    WSADATA d{};WSAStartup(MAKEWORD(2,2),&d);
    if(argc>=3 && std::string(argv[1])=="--capture") {
        RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4=argv[2];o.seconds=2;
        o.output_directory=argc>=4?fs::u8path(argv[3]):fs::current_path()/"remote-validation";
        if(argc>=5)o.control_port=static_cast<uint16_t>(std::stoi(argv[4]));
        capture.Start(o);auto result=Wait(capture,30);
        std::cout<<RemoteStateName(result.state)<<": "<<result.message<<"\n"<<result.archive.u8string()<<"\n";
        return result.state==RemoteState::complete?0:1;
    }
    int checks=0,failed=0;
    auto test=[&](bool ok,const char* name){++checks;if(!ok){++failed;std::cerr<<"FAIL: "<<name<<"\n";}};
    auto dir=fs::temp_directory_path()/("CyRemoteTests_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::string error;
    {
        CaptureOptions basic;
        test(basic.cpu&&basic.gpu&&basic.frame_stats&&basic.object_counts&&basic.quest_metrics,"basic profile keeps useful low-volume observations");
        test(!basic.stat_groups_enabled&&!basic.memory_stats&&!basic.gpu_details&&!basic.gpu_readbacks&&!basic.gpu_per_command&&!basic.tasks&&!basic.network&&!basic.file_io,"expensive capture paths are opt-in");
        test(basic.quest_gpu_completion&&basic.quest_hz==0&&basic.GpuEventLevel()==2&&!basic.gpu_passes,"Default captures nested GPU groups each frame; individual passes remain opt-in");
        auto passes=basic;passes.gpu_passes=true;test(passes.GpuEventLevel()==3,"Individual passes request RDG level 3");
        auto noGpu=basic;noGpu.gpu=false;test(noGpu.GpuEventLevel()==0,"GPU disabled requests no groups");
        auto perFrame=CaptureOptionsJson(basic);CaptureOptions perFrameParsed;std::string parseError;
        test(ReadCaptureOptions(perFrame,perFrameParsed,parseError)&&perFrameParsed.quest_hz==0,"per-frame mode survives wire serialization");
        CaptureOptions invalidPasses;invalidPasses.gpu=false;invalidPasses.gpu_passes=true;
        test(!invalidPasses.Validate().empty(),"detailed passes require native GPU channel");
        CaptureOptions full=basic;full.quest_gpu_completion=false;full.gpu_passes=true;full.memory_stats=true;full.stat_groups_enabled=true;full.stat_groups="Game,GPU";full.quest_hz=30;full.gpu_frames=3;full.gpu_bindings=true;full.gpu_readbacks=true;
        CaptureOptions parsed;auto wire=CaptureOptionsJson(full);test(ReadCaptureOptions(wire,parsed,error)&&CaptureOptionsJson(parsed).Write(-1)==wire.Write(-1),"all capture fields round-trip");
        for(auto invalid:{"{\"version\":1,\"gpu_frames\":9}","{\"version\":1,\"quest_hz\":-1}","{\"version\":1,\"gpu_readbacks\":1}","{\"version\":2}","{\"version\":1,\"unknown\":true}","{\"version\":1,\"frame_stats\":false,\"stat_groups_enabled\":true}","{\"version\":1,\"stat_groups\":\"Game;quit\"}"}){
            cygi::Json bad;cygi::Json::Parse(invalid,bad);auto previous=CaptureOptionsJson(parsed).Write(-1);
            test(!ReadCaptureOptions(bad,parsed,error)&&CaptureOptionsJson(parsed).Write(-1)==previous,"invalid options refused atomically");
        }
    }
    for(bool per_frame:{true,false}){
        Game game(Mode::old_options);RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=game.port;o.output_directory=dir;o.override_capture_options=true;o.capture.quest_hz=per_frame?0:10;o.capture.quest_gpu_completion=false;
        capture.Start(o);auto deadline=std::chrono::steady_clock::now()+3s;
        while(std::chrono::steady_clock::now()<deadline){auto r=capture.Snapshot();if(r.state==RemoteState::failed||r.state==RemoteState::recording)break;std::this_thread::sleep_for(10ms);}
        if(per_frame)test(capture.Snapshot().state==RemoteState::failed&&!game.got_options,"old plugin rejects per-frame mode before start");
        else{capture.Stop();while(std::chrono::steady_clock::now()<deadline&&capture.Snapshot().state!=RemoteState::complete)std::this_thread::sleep_for(10ms);
            test(capture.Snapshot().state==RemoteState::complete&&game.got_options&&!game.applied.Has("gpu_passes")&&!game.applied.Has("quest_gpu_completion"),"old plugin receives compatible throttled profile");}
    }
    for(bool completed:{true,false}) {
        Game game(Mode::old_gpu_stage);RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=game.port;o.output_directory=dir;o.override_capture_options=true;o.capture.quest_gpu_completion=completed;
        capture.Start(o);auto deadline=std::chrono::steady_clock::now()+3s;
        while(std::chrono::steady_clock::now()<deadline){auto r=capture.Snapshot();if(r.state==RemoteState::failed||r.state==RemoteState::recording)break;std::this_thread::sleep_for(10ms);}
        if(completed)test(capture.Snapshot().state==RemoteState::failed&&!game.got_options,"Old Quest cannot silently claim GPU completion mode");
        else {capture.Stop();while(std::chrono::steady_clock::now()<deadline&&capture.Snapshot().state!=RemoteState::complete)std::this_thread::sleep_for(10ms);
            test(capture.Snapshot().state==RemoteState::complete&&game.got_options&&!game.applied.Has("quest_gpu_completion"),"Explicit fallback omits unsupported completion field on old Quest");}
    }
    error.clear();test(!NativeTraceHeader("2C",false,error)&&error.empty(),"fragmented header waits for more data");
    test(!NativeTraceHeader("2C",true,error)&&!error.empty(),"truncated EOF rejected");error.clear();
    test(!NativeTraceHeader("FAIL",false,error)&&!error.empty(),"invalid native signature rejected");error.clear();
    test(!NativeTraceHeader(std::string("2CRT\xff\xff",6),false,error)&&!error.empty(),"bounded native metadata");error.clear();
    test(NativeTraceHeader(std::string("2CRT\0\0\4\7",8),true,error),"valid native preamble");
    error.clear();
    test(!NativeTraceHeader("ECRT",false,error)&&error.empty(),"fragmented legacy header waits");
    test(!NativeTraceHeader("ECRT",true,error)&&!error.empty(),"truncated legacy header rejected");error.clear();
    test(NativeTraceHeader(std::string("ECRT\3\4",6),true,error),"UE4.27 native preamble accepted");
    test(!NativeTraceHeader(std::string("ECRT\0\4",6),true,error)&&!error.empty(),"invalid legacy transport rejected");error.clear();
    {
        RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="bad IP";o.output_directory=dir;
        capture.Start(o);test(Wait(capture).state==RemoteState::failed,"invalid IP fails off UI thread");
    }
    {
        Game game(Mode::good);RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=game.port;
        capture.Probe(o);test(Wait(capture).state==RemoteState::ready,"probe requires only IP/control port");
    }
    for(auto mode:{Mode::busy,Mode::oversized_response}) {
        Game game(mode);RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=game.port;
        capture.Probe(o);test(Wait(capture).state==RemoteState::failed,"active native trace or oversized response refused");
    }
    {
        RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=0;
        capture.Probe(o);test(Wait(capture).state==RemoteState::failed,"zero control port refused");
        o.control_port=41855;o.seconds=-1;capture.Start(o);
        test(Wait(capture).state==RemoteState::failed,"invalid duration refused");
    }
    for(auto mode:{Mode::good,Mode::legacy,Mode::bad_header,Mode::reset,Mode::wrong_session,Mode::missing_payload,Mode::bad_response,Mode::auto_stop}) {
        Game game(mode);RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=game.port;o.output_directory=dir;
        o.seconds=mode==Mode::auto_stop?1:0;capture.Start(o);
        test(!capture.Start(o),"reject overlapping receiver capture");
        auto until=std::chrono::steady_clock::now()+2s;
        while(capture.Snapshot().state==RemoteState::starting&&std::chrono::steady_clock::now()<until)std::this_thread::sleep_for(20ms);
        if(mode!=Mode::auto_stop)capture.Stop();auto result=Wait(capture);
        if(mode==Mode::good||mode==Mode::legacy||mode==Mode::auto_stop) {
            test(result.state==RemoteState::complete,"stop then native EOF packages capture");
            Capture archived;error.clear();test(Inspect(result.archive,archived,error),"received archive validates");
            auto extracted=dir/(mode==Mode::legacy?"extracted-legacy":mode==Mode::good?"extracted-manual":"extracted-auto");
            test(Extract(archived,extracted,error),"received capture extracts");
            std::ifstream input(extracted/"unreal.utrace",std::ios::binary);std::string received((std::istreambuf_iterator<char>(input)),{});
            test(received==game.payload,"native bytes preserved including delayed final packet");
            test(archived.manifest["sources"].Items().size()==3,"Quest counters and frame anchors share native payload");
        } else {
            test(result.state==RemoteState::failed,"invalid or interrupted transfer fails");
            test(result.archive.empty(),"failed transfer never publishes complete archive");
            if(mode!=Mode::bad_response)test(!result.partial_trace.empty()&&fs::exists(result.partial_trace),"partial native bytes retained");
        }
    }
    {
        Game game(Mode::good);RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=game.port;o.output_directory=dir;o.override_capture_options=true;o.capture.memory_stats=true;o.capture.quest_hz=30;
        capture.Start(o);auto deadline=std::chrono::steady_clock::now()+3s;
        while(capture.Snapshot().state==RemoteState::starting&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(20ms);
        CaptureOptions gpu;gpu.gpu_frames=2;test(capture.RequestGpu(gpu),"bounded extra GPU request queues during owned capture");
        deadline=std::chrono::steady_clock::now()+3s;
        while(capture.Snapshot().gpu_message.empty()&&std::chrono::steady_clock::now()<deadline)std::this_thread::sleep_for(20ms);
        test(capture.Snapshot().gpu_message=="No recorder connected"&&capture.Snapshot().state==RemoteState::recording,"GPU refusal preserves native recording");
        capture.Stop();auto result=Wait(capture);
        test(result.state==RemoteState::complete&&game.got_options&&game.applied["memory_stats"].AsBool()&&game.applied["quest_hz"].AsNumber()==30,"custom options reach target and archive completes");
        test(game.got_gpu&&game.gpu_owned,"GPU request uses owned session");test(!capture.RequestGpu(gpu),"GPU request refused after capture end");
    }
    {
        Game game(Mode::legacy);RemoteCapture capture;RemoteOptions o;o.meta_gpu.enabled=false;o.ipv4="127.0.0.1";o.control_port=game.port;o.output_directory=dir;o.override_capture_options=true;
        capture.Start(o);test(Wait(capture).state==RemoteState::failed&&!game.got_options,"explicit overrides refused for an older plugin");
    }
    std::error_code ec;fs::remove_all(dir,ec);WSACleanup();
    std::cout<<checks<<" checks, "<<failed<<" failures\n";return failed?1:0;
}
