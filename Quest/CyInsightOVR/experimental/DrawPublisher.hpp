// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "DrawCounters.hpp"
namespace cydraw {
// All file I/O is on this worker, never on a draw/submit/present callback.
class Publisher {
    std::mutex lock;std::atomic<bool> stop{false};std::thread worker;
    Counts total,completed;std::map<uintptr_t,Counts> pending;uint64_t submits=0,frames=0;
    const uint64_t session=cytiles::Now();
    void Run(){
        auto path=cytiles::Path();auto at=path.rfind("cyinsights-live-tiles.bin");if(at==std::string::npos)return;
        path.replace(at,std::string::npos,"cyinsights-draw-counters.json");
        while(!stop){
            uint64_t lease=0;int hz=0;{std::ifstream control(path+".enabled");control>>lease>>hz;}
            uint64_t now=cytiles::Now();
            if((hz!=1&&hz!=5&&hz!=10&&hz!=20)||lease>now||now-lease>2000000000ull){std::this_thread::sleep_for(std::chrono::milliseconds(250));continue;}
            Counts value,done;uint64_t s,f;size_t deviceCount;
            {std::lock_guard<std::mutex> guard(lock);value=total;done=completed;s=submits;f=frames;deviceCount=pending.size();}
            char data[2048];int size=std::snprintf(data,sizeof(data),
                "{\"version\":1,\"pid\":%d,\"session\":\"%llu\",\"observed_mono_ns\":\"%llu\",\"submissions\":%llu,\"devices\":%zu,"
                "\"draws\":%llu,\"triangles\":%llu,\"indirect\":%llu,\"unknown_triangles\":%llu,\"unknown_draws\":%llu,\"missing_buffers\":%llu,\"overflow\":%s,"
                "\"presented_frames\":%llu,\"frame_draws\":%llu,\"frame_triangles\":%llu,\"frame_unknown_draws\":%llu,\"frame_unknown_triangles\":%llu,\"frame_missing_buffers\":%llu}\n",
                getpid(),(unsigned long long)session,(unsigned long long)cytiles::Now(),(unsigned long long)s,deviceCount,
                (unsigned long long)value.draws,(unsigned long long)value.triangles,(unsigned long long)value.indirect,
                (unsigned long long)value.unknownTriangles,(unsigned long long)value.unknownDraws,(unsigned long long)value.missingBuffers,value.overflow?"true":"false",
                (unsigned long long)f,(unsigned long long)done.draws,(unsigned long long)done.triangles,(unsigned long long)done.unknownDraws,(unsigned long long)done.unknownTriangles,(unsigned long long)done.missingBuffers);
            int fd=open((path+".tmp").c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);
            if(fd>=0){bool ok=size>0&&size<int(sizeof(data))&&write(fd,data,size)==size;close(fd);if(ok)rename((path+".tmp").c_str(),path.c_str());}
            std::this_thread::sleep_for(std::chrono::milliseconds(1000/hz));
        }
    }
public:
    Publisher(){worker=std::thread([this]{Run();});}
    ~Publisher(){stop=true;if(worker.joinable())worker.join();}
    void Submit(uintptr_t device,const Counts& value){std::lock_guard<std::mutex> guard(lock);total+=value;pending[device]+=value;++submits;}
    void Present(uintptr_t device){std::lock_guard<std::mutex> guard(lock);completed+=pending[device];pending[device]={};++frames;}
    void Forget(uintptr_t device){std::lock_guard<std::mutex> guard(lock);pending.erase(device);}
};
inline Publisher& Output(){static Publisher publisher;return publisher;}
}
