#include "cyui/modules/AudioViews.h"
#include "ModuleInternal.h"
#include <limits>
#include <cstdio>
#include <unordered_set>

namespace cyui {
std::vector<Peak> makePeaks(ArrayView<float> data, std::size_t columns) {
    if (!columns || !data.count)
        return {};
    columns = std::min(columns, data.count);
    std::vector<Peak> out(columns);
    for (std::size_t x = 0; x < columns; ++x) {
        const auto a = x * data.count / columns, b = (x + 1) * data.count / columns;
        bool have = false;
        for (auto i = a; i < b; ++i)
            if (std::isfinite(data[i])) {
                if (!have) {
                    out[x] = {data[i], data[i]};
                    have = true;
                } else {
                    out[x].minimum = std::min(out[x].minimum, data[i]);
                    out[x].maximum = std::max(out[x].maximum, data[i]);
                }
            }
        if (!have)
            out[x] = {std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN()};
    }
    return out;
}
WaveformResult waveform(const char *id, WaveformState &s, const WaveformData &data, double playhead,
                        ImVec2 size) {
    CanvasState canvas;
    CanvasOptions o;
    o.grid = o.navigation = false;
    o.claimWheel = true;
    CanvasScope scope(id, canvas, size, o);
    const auto f = scope.frame();
    const auto room = f.bounds.size();
    auto *d = ImGui::GetWindowDrawList();
    auto &io = ImGui::GetIO();
    WaveformResult result;
    if (!s.view.valid())
        s.view = data.coverage.valid() ? data.coverage : Range{0, 1};
    const double mouse = s.view.min + (io.MousePos.x - f.bounds.min.x) / room.x * s.view.span();
    auto x = [&](double t) { return f.bounds.min.x + float((t - s.view.min) / s.view.span()) * room.x; };
    if (f.wheel != 0) {
        if (io.KeyShift)
            s.view = detail::panView(s.view, -f.wheel * s.view.span() * .1);
        else
            s.view = detail::zoomView(s.view, mouse, std::exp(-f.wheel * .15));
    }
    if (f.hovered && ImGui::IsMouseClicked(2))
        s.panning = true;
    if (!ImGui::IsMouseDown(2))
        s.panning = false;
    if (s.panning)
        s.view = detail::panView(s.view, -io.MouseDelta.x / room.x * s.view.span());
    if (f.hovered && ImGui::IsMouseClicked(0)) {
        s.selection = {mouse, mouse};
        s.anchor = mouse;
        s.selecting = true;
        result.seek = true;
        result.time = mouse;
    }
    if (s.selecting) {
        s.selection = {std::min(s.anchor, mouse), std::max(s.anchor, mouse)};
        result.selectionChanged = true;
        if (!ImGui::IsMouseDown(0))
            s.selecting = false;
    }
    if (f.hovered && ImGui::IsMouseDoubleClicked(0)) {
        s.selection = {0, 0};
        s.selecting = false;
        result.selectionChanged = true;
    }
    if (data.coverage.valid())
        for (std::size_t channel = 0; channel < data.channels.size(); ++channel) {
            const auto peaks = data.channels[channel];
            if (!peaks.count)
                continue;
            const float top = f.bounds.min.y + room.y * float(channel) / data.channels.size(),
                        height = room.y / data.channels.size(), mid = top + height * .5f;
            d->AddLine({f.bounds.min.x, mid}, {f.bounds.max.x, mid}, palette().grid, dp(1));
            const int columns = std::clamp(int(room.x), 1, 8192);
            for (int pixel = 0; pixel < columns; ++pixel) {
                const double t = s.view.min + (double(pixel) + .5) / columns * s.view.span();
                if (!data.coverage.contains(t))
                    continue;
                const auto index = std::min(peaks.count - 1, std::size_t((t - data.coverage.min) /
                                                                         data.coverage.span() * peaks.count));
                const auto p = peaks[index];
                if (std::isfinite(p.minimum) && std::isfinite(p.maximum))
                    d->AddLine({f.bounds.min.x + room.x * pixel / columns,
                                mid - std::clamp(p.maximum, -1.f, 1.f) * height * .45f},
                               {f.bounds.min.x + room.x * pixel / columns,
                                mid - std::clamp(p.minimum, -1.f, 1.f) * height * .45f},
                               palette().info, dp(1));
            }
        }
    if (s.selection.min != s.selection.max)
        d->AddRectFilled({x(std::min(s.selection.min, s.selection.max)), f.bounds.min.y},
                         {x(std::max(s.selection.min, s.selection.max)), f.bounds.max.y},
                         palette().selection);
    d->AddLine({x(playhead), f.bounds.min.y}, {x(playhead), f.bounds.max.y}, palette().accent, dp(2));
    return result;
}
namespace {
bool validEnvelopeKnots(ArrayView<Point> knots, Range time) {
    if (!knots.count) return true;
    if (knots.count < 2 || knots.count > 1026 || knots[0].x != time.min || knots[0].y != 0 ||
        knots[knots.count - 1].x != time.max || knots[knots.count - 1].y != 1) return false;
    for (std::size_t i = 1; i < knots.count; ++i)
        if (!(knots[i - 1].x < knots[i].x && knots[i - 1].y < knots[i].y)) return false;
    return true;
}

double mapEnvelopeTime(double value, ArrayView<Point> knots, double Point::*from, double Point::*to) {
    const auto end = knots.data + knots.count;
    auto next = std::upper_bound(knots.data + 1, end - 1, value,
                                 [from](double v, const Point &p) { return v < p.*from; });
    const auto &a = next[-1], &b = *next;
    return a.*to + (value - a.*from) / (b.*from - a.*from) * (b.*to - a.*to);
}

double signedPower(double v, double exponent) {
    return std::copysign(std::pow(std::abs(v), exponent), v);
}
double envelopeTimeAt(double fraction, Range time, double power, ArrayView<Point> knots) {
    return knots.count ? mapEnvelopeTime(fraction, knots, &Point::y, &Point::x)
                       : time.min + signedPower(fraction, 1 / power) * time.span();
}

void cancelEnvelopeEdit(EnvelopeEditState *state, EnvelopeResult &result) {
    if (state && state->original.id) {
        result.edit = {EditPhase::cancel, state->original};
        *state = {};
    }
}

EnvelopeResult drawEnvelope(const char *id, EnvelopeEditState *state, const EnvelopeData &data,
                            ArrayView<EnvelopeHandle> handles, ImVec2 size, const EnvelopeOptions &o) {
    CanvasState canvas;
    CanvasOptions options;
    options.grid = options.navigation = options.drawBackground = false;
    CanvasScope scope(id, canvas, size, options);
    const auto frame = scope.frame();
    auto *d = ImGui::GetWindowDrawList();
    EnvelopeResult result;
    auto invalid = [&] { cancelEnvelopeEdit(state, result); return result; };
    if (!o.time.valid() || !o.levels.valid() || !std::isfinite(o.time.span()) ||
        !std::isfinite(o.levels.span()) || !std::isfinite(o.baseline) ||
        !std::isfinite(o.timePower) || o.timePower <= 0 || data.points.count < 2 ||
        data.points.count > std::min<std::size_t>(o.maxPoints, 262144) ||
        data.stages.count > std::min<std::size_t>(o.maxStages, 1024) || handles.count > 1024 ||
        !validEnvelopeKnots(data.timeKnots, o.time)) return invalid();
    for (float metric : {o.margins.left, o.margins.top, o.margins.right, o.margins.bottom, o.frameRounding,
                         o.borderWidth, o.curveWidth, o.levelDotRadius, o.levelDotInset})
        if (!std::isfinite(metric) || metric < 0) return invalid();
    std::unordered_set<ItemId> ids;
    for (std::size_t i = 0; i < handles.count; ++i) {
        const auto &h = handles[i];
        if (!h.id || !std::isfinite(h.position.x) || !std::isfinite(h.position.y) ||
            !ids.insert(h.id).second) return invalid();
    }
    double previousEnd = -HUGE_VAL;
    for (std::size_t i = 0; i < data.stages.count; ++i) {
        const auto &time = data.stages[i].time;
        if (!std::isfinite(time.min) || !std::isfinite(time.max) || time.max < time.min ||
            time.min < previousEnd) return invalid();
        previousEnd = time.max;
    }
    const bool labels = o.phaseLabels && data.stages.count;
    const Rect r{{frame.bounds.min.x + dp(o.margins.left), frame.bounds.min.y + dp(o.margins.top) + (labels ? ImGui::GetFontSize() + dp(6) : 0)},
                 {frame.bounds.max.x - dp(o.margins.right), frame.bounds.max.y - dp(o.margins.bottom)}};
    const auto room = r.size();
    if (room.x <= 0 || room.y <= 0) return invalid();
    auto fractionAt = [&](double t) {
        return data.timeKnots.count ? mapEnvelopeTime(t, data.timeKnots, &Point::x, &Point::y)
                                    : signedPower((t - o.time.min) / o.time.span(), o.timePower);
    };
    auto x = [&](double t) { return r.min.x + float(fractionAt(t)) * room.x; };
    auto y = [&](double level) { return r.max.y - float((level - o.levels.min) / o.levels.span()) * room.y; };
    std::vector<ImVec2> points;
    points.reserve(data.points.count);
    double previousTime = -HUGE_VAL;
    for (std::size_t i = 0; i < data.points.count; ++i) {
        const auto &p = data.points[i];
        if (!std::isfinite(p.x) || !std::isfinite(p.y)) {
            points.push_back({NAN, NAN}); previousTime = -HUGE_VAL; continue;
        }
        if (p.x < previousTime) return invalid();
        const ImVec2 at{x(p.x), y(p.y)};
        if (!std::isfinite(at.x) || !std::isfinite(at.y)) return invalid();
        points.push_back(at); previousTime = p.x;
    }
    result.valid = true;
    result.bounds = r;
    const auto mouse = ImGui::GetIO().MousePos;
    result.hovered = frame.hovered && r.contains(mouse);
    result.clicked = result.hovered && ImGui::IsMouseClicked(0);
    result.mouse = {envelopeTimeAt((mouse.x - r.min.x) / room.x, o.time, o.timePower, data.timeKnots),
                    o.levels.min + (r.max.y - mouse.y) / room.y * o.levels.span()};
    const auto color = o.curveColor.value_or(categorical(o.colorIndex));
    d->AddRectFilled(frame.bounds.min, frame.bounds.max, o.backgroundColor.value_or(palette().bg0), dp(o.frameRounding));
    d->PushClipRect(r.min, r.max, true);
    if (o.grid)
        for (int i = 1; i < 4; ++i) {
            const float yy = r.min.y + room.y * i / 4;
            d->AddLine({r.min.x, yy}, {r.max.x, yy}, palette().grid, dp(1));
        }
    if (color & IM_COL32_A_MASK)
        drawCurveFill(points, y(std::clamp(o.baseline, o.levels.min, o.levels.max)), {color, o.fillAlpha, o.gradient});
    double previousBoundary = -HUGE_VAL;
    if (o.phaseLines)
        for (std::size_t i = 0; i < data.stages.count; ++i) {
            const auto &time = data.stages[i].time;
            for (const double boundary : {time.min, time.max}) {
                if (boundary > o.time.min && boundary < o.time.max && boundary != previousBoundary)
                    d->AddLine({x(boundary), r.min.y}, {x(boundary), r.max.y}, palette().grid, dp(1));
                previousBoundary = boundary;
            }
        }
    // Thick antialiased polylines can emit four vertices per point. PrimReserve can
    // start a new draw command between batches, never in the middle of AddPolyline.
    for (std::size_t begin = 0; begin < points.size();) {
        if (!std::isfinite(points[begin].x)) { ++begin; continue; }
        auto end = begin + 1;
        while (end < points.size() && std::isfinite(points[end].x)) ++end;
        if (o.curveWidth > 0) {
            for (auto start = begin; start + 1 < end;) {
                const auto count = std::min<std::size_t>(end - start, 16383);
                d->AddPolyline(points.data() + start, int(count), color, 0, dp(o.curveWidth));
                start += count - 1;
            }
        }
        begin = end;
    }
    const bool playhead = std::isfinite(data.playhead) && o.time.contains(data.playhead);
    const bool level = std::isfinite(data.level) && o.levels.contains(data.level);
    if (playhead) d->AddLine({x(data.playhead), r.min.y}, {x(data.playhead), r.max.y}, palette().accent, dp(1));
    if (level) {
        d->AddLine({r.min.x, y(data.level)}, {r.max.x, y(data.level)}, o.levelLineColor.value_or(alpha(palette().textDim, .5f)), dp(1));
    }
    d->PopClipRect();
    if (level && o.levelDotRadius > 0)
        d->AddCircleFilled({playhead ? x(data.playhead) : std::max(r.min.x, r.max.x - dp(o.levelDotInset)), y(data.level)},
                           dp(o.levelDotRadius), o.levelDotColor.value_or(palette().accent));
    for (std::size_t i = 0; i < data.stages.count; ++i) {
        const auto &stage = data.stages[i];
        if (stage.time.max <= stage.time.min) continue;
        if (result.hovered && result.mouse.x >= stage.time.min &&
            (result.mouse.x < stage.time.max || (i + 1 == data.stages.count && result.mouse.x == stage.time.max)))
            result.stage = i;
        const double first = std::max(o.time.min, stage.time.min), last = std::min(o.time.max, stage.time.max);
        if (labels && last > first && stage.label && stage.label[0]) {
            const float left = x(first), right = x(last), width = ImGui::CalcTextSize(stage.label).x;
            if (width + dp(4) <= right - left)
                d->AddText({(left + right - width) * .5f, frame.bounds.min.y + dp(o.margins.top)}, palette().textDim, stage.label);
        }
    }
    if (o.borderWidth > 0)
        d->AddRect(frame.bounds.min, frame.bounds.max, o.borderColor.value_or(palette().line), dp(o.frameRounding), 0, dp(o.borderWidth));
    if (!state) return result;

    const bool editable = !o.readOnly && !(GImGui->LastItemData.ItemFlags & ImGuiItemFlags_Disabled);
    const EnvelopeHandle *nearest = nullptr, *active = nullptr;
    float closest = dp(14);
    for (std::size_t i = 0; i < handles.count; ++i) {
        const auto &h = handles[i];
        if (h.id == state->original.id) active = &h;
        if (!frame.hovered || !o.time.contains(h.position.x) || !o.levels.contains(h.position.y)) continue;
        const float distance = std::hypot(mouse.x - x(h.position.x), mouse.y - y(h.position.y));
        if (distance < closest) { closest = distance; nearest = &h; }
    }
    const int frameNumber = ImGui::GetFrameCount();
    if (nearest) result.hoveredHandle = nearest->id;
    if (state->original.id) {
        if (!editable || !active || active->axes != state->original.axes || state->revision != o.revision ||
            state->lastFrame != frameNumber - 1 || !frame.focused || ImGui::IsKeyPressed(ImGuiKey_Escape) ||
            (!ImGui::IsItemActive() && !ImGui::IsMouseReleased(0))) {
            cancelEnvelopeEdit(state, result);
        } else {
            auto proposal = state->original;
            const double dx = (mouse.x - state->pointerStart.x) / state->plotSize.x;
            const double dy = (state->pointerStart.y - mouse.y) / state->plotSize.y;
            if (proposal.axes != EnvelopeAxes::level && dx != 0)
                proposal.position.x = envelopeTimeAt(std::clamp(state->startFraction + dx, 0., 1.),
                                                     state->time, state->timePower, state->timeKnots);
            if (proposal.axes != EnvelopeAxes::time && dy != 0)
                proposal.position.y = std::clamp(proposal.position.y + dy * state->levels.span(),
                                                 state->levels.min, state->levels.max);
            const bool release = ImGui::IsMouseReleased(0);
            if (release || proposal.position.x != state->preview.position.x || proposal.position.y != state->preview.position.y)
                result.edit = {release ? EditPhase::commit : EditPhase::update, proposal};
            state->preview = proposal;
            state->lastFrame = frameNumber;
            if (release) *state = {};
        }
    } else if (editable && nearest && ImGui::IsItemActive() && ImGui::IsMouseClicked(0)) {
        state->original = state->preview = *nearest;
        state->pointerStart = mouse;
        state->plotSize = room;
        state->time = o.time;
        state->levels = o.levels;
        state->startFraction = fractionAt(nearest->position.x);
        state->timePower = o.timePower;
        if (data.timeKnots.count)
            state->timeKnots.assign(data.timeKnots.data, data.timeKnots.data + data.timeKnots.count);
        state->revision = o.revision;
        state->lastFrame = frameNumber;
        result.edit = {EditPhase::begin, *nearest};
    }
    const auto &displayed = result.edit.committed() ? result.edit.value : state->preview;
    for (std::size_t i = 0; i < handles.count; ++i) {
        const auto &h = handles[i].id == displayed.id ? displayed : handles[i];
        if (!o.time.contains(h.position.x) || !o.levels.contains(h.position.y)) continue;
        const bool lit = editable && (h.id == state->original.id || h.id == result.hoveredHandle);
        const ImVec2 at{x(h.position.x), y(h.position.y)};
        d->AddCircleFilled(at, dp(lit ? 5.5f : 4.f), lit ? color : palette().bg2);
        d->AddCircle(at, dp(lit ? 5.5f : 4.f), color, 0, dp(1.5f));
    }
    return result;
}
} // namespace

EnvelopeResult envelope(const char *id, const EnvelopeData &data, ImVec2 size, const EnvelopeOptions &o) {
    return drawEnvelope(id, nullptr, data, {}, size, o);
}
EnvelopeResult envelope(const char *id, EnvelopeEditState &state, const EnvelopeData &data,
                        ArrayView<EnvelopeHandle> handles, ImVec2 size, const EnvelopeOptions &o) {
    return drawEnvelope(id, &state, data, handles, size, o);
}
PlotResult spectrum(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size,
                    const PlotOptions &o) {
    s.x.logarithmic = true;
    if (!s.x.range.valid() || s.x.range.min <= 0)
        s.x.range = {20, 20000};
    return plot(id, s, data, size, o);
}
PlotResult spectrum(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size) {
    return spectrum(id, s, data, size, PlotOptions{});
}
PlotResult oscilloscope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size,
                        const PlotOptions &o) {
    s.x.logarithmic = s.y.logarithmic = false;
    return plot(id, s, data, size, o);
}
PlotResult oscilloscope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size) {
    return oscilloscope(id, s, data, size, PlotOptions{});
}
PlotResult stereoScope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size,
                       const PlotOptions &o) {
    // Square, with one symmetric extent on both axes: wheel zoom scales it, the centre stays at 0.
    s.x.logarithmic = s.y.logarithmic = false;
    const auto avail = ImGui::GetContentRegionAvail();
    const float side = std::max(dp(1), std::min(size.x > 0 ? size.x : avail.x + size.x,
                                                 size.y > 0 ? size.y : avail.y + size.y));
    const double extent = s.x.range.valid() ? s.x.range.span() * .5 : 1;
    s.x.range = s.y.range = {-extent, extent};
    auto result = plot(id, s, data, {side, side}, o);
    const double zoomed = s.x.range.valid() ? s.x.range.span() * .5 : extent;
    s.x.range = s.y.range = {-zoomed, zoomed};
    return result;
}
PlotResult stereoScope(const char *id, PlotState &s, ArrayView<PlotSeries> data, ImVec2 size) {
    return stereoScope(id, s, data, size, PlotOptions{});
}
namespace {
std::string audioNumber(double value, const char *unit = "") {
    if (std::isnan(value) || value == HUGE_VAL) return "--";
    if (value == -HUGE_VAL) return std::string("-inf ") + unit;
    char text[80];
    std::snprintf(text, sizeof(text), "%.1f %s", value, unit);
    return text;
}
std::string audioHz(double value) {
    char text[48];
    std::snprintf(text, sizeof(text), value >= 1000 ? "%.2g kHz" : "%.3g Hz", value >= 1000 ? value / 1000 : value);
    return text;
}
void audioLabel(ImDrawList *d, ImVec2 at, const std::string &text, ImU32 color) {
    const auto extent = ImGui::CalcTextSize(text.c_str());
    d->AddRectFilled({at.x - dp(2), at.y}, {at.x + extent.x + dp(2), at.y + extent.y}, alpha(palette().bg0, .9f));
    d->AddText(at, color, text.c_str());
}
float audioFraction(double value, Range range) {
    return range.valid() && !std::isnan(value) ? float(std::clamp((value - range.min) / range.span(), 0., 1.)) : 0;
}
void audioBar(ImDrawList *d, Rect r, double value, Range range, ImU32 color, double marker = missingAudioValue) {
    d->AddRectFilled(r.min, r.max, palette().bg3, dp(2));
    const float x = r.min.x + r.size().x * audioFraction(value, range);
    if (!std::isnan(value)) d->AddRectFilled(r.min, {x, r.max.y}, color, dp(2));
    if (std::isfinite(marker) && range.contains(marker)) {
        const float m = r.min.x + r.size().x * audioFraction(marker, range);
        d->AddLine({m, r.min.y - dp(2)}, {m, r.max.y + dp(2)}, palette().text, dp(1));
    }
}
float audioHeight(ImVec2 size) {
    return std::max(dp(1), size.y > 0 ? size.y : ImGui::GetContentRegionAvail().y + size.y);
}
bool audioReset(bool enabled) {
    return enabled && ImGui::SmallButton(trLabel("Reset measurements###reset").c_str());
}
}

