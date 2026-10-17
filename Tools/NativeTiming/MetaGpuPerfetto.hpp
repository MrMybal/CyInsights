// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
// Independent decoder of the documented Perfetto GPU wire messages. No
// Perfetto implementation is vendored. See Docs/MetaQuestGpu.md for schemas.
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
namespace cymeta {
struct Field {uint32_t id=0,wire=0;uint64_t number=0;std::string_view bytes;};
class Wire {
    std::string_view data;size_t pos=0;
public:
    explicit Wire(std::string_view d):data(d){}
    uint64_t Var(){uint64_t n=0;for(int s=0;s<70;s+=7){if(pos==data.size())throw std::runtime_error("Truncated Perfetto varint");auto b=uint8_t(data[pos++]);if(s==63&&(b&254))throw std::runtime_error("Overflowed Perfetto varint");n|=uint64_t(b&127)<<s;if(!(b&128))return n;}throw std::runtime_error("Invalid Perfetto varint");}
    bool Next(Field& f){if(pos==data.size())return false;auto tag=Var();f={};f.id=uint32_t(tag>>3);f.wire=tag&7;if(!f.id)throw std::runtime_error("Invalid Perfetto field");if(f.wire==0)f.number=Var();else{uint64_t n=f.wire==2?Var():f.wire==1?8:f.wire==5?4:0;if(!n&&f.wire!=2)throw std::runtime_error("Unsupported Perfetto wire type");if(n>data.size()-pos)throw std::runtime_error("Truncated Perfetto field");f.bytes=data.substr(pos,size_t(n));pos+=size_t(n);if(f.wire==1)std::memcpy(&f.number,f.bytes.data(),8);}return true;}
};
inline Field Find(std::string_view data,uint32_t id){Wire r(data);Field f;while(r.Next(f))if(f.id==id)return f;return {};}
inline uint64_t Number(std::string_view data,uint32_t id,uint64_t missing=0){auto f=Find(data,id);return f.id?f.number:missing;}
inline std::string Text(std::string_view data,uint32_t id){auto f=Find(data,id);return f.id&&f.wire==2?std::string(f.bytes):std::string();}
inline void Packets(const std::filesystem::path& path,const std::function<void(std::string_view)>& sink){
    std::ifstream in(path,std::ios::binary);if(!in)throw std::runtime_error("Meta Perfetto file unavailable");
    auto var=[&](){uint64_t n=0;for(int s=0;s<70;s+=7){int b=in.get();if(b==EOF)throw std::runtime_error("Truncated Perfetto file");if(s==63&&(b&254))throw std::runtime_error("Overflowed Perfetto length");n|=uint64_t(b&127)<<s;if(!(b&128))return n;}throw std::runtime_error("Invalid Perfetto length");};
    while(in.peek()!=EOF){const auto tag=var();if(tag!=10)throw std::runtime_error("Expected Perfetto Trace.packet");auto n=var();if(n>256ull*1024*1024)throw std::runtime_error("Malformed oversized Perfetto packet");std::string packet(size_t(n),'\0');if(!in.read(packet.data(),std::streamsize(n)))throw std::runtime_error("Truncated Perfetto packet");sink(packet);}
}
inline void Var(std::string& out,uint64_t n){while(n>=128){out+=char((n&127)|128);n>>=7;}out+=char(n);}
inline void Integer(std::string& out,uint32_t id,uint64_t n){Var(out,uint64_t(id)<<3);Var(out,n);}
inline void Message(std::string& out,uint32_t id,std::string_view s){Var(out,(uint64_t(id)<<3)|2);Var(out,s.size());out.append(s);}
// Streaming file config, never a whole-capture ring buffer or text-parser API.
inline std::string Config(uint32_t duration_ms){
    std::string config,buffer;Integer(buffer,1,32768);Integer(buffer,4,2);Message(config,1,buffer);
    for(const auto& name:{"gpu.renderstages.oculus","gpu.counters.oculus"}){std::string source,c;Message(c,1,name);if(std::string(name)=="gpu.renderstages.oculus"){std::string stages;Integer(stages,1,1);Integer(stages,2,0);Message(c,133,stages);}Message(source,1,c);Message(config,2,source);}
    {std::string source,c,ftrace;Message(c,1,"linux.ftrace");Message(ftrace,1,"ftrace/print");Message(ftrace,2,"renderstage");Message(ftrace,2,"surfaceworkload");Message(ftrace,3,"*");Message(c,100,ftrace);Message(source,1,c);Message(config,2,source);}
    Integer(config,3,duration_ms);Integer(config,8,1);Integer(config,9,250);Integer(config,13,1000);return config;
}
struct Clock {uint64_t source=0,mono=0,multiplier=1;};
struct Stage {uint64_t mono_ns=0,duration_ns=0,context=0,surface=0,submission=0;int32_t pid=0;std::string name,queue;};
struct Counter {uint64_t mono_ns=0;uint32_t id=0;double value=0;std::string name;};
struct Counts {uint64_t stages=0,counters=0,unavailable=0,stage_packets=0,timed_stage_packets=0,unaligned_stages=0;};
class Decoder {
    struct Specs {std::vector<std::string> queues,stages;std::map<uint64_t,std::string> intern,names;std::map<uint64_t,int32_t> contexts;std::map<uint32_t,std::string> counters;bool valid=true;uint32_t clock=0;};
    std::map<uint32_t,Specs> sequences;
    std::map<uint32_t,std::vector<Clock>> clocks;
    uint32_t primary=6;
    bool Mono(uint64_t ns,uint32_t clock,uint64_t& result)const {
        if(clock==3){result=ns;return true;}auto f=clocks.find(clock);if(f==clocks.end()||f->second.empty())return false;
        const auto mul=f->second.front().multiplier;if(ns>UINT64_MAX/mul)return false;const auto absolute=ns*mul;
        auto next=std::lower_bound(f->second.begin(),f->second.end(),absolute,[](const Clock& c,uint64_t t){return c.source<t;});
        if(next==f->second.end())next=std::prev(next);else if(next!=f->second.begin()&&absolute-std::prev(next)->source<=next->source-absolute)--next;
        const Clock* best=&*next;const auto delta=absolute>best->source?absolute-best->source:best->source-absolute;
        if(absolute>=best->source){if(delta>UINT64_MAX-best->mono)return false;result=best->mono+delta;}else{if(delta>best->mono)return false;result=best->mono-delta;}return true;
    }
    void Snapshot(std::string_view data){
        struct C{uint32_t id;uint64_t time,mul;bool incremental;};std::vector<C> list;Wire w(data);Field f;
        while(w.Next(f))if(f.id==1&&f.wire==2){auto mul=Number(f.bytes,4,1);auto t=Number(f.bytes,2);if(!mul||t>UINT64_MAX/mul)continue;list.push_back({uint32_t(Number(f.bytes,1)),t*mul,mul,Number(f.bytes,3)!=0});}
        auto mono=std::find_if(list.begin(),list.end(),[](const C& c){return c.id==3&&!c.incremental;});if(mono!=list.end())for(const auto& c:list)if(c.id<64&&!c.incremental)clocks[c.id].push_back({c.time,mono->time,c.mul});
        primary=uint32_t(Number(data,2,primary));
    }
    static void Intern(Specs& s,std::string_view data){Wire w(data);Field f;while(w.Next(f))if(f.wire==2){const auto id=Number(f.bytes,1);if(f.id==24)s.intern[id]=Text(f.bytes,2);else if(f.id==23)s.contexts[id]=int32_t(Number(f.bytes,2));else if(f.id==2)s.names[id]=Text(f.bytes,2);}}
    static void Legacy(Specs& s,std::string_view data){Wire w(data);Field f;std::vector<std::string> queues,stages;while(w.Next(f))if(f.wire==2){if(f.id==1)s.contexts[Number(f.bytes,1)]=int32_t(Number(f.bytes,2));else if(f.id==2)queues.push_back(Text(f.bytes,1));else if(f.id==3)stages.push_back(Text(f.bytes,1));}if(!queues.empty())s.queues=std::move(queues);if(!stages.empty())s.stages=std::move(stages);}
public:
    Counts Read(const std::filesystem::path& file,const std::function<void(const Stage&)>& stageSink={},const std::function<void(const Counter&)>& counterSink={}) {
        clocks.clear();sequences.clear();primary=6;Counts count;
        // Clock snapshots can arrive after the packets whose timestamps they
        // explain. Retain only their small relation tables, never all events.
        Packets(file,[&](std::string_view p){auto snap=Find(p,6);if(snap.id)Snapshot(snap.bytes);if(Find(p,50).id||Find(p,133).id)throw std::runtime_error("Compressed Perfetto packets unsupported; raw trace retained");auto event=Find(p,53);if(event.id){++count.stage_packets;if(Number(event.bytes,2))++count.timed_stage_packets;}});
        for(auto& pair:clocks)std::stable_sort(pair.second.begin(),pair.second.end(),[](const Clock& a,const Clock& b){return a.source<b.source;});
        Packets(file,[&](std::string_view p){const uint32_t seq=uint32_t(Number(p,10));auto& specs=sequences[seq];const auto flags=Number(p,13);if(Number(p,42)){specs={};specs.valid=false;}if((flags&1)||Number(p,41))specs={};if((flags&2)&&!specs.valid){++count.unavailable;return;}auto defaults=Find(p,59);if(defaults.id)specs.clock=uint32_t(Number(defaults.bytes,58));auto intern=Find(p,12);if(intern.id)Intern(specs,intern.bytes);
            const auto ns=Number(p,8);const auto clock=uint32_t(Number(p,58,specs.clock?specs.clock:primary));uint64_t mono=0;const bool aligned=Mono(ns,clock,mono);
            auto event=Find(p,53);if(event.id){auto legacy=Find(event.bytes,7);if(legacy.id)Legacy(specs,legacy.bytes);auto dur=Find(event.bytes,2);
                if(dur.id&&dur.number){if(!aligned){++count.unavailable;++count.unaligned_stages;return;}Stage s;s.mono_ns=mono;s.duration_ns=dur.number;s.context=Number(event.bytes,5);s.surface=Number(event.bytes,8);s.submission=Number(event.bytes,10);
                    auto ctx=specs.contexts.find(s.context);if(ctx!=specs.contexts.end())s.pid=ctx->second;
                    s.name=Text(event.bytes,17);if(s.name.empty()){auto iid=Number(event.bytes,20);auto f=specs.names.find(iid);if(iid&&f!=specs.names.end())s.name=f->second;}
                    if(s.name.empty()){auto iid=Number(event.bytes,14);auto f=specs.intern.find(iid);if(iid&&f!=specs.intern.end())s.name=f->second;else{auto id=Number(event.bytes,4,UINT64_MAX);s.name=id<specs.stages.size()?specs.stages[size_t(id)]:"Stage "+std::to_string(id);}}
                    auto iid=Number(event.bytes,13);auto q=specs.intern.find(iid);if(iid&&q!=specs.intern.end())s.queue=q->second;else{auto id=Number(event.bytes,3,UINT64_MAX);s.queue=id<specs.queues.size()?specs.queues[size_t(id)]:"Queue unknown";}
                    ++count.stages;if(stageSink)stageSink(s);
                }
            }
            auto counters=Find(p,52);if(counters.id){auto desc=Find(counters.bytes,1);if(desc.id){Wire w(desc.bytes);Field f;while(w.Next(f))if(f.id==1&&f.wire==2)specs.counters[uint32_t(Number(f.bytes,1))]=Text(f.bytes,2);}
                Wire w(counters.bytes);Field f;while(w.Next(f))if(f.id==2&&f.wire==2){Counter c;c.id=uint32_t(Number(f.bytes,1));auto value=Find(f.bytes,3);if(value.id&&value.wire==1)std::memcpy(&c.value,&value.number,8);else{value=Find(f.bytes,2);if(!value.id)continue;c.value=double(int64_t(value.number));}if(!aligned||!std::isfinite(c.value)||c.value<0){++count.unavailable;continue;}c.mono_ns=mono;auto name=specs.counters.find(c.id);c.name=name==specs.counters.end()?"Counter "+std::to_string(c.id):name->second;++count.counters;if(counterSink)counterSink(c);}
            }
        });return count;
    }
};
}
