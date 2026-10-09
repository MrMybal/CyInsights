// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#include "NativeTrace.hpp"
#include "TimingModel.hpp"
#include "AnalysisWorkers.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <thread>
int main(int argc, char **argv) {
    if (argc == 2) {
        // Read-only validation of an existing native export; no analyzer or UI
        // process is launched by this diagnostic mode.
        auto begin = std::chrono::steady_clock::now();
        std::vector<cyinsights::TimingEvent> events;
        std::string error;
        if (!cyinsights::NativeTrace::ReadCsv(std::filesystem::u8path(argv[1]), events, error)) {
            std::cerr << error;
            return 1;
        }
        auto read = std::chrono::steady_clock::now();
        cyinsights::TimingModel model;
        model.Build(events);
        auto built = std::chrono::steady_clock::now();
        if (!model.frames.empty()) {
            size_t middle = model.frames.size() / 2;
            double a = model.frames[middle].first,
                   b = model.frames[std::min(middle + 2, model.frames.size() - 1)].second;
            model.Aggregate(events, a, b);
            size_t visible = 0;
            for (const auto &track : model.tracks) {
                std::vector<size_t> indices;
                track.Query(events, a, b, 24, indices);
                visible += indices.size();
            }
            auto selected = std::chrono::steady_clock::now();
            std::cout << "Visible scopes across three tick candidates: " << visible
                      << "\nInterval calculation ms: "
                      << std::chrono::duration<double, std::milli>(selected - built).count() << "\n";
        }
        std::cout << "Events: " << events.size() << " / tracks: " << model.tracks.size()
                  << " / timers: " << model.names.size() << " / tick candidates: " << model.frames.size()
                  << "\nRead ms: " << std::chrono::duration<double, std::milli>(read - begin).count()
                  << " / index ms: " << std::chrono::duration<double, std::milli>(built - read).count()
                  << "\n";
        size_t represented = 0, drawn = 0;
        for (const auto &track : model.tracks) {
            std::vector<size_t> indices;
            std::vector<cyinsights::TimingGlyph> glyphs;
            track.Query(events, model.start, model.end, track.RowCount(0), indices);
            cyinsights::BuildTimingGlyphs(events, indices, model.start, model.end, 1600, -1, glyphs);
            drawn += glyphs.size();
            for (const auto &glyph : glyphs)
                represented += glyph.count;
        }
        std::cout << "Full continuous overview at 1600px: " << drawn << " glyphs representing " << represented
                  << " source scopes\n";
        if (represented != events.size()) {
            std::cerr << "Overview lost source scopes\n";
            return 1;
        }
        return 0;
    }
    auto file = std::filesystem::temp_directory_path() /
                ("CyNativeTraceTest_" + std::to_string(GetCurrentProcessId()) + ".csv");
    std::ofstream(file) << "ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth\n"
                           "1,GameThread,1,\"Name, with comma\",1,2,1,0\n"
                           "2,Worker,2,Unfinished,1.5,inf,inf,1\n";
    std::vector<cyinsights::TimingEvent> events;
    std::string error;
    int failures = 0, checks = 0;
    auto check = [&](bool value, const char *name) {
        ++checks;
        if (!value) {
            ++failures;
            std::cerr << name << "\n";
        }
    };
    if (!cyinsights::NativeTrace::ReadCsv(file, events, error) || events.size() != 2) {
        std::cerr << error;
        return 1;
    }
    check(events[0].name == "Name, with comma", "CSV quoting");
    check(events[1].open_ended && events[1].end == 2, "Unfinished scopes");
    check(events[0].has_thread_id && events[0].thread_id == 1 && events[1].has_timer_id &&
              events[1].timer_id == 2,
          "Native identities must survive import");
    std::ofstream(file)<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth\n"
        "16777216,Game Frames,7,Game Frame 4294967303,1,2,1,1\n"
        "16777217,Rendering Frames,7,Rendering Frame 7,1,2,1,1\n"
        "1,Worker,7,Game Frame 7,1,2,1,0\n";
    check(cyinsights::NativeTrace::ReadCsv(file,events,error)&&events[0].frame_type==0&&events[0].frame_index==4294967303ull&&!events[0].has_timer_id,
          "Frame rows retain full index from label without claiming TimerId identity");
    check(events[1].frame_type==1&&events[1].frame_index==7&&!events[1].has_timer_id&&events[2].frame_type==-1&&events[2].has_timer_id,
          "Frame recognition requires reserved track identity and keeps ordinary same-name scopes");
    std::ofstream(file)<<"ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Duration,Depth\n"
        "16777216,Game Frames,9,Game Frame 7,1,2,1,1\n";
    check(!cyinsights::NativeTrace::ReadCsv(file,events,error),"Disagreeing frame export identity is rejected");
    std::ofstream(file) << "ThreadName,TimerName,StartTime,EndTime,Depth\nGameThread,Legacy,1,2,0\n";
    check(cyinsights::NativeTrace::ReadCsv(file, events, error) && !events[0].has_thread_id &&
              !events[0].has_timer_id,
          "Missing IDs must stay unavailable");
    std::ofstream(file) << "ThreadName,TimerName,StartTime,EndTime,Depth\nGameThread,Invalid,2,1,0\n";
    check(!cyinsights::NativeTrace::ReadCsv(file, events, error), "Inverted interval rejected");
    for (const char *id : {"-1", "1bad", "18446744073709551616"}) {
        std::ofstream(file) << "ThreadId,ThreadName,TimerName,StartTime,EndTime,Depth\n"
                            << id << ",Worker,Invalid,1,2,0\n";
        check(!cyinsights::NativeTrace::ReadCsv(file, events, error), "Malformed native identity rejected");
    }
    check(cyinsights::NativeTrace::Quote(L"folder path\\") == L"\"folder path\\\\\"",
          "Process argument quoting");
    std::ofstream(file)
        << "ThreadId,ThreadName,TimerId,TimerName,StartTime,EndTime,Depth\n1,Worker,2,Async,1,2,0\n";
    cyinsights::NativeTrace importer;
    std::string status;
    check(importer.ImportCsv(file, error) && importer.Busy(), "Background import must expose a busy state");
    check(!importer.ImportCsv(file, error), "Concurrent imports must be rejected");
    auto poll = [&] {
        auto limit = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (importer.Busy() && std::chrono::steady_clock::now() < limit) {
            importer.Poll(status);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        return !importer.Busy();
    };
    check(poll() && importer.events.size() == 1 && importer.events[0].name == "Async",
          "Background import must publish a completed result");
    auto absent = file;
    absent += L".missing";
    check(importer.ImportCsv(absent, error) && poll() && status.find("no timing CSV") != std::string::npos,
          "Background failures must return an error and clear busy state");
    {
        std::ofstream fixture(file);fixture<<"TimerName,Depth,EndTime,ThreadName,StartTime,TimerId,ThreadId\n";
        for(int i=0;i<12000;++i)fixture<<"\"Shader \"\"quoted\"\", name\",0,"<<i+1<<",Worker,"<<i<<",7,"<<i%4+1<<'\n';
    }
    std::vector<cyinsights::TimingEvent> serial,parallel;cyinsights::NativeReadProgress progress;
    cyinsights::SetAnalysisWorkerCount(1);bool first=cyinsights::NativeTrace::ReadCsv(file,serial,error,&progress);
    cyinsights::SetAnalysisWorkerCount(8);bool second=cyinsights::NativeTrace::ReadCsv(file,parallel,error,&progress);
    check(first&&second&&parallel.size()==12000,"Parallel CSV batches retain all rows across batch boundaries");
    bool same=serial.size()==parallel.size();
    if(same)for(size_t i=0;i<serial.size();++i){const auto& a=serial[i];const auto& b=parallel[i];
        same&=a.name==b.name&&a.thread==b.thread&&a.start==b.start&&a.end==b.end&&a.thread_id==b.thread_id&&a.timer_id==b.timer_id&&a.depth==b.depth;}
    check(same&&parallel.front().name=="Shader \"quoted\", name"&&parallel.back().start==11999,"Parallel CSV decoding preserves exact source order, identities and quoted fields");
    check(progress.bytes.load()==progress.total.load()&&progress.events.load()==12000,"Batched import publishes exact final byte/event progress");
    {
        std::ofstream fixture(file);fixture<<"ThreadName,TimerName,StartTime,EndTime,Depth\n";
        for(int i=0;i<9000;++i)fixture<<"Worker,Scope,"<<i<<','<<(i==4500?"bad":std::to_string(i+1))<<",0\n";
    }
    auto kept=parallel.front().name;check(!cyinsights::NativeTrace::ReadCsv(file,parallel,error)&&parallel.size()==12000&&parallel.front().name==kept,"Worker parse errors do not publish partially decoded captures");
    cyinsights::SetAnalysisWorkerCount(0);
    std::filesystem::remove(file);
    std::cout << checks << " checks, " << failures << " failures\n";
    return failures ? 1 : 0;
}