bool SpectrogramHistory::reset(std::size_t capacity, std::size_t bins, double hop, Range frequency,
                               bool logarithmic, double start) {
    if (!capacity || !bins || capacity > 4194304 / bins || !(hop > 0) || !std::isfinite(hop) ||
        !frequency.valid() || (logarithmic && frequency.min <= 0) || !std::isfinite(start)) return false;
    ring_.assign(capacity * bins, std::numeric_limits<float>::quiet_NaN());
    capacity_ = capacity; bins_ = bins; hop_ = hop; frequency_ = frequency; logarithmic_ = logarithmic;
    clear(start); return true;
}
void SpectrogramHistory::clear(double start) {
    start_ = std::isfinite(start) ? start : 0; head_ = count_ = 0; total_ = 0;
    ordered_.clear(); dirty_ = true;
}
bool SpectrogramHistory::append(ArrayView<float> values) {
    if (!capacity_ || values.count != bins_ || total_ == std::numeric_limits<std::uint64_t>::max() ||
        !std::isfinite(start_ + double(total_ + 1) * hop_)) return false;
    std::copy(values.data, values.data + bins_, ring_.begin() + head_ * bins_);
    head_ = (head_ + 1) % capacity_; count_ = std::min(capacity_, count_ + 1); ++total_; dirty_ = true;
    return true;
}
SpectrogramData SpectrogramHistory::view() {
    if (dirty_) {
        ordered_.resize(count_ * bins_);
        const auto first = (head_ + capacity_ - count_) % std::max<std::size_t>(1, capacity_);
        for (std::size_t n = 0; n < count_; ++n)
            std::copy_n(ring_.begin() + ((first + n) % capacity_) * bins_, bins_, ordered_.begin() + n * bins_);
        dirty_ = false;
    }
    return {ordered_, count_, bins_, {start_ + double(total_ - count_) * hop_, start_ + double(total_) * hop_}, frequency_, logarithmic_};
}
SpectrogramResult spectrogram(const char *id, PlotState &s, const SpectrogramData &data, ImVec2 size,
                              const SpectrogramOptions &o) {
    CanvasState canvas; CanvasOptions options;
    options.grid = options.navigation = false; options.claimWheel = o.navigation;
    CanvasScope scope(id, canvas, size, options);
    const auto &f = scope.frame();
    Rect r = f.bounds;
    const bool legend = o.legend && r.size().x > dp(150);
    if (legend) r.max.x -= dp(76);
    const auto room = r.size();
    SpectrogramResult result; result.bounds = r;
    result.valid = data.frames && data.bins && data.frames <= data.values.count / data.bins &&
                   data.time.valid() && data.frequency.valid() &&
                   (!data.logarithmicBins || data.frequency.min > 0) && o.intensity.valid();
    if (!result.valid) return result;
    if (!s.x.range.valid()) s.x.range = data.time;
    if (!s.y.range.valid() || (s.y.logarithmic && s.y.range.min <= 0)) s.y.range = data.frequency;
    s.x.logarithmic = false;
    auto &io = ImGui::GetIO();
    const bool horizontal = o.orientation == SpectrogramOrientation::timeHorizontal;
    const bool hovered = f.hovered && r.contains(io.MousePos);
    auto semantic = [&](double x, double y) {
        double t = horizontal ? x : 1 - y;
        if (o.reverseTime) t = 1 - t;
        return Point{s.x.value(t), s.y.value(horizontal ? 1 - y : x)};
    };
    auto mouse = [&] { return semantic((io.MousePos.x - r.min.x) / room.x, (io.MousePos.y - r.min.y) / room.y); };
    result.mouse = mouse();
    if (o.navigation) {
        if (hovered && ImGui::IsMouseClicked(2)) s.dragging = true;
        if (!ImGui::IsMouseDown(2)) s.dragging = false;
        if (s.dragging) {
            const double shiftTime = (horizontal ? -io.MouseDelta.x / room.x : io.MouseDelta.y / room.y) * (o.reverseTime ? -1 : 1);
            const double shiftFrequency = horizontal ? io.MouseDelta.y / room.y : -io.MouseDelta.x / room.x;
            const bool x = detail::navigateAxis(s.x, shiftTime, result.mouse.x, 1);
            const bool y = detail::navigateAxis(s.y, shiftFrequency, result.mouse.y, 1);
            result.viewChanged = x || y;
        }
        if (hovered && f.wheel != 0) {
            const double factor = std::exp(-f.wheel * .15);
            result.viewChanged = detail::navigateAxis(s.x, 0, result.mouse.x, factor) || result.viewChanged;
            if (io.KeyCtrl) result.viewChanged = detail::navigateAxis(s.y, 0, result.mouse.y, factor) || result.viewChanged;
        }
    } else s.dragging = false;
    result.mouse = mouse();
    const Axis time{data.time, false}, frequency{data.frequency, data.logarithmicBins};
    const Axis frequencyColor{data.frequency, data.frequency.min > 0};
    auto index = [](double fraction, std::size_t count) { return std::min(count - 1, std::size_t(std::clamp(fraction, 0., 1.) * count)); };
    auto *d = ImGui::GetWindowDrawList();
    const auto budget = std::clamp<std::size_t>(o.maxCells, 1, 262144);
    const auto nx = std::max<std::size_t>(1, std::min({horizontal ? data.frames : data.bins, budget, std::size_t(std::min(4096.f, std::ceil(room.x)))}));
    const auto ny = std::max<std::size_t>(1, std::min({horizontal ? data.bins : data.frames, budget / nx, std::size_t(std::min(4096.f, std::ceil(room.y)))}));
    d->PushClipRect(r.min, r.max, true);
    for (std::size_t y = 0; y < ny; ++y) for (std::size_t x = 0; x < nx; ++x) {
        const auto point = semantic((x + .5) / nx, (y + .5) / ny);
        const double fx = time.fraction(point.x), fy = frequency.fraction(point.y);
        if (!std::isfinite(fx) || !std::isfinite(fy) || fx < 0 || fx > 1 || fy < 0 || fy > 1) continue;
        const float value = data.values[index(fx, data.frames) * data.bins + index(fy, data.bins)];
        if (!std::isfinite(value)) continue;
        const float strength = audioFraction(value, o.intensity);
        const auto color = sampleRamp(o.ramp, o.colorByFrequency ? frequencyColor.fraction(point.y) : strength);
        d->AddRectFilled({r.min.x + room.x * float(x) / nx, r.min.y + room.y * float(y) / ny},
                        {r.min.x + room.x * float(x + 1) / nx, r.min.y + room.y * float(y + 1) / ny}, o.colorByFrequency ? alpha(color, strength) : color);
    }
    if (o.axes) {
        for (int tick = 0; tick <= 4; ++tick) {
            const double part = tick / 4.;
            const auto bottom = semantic(part, 1), left = semantic(0, 1 - part);
            const auto xText = horizontal ? audioNumber(bottom.x, "s") : audioHz(bottom.y);
            const auto yText = horizontal ? audioHz(left.y) : audioNumber(left.x, "s");
            const float width = ImGui::CalcTextSize(xText.c_str()).x;
            audioLabel(d, {std::clamp(r.min.x + float(part) * room.x - width / 2, r.min.x + dp(3), std::max(r.min.x + dp(3), r.max.x - width - dp(3))), r.max.y - ImGui::GetFontSize() - dp(3)}, xText, palette().textDim);
            if (tick) audioLabel(d, {r.min.x + dp(4), std::clamp(r.max.y - float(part) * room.y - ImGui::GetFontSize() / 2,
                r.min.y + dp(3), std::max(r.min.y + dp(3), r.max.y - ImGui::GetFontSize() - dp(3)))}, yText, palette().textDim);
        }
    }
    const double fx = time.fraction(result.mouse.x), fy = frequency.fraction(result.mouse.y);
    result.hovered = hovered && std::isfinite(fx) && std::isfinite(fy) && fx >= 0 && fx <= 1 && fy >= 0 && fy <= 1;
    if (result.hovered) {
        result.frame = index(fx, data.frames); result.bin = index(fy, data.bins);
        result.intensity = data.values[result.frame * data.bins + result.bin]; result.clicked = ImGui::IsMouseClicked(0);
        d->AddLine({io.MousePos.x, r.min.y}, {io.MousePos.x, r.max.y}, alpha(palette().text, .4f), dp(1));
        d->AddLine({r.min.x, io.MousePos.y}, {r.max.x, io.MousePos.y}, alpha(palette().text, .4f), dp(1));
        ImGui::SetTooltip("%s | %s | %s", audioNumber(result.mouse.x, "s").c_str(), audioHz(result.mouse.y).c_str(), audioNumber(result.intensity, o.unit ? o.unit : "").c_str());
    }
    d->PopClipRect();
    if (legend) drawColorScale({{r.max.x + dp(8), r.min.y + dp(6)}, {r.max.x + dp(20), r.max.y - dp(6)}}, o.ramp,
        o.colorByFrequency ? data.frequency : o.intensity, o.colorByFrequency ? "Hz" : o.unit, true, o.colorByFrequency && data.frequency.min > 0);
    return result;
}
SpectrogramResult spectrogram(const char *id, SpectrogramState &state, const SpectrogramData &data,
                              ImVec2 size, const SpectrogramOptions &options) {
    if (state.followLatest && data.time.valid()) {
        const double window = std::isfinite(state.window) && state.window > 0 ? state.window : 8;
        state.view.x.range = {std::max(data.time.min, data.time.max - window), data.time.max};
    }
    auto result = spectrogram(id, state.view, data, size, options);
    if (result.viewChanged) state.followLatest = false;
    if (result.hovered && options.navigation && ImGui::IsMouseDoubleClicked(0)) state.followLatest = true;
    return result;
}
PlotResult bandSpectrum(const char *id, PlotState &view, ArrayView<SpectrumBand> bands,
                        ArrayView<PlotSeries> overlays, ImVec2 size, const BandSpectrumOptions &o) {
    view.x.logarithmic = true; view.y.logarithmic = false;
    if (!view.x.range.valid() || view.x.range.min <= 0) view.x.range = {20, 20000};
    std::vector<Point> points; std::vector<Range> widths;
    const bool oversized = bands.count > o.plot.maxPoints;
    if (!oversized) for (std::size_t n = 0; n < bands.count; ++n) {
        const auto &b = bands[n];
        points.push_back({b.frequency.valid() && b.frequency.min > 0 ? std::exp((std::log(b.frequency.min) + std::log(b.frequency.max)) / 2) : missingAudioValue, b.level});
        widths.push_back(b.frequency);
    }
    PlotSeries bars{o.seriesId, o.label, points, PlotStyle::bars};
    bars.baseline = std::isfinite(o.floor) ? o.floor : -90; bars.fillAlpha = .85f;
    bars.gradient = o.gradient; bars.colorByX = o.colorByFrequency; bars.ramp = o.ramp; bars.bands = widths;
    std::vector<PlotSeries> series{bars};
    for (std::size_t n = 0; n < overlays.count; ++n) series.push_back(overlays[n]);
    auto options = o.plot;
    if (!options.formatX) options.formatX = audioHz;
    if (!options.formatY) options.formatY = [](double v) { return audioNumber(v, "dB"); };
    auto result = plot(id, view, series, size, options);
    if (oversized) result.skippedSeries.push_back(o.seriesId);
    return result;
}

