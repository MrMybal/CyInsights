// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "RemoteCapture.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <stdexcept>

namespace cyinsights {
namespace {
using Clock = std::chrono::steady_clock;
constexpr uint64_t MaxBytes = 128ull * 1024 * 1024 * 1024 - 1024 * 1024;
struct Winsock {
    Winsock() { WSADATA d{}; if (WSAStartup(MAKEWORD(2,2), &d)) throw std::runtime_error("Cannot initialize TCP"); }
    ~Winsock() { WSACleanup(); }
};
struct Socket {
    SOCKET value = INVALID_SOCKET;
    Socket() = default;
    explicit Socket(SOCKET v) : value(v) {}
    ~Socket() { if (value != INVALID_SOCKET) closesocket(value); }
    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;
    void Reset(SOCKET v) { if (value != INVALID_SOCKET) closesocket(value); value=v; }
};
void Require(bool ok, const std::string& why) { if (!ok) throw std::runtime_error(why); }
void NonBlocking(SOCKET s) { u_long one=1; Require(!ioctlsocket(s,FIONBIO,&one),"Cannot configure TCP socket"); }
bool Ready(SOCKET s, bool write, long milliseconds) {
    fd_set set; FD_ZERO(&set); FD_SET(s,&set);
    timeval t{milliseconds/1000,(milliseconds%1000)*1000};
    int result=select(0,write?nullptr:&set,write?&set:nullptr,nullptr,&t);
    Require(result!=SOCKET_ERROR,"TCP connection interrupted");return result>0;
}
sockaddr_in Address(const RemoteOptions& o) {
    sockaddr_in a{};a.sin_family=AF_INET;a.sin_port=htons(o.control_port);
    Require(o.control_port!=0 && inet_pton(AF_INET,o.ipv4.c_str(),&a.sin_addr)==1 &&
        a.sin_addr.s_addr!=INADDR_ANY && a.sin_addr.s_addr!=INADDR_BROADCAST,"Enter a valid device IPv4 address");
    Require(std::isfinite(o.seconds)&&o.seconds>=0&&o.seconds<=3600,"Duration must be 0..3600 seconds");return a;
}
void Connect(Socket& s, const sockaddr_in& a, const std::atomic<bool>& cancel,
             const std::atomic<bool>* stop=nullptr,const std::function<void()>& pulse={}) {
    s.Reset(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));Require(s.value!=INVALID_SOCKET,"Cannot create control socket");
    NonBlocking(s.value);
    if(connect(s.value,reinterpret_cast<const sockaddr*>(&a),sizeof(a))==0)return;
    Require(WSAGetLastError()==WSAEWOULDBLOCK,"Cannot connect to the game's capture endpoint");
    auto deadline=Clock::now()+std::chrono::seconds(3);
    while(Clock::now()<deadline&&!cancel.load()&&(!stop||!stop->load())) {
        if(pulse)pulse();
        fd_set write,errors;FD_ZERO(&write);FD_ZERO(&errors);FD_SET(s.value,&write);FD_SET(s.value,&errors);
        timeval t{0,50000};int n=select(0,nullptr,&write,&errors,&t);
        Require(n!=SOCKET_ERROR,"TCP connection failed");
        if(n) {int e=0,len=sizeof(e);Require(!getsockopt(s.value,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&e),&len)&&e==0,
            "Cannot connect: check the device IP, plugin and TCP control port");return;}
    }
    throw std::runtime_error(cancel.load()|| (stop&&stop->load())?"Capture stopped during startup":"Connection timed out: check device IP and TCP control port");
}
class Rpc {
public:
    Rpc(SOCKET s, const std::atomic<bool>& cancel): socket_(s),cancel_(cancel) {}
    void Setup(const std::atomic<bool>& stop,std::function<void()> pulse){setup_stop_=&stop;pulse_=std::move(pulse);}
    void Recording(){setup_stop_=nullptr;pulse_={};}
    cygi::Json Call(const char* command, const std::string& session={}, uint16_t port=0, double seconds=0,const CaptureOptions* options=nullptr,bool allow_refusal=false,unsigned timeout_ms=0) {
        cygi::Json request=cygi::Json::Object();request["version"]=1;request["command"]=command;
        if(!session.empty())request["session_id"]=session;
        if(port){request["receiver_port"]=int(port);request["seconds"]=seconds;}
        if(options){
            const auto policy=CaptureOptionsJson(*options);auto compatible=cygi::Json::Object();
            for(const auto& entry:policy.Members())if((entry.first!="gpu_passes"||native_pass_option_)&&(entry.first!="quest_gpu_completion"||quest_completion_option_))compatible[entry.first]=entry.second;
            request["capture_options"]=compatible;
        }
        std::string wire=request.Write(-1)+"\n";
        const auto deadline=Clock::now()+std::chrono::milliseconds(timeout_ms?timeout_ms:std::string(command)=="hello"?15000:3000);
        size_t sent=0;
        while(sent<wire.size()) {
            Check(deadline);if(!Ready(socket_,true,50))continue;
            int n=send(socket_,wire.data()+sent,static_cast<int>(wire.size()-sent),0);
            if(n==SOCKET_ERROR&&WSAGetLastError()==WSAEWOULDBLOCK)continue;
            Require(n>0,"Capture control connection closed");sent+=n;
        }
        for(;;) {
            Check(deadline);
            auto end=input_.find('\n');
            if(end!=std::string::npos) {
                cygi::Json reply;Require(cygi::Json::Parse(input_.substr(0,end),reply)&&reply.IsObject()&&reply["version"].AsNumber()==1,
                    "Invalid capture control response/version");input_.erase(0,end+1);
                if(!allow_refusal)Require(reply["ok"].AsBool(),reply["error"].AsString().empty()?"Remote capture refused":reply["error"].AsString());
                if(std::string(command)=="hello"){native_pass_option_=reply["capture_capabilities"]["native_gpu_passes"].AsBool();quest_completion_option_=reply["capture_capabilities"]["quest_gpu_completion"].AsBool();}
                return reply;
            }
            if(!Ready(socket_,false,50))continue;
            char bytes[4096];int n=recv(socket_,bytes,sizeof(bytes),0);
            if(n==SOCKET_ERROR&&WSAGetLastError()==WSAEWOULDBLOCK)continue;
            Require(n>0,"Capture control connection closed");input_.append(bytes,n);
            Require(input_.size()<=65536,"Capture control response exceeds limit");
        }
    }
    void Reset(SOCKET socket){socket_=socket;input_.clear();}
private:
    void Check(Clock::time_point deadline) {
        Require(!cancel_.load(),"Capture cancelled; received partial trace retained");
        Require(!setup_stop_||!setup_stop_->load(),"Capture stopped during startup");
        if(pulse_)pulse_();
        Require(Clock::now()<deadline,"Capture control response timed out");
    }
    SOCKET socket_; const std::atomic<bool>& cancel_;std::string input_;bool native_pass_option_=false,quest_completion_option_=false;
    const std::atomic<bool>* setup_stop_=nullptr;std::function<void()> pulse_;
};
}
const char* RemoteStateName(RemoteState s) {
    switch(s) {
        case RemoteState::idle:return "Idle";case RemoteState::probing:return "Connecting";case RemoteState::ready:return "Device ready";
        case RemoteState::starting:return "Starting capture";case RemoteState::recording:return "Recording";
        case RemoteState::stopping:return "Receiving final trace data";case RemoteState::packaging:return "Packaging capture";
        case RemoteState::complete:return "Capture received";case RemoteState::failed:return "Capture failed";
    }return "Unknown";
}
bool NativeTraceHeader(const std::string& p,bool eof,std::string& error) {
    // UE4.27 and early UE5 use TRCE (ECRT on the wire), followed directly
    // by transport/protocol bytes. TRC2 adds the metadata-length envelope.
    if(p.size()>=4 && p.compare(0,4,"ECRT")==0) {
        if(p.size()>=6) {
            if(static_cast<unsigned char>(p[4])==0||static_cast<unsigned char>(p[5])==0) {
                error="Invalid legacy native trace transport/protocol";return false;
            }
            return true;
        }
        if(eof)error="Incomplete native Unreal trace header";
        return false;
    }
    if(p.size()>=4 && p.compare(0,4,"2CRT")!=0){error="Invalid native Unreal trace signature";return false;}
    if(p.size()>=6) {
        size_t size=static_cast<unsigned char>(p[4])+(size_t(static_cast<unsigned char>(p[5]))<<8);
        if(size>4096){error="Native trace metadata exceeds limit";return false;}
        if(p.size()>=size+8)return true;
    }
    if(eof)error="Incomplete native Unreal trace header";
    return false;
}
RemoteCapture::~RemoteCapture() {cancel_.store(true);if(worker_.joinable())worker_.join();}
RemoteSnapshot RemoteCapture::Snapshot() const {std::lock_guard<std::mutex> lock(mutex_);auto result=snapshot_;if(result.state==RemoteState::starting||result.state==RemoteState::probing)result.startup_elapsed=std::chrono::duration<double>(Clock::now()-launched_at_).count();return result;}
void RemoteCapture::Publish(const RemoteSnapshot& s) {std::lock_guard<std::mutex> lock(mutex_);snapshot_=s;}
bool RemoteCapture::Probe(const RemoteOptions& o){return Launch(o,true);}
bool RemoteCapture::Start(const RemoteOptions& o){return Launch(o,false);}
bool RemoteCapture::RequestGpu(const CaptureOptions& o){
    if(!o.Validate().empty())return false;
    std::lock_guard<std::mutex> lock(mutex_);if(snapshot_.state!=RemoteState::recording||gpu_request_)return false;gpu_request_=o;return true;
}
bool RemoteCapture::Launch(const RemoteOptions& o,bool probe) {
    if(Snapshot().Busy())return false;
    if(worker_.joinable())worker_.join();
    stop_.store(false);cancel_.store(false);
    {std::lock_guard<std::mutex> lock(mutex_);gpu_request_.reset();launched_at_=Clock::now();}
    RemoteSnapshot s;s.state=probe?RemoteState::probing:RemoteState::starting;Publish(s);
    try {worker_=std::thread([this,o,probe](){Run(o,probe);});return true;}
    catch(const std::exception& e){s.state=RemoteState::failed;s.message=e.what();Publish(s);return false;}
}
void RemoteCapture::Run(RemoteOptions options,bool probe) {
    RemoteSnapshot state;state.state=probe?RemoteState::probing:RemoteState::starting;
    MetaQuestCapture meta(cancel_,&stop_);bool meta_requested=false;
    std::ofstream journal;std::string journal_phase,journal_message;auto next_pulse=Clock::now();
    auto publish=[&]{
        state.startup_elapsed=std::chrono::duration<double>(Clock::now()-launched_at_).count();Publish(state);
        if(journal&&(state.phase!=journal_phase||state.message!=journal_message||!state.Busy())){
            auto row=cygi::Json::Object();row["phase"]=state.phase;row["state"]=RemoteStateName(state.state);row["seconds"]=state.startup_elapsed;row["message"]=state.message;row["meta_message"]=state.meta_message;row["bytes"]=std::to_string(state.bytes);
            journal<<row.Write(-1)<<'\n';journal.flush();journal_phase=state.phase;journal_message=state.message;
        }
    };
    auto pulse=[&]{if(Clock::now()>=next_pulse){next_pulse=Clock::now()+std::chrono::milliseconds(250);publish();}};
    try {
        Winsock winsock;auto target=Address(options);
        if(!probe){Require(!options.output_directory.empty(),"Choose a capture output directory");fs::create_directories(options.output_directory);
            state.diagnostic_log=options.output_directory/("capture_control_"+std::to_string(Clock::now().time_since_epoch().count())+".jsonl");journal.open(state.diagnostic_log,std::ios::binary);Require(bool(journal),"Cannot create capture diagnostics");}
        state.phase="connect";state.message="Connecting to the capture plugin";publish();
        Socket control;Connect(control,target,cancel_,&stop_,pulse);Rpc rpc(control.value,cancel_);rpc.Setup(stop_,pulse);
        state.phase="hello";state.message="Waiting for the game's capture service to respond";publish();
        auto hello=rpc.Call("hello");state.manifest=hello["manifest"];
        Require(state.manifest.IsObject()&&state.manifest["metadata"].IsObject(),"Capture plugin returned no device metadata");
        state.capture_defaults=hello["capture_defaults"];state.capture_capabilities=hello["capture_capabilities"];
        Require(hello["state"].AsString()=="idle"&&!hello["native_trace_active"].AsBool(),"Another native capture is already active on the device");
        if(probe){state.state=RemoteState::ready;state.message="Capture plugin connected";publish();return;}
        meta_requested=options.meta_gpu.enabled&&hello["manifest"]["metadata"]["platform"].AsString()=="Android";
        if(meta_requested){
            const auto original_metadata=hello["manifest"]["metadata"];const auto old_pid=original_metadata["process_id"].AsUInt();
            state.phase="meta_prepare";state.message="Preparing the selected Quest game for GPU capture";state.meta_message="Preparing Meta hardware GPU capture";publish();
            if(options.meta_gpu.restart_game){control.Reset(INVALID_SOCKET);rpc.Reset(INVALID_SOCKET);}
            meta.Prepare(options.meta_gpu,options.ipv4,original_metadata);state.meta_message=meta.Message();publish();
            if(meta.Restarted()){
                const auto deadline=Clock::now()+std::chrono::seconds(120);bool connected=false;unsigned attempt=0;std::string last_error;
                state.phase="reconnect";
                while(!connected&&Clock::now()<deadline&&!cancel_.load()&&!stop_.load()){
                    state.message="Reconnecting to restarted game (attempt "+std::to_string(++attempt)+") at "+options.ipv4+":"+std::to_string(options.control_port)+(last_error.empty()?std::string():". Last error: "+last_error);publish();
                    try{
                        Connect(control,target,cancel_,&stop_,pulse);rpc.Reset(control.value);
                        state.message="Capture port connected; waiting for the restarted game's response";publish();
                        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-Clock::now()).count();Require(remaining>0,"Restart connection deadline reached");
                        hello=rpc.Call("hello",{},0,0,nullptr,false,unsigned(remaining));
                        const auto& metadata=hello["manifest"]["metadata"];
                        Require(metadata.IsObject(),"Restarted game returned no device metadata");
                        Require(original_metadata["android_package"].AsString().empty()||metadata["android_package"].AsString()==original_metadata["android_package"].AsString(),"Capture port belongs to another Android package");
                        Require(!old_pid||!metadata["process_id"].AsUInt()||metadata["process_id"].AsUInt()!=old_pid,"The previous game process is still answering after restart");
                        while(hello.Has("runtime_ready")&&!hello["runtime_ready"].AsBool()&&Clock::now()<deadline&&!stop_.load()&&!cancel_.load()){
                            state.message="Capture plugin connected; waiting for the first game frame";publish();std::this_thread::sleep_for(std::chrono::milliseconds(100));hello=rpc.Call("hello",{},0,0,nullptr,false,3000);
                        }
                        Require(!hello.Has("runtime_ready")||hello["runtime_ready"].AsBool(),"Game has not reached its first frame after restart");connected=true;
                    }catch(const std::exception& e){last_error=e.what();control.Reset(INVALID_SOCKET);rpc.Reset(INVALID_SOCKET);if(!stop_.load()&&!cancel_.load())std::this_thread::sleep_for(std::chrono::milliseconds(250));}
                }
                Require(!stop_.load()&&!cancel_.load(),"Capture stopped while reconnecting to restarted game");
                Require(connected,"The game restarted but its capture service did not become ready at "+options.ipv4+":"+std::to_string(options.control_port)+". "+last_error+". Check the plugin's TCP listener and the headset launch state; see capture diagnostics");
                Require(hello["state"].AsString()=="idle"&&!hello["native_trace_active"].AsBool(),"Quest game already has a capture after restart");
                state.capture_defaults=hello["capture_defaults"];state.capture_capabilities=hello["capture_capabilities"];
                state.manifest=hello["manifest"];state.message="Restarted game capture service ready";publish();
            }else if(control.value==INVALID_SOCKET){
                // Preparation can fail before restarting. Native capture still works,
                // using a fresh control connection rather than a closed old socket.
                state.phase="reconnect";state.message="Reconnecting for native capture after Meta preparation";publish();
                Connect(control,target,cancel_,&stop_,pulse);rpc.Reset(control.value);hello=rpc.Call("hello");state.manifest=hello["manifest"];
                Require(hello["state"].AsString()=="idle"&&!hello["native_trace_active"].AsBool(),"Another native capture became active during Meta preparation");
            }
        }
        if(hello.Has("runtime_ready")&&!hello["runtime_ready"].AsBool()){
            const auto ready_deadline=Clock::now()+std::chrono::seconds(120);state.phase="game_ready";state.message="Capture plugin connected; waiting for the first game frame";publish();
            while(!hello["runtime_ready"].AsBool()&&Clock::now()<ready_deadline&&!stop_.load()&&!cancel_.load()){
                std::this_thread::sleep_for(std::chrono::milliseconds(100));hello=rpc.Call("hello",{},0,0,nullptr,false,30000);
            }
            Require(!stop_.load()&&!cancel_.load(),"Capture stopped while waiting for the first game frame");
            Require(hello["runtime_ready"].AsBool(),"The game's capture service is reachable but no game frame has begun");state.manifest=hello["manifest"];publish();
        }
        if(options.override_capture_options){Require(options.capture.Validate().empty(),options.capture.Validate());
            Require(hello["capture_capabilities"]["capture_options_version"].AsNumber()==1,"This plugin cannot apply capture options; update it before overriding its defaults");
            Require(options.capture.quest_hz!=0||hello["capture_capabilities"]["quest_per_frame_observations"].AsBool(),"Update the target CyInsights plugin for per-frame Quest observations");
            Require(!options.capture.gpu||!options.capture.quest_gpu_completion||hello["manifest"]["metadata"]["platform"].AsString()!="Android"||hello["capture_capabilities"]["quest_gpu_completion"].AsBool(),"Update the Quest CyInsights plugin for GPU completion timestamps, or disable this option explicitly");
            Require(!options.capture.gpu_passes||hello["capture_capabilities"]["native_gpu_passes"].AsBool(),"Update the target CyInsights plugin for detailed native GPU passes");}
        Require(!options.output_directory.empty(),"Choose a capture output directory");
        Require(!stop_.load(),"Capture stopped during Meta GPU preparation");
        sockaddr_in local{};int length=sizeof(local);
        Require(getsockname(control.value,reinterpret_cast<sockaddr*>(&local),&length)==0,"Cannot determine PC route to device");
        local.sin_port=0;Socket listener(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));
        Require(listener.value!=INVALID_SOCKET&&bind(listener.value,reinterpret_cast<sockaddr*>(&local),sizeof(local))==0&&listen(listener.value,1)==0,
            "Cannot listen for native trace data");NonBlocking(listener.value);
        length=sizeof(local);Require(!getsockname(listener.value,reinterpret_cast<sockaddr*>(&local),&length),"Cannot determine trace receiver port");
        fs::create_directories(options.output_directory);
        auto name="remote_"+std::to_string(Clock::now().time_since_epoch().count());
        auto staging=options.output_directory/fs::u8path(name);
        Require(fs::create_directory(staging),"Cannot create fresh capture staging directory");
        if(meta_requested){state.phase="meta_start";state.message="Starting Meta recording on the headset";publish();auto metadata=hello["manifest"]["metadata"];for(const auto& clock:hello["manifest"]["clocks"].Items())if(clock["id"].AsString()=="unreal.cpu")metadata["cpu_frequency_hz"]=clock["frequency_hz"];meta.Begin(staging,options.seconds,metadata);state.meta_message=meta.Message();publish();}
        Require(!stop_.load(),"Capture stopped before native recording started");
        state.partial_trace=staging/"unreal.utrace.partial";
        std::ofstream out(state.partial_trace,std::ios::binary);Require(bool(out),"Cannot create received trace file");
        state.phase="native_start";state.message="Requesting native trace recording";publish();
        auto reply=rpc.Call("start",{},ntohs(local.sin_port),options.seconds,options.override_capture_options?&options.capture:nullptr,false,30000);
        rpc.Recording();
        state.session_id=reply["session_id"].AsString();Require(!state.session_id.empty(),"Remote capture has no session identity");
        if(meta_requested)meta.BindSession(state.session_id);
        state.phase="native_stream";state.manifest=reply["manifest"];state.gpu_message=reply["gpu_capture_message"].AsString();state.message="Waiting for device trace stream";publish();
        const auto started=Clock::now();auto next_status=started;
        Clock::time_point drain_deadline{};bool eof=false,stopped=false,stop_sent=false,header=false;
        Socket data;std::string prefix;
        std::array<char,65536> bytes{};
        while(!eof||!stopped) {
            Require(!cancel_.load(),"Capture cancelled; partial trace retained");
            auto now=Clock::now();state.elapsed=std::chrono::duration<double>(now-started).count();
            std::optional<CaptureOptions> gpu;
            {std::lock_guard<std::mutex> lock(mutex_);gpu=std::move(gpu_request_);gpu_request_.reset();}
            if(gpu&&!stop_sent&&!stopped&&!stop_.load()){
                const auto response=rpc.Call("gpu",state.session_id,0,0,&*gpu,true);
                state.gpu_message=response["ok"].AsBool()?response["gpu_capture_message"].AsString():response["error"].AsString();
            }
            if(stop_.load()&&!stop_sent) {
                reply=rpc.Call("stop",state.session_id);stop_sent=true;state.state=RemoteState::stopping;
                next_status=Clock::now();
            }
            if(now>=next_status) {
                reply=rpc.Call("status");next_status=Clock::now()+std::chrono::seconds(1);
                state.capture_capabilities=reply["capture_capabilities"];
                Require(reply["session_id"].AsString()==state.session_id,"Remote session changed during capture");
                Require(reply["capture_error"].AsString().empty(),reply["capture_error"].AsString());
                const auto remote_state=reply["state"].AsString();
                Require(remote_state=="starting"||remote_state=="recording"||remote_state=="idle","Unknown remote capture state");
                if(remote_state=="idle") {
                    state.manifest=reply["manifest"];
                    if(!stopped){stopped=true;drain_deadline=Clock::now()+std::chrono::seconds(10);}
                    state.state=RemoteState::stopping;
                }
            }
            now=Clock::now();
            Require(!stopped||now<drain_deadline,"Trace writer did not close after stop; partial trace retained");
            if(data.value==INVALID_SOCKET) {
                Require(now-started<std::chrono::seconds(15),"Device could not send trace data to the PC; check the PC firewall/route");
                if(Ready(listener.value,false,20)) {
                    sockaddr_in peer{};int n=sizeof(peer);SOCKET candidate=accept(listener.value,reinterpret_cast<sockaddr*>(&peer),&n);
                    if(candidate!=INVALID_SOCKET) {
                        if(peer.sin_addr.s_addr==target.sin_addr.s_addr){data.Reset(candidate);NonBlocking(data.value);}
                        else closesocket(candidate);
                    }
                }
            } else if(!eof&&Ready(data.value,false,20)) {
                int n=recv(data.value,bytes.data(),static_cast<int>(bytes.size()),0);
                if(n==SOCKET_ERROR&&WSAGetLastError()==WSAEWOULDBLOCK)continue;
                Require(n!=SOCKET_ERROR,"Trace stream interrupted; partial trace retained");
                if(n==0) {
                    eof=true;std::string error;Require(header||NativeTraceHeader(prefix,true,error),error);
                    if(!stopped){next_status=Clock::now();drain_deadline=now+std::chrono::seconds(3);}
                } else {
                    Require(state.bytes+uint64_t(n)<=MaxBytes,"Trace exceeds capture size limit");
                    out.write(bytes.data(),n);Require(bool(out),"Cannot save trace bytes; partial trace retained");state.bytes+=n;
                    if(!header) {
                        prefix.append(bytes.data(),std::min<size_t>(size_t(n),4104-prefix.size()));
                        std::string error;header=NativeTraceHeader(prefix,false,error);Require(error.empty(),error);
                    }
                    if(header&&!stopped&&!stop_sent){state.state=RemoteState::recording;state.message="Native trace streaming from device";}
                }
            } else if(eof) {
                Require(stopped||now<drain_deadline,"Trace ended without a confirmed capture stop");
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            publish();
        }
        out.close();Require(bool(out),"Cannot finalize received trace");
        std::string error;Require(ValidateManifest(state.manifest,error),error);
        Require(state.manifest["session_id"].AsString()==state.session_id,"Final manifest session mismatch");
        // Every advertised payload must actually be present in the archive.
        for(const auto& source:state.manifest["sources"].Items())
            if(source.Has("path"))Require(source["path"].AsString()=="unreal.utrace","Remote manifest references an untransferred payload");
        if(meta_requested){state.phase="meta_receive";state.state=RemoteState::packaging;state.meta_message="Receiving Meta hardware GPU trace";publish();meta.End();state.meta_message=meta.Message();auto sources=state.manifest["sources"];sources.Push(meta.Source());state.manifest["sources"]=sources;}
        auto transport=state.manifest["network"];
        transport["received_bytes"]=std::to_string(state.bytes);transport["completion"]="native EOF and remote stop acknowledged";
        state.manifest["network"]=transport;
        std::ofstream manifest(staging/"manifest.json",std::ios::binary);manifest<<state.manifest.Write(2);manifest.close();
        Require(bool(manifest),"Cannot save device manifest");
        auto trace=staging/"unreal.utrace";fs::rename(state.partial_trace,trace);state.partial_trace=trace;
        state.phase="package";state.state=RemoteState::packaging;state.message="Packaging native trace and device metadata";publish();
        auto archive=options.output_directory/fs::u8path(name+".cycapture");Require(Pack(staging,archive,error),error);
        state.phase="complete";state.archive=archive;state.partial_trace.clear();state.state=RemoteState::complete;state.message="Capture saved on PC";publish();
    } catch(const std::exception& e) {
        state.phase="failed";state.state=RemoteState::failed;state.message=e.what();state.finishing=meta_requested;publish();
        if(meta_requested){meta.End();state.meta_message=meta.Message();}
        // Closing our control connection asks the plugin to stop only our session.
        state.finishing=false;publish();
    }
}
}
