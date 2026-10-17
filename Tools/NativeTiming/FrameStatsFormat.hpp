// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
// Little-endian v1 analysis cache. Original data stays inside .utrace and the
// optional raw Meta .pftrace source in .cycapture. This cache is rebuildable.
namespace cyfs {
inline constexpr char magic[8]={'C','Y','F','S','0','0','0','1'};
enum Record : uint8_t {Spec=1,Value=2,End=255};
enum Flags : uint32_t {FrameBounds=1,Delayed=2,ExecutionUnknown=4,Aggregate=8,Valid=16};
struct Sample {
    uint32_t metric,flags;
    double begin,end,observed;
    uint64_t game_frame;
    int64_t stats_frame,integer;
    double real;
};
static_assert(sizeof(Sample)==64,"Fixed frame-stat sample layout");
}