void DensityState::clear() {
    energy.clear(); color.clear(); scratch.clear(); columns = rows = 0; projection = -1;
    received = false; time = missingAudioValue;
}
namespace {
void densityPrepare(DensityState &s, const PlotState &view, const PersistenceOptions &o, int projection) {
    const int resolution = std::clamp(o.resolution, 16, 256);
    const bool reset = s.columns != resolution || s.rows != resolution || s.projection != projection ||
        s.x.min != view.x.range.min || s.x.max != view.x.range.max || s.y.min != view.y.range.min || s.y.max != view.y.range.max ||
        s.logX != view.x.logarithmic || s.logY != view.y.logarithmic;
    const double now = ImGui::GetTime();
    if (reset) {
        s.clear(); s.columns = s.rows = resolution; s.projection = projection;
        s.x = view.x.range; s.y = view.y.range; s.logX = view.x.logarithmic; s.logY = view.y.logarithmic;
        s.energy.assign(std::size_t(resolution) * resolution, 0); s.color = s.energy; s.scratch = s.energy;
    }
    const double elapsed = std::isfinite(s.time) ? std::clamp(now - s.time, 0., 60.) : 0;
    s.time = now;
    const float fade = std::isfinite(o.halfLife) && o.halfLife > 0 ? float(std::exp2(-elapsed / o.halfLife)) : 0;
    if (fade == 0) s.received = false;
    for (std::size_t n = 0; n < s.energy.size(); ++n) { s.energy[n] *= fade; s.color[n] *= fade; }
    const float diffusion = std::isfinite(o.diffusion) ? float(std::clamp(elapsed * o.diffusion, 0., .24)) : 0;
    auto diffuse = [&](std::vector<float> &buffer) {
        if (diffusion == 0) return;
        for (int y = 0; y < resolution; ++y) for (int x = 0; x < resolution; ++x) {
            const auto at = std::size_t(y) * resolution + x;
            const float neighbors = buffer[std::size_t(std::max(0, y - 1)) * resolution + x] +
                buffer[std::size_t(std::min(resolution - 1, y + 1)) * resolution + x] +
                buffer[std::size_t(y) * resolution + std::max(0, x - 1)] + buffer[std::size_t(y) * resolution + std::min(resolution - 1, x + 1)];
            s.scratch[at] = buffer[at] * (1 - 4 * diffusion) + diffusion * neighbors;
        }
        buffer.swap(s.scratch);
    };
    diffuse(s.energy); diffuse(s.color);
}
void densityPoint(DensityState &s, const Rect &r, double u, double v, float weight, double color, float radius) {
    if (!std::isfinite(u) || !std::isfinite(v) || !std::isfinite(weight) || weight <= 0 || u < 0 || u > 1 || v < 0 || v > 1) return;
    const double x = u * (s.columns - 1), y = (1 - v) * (s.rows - 1);
    const float pixels = std::isfinite(radius) ? std::clamp(dp(radius), dp(.5f), dp(32)) : dp(4);
    const double rx = std::clamp(double(pixels * s.columns / r.size().x), .6, 6.), ry = std::clamp(double(pixels * s.rows / r.size().y), .6, 6.);
    for (int row = std::max(0, int(std::floor(y - ry * 2))); row <= std::min(s.rows - 1, int(std::ceil(y + ry * 2))); ++row)
        for (int column = std::max(0, int(std::floor(x - rx * 2))); column <= std::min(s.columns - 1, int(std::ceil(x + rx * 2))); ++column) {
            const auto at = std::size_t(row) * s.columns + column;
            const float amount = weight * float(std::exp(-2 * (std::pow((column - x) / rx, 2) + std::pow((row - y) / ry, 2))));
            if (s.energy[at] + amount > 10000) continue;
            s.energy[at] += amount; s.color[at] += amount * float(std::clamp(color, 0., 1.));
        }
}
void densityDraw(const DensityState &s, const Rect &r, const PersistenceOptions &o, const ColorRamp &ramp, bool frequencyColor) {
    auto *d = ImGui::GetWindowDrawList();
    const float exposure = std::isfinite(o.exposure) ? std::clamp(o.exposure, 0.f, 64.f) : .2f;
    for (int y = 0; y < s.rows; ++y) for (int x = 0; x < s.columns; ++x) {
        const auto at = std::size_t(y) * s.columns + x;
        const float intensity = 1 - std::exp(-s.energy[at] * exposure);
        if (intensity < .002f) continue;
        const auto color = sampleRamp(ramp, frequencyColor ? s.color[at] / std::max(1e-12f, s.energy[at]) : intensity);
        d->AddRectFilled({r.min.x + r.size().x * x / s.columns, r.min.y + r.size().y * y / s.rows},
            {r.min.x + r.size().x * (x + 1) / s.columns, r.min.y + r.size().y * (y + 1) / s.rows},
            frequencyColor ? alpha(color, intensity) : color);
    }
}
}
PlotResult stereoDensity(const char *id, StereoDensityState &s, ArrayView<Point> points,
                         std::uint64_t revision, ImVec2 size, const StereoDensityOptions &o) {
    const auto &persistence = o.persistence;
    s.view.x.logarithmic = s.view.y.logarithmic = false;
    const auto available = ImGui::GetContentRegionAvail();
    const float side = std::max(dp(1), std::min(size.x > 0 ? size.x : available.x + size.x, size.y > 0 ? size.y : available.y + size.y));
    const auto point = [&](Point p) { return o.midSide ? Point{(p.x - p.y) * .5, (p.x + p.y) * .5} : p; };
    const bool oversized = points.count > persistence.maxPoints;
    PlotOptions options; options.axes = persistence.axes; options.navigation = persistence.navigation; options.legend = false;
    options.drawBackground = [&](const Rect &r, const PlotState &view) {
        densityPrepare(s.density, view, persistence, o.midSide ? 1 : 0);
        if ((!s.density.received || s.density.revision != revision) && !oversized) {
            for (std::size_t n = 0; n < points.count; ++n) {
                const auto p = point(points[n]); densityPoint(s.density, r, view.x.fraction(p.x), view.y.fraction(p.y), 1, 0, persistence.pointRadius);
            }
            s.density.received = true; s.density.revision = revision;
        }
        densityDraw(s.density, r, persistence, persistence.ramp, false);
        auto *d = ImGui::GetWindowDrawList();
        const ImVec2 a{r.min.x, r.max.y}, b{r.max.x, r.min.y};
        d->AddLine(a, b, alpha(palette().gridStrong, .6f), dp(1));
        d->AddLine(r.min, r.max, alpha(palette().gridStrong, .6f), dp(1));
        audioLabel(d, {r.min.x + dp(6), r.min.y + dp(4)}, o.midSide ? "S / M" : "L / R", palette().textDim);
    };
    auto result = plot(id, s.view, {}, {side, side}, options);
    if (oversized) result.skippedSeries.push_back(1);
    return result;
}
PlotResult spatialScope(const char *id, SpatialScopeState &s, ArrayView<SpatialSample> points,
                        std::uint64_t revision, ImVec2 size, const SpatialScopeOptions &o) {
    const auto &persistence = o.persistence;
    s.view.x.logarithmic = false;
    if (!s.view.y.range.valid() || (s.view.y.logarithmic && s.view.y.range.min <= 0)) s.view.y.range = {20, 20000};
    const bool oversized = points.count > persistence.maxPoints;
    const auto levels = o.levels.valid() ? o.levels : Range{-90, 0};
    const Axis colorAxis{o.colorFrequency.valid() && o.colorFrequency.min > 0 ? o.colorFrequency : Range{20, 20000}, true};
    PlotOptions options; options.axes = persistence.axes; options.navigation = persistence.navigation; options.legend = false;
    options.formatX = [](double v) { return std::abs(v) < .001 ? tr("Center") : audioNumber(std::abs(v), v < 0 ? "L" : "R"); };
    options.formatY = audioHz;
    options.drawBackground = [&](const Rect &r, const PlotState &view) {
        densityPrepare(s.density, view, persistence, o.colorByFrequency ? 3 : 2);
        if ((!s.density.received || s.density.revision != revision) && !oversized) {
            for (std::size_t n = 0; n < points.count; ++n) {
                const auto p = points[n];
                if (!std::isfinite(p.frequency) || p.frequency <= 0 || !std::isfinite(p.position) || std::abs(p.position) > 1 || !std::isfinite(p.level)) continue;
                const float amount = audioFraction(p.level, levels);
                densityPoint(s.density, r, view.x.fraction(p.position), view.y.fraction(p.frequency), amount * amount,
                    colorAxis.fraction(p.frequency), persistence.pointRadius);
            }
            s.density.received = true; s.density.revision = revision;
        }
        densityDraw(s.density, r, persistence, o.colorByFrequency ? o.ramp : persistence.ramp, o.colorByFrequency);
    };
    auto result = plot(id, s.view, {}, size, options);
    if (oversized) result.skippedSeries.push_back(1);
    return result;
}

