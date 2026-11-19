#include "cyui/modules/Timeline.h"
#include <unordered_set>
#include <stdexcept>
namespace cyui {
namespace {
TimelineOperation operation(ArrayView<TimelineClip> clips, ArrayView<ItemId> selection, double value,
                            const std::function<ItemId()> &allocate, bool split, double minimum) {
    TimelineOperation result;
    try {
        if (!allocate || !std::isfinite(value) || !std::isfinite(minimum) || minimum <= 0)
            throw std::invalid_argument("Invalid timeline operation");
        std::unordered_set<ItemId> ids, chosen;
        for (std::size_t i = 0; i < selection.count; ++i)
            chosen.insert(selection[i]);
        for (std::size_t i = 0; i < clips.count; ++i) {
            const auto &c = clips[i];
            if (!ids.insert(c.id).second || !std::isfinite(c.start) || !std::isfinite(c.duration) ||
                c.duration <= 0 || !std::isfinite(c.start + c.duration))
                throw std::invalid_argument("Invalid clips or duplicate IDs");
        }
        for (std::size_t i = 0; i < clips.count; ++i) {
            auto c = clips[i];
            const bool active = chosen.count(c.id) && !c.locked;
            if (split && active && value - c.start >= minimum && c.start + c.duration - value >= minimum) {
                auto copy = c;
                copy.id = allocate();
                if (!copy.id || !ids.insert(copy.id).second)
                    throw std::invalid_argument("Allocator returned duplicate or zero ID");
                copy.start = value;
                copy.duration = c.start + c.duration - value;
                c.duration = value - c.start;
                result.clips.push_back(c);
                result.clips.push_back(copy);
                result.changed = true;
            } else {
                result.clips.push_back(c);
                if (!split && active) {
                    auto copy = c;
                    copy.id = allocate();
                    if (!copy.id || !ids.insert(copy.id).second)
                        throw std::invalid_argument("Allocator returned duplicate or zero ID");
                    copy.start += value;
                    if (!std::isfinite(copy.start) || !std::isfinite(copy.start + copy.duration))
                        throw std::invalid_argument("Clip position overflow");
                    result.clips.push_back(copy);
                    result.changed = true;
                }
            }
        }
        return result;
    } catch (const std::exception &e) {
        return {{}, e.what(), false};
    }
}
} // namespace
TimelineOperation splitTimelineClips(ArrayView<TimelineClip> clips, ArrayView<ItemId> selected, double time,
                                     const std::function<ItemId()> &allocate, double minimum) {
    return operation(clips, selected, time, allocate, true, minimum);
}
TimelineOperation duplicateTimelineClips(ArrayView<TimelineClip> clips, ArrayView<ItemId> selected,
                                         double offset, const std::function<ItemId()> &allocate) {
    return operation(clips, selected, offset, allocate, false, 1e-6);
}
std::vector<TimelineLane> reorderTimelineLane(ArrayView<TimelineLane> lanes, ItemId id,
                                              std::optional<ItemId> before) {
    std::vector<TimelineLane> result;
    const TimelineLane *moved = nullptr;
    bool target = false;
    for (std::size_t i = 0; i < lanes.count; ++i) {
        if (lanes[i].id == id)
            moved = &lanes[i];
        if (before && lanes[i].id == *before)
            target = true;
    }
    if (!moved || (before && id == *before) || (!target && before)) {
        for (std::size_t i = 0; i < lanes.count; ++i)
            result.push_back(lanes[i]);
        return result;
    }
    for (std::size_t i = 0; i < lanes.count; ++i) {
        if (lanes[i].id == id)
            continue;
        if (before && lanes[i].id == *before)
            result.push_back(*moved);
        result.push_back(lanes[i]);
    }
    if (!before)
        result.push_back(*moved);
    return result;
}
} // namespace cyui
