// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
// Development-layer input. Private target storage only; no network endpoint.
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <time.h>
#include <unistd.h>
namespace cytiles {
inline uint64_t Now(){timespec t{};clock_gettime(CLOCK_MONOTONIC,&t);return uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;}
struct Tile {float x,y,width,height,cost;};
struct Sample {uint32_t width=0,height=0,position=4;float ceiling=500,opacity=.25f;uint64_t stamp=0;std::vector<Tile> tiles;};
inline std::string Path(){std::ifstream f("/proc/self/cmdline",std::ios::binary);std::string package;std::getline(f,package,'\0');
    if(package.empty()||package.size()>200||package.find('.')==std::string::npos)return {};
    for(char c:package)if(!((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.'))return {};
    return "/data/user/"+std::to_string(getuid()/100000)+"/"+package+"/files/cyinsights-live-tiles.bin";
}
class Reader {
    std::atomic<bool> stopping{false};std::thread worker;std::mutex mutex;std::shared_ptr<const Sample> current;
    static bool Read(std::istream& f,void* value,size_t size){return bool(f.read(static_cast<char*>(value),size));}
    void Poll(const std::string& path){
        while(!stopping){
            std::ifstream f(path,std::ios::binary);uint32_t magic=0,version=0,count=0;auto s=std::make_shared<Sample>();
            bool valid=Read(f,&magic,4)&&magic==0x43595431&&Read(f,&version,4)&&version==1&&Read(f,&s->width,4)&&Read(f,&s->height,4)&&Read(f,&count,4)&&Read(f,&s->position,4)&&Read(f,&s->ceiling,4)&&Read(f,&s->opacity,4)&&Read(f,&s->stamp,8);
            uint64_t now=Now();valid=valid&&s->width>0&&s->width<=16384&&s->height>0&&s->height<=16384&&count>0&&count<=1024&&(s->position&~256u)<9&&std::isfinite(s->ceiling)&&s->ceiling>0&&std::isfinite(s->opacity)&&s->opacity>0&&s->opacity<=1&&s->stamp<=now&&now-s->stamp<3000000000ull;
            if(valid){s->tiles.resize(count);valid=Read(f,s->tiles.data(),count*sizeof(Tile));}
            if(valid)for(const auto& t:s->tiles)if(!std::isfinite(t.x)||!std::isfinite(t.y)||!std::isfinite(t.width)||!std::isfinite(t.height)||!std::isfinite(t.cost)||t.x<0||t.y<0||t.width<=0||t.height<=0||t.x+t.width>s->width+.5f||t.y+t.height>s->height+.5f||t.cost< -1){valid=false;break;}
            if(valid){std::lock_guard<std::mutex> lock(mutex);current=std::move(s);}
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
public:
    Reader(){auto path=Path();if(!path.empty()){
        const std::string ready=path+".ready";int fd=open(ready.c_str(),O_WRONLY|O_CREAT|O_TRUNC|O_CLOEXEC,0600);if(fd>=0){char json[128];int n=std::snprintf(json,sizeof(json),"{\"version\":1,\"pid\":%d,\"sampled\":true}\n",getpid());write(fd,json,size_t(n));close(fd);}
        worker=std::thread([this,path]{Poll(path);});}}
    ~Reader(){stopping=true;if(worker.joinable())worker.join();}
    std::shared_ptr<const Sample> Latest(){std::lock_guard<std::mutex> lock(mutex);auto s=current;if(s&&Now()-s->stamp<3000000000ull)return s;return {};}
};
}
