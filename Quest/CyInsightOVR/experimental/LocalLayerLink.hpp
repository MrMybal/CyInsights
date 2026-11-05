// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <android/log.h>
#include <dlfcn.h>
#include <jni.h>
#include "GraphHudData.hpp"
#include "TargetMetaBridge.hpp"
namespace cylink {
// No render callback performs IPC, shell commands, file parsing or waits.
class Link {
    std::atomic<bool> stop{false},connected{false};std::atomic<int> rate{0},socketFd{-1};std::thread worker;
    std::mutex lock;std::vector<char> tiles;std::string counters;std::shared_ptr<const cygraphs::Sample> graphs;
    std::atomic<uint64_t> hudRecorded{0};std::atomic<uint32_t> hudWidth{0},hudHeight{0},hudMetrics{0};
    static bool Receive(int fd,void* data,size_t size){auto p=static_cast<char*>(data);while(size){auto n=recv(fd,p,size,0);if(n<=0)return false;p+=n;size-=size_t(n);}return true;}
    static bool Send(int fd,const void* data,size_t size){auto p=static_cast<const char*>(data);while(size){auto n=send(fd,p,size,MSG_NOSIGNAL);if(n<=0)return false;p+=n;size-=size_t(n);}return true;}
    static bool Word(int fd,uint32_t value){value=htonl(value);return Send(fd,&value,4);}
    static int OpenChannel(int& stage){
        // The JVM belongs to the target. Android authenticates this ContentProvider
        // request with its real UID/PID and transfers a private socket descriptor.
        stage=1;using GetVMs=jint(*)(JavaVM**,jsize,jsize*);
        auto getVMs=reinterpret_cast<GetVMs>(dlsym(RTLD_DEFAULT,"JNI_GetCreatedJavaVMs"));
        if(!getVMs){static void* helper=dlopen("libnativehelper.so",RTLD_NOW|RTLD_LOCAL);if(helper)getVMs=reinterpret_cast<GetVMs>(dlsym(helper,"JNI_GetCreatedJavaVMs"));}
        if(!getVMs)return -1;stage=2;JavaVM* vm=nullptr;jsize count=0;if(getVMs(&vm,1,&count)!=JNI_OK||count!=1||!vm)return -1;
        stage=3;JNIEnv* env=nullptr;if(vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)return -1;
        int fd=-1;jobject provider=nullptr;
        if(env->PushLocalFrame(32)==JNI_OK){
            auto open=[&]()->int{
                stage=4;auto activity=env->FindClass("android/app/ActivityThread");if(!activity)return -1;
                auto current=env->GetStaticMethodID(activity,"currentApplication","()Landroid/app/Application;");if(!current)return -1;
                stage=5;auto app=env->CallStaticObjectMethod(activity,current);if(!app||env->ExceptionCheck())return -1;
                auto context=env->GetObjectClass(app);if(!context)return -1;
                auto getResolver=env->GetMethodID(context,"getContentResolver","()Landroid/content/ContentResolver;");if(!getResolver)return -1;
                stage=6;auto resolver=env->CallObjectMethod(app,getResolver);if(!resolver||env->ExceptionCheck())return -1;
                auto uriClass=env->FindClass("android/net/Uri");if(!uriClass)return -1;
                auto parse=env->GetStaticMethodID(uriClass,"parse","(Ljava/lang/String;)Landroid/net/Uri;");if(!parse)return -1;
                auto text=env->NewStringUTF("content://org.cyinsights.ovr.layer/visibility");if(!text)return -1;
                auto uri=env->CallStaticObjectMethod(uriClass,parse,text);if(!uri||env->ExceptionCheck())return -1;
                auto resolverClass=env->GetObjectClass(resolver);if(!resolverClass)return -1;
                auto acquire=env->GetMethodID(resolverClass,"acquireUnstableContentProviderClient","(Landroid/net/Uri;)Landroid/content/ContentProviderClient;");if(!acquire)return -1;
                provider=env->CallObjectMethod(resolver,acquire,uri);if(!provider||env->ExceptionCheck())return -1;
                auto providerClass=env->GetObjectClass(provider);if(!providerClass)return -1;
                auto method=env->GetMethodID(providerClass,"openFile","(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;");if(!method)return -1;
                auto mode=env->NewStringUTF("rw");if(!mode)return -1;
                stage=7;auto descriptor=env->CallObjectMethod(provider,method,uri,mode);if(!descriptor||env->ExceptionCheck())return -1;
                auto descriptorClass=env->GetObjectClass(descriptor);if(!descriptorClass)return -1;
                auto detach=env->GetMethodID(descriptorClass,"detachFd","()I");if(!detach)return -1;
                stage=8;auto value=env->CallIntMethod(descriptor,detach);return env->ExceptionCheck()?-1:value;
            };
            fd=open();if(env->ExceptionCheck())env->ExceptionClear();
            // Never keep a stable provider dependency on the profiling app:
            // updating/restarting it must not terminate the target game.
            if(provider){auto type=env->GetObjectClass(provider);auto close=type?env->GetMethodID(type,"close","()V"):nullptr;if(close)env->CallVoidMethod(provider,close);if(env->ExceptionCheck())env->ExceptionClear();}
            env->PopLocalFrame(nullptr);
        }else if(env->ExceptionCheck())env->ExceptionClear();
        vm->DetachCurrentThread();return fd;
    }
    void Run(){
        std::ifstream process("/proc/self/cmdline",std::ios::binary);std::string name;std::getline(process,name,'\0');if(name.empty()||name.size()>200)return;
        auto reported=std::chrono::steady_clock::time_point{};
        while(!stop){int stage=0;int fd=OpenChannel(stage);socketFd=fd;std::unique_ptr<TargetMetaBridge> meta;std::string metaReport="{}";
            if(fd>=0){fcntl(fd,F_SETFD,FD_CLOEXEC);timeval timeout{2,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
                bool valid=Word(fd,0x43594c31)&&Word(fd,4)&&Word(fd,uint32_t(name.size()))&&Send(fd,name.data(),name.size());
                uint32_t apkLength=0;valid=valid&&Receive(fd,&apkLength,4);apkLength=ntohl(apkLength);
                if(!valid||apkLength<1||apkLength>2048){close(fd);fd=-1;socketFd=-1;}
                else {std::string apk(apkLength,'\0');if(!Receive(fd,&apk[0],apk.size())||apk.find("/data/app/")!=0||apk.size()<9||apk.substr(apk.size()-9)!="/base.apk"){close(fd);fd=-1;socketFd=-1;}else meta=std::make_unique<TargetMetaBridge>(apk);}}
            if(fd<0){auto now=std::chrono::steady_clock::now();if(now-reported>=std::chrono::seconds(10)){__android_log_print(ANDROID_LOG_WARN,"CyOVRLink","Android GPU channel unavailable stage=%d; open CyInsightOVR and prepare this target",stage);reported=now;}}
            while(fd>=0&&!stop){std::string sample;{std::lock_guard<std::mutex> guard(lock);sample=counters;}char report[256];
                std::snprintf(report,sizeof(report),"{\"pid\":%d,\"hud_recorded_ns\":\"%llu\",\"hud_width\":%u,\"hud_height\":%u,\"hud_metrics\":%u,\"counters\":",getpid(),(unsigned long long)hudRecorded.load(),hudWidth.load(),hudHeight.load(),hudMetrics.load());
                sample=std::string(report)+(sample.empty()?"{}":sample)+",\"meta_hud\":"+metaReport+"}";
                if(!Word(fd,uint32_t(sample.size()))||!Send(fd,sample.data(),sample.size()))break;uint32_t header[3];if(!Receive(fd,header,sizeof(header)))break;
                auto magic=ntohl(header[0]),hz=ntohl(header[1]),length=ntohl(header[2]);if(magic!=0x43594c31||(hz!=0&&hz!=1&&hz!=5&&hz!=10&&hz!=20)||length>20520)break;
                std::vector<char> bytes(length);if(!Receive(fd,bytes.data(),bytes.size()))break;
                uint32_t graphLength=0;if(!Receive(fd,&graphLength,4))break;graphLength=ntohl(graphLength);if(graphLength>32768)break;std::vector<char> graphBytes(graphLength);if(!Receive(fd,graphBytes.data(),graphBytes.size()))break;auto curves=cygraphs::Parse(graphBytes);
                uint32_t metaLength=0;if(!Receive(fd,&metaLength,4))break;metaLength=ntohl(metaLength);if(metaLength>32768)break;std::string metaPacket(metaLength,'\0');if(metaLength&&!Receive(fd,&metaPacket[0],metaLength))break;if(metaLength&&meta)metaReport=meta->Update(metaPacket);
                {std::lock_guard<std::mutex> guard(lock);tiles=std::move(bytes);graphs=std::move(curves);}rate=int(hz);if(!connected)__android_log_print(ANDROID_LOG_INFO,"CyOVRLink","Local GPU service connected and authenticated");connected=true;
                std::this_thread::sleep_for(std::chrono::milliseconds(hz?1000/hz:100));
            }
            meta.reset();connected=false;rate=0;{std::lock_guard<std::mutex> guard(lock);tiles.clear();counters.clear();graphs.reset();}socketFd=-1;if(fd>=0)close(fd);
            for(int i=0;i<20&&!stop;++i)std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
public:
    Link(){worker=std::thread([this]{Run();});}
    ~Link(){stop=true;int fd=socketFd;if(fd>=0)shutdown(fd,SHUT_RDWR);if(worker.joinable())worker.join();}
    bool Connected()const{return connected;}
    int CounterRate()const{return rate;}
    std::vector<char> Tiles(){std::lock_guard<std::mutex> guard(lock);return tiles;}
    std::shared_ptr<const cygraphs::Sample> Graphs(){std::lock_guard<std::mutex> guard(lock);return graphs;}
    void Counters(const std::string& json){std::lock_guard<std::mutex> guard(lock);if(json.size()<=16384)counters=json;}
    void Recorded(uint64_t stamp,uint32_t width,uint32_t height,uint32_t count){hudWidth=width;hudHeight=height;hudMetrics=count;hudRecorded=stamp;}
};
inline Link& Host(){static Link link;return link;}
}
