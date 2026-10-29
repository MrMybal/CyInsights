// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include "OvrCapture.hpp"
#include <filesystem>
#include <fstream>
#include <numeric>
#include <regex>
#include <set>
namespace cyovr {
namespace fs = std::filesystem;
inline uint64_t Tick(const cygi::Json &value) {
    const auto &s = value.AsString();
    if (s.empty() || s.size() > 20 || s.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("Invalid monotonic observation");
    size_t n = 0;
    auto v = std::stoull(s, &n);
    if (n != s.size())
        throw std::runtime_error("Invalid observation suffix");
    return v;
}
inline cygi::Json JsonFile(const fs::path &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        throw std::runtime_error("Cannot open " + path.filename().u8string());
    std::string data((std::istreambuf_iterator<char>(in)), {});
    cygi::Json value;
    std::string error;
    if (!cygi::Json::Parse(data, value, &error))
        throw std::runtime_error(error);
    return value;
}
inline bool SamplePath(const std::string &name) {
    static const std::regex pattern("samples/[0-9]{6,12}\\.cyovr");
    return std::regex_match(name, pattern);
}
inline bool ImagePath(const std::string &name) {
    static const std::regex pattern("images/[0-9]{6,12}\\.jpg");
    return std::regex_match(name, pattern);
}
struct Observation {
    std::string path, image;
    uint64_t mono = 0;
    double relative = 0, max_render_us = NAN, image_delta_ms = NAN;
    uint64_t tiles = 0;
    bool black_image = false;
};
struct Recording {
    fs::path directory;
    std::string id, package, boot, error;
    uint64_t origin = 0, anchor = 0;
    double native_seconds = 0;
    bool aligned = false;
    std::vector<Observation> samples;
    double Seconds(size_t i) const {
        auto tick = samples.at(i).mono;
        return aligned ? native_seconds + (tick >= anchor ? double(tick - anchor) : -double(anchor - tick)) * 1e-9
                       : samples.at(i).relative;
    }
};
inline Recording ReadRecording(const fs::path &directory) {
    Recording r;
    r.directory = directory;
    auto info = JsonFile(directory / "info.json");
    if (info["schema"].AsString() != "cyinsights.quest-tiles" || info["version"].AsNumber() != 1)
        throw std::runtime_error("Unsupported Quest tile recording");
    r.id = info["recording_id"].AsString();
    r.package = info["target_package"].AsString();
    r.boot = info["android_boot_id"].AsString();
    r.origin = Tick(info["origin_mono_ns"]);
    r.error = info["error"].AsString();
    std::ifstream index(directory / "index.jsonl");
    if (!index)
        throw std::runtime_error("Quest tile index missing");
    std::string line;
    uint64_t previous = 0, images = 0;
    std::set<std::string> paths;
    if (r.id.empty() || r.package.empty() || r.boot.empty())
        throw std::runtime_error("Missing recording identity");
    while (std::getline(index, line)) {
        cygi::Json j;
        std::string error;
        if (!cygi::Json::Parse(line, j, &error))
            throw std::runtime_error("Invalid tile index: " + error);
        Observation sample;
        sample.path = j["path"].AsString();
        if (!SamplePath(sample.path) || !paths.insert(sample.path).second)
            throw std::runtime_error("Unsafe tile snapshot path");
        sample.mono = Tick(j["observed_mono_ns"]);
        if (sample.mono < r.origin || sample.mono < previous)
            throw std::runtime_error("Non-monotonic tile recording");
        previous = sample.mono;
        sample.relative = double(sample.mono - r.origin) * 1e-9;
        sample.max_render_us = j["max_render_us"].AsNumber(NAN);
        if ((!std::isfinite(sample.max_render_us) && !j["max_render_us"].IsNull()) || sample.max_render_us < 0)
            throw std::runtime_error("Invalid tile cost");
        sample.tiles = j["tile_count"].AsUInt();
        if (j["image"]["status"].AsString() == "measured") {
            sample.image = j["image"]["path"].AsString();
            if (!ImagePath(sample.image) || !paths.insert(sample.image).second)
                throw std::runtime_error("Unsafe shared image path");
            sample.image_delta_ms = j["image"]["delta_ms"].AsNumber(NAN);
            sample.black_image = j["image"]["black_frame"].AsBool(false);
            if (!std::isfinite(sample.image_delta_ms) || std::abs(sample.image_delta_ms) > 1000)
                throw std::runtime_error("Invalid image association");
            const auto &visual = j["image"];
            if (visual["association"].AsString() != "nearest_observation_approximate" ||
                visual["mapping"].AsString() != "composited_view_unregistered")
                throw std::runtime_error("Unknown image association");
            auto observed = Tick(visual["observed_mono_ns"]);
            double delta =
                (observed >= sample.mono ? double(observed - sample.mono) : -double(sample.mono - observed)) * 1e-6;
            if (std::abs(delta - sample.image_delta_ms) > .001)
                throw std::runtime_error("Image offset disagrees with its observation");
            ++images;
        }
        r.samples.push_back(std::move(sample));
    }
    if (info["sample_count"].AsNumber(-1) != double(r.samples.size()) ||
        info["image_count"].AsNumber(-1) != double(images))
        throw std::runtime_error("Incomplete tile recording index");
    return r;
}
inline bool AlignRecording(Recording &r, const fs::path &clock) {
    r.aligned = false;
    try {
        auto j = JsonFile(clock);
        if (j["android_boot_id"].AsString() != r.boot || j["kind"].AsString() != "observation_not_execution")
            return false;
        r.anchor = Tick(j["mono_ns"]);
        r.native_seconds = j["native_seconds"].AsNumber(NAN);
        r.aligned = r.anchor && std::isfinite(r.native_seconds) && r.native_seconds >= 0;
        return r.aligned;
    } catch (...) {
        return false;
    }
}
struct HotTiles {
    size_t measured = 0, missing = 0, hot = 0;
    double median = 0, p95 = 0, maximum = 0, total = 0;
    std::vector<size_t> descending;
};
inline HotTiles AnalyzeTiles(const Surface &s, const std::string &stage, double absolute_us, double median_factor) {
    HotTiles result;
    std::vector<double> costs;
    for (size_t i = 0; i < s.tiles.size(); ++i) {
        double v = s.tiles[i].Cost(stage);
        if (!std::isfinite(v)) {
            ++result.missing;
            continue;
        }
        result.descending.push_back(i);
        costs.push_back(v);
        result.total += v;
    }
    std::sort(costs.begin(), costs.end());
    result.measured = costs.size();
    if (costs.empty())
        return result;
    result.median = costs[costs.size() / 2];
    if (costs.size() % 2 == 0)
        result.median = (result.median + costs[costs.size() / 2 - 1]) * .5;
    result.p95 = costs[std::min(costs.size() - 1, size_t(std::ceil(costs.size() * .95) - 1))];
    result.maximum = costs.back();
    for (double v : costs)
        if (v >= absolute_us || (v > 50 && v > result.median * median_factor))
            ++result.hot;
    std::sort(result.descending.begin(), result.descending.end(),
              [&](size_t a, size_t b) { return s.tiles[a].Cost(stage) > s.tiles[b].Cost(stage); });
    return result;
}
} // namespace cyovr
