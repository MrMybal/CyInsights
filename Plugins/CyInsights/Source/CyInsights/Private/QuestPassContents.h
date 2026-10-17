// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace cyinsights_quest {
enum ContentFlags:uint32_t {PartialSecondary=1,IndirectExecutionUnknown=2};
struct PassWork {
    uint64_t direct=0,indexed=0,indirect=0,indirect_commands=0,vertices=0,indices=0,instances=0;
    uint32_t flags=0;
    void Draw(uint32_t count,uint32_t instanceCount,bool index){
        if(index){++indexed;indices+=uint64_t(count)*instanceCount;}else{++direct;vertices+=uint64_t(count)*instanceCount;}
        instances+=instanceCount;
    }
    void Indirect(uint32_t commands){++indirect;indirect_commands+=commands;flags|=IndirectExecutionUnknown;}
    void Merge(const PassWork& other){direct+=other.direct;indexed+=other.indexed;indirect+=other.indirect;indirect_commands+=other.indirect_commands;vertices+=other.vertices;indices+=other.indices;instances+=other.instances;flags|=other.flags;}
};
}
