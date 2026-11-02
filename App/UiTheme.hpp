// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <cyui/Core.h>
namespace cyinsights {
void InitializeUi(float dpi);
void AppearanceMenu();
void RenderUiExtras();
void ShowPaletteEditor();
// Host-owned palette transactions. Drafts belong to Inspector until commit.
void CommitUiPalette(const cyui::Palette& palette);
bool UndoUiPalette();
bool RedoUiPalette();
inline ImU32 ScopeColour(const std::string& name) {
    uint32_t hash=2166136261u;for(unsigned char c:name){hash^=c;hash*=16777619u;}
    return cyui::categorical(static_cast<int>(hash%12));
}
// Text over categorical scope fills needs contrast in both light/dark themes.
inline ImU32 ScopeTextColour(ImU32 fill) {
    const auto c=cyui::vec(fill);
    return c.x*.2126f+c.y*.7152f+c.z*.0722f>.5f?
        cyui::darker(cyui::palette().text, .92f):cyui::brighter(cyui::palette().text, .95f);
}
}