AudioMeterResult loudness(const char *id, PlotState &view, const LoudnessData &data, ImVec2 size, const LoudnessOptions &o) {
    ImGui::PushID(id); ImGui::BeginGroup();
    const float height = audioHeight(size);
    AudioMeterResult result; result.resetRequested = audioReset(o.resetButton);
    const float header = o.resetButton ? ImGui::GetFrameHeightWithSpacing() : 0;
    const double values[]{data.momentary, data.shortTerm, data.integrated,
        std::isfinite(data.rangeLow) && std::isfinite(data.rangeHigh) && data.rangeHigh >= data.rangeLow ? data.rangeHigh - data.rangeLow : missingAudioValue};
    const char *labels[]{"Momentary", "Short-term", "Integrated", "Loudness range"};
    const Range levels = o.levels.valid() ? o.levels : Range{-60, 0};
    {
        CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
        CanvasScope scope("##readings", canvas, {size.x, dp(66)}, options);
        const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
        for (int n = 0; n < 4; ++n) {
            const float left = r.min.x + r.size().x * n / 4 + dp(6), right = r.min.x + r.size().x * (n + 1) / 4 - dp(6);
            d->PushClipRect({left, r.min.y}, {std::max(left, right), r.max.y}, true);
            audioLabel(d, {left, r.min.y + dp(5)}, tr(labels[n]), palette().textDim);
            audioLabel(d, {left, r.min.y + dp(23)}, audioNumber(values[n], n == 3 ? "LU" : "LUFS"), palette().text);
            const Rect bar{{left, r.min.y + dp(48)}, {std::max(left, right), r.min.y + dp(57)}};
            if (n != 3) audioBar(d, bar, values[n], levels, palette().info, o.target);
            else {
                audioBar(d, bar, missingAudioValue, levels, palette().info);
                if (std::isfinite(values[n])) {
                    const float a = bar.min.x + bar.size().x * audioFraction(data.rangeLow, levels), b = bar.min.x + bar.size().x * audioFraction(data.rangeHigh, levels);
                    d->AddRectFilled({a, bar.min.y}, {b, bar.max.y}, palette().accent);
                }
            }
            d->PopClipRect();
        }
    }
    view.y.range = levels; view.y.logarithmic = view.x.logarithmic = false;
    PlotOptions options; options.formatX = [](double v) { return audioNumber(v, "s"); };
    options.formatY = [](double v) { return audioNumber(v, "LUFS"); };
    plot("##history", view, data.history, {size.x, std::max(dp(40), height - header - dp(66) - ImGui::GetStyle().ItemSpacing.y)}, options);
    ImGui::EndGroup(); ImGui::PopID(); return result;
}

