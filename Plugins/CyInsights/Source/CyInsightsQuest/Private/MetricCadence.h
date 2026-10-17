// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <cmath>

namespace cyinsights::quest {
// This schedules observations, never assigns a runtime value to a GPU frame.
class MetricCadence {
public:
    void Reset() { observed=false; last_frame=0; last_seconds=0; }
    bool Due(uint64_t frame,double seconds,double hertz) {
        if(observed&&frame==last_frame)return false;
        if(!std::isfinite(hertz)||hertz<0)hertz=0;
        if(observed&&hertz>0&&seconds>=last_seconds&&seconds-last_seconds<1.0/hertz)return false;
        observed=true;last_frame=frame;last_seconds=seconds;return true;
    }
private:
    bool observed=false;
    uint64_t last_frame=0;
    double last_seconds=0;
};
}
