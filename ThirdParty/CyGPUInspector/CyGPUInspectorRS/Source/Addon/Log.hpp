// CyGPUInspectorRS — logging through ReShade's log file.
//
// Copyright (C) 2026 Cyberalien. SPDX-License-Identifier: MIT
#pragma once

namespace cygi::log
{
	void SetVerbose(bool verbose);
	bool IsVerbose();

	void Info(const char *format, ...);
	void Warning(const char *format, ...);
	void Error(const char *format, ...);
	// Only written when verbose logging is enabled in ReShade.ini.
	void Debug(const char *format, ...);
}