AudioMeterResult truePeak(const char *id, ArrayView<TruePeakChannel> channels, ImVec2 size, const TruePeakOptions &o) {
    ImGui::PushID(id); ImGui::BeginGroup();
    const float height = audioHeight(size);
    AudioMeterResult result; result.resetRequested = audioReset(o.resetButton);
    for (std::size_t n = 0; n < channels.count; ++n)
        if ((!std::isnan(channels[n].level) && channels[n].level > o.ceiling) || channels[n].overs) ++result.clippedChannels;
    const float areaHeight = std::max(dp(24), height - (o.resetButton ? ImGui::GetFrameHeightWithSpacing() : 0));
    if (ImGui::BeginChild("##channels", {size.x, areaHeight})) {
        const auto range = o.levels.valid() ? o.levels : Range{-60, 6};
        const int count = int(std::min<std::size_t>(channels.count, std::numeric_limits<int>::max()));
        const float row = std::max(dp(38), ImGui::GetContentRegionAvail().y / std::max(1, count));
        ScopedVar tight(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0));
        ImGuiListClipper clipper; clipper.Begin(count, row);
        while (clipper.Step()) for (int n = clipper.DisplayStart; n < clipper.DisplayEnd; ++n) {
            ItemScope item(ItemId(n) + 1);
            CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
            CanvasScope scope("##level", canvas, {0, row}, options);
            const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
            const auto &v = channels[std::size_t(n)];
            const auto color = v.level > o.ceiling || v.overs ? palette().error : (v.level > o.warning ? palette().warning : palette().success);
            std::string text = std::string(v.label ? v.label : "") + "  " + audioNumber(v.level, "dBTP") + "   " + tr("Held") + " " + audioNumber(v.held, "dBTP") + "   " + tr("Overs") + " " + std::to_string(v.overs);
            audioLabel(d, {r.min.x + dp(6), r.min.y + dp(3)}, text, color);
            const Rect bar{{r.min.x + dp(6), r.min.y + dp(23)}, {std::max(r.min.x + dp(6), r.max.x - dp(6)), r.min.y + dp(32)}};
            audioBar(d, bar, v.level, range, color, o.ceiling);
            if (std::isfinite(v.held) && range.contains(v.held)) {
                const float x = bar.min.x + bar.size().x * audioFraction(v.held, range);
                d->AddLine({x, bar.min.y - dp(2)}, {x, bar.max.y + dp(2)}, palette().accent, dp(2));
            }
        }
        if (!channels.count) ImGui::TextDisabled("%s", tr("No channels").c_str());
    }
    ImGui::EndChild();
    ImGui::EndGroup(); ImGui::PopID(); return result;
}

