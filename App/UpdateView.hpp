// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <filesystem>
#include <vector>
namespace cyinsights {
void UpdateMenu();
void ShowUpdates();
void SuppressAutomaticUpdateCheck();
bool UpdatesVisible();
void DrawUpdates(bool can_install,const std::vector<std::filesystem::path>& reopen);
bool UpdateExitRequested();
}
