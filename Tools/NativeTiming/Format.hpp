// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
// Local IPC format, little endian / IEEE-754, explicitly versioned. No Epic types.
namespace cynt {
inline constexpr char magic[8]={'C','Y','N','T','I','M','0','1'};
inline constexpr char footer[8]={'C','Y','N','T','E','N','D','1'};
struct Event { double start,end;uint32_t timer,depth,flags,reserved; };
struct Frame { double start,end;uint64_t index;uint32_t type,reserved; };
static_assert(sizeof(Event)==32&&sizeof(Frame)==32);
}
