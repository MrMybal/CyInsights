---
id: updates
title: Updates, preferences and notices
description: Stable releases, signing continuity and preserved data.
section: Guides
order: 20
parent: reference
revision: fab8f5e9936a
---

Use Help > Updates on Windows or Updates on Quest. Automatic checks run at most daily and can be disabled. Only newer stable releases are offered: drafts, alpha, beta and release candidates are excluded. Download alpha test packages manually.

Windows verifies/prepares the complete ZIP before installation. The folder must be writable and other instances must release executables. The updater backs up replaced files and attempts rollback on failure. Open capture paths can be reopened after restart; ordinary updates do not deliberately erase preferences/recordings.

The portable edition keeps update transactions and preferences in its adjacent Data/ folder. Updating from the compatibility Windows ZIP preserves its portable marker and existing Data files. The installed edition uses per-user storage; rerunning Setup also refreshes Windows installation metadata and its uninstaller file list.

Quest keeps Standard/Vulkan variant, verifies identity/version/arm64 libraries, SHA-256 and signing continuity before Android installation. Android confirmation/install permission can be required. A different signing certificate blocks an in-place update. Keep your development key; a data-losing uninstall is not a normal update step. A layer update may require a manual game reopen.

Appearance stores theme, density and UI language. This manual has its own English/French choice and works offline. LICENSE and third-party notices, including CyUI MIT and CyDoc MIT notices, remain in the packages. These notices describe included components; they are not a legal assessment of a different redistribution.

CyInsights and CyInsightOVR use AGPL-3.0-or-later. The CyInsights Unreal plugin uses MIT. CyUI and CyDoc use MIT; their full notices and the other component licenses accompany the application.

Network failures, rate limits and missing stable packages are reported. Signing/build checks are not an antivirus verdict. See [Troubleshooting](doc:troubleshooting).
