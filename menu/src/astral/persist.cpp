// Astral-Remake - made by kdt255 (https://github.com/kdt255/Astral-Remake)
// Free software: MIT + Commons Clause, selling is not permitted. If you paid for this, you got scammed.
#include "astral/persist.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace astral::persist {

namespace fs = std::filesystem;

namespace {

constexpr std::uint32_t kMagic = 0x52545341u; // "ASTR"
constexpr std::uint32_t kVersion = 2u;
constexpr const char* kExportPrefix = "ASTRAL1:";

struct Header {
    std::uint32_t magic;
    std::uint32_t version;
    std::uint32_t size;
    std::uint32_t checksum;
};

[[nodiscard]] std::uint32_t Fnv1a(const unsigned char* data, size_t size) noexcept
{
    std::uint32_t h = 2166136261u;
    for (size_t i = 0; i < size; ++i) {
        h ^= data[i];
        h *= 16777619u;
    }
    return h;
}

[[nodiscard]] fs::path ExeDirectory()
{
    std::wstring buffer(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (n == 0) return fs::current_path();
        if (n < buffer.size()) {
            buffer.resize(n);
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return fs::path(buffer).parent_path();
}

[[nodiscard]] fs::path Folder(bool scripts)
{
    fs::path dir = ExeDirectory() / (scripts ? L"scripts" : L"configs");
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

[[nodiscard]] fs::path ConfigPath(const std::string& name) { return Folder(false) / fs::u8path(name + ".astral"); }

[[nodiscard]] FileEntry Describe(const fs::path& path)
{
    FileEntry e;
    e.name = path.stem().u8string();
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        e.size = (static_cast<std::uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        e.stamp = (static_cast<std::uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
        FILETIME local{};
        SYSTEMTIME st{};
        if (FileTimeToLocalFileTime(&data.ftLastWriteTime, &local) && FileTimeToSystemTime(&local, &st)) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%02u.%02u.%04u %02u:%02u", st.wDay, st.wMonth, st.wYear, st.wHour, st.wMinute);
            e.modified = buf;
        }
    }
    return e;
}

[[nodiscard]] std::vector<FileEntry> List(bool scripts, const wchar_t* ext)
{
    std::vector<FileEntry> out;
    std::error_code ec;
    for (fs::directory_iterator it(Folder(scripts), ec), end; !ec && it != end; it.increment(ec)) {
        if (!it->is_regular_file(ec) || _wcsicmp(it->path().extension().c_str(), ext) != 0) continue;
        FileEntry e = Describe(it->path());
        if (IsValidName(e.name)) out.push_back(std::move(e));
    }
    return out;
}

[[nodiscard]] std::vector<unsigned char> Encode(const Config& cfg)
{
    Header h{ kMagic, kVersion, static_cast<std::uint32_t>(sizeof(Config)), 0u };
    h.checksum = Fnv1a(reinterpret_cast<const unsigned char*>(&cfg), sizeof(Config));
    std::vector<unsigned char> blob(sizeof(Header) + sizeof(Config));
    std::memcpy(blob.data(), &h, sizeof(Header));
    std::memcpy(blob.data() + sizeof(Header), &cfg, sizeof(Config));
    return blob;
}

bool Decode(Config& cfg, const unsigned char* data, size_t size, std::string& error)
{
    if (size < sizeof(Header)) {
        error = "File is too small";
        return false;
    }
    Header h{};
    std::memcpy(&h, data, sizeof(Header));
    if (h.magic != kMagic) {
        error = "Not an Astral config";
        return false;
    }
    if (h.version != kVersion || h.size != sizeof(Config) || size != sizeof(Header) + sizeof(Config)) {
        error = "Config was made by another build";
        return false;
    }
    if (Fnv1a(data + sizeof(Header), sizeof(Config)) != h.checksum) {
        error = "Checksum mismatch";
        return false;
    }
    // Security warning: the blob is copied verbatim; the checksum only detects corruption, not tampering, so every
    // value that can index an array is clamped afterwards. Only import configs from sources you trust.
    Config loaded;
    std::memcpy(&loaded, data + sizeof(Header), sizeof(Config));
    Sanitize(loaded);
    cfg = loaded;
    return true;
}

constexpr char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

[[nodiscard]] std::string Base64(const std::vector<unsigned char>& in)
{
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (size_t i = 0; i < in.size(); i += 3) {
        const std::uint32_t n = (static_cast<std::uint32_t>(in[i]) << 16) |
                                (i + 1 < in.size() ? static_cast<std::uint32_t>(in[i + 1]) << 8 : 0u) |
                                (i + 2 < in.size() ? static_cast<std::uint32_t>(in[i + 2]) : 0u);
        out += kB64[(n >> 18) & 63];
        out += kB64[(n >> 12) & 63];
        out += i + 1 < in.size() ? kB64[(n >> 6) & 63] : '=';
        out += i + 2 < in.size() ? kB64[n & 63] : '=';
    }
    return out;
}

[[nodiscard]] bool Unbase64(const char* text, std::vector<unsigned char>& out)
{
    std::array<int, 256> map{};
    map.fill(-1);
    for (int i = 0; i < 64; ++i) map[static_cast<unsigned char>(kB64[i])] = i;
    std::uint32_t acc = 0;
    int bits = 0;
    for (const char* p = text; *p != '\0'; ++p) {
        const unsigned char ch = static_cast<unsigned char>(*p);
        if (ch == '=' || std::isspace(ch)) continue;
        if (map[ch] < 0) return false;
        acc = (acc << 6) | static_cast<std::uint32_t>(map[ch]);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((acc >> bits) & 0xFFu));
        }
        if (out.size() > sizeof(Header) + sizeof(Config)) return false;
    }
    return true;
}

template <typename T>
void ClampIndex(T& v, int count)
{
    v = static_cast<T>(std::clamp(static_cast<int>(v), 0, count - 1));
}

} // namespace

bool IsValidName(const std::string& name)
{
    if (name.empty() || name.size() > 32) return false;
    bool has_visible = false;
    for (const char ch : name) {
        const unsigned char u = static_cast<unsigned char>(ch);
        if (!(std::isalnum(u) || ch == ' ' || ch == '_' || ch == '-')) return false;
        if (ch != ' ') has_visible = true;
    }
    if (!has_visible || name.front() == ' ' || name.back() == ' ') return false;
    std::string upper(name);
    for (char& ch : upper) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
    static const char* const kReserved[] = { "CON", "PRN", "AUX", "NUL", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6",
                                             "COM7", "COM8", "COM9", "LPT1", "LPT2", "LPT3", "LPT4", "LPT5", "LPT6",
                                             "LPT7", "LPT8", "LPT9" };
    for (const char* r : kReserved)
        if (upper == r) return false;
    return true;
}

std::vector<FileEntry> ListConfigs(SortOrder order)
{
    std::vector<FileEntry> out = List(false, L".astral");
    if (order == SortOrder::Modified)
        std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) { return a.stamp > b.stamp; });
    else
        std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) { return _stricmp(a.name.c_str(), b.name.c_str()) < 0; });
    return out;
}

