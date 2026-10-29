// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "../Core/OvrRecording.hpp"
#include <chrono>
#include <iostream>
using cygi::Json;
int main() {
    auto dir = std::filesystem::temp_directory_path() /
               ("cyovr-recording-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        std::filesystem::create_directory(dir);
        auto check = [](bool value, const char *text) {
            if (!value)
                throw std::runtime_error(text);
        };
        auto reject = [&](auto action) {
            try {
                action();
            } catch (const std::exception &) {
                return;
            }
            throw std::runtime_error("Invalid input accepted");
        };
        auto write = [&](const char *name, const Json &j) { std::ofstream(dir / name) << j.Write(); };
        Json info;
        Json::Parse(
            R"({"schema":"cyinsights.quest-tiles","version":1,"recording_id":"fixture","target_package":"org.fixture.game","android_boot_id":"boot-A","origin_mono_ns":"9007199254740993","sample_count":1,"image_count":1})",
            info);
        Json row;
        Json::Parse(
            R"({"path":"samples/000001.cyovr","observed_mono_ns":"9007199255740993","max_render_us":800,"tile_count":10,"image":{"status":"measured","path":"images/000001.jpg","observed_mono_ns":"9007199245740993","delta_ms":-10,"association":"nearest_observation_approximate","mapping":"composited_view_unregistered"}})",
            row);
        row["image"]["black_frame"] = true;
        write("info.json", info);
        write("index.jsonl", row);
        auto r = cyovr::ReadRecording(dir);
        check(r.samples.size() == 1 && r.origin == 9007199254740993ull, "Nanosecond precision lost");
        check(r.samples.front().black_image,"A blank image must retain its visibility diagnostic");
        check(std::abs(r.Seconds(0) - .001) < 1e-12, "Relative time is not from observations");
        auto clock = Json::Object();
        clock["android_boot_id"] = "boot-A";
        clock["mono_ns"] = "9007199254740993";
        clock["native_seconds"] = 42.;
        clock["kind"] = "observation_not_execution";
        write("clock.json", clock);
        check(cyovr::AlignRecording(r, dir / "clock.json") && std::abs(r.Seconds(0) - 42.001) < 1e-9,
              "Native anchor alignment failed");
        clock["android_boot_id"] = "boot-B";
        write("clock.json", clock);
        check(!cyovr::AlignRecording(r, dir / "clock.json") && !r.aligned, "Headset reboot was ignored");
        clock["android_boot_id"] = "boot-A";
        clock["kind"] = "unverified";
        write("clock.json", clock);
        check(!cyovr::AlignRecording(r, dir / "clock.json"), "Unverified clock accepted");
        auto invalid = row;
        invalid["path"] = "../other.cyovr";
        write("index.jsonl", invalid);
        reject([&] { cyovr::ReadRecording(dir); });
        invalid = row;
        invalid["image"]["path"] = "images/../../secret.jpg";
        write("index.jsonl", invalid);
        reject([&] { cyovr::ReadRecording(dir); });
        invalid = row;
        invalid["image"]["delta_ms"] = 1001.;
        write("index.jsonl", invalid);
        reject([&] { cyovr::ReadRecording(dir); });
        invalid = row;
        invalid["image"]["delta_ms"] = 1.;
        write("index.jsonl", invalid);
        reject([&] { cyovr::ReadRecording(dir); });
        invalid = row;
        invalid["observed_mono_ns"] = "1";
        write("index.jsonl", invalid);
        reject([&] { cyovr::ReadRecording(dir); });
        invalid = row;
        invalid["image"]["association"] = "exact_gpu_frame";
        write("index.jsonl", invalid);
        reject([&] { cyovr::ReadRecording(dir); });
        write("index.jsonl", row);
        {
            std::ofstream out(dir / "index.jsonl", std::ios::app);
            out << '\n' << row.Write();
        }
        reject([&] { cyovr::ReadRecording(dir); });
        write("index.jsonl", row);
        info["sample_count"] = 2;
        write("info.json", info);
        reject([&] { cyovr::ReadRecording(dir); });
        info["sample_count"] = 1;
        write("info.json", info);
        invalid = row;
        invalid["max_render_us"] = Json();
        write("index.jsonl", invalid);
        check(std::isnan(cyovr::ReadRecording(dir).samples[0].max_render_us), "Missing Render was converted to zero");
        reject([&] { cyovr::Tick(Json("-1")); });
        reject([&] { cyovr::Tick(Json("18446744073709551616")); });
        cyovr::Surface s;
        for (double cost : {0., 10., 20., 30., 1000.}) {
            cyovr::Tile t;
            t.stages_us["Render"] = cost;
            s.tiles.push_back(t);
        }
        s.tiles.push_back(cyovr::Tile{});
        auto a = cyovr::AnalyzeTiles(s, "Render", 500, 2.5);
        check(a.measured == 5 && a.missing == 1 && a.median == 20 && a.p95 == 1000 && a.hot == 1 &&
                  a.descending[0] == 4,
              "Hot tile analysis failed");
        s.tiles.resize(4);
        a = cyovr::AnalyzeTiles(s, "Render", 500, 2.5);
        check(a.median == 15 && a.hot == 0, "Even median or threshold failed");
        s.tiles.clear();
        a = cyovr::AnalyzeTiles(s, "Render", 500, 2.5);
        check(a.measured == 0 && a.hot == 0, "Empty surface invented costs");
        std::filesystem::remove_all(dir);
        std::cout << "20 Quest recording path, clock, precision, image provenance and hot-tile checks passed "
                     "(synthetic fixtures)\n";
        return 0;
    } catch (const std::exception &e) {
        std::filesystem::remove_all(dir);
        std::cerr << e.what() << '\n';
        return 1;
    }
}