PlotResult correlation(const char *id, PlotState &view, double value, ArrayView<PlotSeries> history, ImVec2 size) {
    ImGui::PushID(id); ImGui::BeginGroup(); const float height = audioHeight(size);
    {
        CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
        CanvasScope scope("##gauge", canvas, {size.x, dp(48)}, options);
        const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
        audioLabel(d, {r.min.x + dp(6), r.min.y + dp(3)}, tr("Correlation") + "  " + audioNumber(value), palette().text);
        const Rect bar{{r.min.x + dp(6), r.min.y + dp(26)}, {r.max.x - dp(6), r.min.y + dp(35)}};
        const float centre = (bar.min.x + bar.max.x) / 2;
        d->AddRectFilled(bar.min, {centre, bar.max.y}, alpha(palette().error, .3f));
        d->AddRectFilled({centre, bar.min.y}, bar.max, alpha(palette().success, .3f));
        d->AddLine({centre, bar.min.y}, {centre, bar.max.y}, palette().textDim, dp(1));
        if (std::isfinite(value)) {
            const float x = bar.min.x + bar.size().x * audioFraction(value, {-1, 1});
            d->AddCircleFilled({x, (bar.min.y + bar.max.y) / 2}, dp(5), value < 0 ? palette().error : palette().success);
        }
        audioLabel(d, {bar.min.x, bar.max.y}, "-1", palette().textDim);
        audioLabel(d, {bar.max.x - dp(16), bar.max.y}, "+1", palette().textDim);
    }
    PlotResult result;
    if (history.count) {
        view.y = {{-1, 1}, false}; view.x.logarithmic = false;
        PlotOptions options; options.formatX = [](double v) { return audioNumber(v, "s"); };
        result = plot("##history", view, history, {size.x, std::max(dp(40), height - dp(48) - ImGui::GetStyle().ItemSpacing.y)}, options);
    }
    ImGui::EndGroup(); ImGui::PopID(); return result;
}