bool SaveConfig(const Config& cfg, const std::string& name, std::string& error)
{
    if (!IsValidName(name)) {
        error = "Invalid name";
        return false;
    }
    const fs::path target = ConfigPath(name);
    const fs::path temp = fs::path(target).concat(L".tmp");
    const std::vector<unsigned char> blob = Encode(cfg);
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out.write(reinterpret_cast<const char*>(blob.data()), static_cast<std::streamsize>(blob.size()))) {
            error = "Write failed";
            return false;
        }
    }
    if (!MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        std::error_code ec;
        fs::remove(temp, ec);
        error = "Replace failed";
        return false;
    }
    return true;
}

bool LoadConfig(Config& cfg, const std::string& name, std::string& error)
{
    if (!IsValidName(name)) {
        error = "Invalid name";
        return false;
    }
    std::ifstream in(ConfigPath(name), std::ios::binary);
    if (!in) {
        error = "File not found";
        return false;
    }
    std::vector<unsigned char> data(sizeof(Header) + sizeof(Config) + 1);
    in.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return Decode(cfg, data.data(), static_cast<size_t>(in.gcount()), error);
}

bool DeleteConfig(const std::string& name, std::string& error)
{
    if (!IsValidName(name)) {
        error = "Invalid name";
        return false;
    }
    std::error_code ec;
    if (!fs::remove(ConfigPath(name), ec)) {
        error = ec ? "Delete failed" : "File not found";
        return false;
    }
    return true;
}

