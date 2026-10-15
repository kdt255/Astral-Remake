// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#pragma once
// Config / script file management next to the executable (configs\*.astral, scripts\*.lua).
#include "astral/config.h"

#include <cstdint>
#include <string>
#include <vector>

namespace astral::persist {

struct FileEntry {
    std::string name;      // without extension
    std::string modified;  // local time, "dd.mm.yyyy hh:mm"
    std::uint64_t stamp = 0;
    std::uint64_t size = 0;
};

enum class SortOrder { Modified, Name };

// Security warning: names are user input and become file names; IsValidName() rejects separators, dots-only names,
// reserved device names and anything outside [A-Za-z0-9 _-], so no path can escape the managed folders.
[[nodiscard]] bool IsValidName(const std::string& name);

[[nodiscard]] std::vector<FileEntry> ListConfigs(SortOrder order);
bool SaveConfig(const Config& cfg, const std::string& name, std::string& error);
bool LoadConfig(Config& cfg, const std::string& name, std::string& error);
bool DeleteConfig(const std::string& name, std::string& error);

[[nodiscard]] std::string ExportConfig(const Config& cfg);
bool ImportConfig(Config& cfg, const char* text, std::string& error);

// Clamps every value that is used as an array index, so a tampered blob cannot index out of bounds.
void Sanitize(Config& cfg);

[[nodiscard]] std::vector<FileEntry> ListScripts();
[[nodiscard]] std::vector<std::string> ReadScriptPreview(const std::string& name, int max_lines);
[[nodiscard]] std::vector<std::string> LoadAutoload();
bool SaveAutoload(const std::vector<std::string>& names, std::string& error);
void OpenFolder(bool scripts);

} // namespace astral::persist