ResponseResult filterResponse(const char *id, ResponseState &s, ArrayView<PlotSeries> gain,
    ArrayView<PlotSeries> phase, const std::vector<ResponsePoint> &points, ImVec2 size, const ResponseOptions &o) {
    ResponseResult result;
    const auto frequency = o.frequency.valid() && o.frequency.min > 0 ? o.frequency : Range{20, 20000};
    const auto gains = o.gain.valid() ? o.gain : Range{-24, 24};
    if (!s.view.x.range.valid() || s.view.x.range.min <= 0) s.view.x.range = frequency;
    if (!s.view.y.range.valid()) s.view.y.range = gains;
    s.view.x.logarithmic = true; s.view.y.logarithmic = false;
    auto options = o.plot;
    if (!options.formatX) options.formatX = audioHz;
    if (!options.formatY) options.formatY = [](double v) { return audioNumber(v, "dB"); };
    result.plot = plot(id, s.view, gain, size, options);
    const Rect r{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}; const auto room = r.size();
    auto *d = ImGui::GetWindowDrawList(); d->PushClipRect(r.min, r.max, true);
    const auto screen = [&](double hz, double db) { return ImVec2{r.min.x + float(s.view.x.fraction(hz)) * room.x, r.max.y - float(s.view.y.fraction(db)) * room.y}; };
    std::unordered_set<ItemId> ids;
    bool valid = points.size() <= 1024;
    if (valid) for (const auto &p : points) valid = p.id && std::isfinite(p.frequency) && p.frequency > 0 && std::isfinite(p.gain) && ids.insert(p.id).second && valid;
    bool same = points.size() == s.before.size();
    if (same) for (std::size_t n = 0; n < points.size(); ++n) {
        const auto &a = points[n], &b = s.before[n];
        same = same && a.id == b.id && a.frequency == b.frequency && a.gain == b.gain && a.enabled == b.enabled;
    }
    const bool focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    if (s.dragging && (o.readOnly || !valid || !same || o.revision != s.revision || !focused || ImGui::IsKeyPressed(ImGuiKey_Escape))) {
        result.points = {EditPhase::cancel, s.before}; s.dragging = false; s.active = 0;
    }
    if (o.phase.valid()) {
        const Axis axis{o.phase, false};
        for (std::size_t n = 0; n < phase.count; ++n) {
            const auto &line = phase[n];
            if (line.points.count > options.maxPoints) { result.plot.skippedSeries.push_back(line.id); continue; }
            bool previous = false; ImVec2 last{};
            for (std::size_t i = 0; i < line.points.count; ++i) {
                const auto p = line.points[i]; const double x = s.view.x.fraction(p.x), y = axis.fraction(p.y);
                if (!std::isfinite(x) || !std::isfinite(y)) { previous = false; continue; }
                const ImVec2 at{r.min.x + float(x) * room.x, r.max.y - float(y) * room.y};
                if (previous) d->AddLine(last, at, alpha(categorical(line.colorIndex), .65f), dp(1));
                previous = true; last = at;
            }
        }
        if (options.legend) {
            float x = r.min.x + dp(70);
            const auto phaseLabel = tr("Phase");
            for (std::size_t n = 0; n < phase.count; ++n) {
                const auto &line = phase[n];
                const auto text = phaseLabel + (line.label && line.label[0] && phaseLabel != line.label ? std::string(": ") + line.label : "");
                const float width = ImGui::CalcTextSize(text.c_str()).x + dp(18);
                if (x + width > r.max.x - dp(70)) break;
                audioLabel(d, {x, r.min.y + ImGui::GetFontSize() + dp(7)}, text, categorical(line.colorIndex));
                x += width;
            }
        }
        if (phase.count && options.axes) for (int i = 0; i <= 2; ++i) {
            const auto text = audioNumber(axis.value(i / 2.), "deg");
            audioLabel(d, {r.max.x - ImGui::CalcTextSize(text.c_str()).x - dp(4), std::clamp(r.max.y - room.y * i / 2, r.min.y + dp(3), std::max(r.min.y + dp(3), r.max.y - ImGui::GetFontSize() - dp(3)))}, text, palette().info);
        }
    }
    const auto &visible = s.dragging ? s.preview : points;
    ItemId hit = 0; float closest = dp(10); auto &io = ImGui::GetIO();
    if (valid) for (const auto &p : visible) {
        const auto at = screen(p.frequency, p.gain);
        if (!r.contains(at)) continue;
        d->AddCircleFilled(at, dp(5), p.enabled ? palette().accent : palette().bg5);
        d->AddCircle(at, dp(5), palette().text, 0, dp(1));
        const float distance = std::hypot(at.x - io.MousePos.x, at.y - io.MousePos.y);
        if (distance < closest) { closest = distance; hit = p.id; }
    }
    if (result.points.phase == EditPhase::none && !o.readOnly && !s.dragging && result.plot.hovered && hit && ImGui::IsMouseClicked(0)) {
        s.before = s.preview = points; s.active = hit; s.revision = o.revision; s.dragging = true;
        result.points = {EditPhase::begin, s.preview};
    } else if (s.dragging) {
        if (ImGui::IsMouseDown(0)) {
            for (auto &p : s.preview) if (p.id == s.active) {
                p.frequency = std::clamp(s.view.x.value(std::clamp(double(io.MousePos.x - r.min.x) / room.x, 0., 1.)), frequency.min, frequency.max);
                p.gain = std::clamp(s.view.y.value(std::clamp(1. - double(io.MousePos.y - r.min.y) / room.y, 0., 1.)), gains.min, gains.max);
            }
            result.points = {EditPhase::update, s.preview};
        } else { result.points = {EditPhase::commit, s.preview}; s.dragging = false; s.active = 0; }
    }
    if (hit && result.plot.hovered) for (const auto &p : visible) if (p.id == hit)
        ImGui::SetTooltip("%s | %s", audioHz(p.frequency).c_str(), audioNumber(p.gain, "dB").c_str());
    d->PopClipRect(); return result;
}