std::string ExportConfig(const Config& cfg) { return std::string(kExportPrefix) + Base64(Encode(cfg)); }

bool ImportConfig(Config& cfg, const char* text, std::string& error)
{
    if (text == nullptr) {
        error = "Clipboard is empty";
        return false;
    }
    while (std::isspace(static_cast<unsigned char>(*text))) ++text;
    const size_t prefix = std::strlen(kExportPrefix);
    if (std::strncmp(text, kExportPrefix, prefix) != 0) {
        error = "Clipboard has no Astral config";
        return false;
    }
    std::vector<unsigned char> blob;
    if (!Unbase64(text + prefix, blob)) {
        error = "Malformed config text";
        return false;
    }
    return Decode(cfg, blob.data(), blob.size(), error);
}

void Sanitize(Config& cfg)
{
    ClampIndex(cfg.legit.group, kGroupCount);
    ClampIndex(cfg.rage.category, kGroupCount + 1);
    ClampIndex(cfg.visuals.target, 4);
    ClampIndex(cfg.skins.group, kGroupCount);
    for (int g = 0; g < kGroupCount; ++g) {
        ClampIndex(cfg.legit.weapon[g], 8);
        ClampIndex(cfg.rage.weapon[g], 8);
        ClampIndex(cfg.skins.weapon[g], 8);
        for (SkinEntry& e : cfg.skins.entries[g]) e.name_tag[sizeof(e.name_tag) - 1] = '\0';
    }
    ClampIndex(cfg.misc.page, 3);
    ClampIndex(cfg.misc.accent, static_cast<int>(AccentPreset::Count));
    ClampIndex(cfg.misc.dpi_mode, 6);
}

std::vector<FileEntry> ListScripts()
{
    std::vector<FileEntry> out = List(true, L".lua");
    std::sort(out.begin(), out.end(), [](const FileEntry& a, const FileEntry& b) { return _stricmp(a.name.c_str(), b.name.c_str()) < 0; });
    return out;
}

std::vector<std::string> ReadScriptPreview(const std::string& name, int max_lines)
{
    std::vector<std::string> lines;
    if (!IsValidName(name)) return lines;
    std::ifstream in(Folder(true) / fs::u8path(name + ".lua"));
    std::string line;
    while (static_cast<int>(lines.size()) < max_lines && std::getline(in, line)) {
        for (char& ch : line)
            if (ch == '\t') ch = ' ';
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.size() > 120) line.resize(120);
        lines.push_back(std::move(line));
    }
    return lines;
}

std::vector<std::string> LoadAutoload()
{
    std::vector<std::string> names;
    std::ifstream in(Folder(true) / L"autoload.txt");
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (IsValidName(line)) names.push_back(line);
    }
    return names;
}

bool SaveAutoload(const std::vector<std::string>& names, std::string& error)
{
    std::ofstream out(Folder(true) / L"autoload.txt", std::ios::trunc);
    for (const std::string& n : names)
        if (IsValidName(n)) out << n << '\n';
    if (!out) {
        error = "Write failed";
        return false;
    }
    return true;
}

void OpenFolder(bool scripts)
{
    const fs::path dir = Folder(scripts);
    ShellExecuteW(nullptr, L"open", dir.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

} // namespace astral::persist
