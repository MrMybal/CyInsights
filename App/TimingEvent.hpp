// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <cstdint>
#include <string>
namespace cyinsights {
struct TimingEvent {
    std::string thread, name;
    double start = 0, end = 0;
    int depth = 0;
    bool open_ended = false;
    uint64_t thread_id = 0, timer_id = 0;
    bool has_thread_id = false, has_timer_id = false;
    // UE 5.8 exports frame rows in the timing CSV. Their TimerId column is
    // actually a frame ordinal, in a separate identity namespace.
    int frame_type = -1; // 0 game, 1 rendering; -1 regular timing scope.
    uint64_t frame_index = 0;
    // Rendering aggregates are explicitly derived; no single scope identity.
    uint64_t represented = 1;
    bool display_aggregate = false;
    bool exact_name_match = false; // Aggregate contains a match; never a unique identity.
};
} // namespace cyinsights
