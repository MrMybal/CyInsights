#pragma once
#include <imgui_internal.h>

namespace cyui::detail {
// Ratios, independent of display density and global interface font size.
constexpr float codeTextScale(float value) {
    return value != value ? 1.f : value < .5f ? .5f : value > 3.f ? 3.f : value;
}

// Construct inside the source child, after choosing the code font. Pop before
// EndChild. Only that hovered child owns Ctrl+wheel; toolbar fonts stay unchanged.
class CodeTextScope final {
  public:
    explicit CodeTextScope(float &scale, bool wheelZoom = true) {
        scale = codeTextScale(scale);
        auto *window = ImGui::GetCurrentWindow();
        auto *storage = ImGui::GetStateStorage();
        const auto key = ImGui::GetID("##cyui-code-text-scale");
        const float previous = codeTextScale(storage->GetFloat(key, scale));
        const auto &io = ImGui::GetIO();
        if (wheelZoom && ImGui::IsWindowHovered() && io.KeyCtrl && !io.KeyAlt && !io.KeyShift &&
            ImGui::TestKeyOwner(ImGuiKey_MouseWheelY, window->ID)) {
            ImGui::SetKeyOwner(ImGuiKey_MouseWheelY, window->ID);
            scale = codeTextScale(scale + io.MouseWheel * .1f);
        }
        ImGui::PushFont(nullptr, GImGui->FontSizeBase * scale);
        if (scale != previous) {
            const float ratio = scale / previous;
            ImGui::SetScrollY(ImGui::GetScrollY() * ratio);
            ImGui::SetScrollX(ImGui::GetScrollX() * ratio);
        }
        storage->SetFloat(key, scale);
    }
    ~CodeTextScope() { ImGui::PopFont(); }
    CodeTextScope(const CodeTextScope &) = delete;
    CodeTextScope &operator=(const CodeTextScope &) = delete;
};
} // namespace cyui::detail