PlotResult dynamics(const char *id, PlotState &view, const DynamicsData &data, ImVec2 size, const DynamicsOptions &o) {
    ImGui::PushID(id); ImGui::BeginGroup(); const float height = audioHeight(size);
    {
        CanvasState canvas; CanvasOptions options; options.grid = options.navigation = false;
        CanvasScope scope("##reduction", canvas, {size.x, dp(43)}, options);
        const auto r = scope.frame().bounds; auto *d = ImGui::GetWindowDrawList();
        audioLabel(d, {r.min.x + dp(6), r.min.y + dp(3)}, tr("Gain reduction") + "  " + audioNumber(data.gainReduction, "dB"), palette().text);
        audioBar(d, {{r.min.x + dp(6), r.min.y + dp(26)}, {r.max.x - dp(6), r.min.y + dp(35)}}, data.gainReduction, o.reduction.valid() ? o.reduction : Range{0, 36}, palette().warning);
    }
    view.x.logarithmic = view.y.logarithmic = false;
    auto options = o.plot;
    if (!options.formatX) options.formatX = [](double v) { return audioNumber(v, tr("dB in").c_str()); };
    if (!options.formatY) options.formatY = [](double v) { return audioNumber(v, tr("dB out").c_str()); };
    auto result = plot("##transfer", view, data.transfer, {size.x, std::max(dp(40), height - dp(43) - ImGui::GetStyle().ItemSpacing.y)}, options);
    const Rect r{ImGui::GetItemRectMin(), ImGui::GetItemRectMax()}; const auto room = r.size(); auto *d = ImGui::GetWindowDrawList();
    d->PushClipRect(r.min, r.max, true);
    if (view.x.range.valid() && view.y.range.valid()) {
        const auto at = [&](double x, double y) { return ImVec2{r.min.x + float(view.x.fraction(x)) * room.x, r.max.y - float(view.y.fraction(y)) * room.y}; };
        const auto lo = std::max(view.x.range.min, view.y.range.min), hi = std::min(view.x.range.max, view.y.range.max);
        if (lo <= hi) d->AddLine(at(lo, lo), at(hi, hi), palette().lineStrong, dp(1));
        if (std::isfinite(data.input) && std::isfinite(data.output) && view.x.range.contains(data.input) && view.y.range.contains(data.output))
            d->AddCircleFilled(at(data.input, data.output), dp(5), palette().accent);
    }
    d->PopClipRect(); ImGui::EndGroup(); ImGui::PopID(); return result;
}
} // namespace cyui
