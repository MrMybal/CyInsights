// Copyright (C) 2026 Cyberalien. AGPL-3.0-or-later.
#pragma once
#include <CyGPUInspectorCore/Json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace cyinsights {
namespace fs = std::filesystem;
// Container v1: CYCAP001, u32 entry count, then records:
// u32 UTF-8 path length, u64 byte length, 32-byte SHA256, path, bytes. Little endian.
struct Entry { std::string name; uint64_t size = 0, offset = 0; std::string sha256; };
struct Capture { cygi::Json manifest; std::vector<Entry> entries; fs::path file; };
bool Pack(const fs::path& directory, const fs::path& output, std::string& error);
bool Inspect(const fs::path& file, Capture& out, std::string& error);
bool Extract(const Capture& capture, const fs::path& fresh_directory, std::string& error);
bool ValidateManifest(const cygi::Json& manifest, std::string& error);
// Affine clock mapping anchored with integer ticks, avoiding loss above 2^53.
struct ClockAnchor { uint64_t ticks; double session_seconds, frequency, uncertainty_seconds; };
bool MapClock(uint64_t ticks, const ClockAnchor& anchor, double& seconds);
}
