#include "cyui/modules/Media.h"
#include <cstdio>
namespace cyui {
void mediaPlayer(const char *id, MediaPlayerState &s, MediaBackend &backend, ImVec2 size) {
    ScopedID scope{id};
    const auto data = backend.snapshot();
    if (size.y <= 0)
        size.y = dp(240);
    if (size.x <= 0)
        size.x = ImGui::GetContentRegionAvail().x;
    ImGui::BeginChild("##player", size);
    if (!data.title.empty())
        heading(data.title.c_str(), 3);
    if (!data.error.empty())
        callout(Tone::error, data.error.c_str());
    if (data.frame.texture && data.frame.width > 0 && data.frame.height > 0) {
        const auto available = ImGui::GetContentRegionAvail();
        float w = std::max(dp(1), available.x), h = w * data.frame.height / data.frame.width;
        const float limit = std::max(dp(1), available.y - dp(90));
        if (h > limit) {
            w *= limit / h;
            h = limit;
        }
        ImGui::Image(data.frame.texture, {w, h}, data.frame.uvMin, data.frame.uvMax);
    }
    ImGui::BeginDisabled(!data.ready);
    if (ghostButton(trLabel(data.playing ? "Pause" : "Play").c_str()))
        backend.play(!data.playing);
    ImGui::SameLine();
    if (ghostButton(trLabel(data.muted ? "Unmute" : "Mute").c_str()))
        backend.mute(!data.muted);
    const double duration = std::isfinite(data.duration) ? std::clamp(data.duration, 0., 315360000.) : 0;
    const double position = std::isfinite(data.position) ? std::clamp(data.position, 0., duration) : 0;
    if (!s.seeking)
        s.seek = float(position);
    ImGui::BeginDisabled(!data.seekable || duration <= 0);
    ImGui::SetNextItemWidth(-dp(100));
    ImGui::SliderFloat(trLabel("Position###seek").c_str(), &s.seek, 0, float(duration), "%.1f s");
    s.seeking = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit())
        backend.seek(std::clamp(double(s.seek), 0., duration));
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Text("%.1f / %.1f s", position, duration);
    float volume = std::isfinite(data.volume) ? std::clamp(data.volume, 0.f, 1.f) : 1.f;
    ImGui::SetNextItemWidth(dp(180));
    if (ImGui::SliderFloat(trLabel("Volume").c_str(), &volume, 0, 1, "%.2f"))
        backend.volume(volume);
    ImGui::EndDisabled();
    ImGui::EndChild();
}
} // namespace cyui
