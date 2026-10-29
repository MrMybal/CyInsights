// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <limits>
#include <map>
#include <set>
#include <vector>

namespace cydraw {
using Handle=uintptr_t;
struct Counts {
    uint64_t draws=0,triangles=0,indirect=0,unknownTriangles=0,unknownDraws=0,missingBuffers=0;
    bool overflow=false;
    void Add(uint64_t& to,uint64_t value){if(value>UINT64_MAX-to){to=UINT64_MAX;overflow=true;}else to+=value;}
    Counts& operator+=(const Counts& other){
        Add(draws,other.draws);Add(triangles,other.triangles);Add(indirect,other.indirect);
        Add(unknownTriangles,other.unknownTriangles);Add(unknownDraws,other.unknownDraws);Add(missingBuffers,other.missingBuffers);
        overflow|=other.overflow;return *this;
    }
};
// Numeric topology values are the Vulkan enum values. These are input primitives,
// before clipping, culling, tessellation, geometry shaders and multiview expansion.
inline Counts Direct(int topology,bool restart,bool indexed,uint32_t vertices,uint32_t instances){
    Counts out;out.draws=1;if(!vertices||!instances)return out;
    if(topology<0||topology==10||(indexed&&restart)){out.unknownTriangles=1;return out;}
    uint64_t count=0;
    switch(topology){
    case 3:count=vertices/3;break;
    case 4:case 5:count=vertices>2?vertices-2:0;break;
    case 8:count=vertices/6;break;
    case 9:count=vertices>4?(vertices-4)/2:0;break;
    default:if(topology>10)out.unknownTriangles=1;break;
    }
    out.triangles=count*uint64_t(instances);return out;
}
inline Counts Indirect(uint32_t count,bool gpuCount=false){
    Counts out;out.indirect=1;
    if(gpuCount){out.unknownDraws=1;out.unknownTriangles=1;}
    else{out.draws=count;out.unknownTriangles=count;}
    return out;
}
struct Recording {Counts counts;std::vector<Handle> secondary;};
// Recording a command never increments submitted totals. Resolve again on every
// successful submission so reused primary and secondary buffers are counted.
class Book {
    std::map<Handle,Recording> recordings;
    Counts Resolve(Handle handle,std::set<Handle>& stack)const{
        auto found=recordings.find(handle);
        if(found==recordings.end()||stack.size()>=16||!stack.insert(handle).second){Counts out;out.missingBuffers=1;return out;}
        Counts out=found->second.counts;
        for(auto child:found->second.secondary)out+=Resolve(child,stack);
        stack.erase(handle);return out;
    }
public:
    void Begin(Handle handle){recordings[handle]={};}
    void Erase(Handle handle){recordings.erase(handle);}
    void Draw(Handle handle,const Counts& counts){recordings[handle].counts+=counts;}
    void Execute(Handle handle,Handle child){recordings[handle].secondary.push_back(child);}
    Counts Submit(const std::vector<Handle>& handles)const{
        Counts out;std::set<Handle> stack;for(auto handle:handles)out+=Resolve(handle,stack);return out;
    }
};
}
