// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <algorithm>
#include <imgui.h>
namespace cyinsights {
inline float UiScale() {
    return std::clamp(ImGui::GetFontSize() / 14.f, .75f, 4.f);
}
inline float NavigationHeight() {
    return 70.f * UiScale();
}
} // namespace cyinsights
