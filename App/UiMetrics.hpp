// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <imgui.h>
#include "UiTheme.hpp"
namespace cyinsights {
inline float UiScale() {
    return std::clamp(ImGui::GetFontSize() / 14.f, .75f, 4.f);
}
inline float NavigationHeight() {
    return ImGui::GetFrameHeight()*2+ImGui::GetStyle().ItemSpacing.y*3+cyui::dp(8);
}
} // namespace cyinsights
