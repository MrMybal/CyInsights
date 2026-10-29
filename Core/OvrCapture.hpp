// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once
#include <CyGPUInspectorCore/Json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace cyovr {
// CLI stage durations are measured; capture receipt is not an execution clock.
// Never assign these snapshots to an Unreal frame or eye from dimensions alone.
struct Tile {
    unsigned id=0, x=0, y=0, width=0, height=0;
    std::map<std::string,double> stages_us;
    double Cost(const std::string& stage) const {
        auto i=stages_us.find(stage);return i==stages_us.end()?NAN:i->second;
    }
};
struct Surface {
    std::string id, process;
    unsigned width=0,height=0,tile_width=0,tile_height=0,msaa=0;
    double duration_us=NAN;
    std::vector<Tile> tiles;
};
struct Capture {
    std::string id,package,boot_id,observed_mono_ns,status,reason;
    std::vector<Surface> surfaces;
};
inline int LargestSurface(const Capture& capture) {
    int result=0;uint64_t area=0;
    for(size_t i=0;i<capture.surfaces.size();++i){const auto& s=capture.surfaces[i];uint64_t next=uint64_t(s.width)*s.height;
        if(next>area){area=next;result=int(i);}}
    return result;
}
inline Capture ParseCapture(const cygi::Json& j) {
    using J=cygi::Json;
    if(!j.IsObject()||j["schema"].AsString()!="cyinsightovr.capture"||
        j["version"].AsNumber()!=1||j["source"].AsString()!="meta.ovrgpuprofiler")
        throw std::runtime_error("Unsupported CyInsightOVR capture schema");
    Capture c;c.id=j["capture_id"].AsString();c.package=j["target_package"].AsString();
    c.boot_id=j["android_boot_id"].AsString();c.observed_mono_ns=j["observed_mono_ns"].AsString();
    c.status=j["status"].AsString();c.reason=j["reason"].AsString();
    if(c.id.empty()||c.package.empty()||c.boot_id.empty()||c.observed_mono_ns.empty())
        throw std::runtime_error("Missing OVR capture identity or observation metadata");
    if(j["time_basis"].AsString()!="capture_relative_unanchored")
        throw std::runtime_error("Unrecognized OVR execution-clock declaration");
    auto dimension=[](const J& v){double n=v.AsNumber(NAN);if(!std::isfinite(n)||n<0||n>65536||std::floor(n)!=n)throw std::runtime_error("Invalid surface geometry");return unsigned(n);};
    auto duration=[](const J& v){double n=v.AsNumber(NAN);if(!std::isfinite(n)||n<0)throw std::runtime_error("Invalid measured stage duration");return n;};
    if(!j["surfaces"].IsArray())throw std::runtime_error("Missing OVR surface array");
    for(const auto& r:j["surfaces"].Items()) {
        Surface s;s.id=r["id"].AsString();s.process=r["process"].AsString();
        s.width=dimension(r["width"]);s.height=dimension(r["height"]);
        s.tile_width=dimension(r["tile_width"]);s.tile_height=dimension(r["tile_height"]);s.msaa=dimension(r["msaa"]);
        s.duration_us=duration(r["duration_us"]);
        if(!s.width||!s.height||!r["tiles"].IsArray())throw std::runtime_error("Missing measured surface geometry");
        if(s.process!=c.package)throw std::runtime_error("OVR surface belongs to another target");
        for(const auto& q:r["tiles"].Items()) {
            Tile t;t.id=dimension(q["id"]);t.x=dimension(q["x"]);t.y=dimension(q["y"]);
            t.width=dimension(q["width"]);t.height=dimension(q["height"]);
            if(!t.width||!t.height||t.x>=s.width||t.y>=s.height||t.width>s.width-t.x||t.height>s.height-t.y)
                throw std::runtime_error("OVR tile lies outside its surface");
            if(!q["stages_us"].IsObject())throw std::runtime_error("Missing tile stages");
            for(const auto& kv:q["stages_us"].Members())t.stages_us[kv.first]=duration(kv.second);
            s.tiles.push_back(std::move(t));
        }
        c.surfaces.push_back(std::move(s));
    }
    if(c.status!="measured"&&c.status!="unavailable")throw std::runtime_error("Invalid OVR availability");
    if(c.status=="measured") {
        if(c.surfaces.empty())throw std::runtime_error("Measured OVR capture has no surfaces");
        for(const auto& s:c.surfaces)if(s.tiles.empty())throw std::runtime_error("Measured OVR surface has no tile geometry");
    }
    return c;
}
}
