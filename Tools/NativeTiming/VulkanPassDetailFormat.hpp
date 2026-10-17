// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
namespace cyvpd {
inline constexpr char magic[8]={'C','Y','V','P','0','0','0','1'};
inline constexpr char footer[8]={'C','Y','V','P','D','O','N','E'};
struct Record {
    double begin,end;
    uint64_t pass,submission;
    uint32_t ordinal,width,height,subpass,subpasses,flags;
    uint64_t direct,indexed,indirect,indirect_commands,vertices,indices,instances;
};
static_assert(sizeof(Record)==112,"Fixed little-endian Vulkan pass detail record");
struct Raw {uint64_t begin,end;Record detail;};
}
