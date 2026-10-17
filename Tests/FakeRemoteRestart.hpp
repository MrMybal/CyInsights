// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Local TCP game double. No Unreal process, headset or external ADB is used.
#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <chrono>
class FakeRemoteRestart {
    SOCKET listener=INVALID_SOCKET;std::thread thread;
    std::atomic<bool> finish{false};std::filesystem::path root;bool stall;
    bool IsRestarted()const{return std::filesystem::exists(root/"restarted");}
    void Serve(){
        while(!finish){
            auto client=accept(listener,nullptr,nullptr);if(client==INVALID_SOCKET)return;
            DWORD timeout=100;setsockopt(client,SOL_SOCKET,SO_RCVTIMEO,reinterpret_cast<char*>(&timeout),sizeof(timeout));
            std::string input;bool recording=false;std::thread stream;std::atomic<bool> stop{false};auto started=std::chrono::steady_clock::now();
            while(!finish){
                char bytes[2048];auto n=recv(client,bytes,sizeof(bytes),0);
                if(n==SOCKET_ERROR&&WSAGetLastError()==WSAETIMEDOUT)continue;
                if(n<=0)break;input.append(bytes,n);auto end=input.find('\n');if(end==std::string::npos)continue;
                cygi::Json request;cygi::Json::Parse(input.substr(0,end),request);input.erase(0,end+1);auto command=request["command"].AsString();
                if(IsRestarted()&&command=="hello"){restart_hello=true;if(stall)continue;}
                if(command=="start"){
                    start_count++;recording=true;started=std::chrono::steady_clock::now();const auto port=uint16_t(request["receiver_port"].AsUInt());
                    stream=std::thread([this,port,&stop]{auto s=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);
                        if(!connect(s,reinterpret_cast<sockaddr*>(&a),sizeof(a))){auto body=std::string("2CRT\0\0\4\7",8)+std::string(256,'X');send(s,body.data(),int(body.size()),0);while(!finish&&!stop)std::this_thread::sleep_for(std::chrono::milliseconds(10));shutdown(s,SD_SEND);}closesocket(s);
                    });
                }
                if(command=="stop"||(recording&&std::chrono::steady_clock::now()-started>std::chrono::milliseconds(200))){recording=false;stop=true;}
                auto reply=cygi::Json::Object();reply["version"]=1;reply["ok"]=true;reply["state"]=recording?"recording":"idle";reply["native_trace_active"]=recording;reply["session_id"]="restart-test";
                auto& manifest=reply["manifest"];manifest["format"]="cycapture";manifest["version"]=1;manifest["session_id"]="restart-test";
                auto& metadata=manifest["metadata"];metadata["platform"]="Android";metadata["android_package"]="com.test.Game";metadata["process_id"]=IsRestarted()?123:122;metadata["project"]="TEST FIXTURE restarted game";
                manifest["clocks"]=cygi::Json::Array();auto clock=cygi::Json::Object();clock["id"]="unreal.cpu";clock["frequency_hz"]=1000000.;manifest["clocks"].Push(clock);
                manifest["sources"]=cygi::Json::Array();auto source=cygi::Json::Object();source["id"]="unreal.trace";source["status"]="measured";source["path"]="unreal.utrace";manifest["sources"].Push(source);
                // Startup capability is optional for old plugins. Here it appears
                // before the first game frame to exercise the readiness wait.
                reply["runtime_ready"]=!IsRestarted()||++ready_polls>=3;reply["game_frame"]=std::to_string(ready_polls);
                auto wire=reply.Write(-1)+"\n";send(client,wire.data(),int(wire.size()),0);
            }
            stop=true;if(stream.joinable())stream.join();closesocket(client);
        }
    }
public:
    uint16_t port=0;std::atomic<bool> restart_hello{false};std::atomic<unsigned> start_count{0},ready_polls{0};
    explicit FakeRemoteRestart(const std::filesystem::path& r,bool s=false):root(r),stall(s){
        listener=socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);sockaddr_in a{};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
        if(bind(listener,reinterpret_cast<sockaddr*>(&a),sizeof(a))||listen(listener,2))throw std::runtime_error("Fake restart listener unavailable");int size=sizeof(a);getsockname(listener,reinterpret_cast<sockaddr*>(&a),&size);port=ntohs(a.sin_port);thread=std::thread([this]{Serve();});
    }
    ~FakeRemoteRestart(){finish=true;closesocket(listener);if(thread.joinable())thread.join();}
};
