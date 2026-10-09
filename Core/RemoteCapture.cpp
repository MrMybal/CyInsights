// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "RemoteCapture.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
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
void Connect(Socket& s, const sockaddr_in& a, const std::atomic<bool>& cancel) {
    s.Reset(socket(AF_INET,SOCK_STREAM,IPPROTO_TCP));Require(s.value!=INVALID_SOCKET,"Cannot create control socket");
    NonBlocking(s.value);
    if(connect(s.value,reinterpret_cast<const sockaddr*>(&a),sizeof(a))==0)return;
    Require(WSAGetLastError()==WSAEWOULDBLOCK,"Cannot connect to the game's capture endpoint");
    auto deadline=Clock::now()+std::chrono::seconds(3);
    while(Clock::now()<deadline&&!cancel.load()) {
        fd_set write,errors;FD_ZERO(&write);FD_ZERO(&errors);FD_SET(s.value,&write);FD_SET(s.value,&errors);
        timeval t{0,50000};int n=select(0,nullptr,&write,&errors,&t);
        Require(n!=SOCKET_ERROR,"TCP connection failed");
        if(n) {int e=0,len=sizeof(e);Require(!getsockopt(s.value,SOL_SOCKET,SO_ERROR,reinterpret_cast<char*>(&e),&len)&&e==0,
            "Cannot connect: check the device IP, plugin and TCP control port");return;}
    }
    throw std::runtime_error(cancel.load()?"Capture cancelled":"Connection timed out: check device IP and TCP control port");
}
class Rpc {
public:
    Rpc(SOCKET s, const std::atomic<bool>& cancel): socket_(s),cancel_(cancel) {}
    cygi::Json Call(const char* command, const std::string& session={}, uint16_t port=0, double seconds=0) {
        cygi::Json request=cygi::Json::Object();request["version"]=1;request["command"]=command;
        if(!session.empty())request["session_id"]=session;
        if(port){request["receiver_port"]=int(port);request["seconds"]=seconds;}
        std::string wire=request.Write(-1)+"\n";
        const auto deadline=Clock::now()+std::chrono::seconds(3);
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
                Require(reply["ok"].AsBool(),reply["error"].AsString().empty()?"Remote capture refused":reply["error"].AsString());
                return reply;
            }
            if(!Ready(socket_,false,50))continue;
            char bytes[4096];int n=recv(socket_,bytes,sizeof(bytes),0);
            if(n==SOCKET_ERROR&&WSAGetLastError()==WSAEWOULDBLOCK)continue;
            Require(n>0,"Capture control connection closed");input_.append(bytes,n);
            Require(input_.size()<=65536,"Capture control response exceeds limit");
        }
    }
private:
    void Check(Clock::time_point deadline) {
        Require(!cancel_.load(),"Capture cancelled; received partial trace retained");
        Require(Clock::now()<deadline,"Capture control response timed out");
    }
    SOCKET socket_; const std::atomic<bool>& cancel_;std::string input_;
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
RemoteSnapshot RemoteCapture::Snapshot() const {std::lock_guard<std::mutex> lock(mutex_);return snapshot_;}
void RemoteCapture::Publish(const RemoteSnapshot& s) {std::lock_guard<std::mutex> lock(mutex_);snapshot_=s;}
bool RemoteCapture::Probe(const RemoteOptions& o){return Launch(o,true);}
bool RemoteCapture::Start(const RemoteOptions& o){return Launch(o,false);}
bool RemoteCapture::Launch(const RemoteOptions& o,bool probe) {
    if(Snapshot().Busy())return false;
    if(worker_.joinable())worker_.join();
    stop_.store(false);cancel_.store(false);
    RemoteSnapshot s;s.state=probe?RemoteState::probing:RemoteState::starting;Publish(s);
    try {worker_=std::thread([this,o,probe](){Run(o,probe);});return true;}
    catch(const std::exception& e){s.state=RemoteState::failed;s.message=e.what();Publish(s);return false;}
}
void RemoteCapture::Run(RemoteOptions options,bool probe) {
    RemoteSnapshot state;state.state=probe?RemoteState::probing:RemoteState::starting;
    try {
        Winsock winsock;auto target=Address(options);Socket control;Connect(control,target,cancel_);Rpc rpc(control.value,cancel_);
        auto hello=rpc.Call("hello");state.manifest=hello["manifest"];
        Require(hello["state"].AsString()=="idle"&&!hello["native_trace_active"].AsBool(),"Another native capture is already active on the device");
        if(probe){state.state=RemoteState::ready;state.message="Capture plugin connected";Publish(state);return;}
        Require(!options.output_directory.empty(),"Choose a capture output directory");
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
        state.partial_trace=staging/"unreal.utrace.partial";
        std::ofstream out(state.partial_trace,std::ios::binary);Require(bool(out),"Cannot create received trace file");
        auto reply=rpc.Call("start",{},ntohs(local.sin_port),options.seconds);
        state.session_id=reply["session_id"].AsString();Require(!state.session_id.empty(),"Remote capture has no session identity");
        state.manifest=reply["manifest"];state.message="Waiting for device trace stream";Publish(state);
        const auto started=Clock::now();auto next_status=started;
        Clock::time_point drain_deadline{};bool eof=false,stopped=false,stop_sent=false,header=false;
        Socket data;std::string prefix;
        std::array<char,65536> bytes{};
        while(!eof||!stopped) {
            Require(!cancel_.load(),"Capture cancelled; partial trace retained");
            auto now=Clock::now();state.elapsed=std::chrono::duration<double>(now-started).count();
            if(stop_.load()&&!stop_sent) {
                reply=rpc.Call("stop",state.session_id);stop_sent=true;state.state=RemoteState::stopping;
                next_status=Clock::now();
            }
            if(now>=next_status) {
                reply=rpc.Call("status");next_status=Clock::now()+std::chrono::seconds(1);
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
            Publish(state);
        }
        out.close();Require(bool(out),"Cannot finalize received trace");
        std::string error;Require(ValidateManifest(state.manifest,error),error);
        Require(state.manifest["session_id"].AsString()==state.session_id,"Final manifest session mismatch");
        // Every advertised payload must actually be present in the archive.
        for(const auto& source:state.manifest["sources"].Items())
            if(source.Has("path"))Require(source["path"].AsString()=="unreal.utrace","Remote manifest references an untransferred payload");
        auto transport=state.manifest["network"];
        transport["received_bytes"]=std::to_string(state.bytes);transport["completion"]="native EOF and remote stop acknowledged";
        state.manifest["network"]=transport;
        std::ofstream manifest(staging/"manifest.json",std::ios::binary);manifest<<state.manifest.Write(2);manifest.close();
        Require(bool(manifest),"Cannot save device manifest");
        auto trace=staging/"unreal.utrace";fs::rename(state.partial_trace,trace);state.partial_trace=trace;
        state.state=RemoteState::packaging;state.message="Packaging native trace and device metadata";Publish(state);
        auto archive=options.output_directory/fs::u8path(name+".cycapture");Require(Pack(staging,archive,error),error);
        state.archive=archive;state.partial_trace.clear();state.state=RemoteState::complete;state.message="Capture saved on PC";Publish(state);
    } catch(const std::exception& e) {
        // Closing our control connection asks the plugin to stop only our session.
        state.state=RemoteState::failed;state.message=e.what();Publish(state);
    }
}
}
