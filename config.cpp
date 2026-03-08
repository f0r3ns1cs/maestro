#include "config.h"

#include <shlobj.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <string_view>

#include <rapidjson/document.h>
#include <rapidjson/filereadstream.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")

namespace {
    constexpr std::wstring_view kAppName     = L"Maestro";
    constexpr std::wstring_view kRegRunPath  = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr std::wstring_view kRegValueName = L"Maestro";

    constexpr std::string_view kDefaultConfig = R"({
    "target_process": "Spotify.exe",
    "volume_step": 2,

    "hotkeys": {
        "toggle_playback": "Ctrl+Shift+F5",
        "volume_up":       "Ctrl+Shift+PageUp",
        "volume_down":     "Ctrl+Shift+PageDown",
        "next_track":      "Ctrl+Shift+Right",
        "previous_track":  "Ctrl+Shift+Left"
    }
}
)";

    [[nodiscard]] std::string to_lower(std::string_view sv) {
        std::string s(sv);
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return s;
    }

    std::wstring utf8_to_wide(const char* str) {
        if (!str || !*str) return {};
        int len = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
        if (len <= 0) return {};
        std::wstring result(static_cast<size_t>(len - 1), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, str, -1, result.data(), len);
        return result;
    }
}

Config::Config() {
    load();
}

const std::wstring& Config::target_process() const noexcept {
    return target_process_;
}

float Config::volume_step() const noexcept {
    return volume_step_;
}

std::optional<HotkeyBinding> Config::hotkey(const std::string& action) const {
    if (auto it = hotkeys_.find(action); it != hotkeys_.end())
        return it->second;
    return std::nullopt;
}

bool Config::startup_enabled() const noexcept {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath.data(), 0, KEY_READ, &key) != ERROR_SUCCESS)
        return false;

    bool exists = (RegQueryValueExW(key, kRegValueName.data(), nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS);
    RegCloseKey(key);
    return exists;
}

void Config::set_startup_enabled(bool enabled) {
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRegRunPath.data(), 0, KEY_SET_VALUE, &key) != ERROR_SUCCESS)
        return;

    if (enabled) {
        std::array<wchar_t, MAX_PATH> buffer{};
        DWORD len = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        RegSetValueExW(key, kRegValueName.data(), 0, REG_SZ,
                       reinterpret_cast<const BYTE*>(buffer.data()),
                       (len + 1) * sizeof(wchar_t));
    }
    else {
        RegDeleteValueW(key, kRegValueName.data());
    }

    RegCloseKey(key);
}

std::wstring Config::config_dir() {
    wchar_t* appdata = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        return {};
    }
    std::wstring dir = appdata;
    CoTaskMemFree(appdata);
    dir += L"\\";
    dir += kAppName;
    return dir;
}

std::wstring Config::config_path() {
    auto dir = config_dir();
    if (dir.empty()) return {};
    return dir + L"\\config.json";
}

void Config::load() {
    auto path = config_path();
    if (path.empty()) return;

    if (!std::filesystem::exists(path)) {
        write_defaults(path);
        return;
    }

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"rb") != 0 || !fp) return;

    std::array<char, 4096> buffer{};
    rapidjson::FileReadStream stream(fp, buffer.data(), buffer.size());

    rapidjson::Document doc;
    doc.ParseStream(stream);
    fclose(fp);

    if (doc.HasParseError() || !doc.IsObject()) return;

    if (doc.HasMember("target_process") && doc["target_process"].IsString())
        target_process_ = utf8_to_wide(doc["target_process"].GetString());

    if (doc.HasMember("volume_step") && doc["volume_step"].IsNumber()) {
        float pct = doc["volume_step"].GetFloat();
        volume_step_ = std::clamp(pct, 0.5f, 50.0f) / 100.0f;
    }

    if (doc.HasMember("hotkeys") && doc["hotkeys"].IsObject()) {
        for (auto it = doc["hotkeys"].MemberBegin(); it != doc["hotkeys"].MemberEnd(); ++it) {
            if (!it->value.IsString()) continue;
            auto binding = parse_hotkey(it->value.GetString());
            if (binding)
                hotkeys_[it->name.GetString()] = *binding;
        }
    }
}

void Config::write_defaults(const std::wstring& path) {
    auto dir = config_dir();
    if (dir.empty()) return;

    std::filesystem::create_directories(dir);

    FILE* fp = nullptr;
    if (_wfopen_s(&fp, path.c_str(), L"wb") != 0 || !fp) return;

    fwrite(kDefaultConfig.data(), 1, kDefaultConfig.size(), fp);
    fclose(fp);

    hotkeys_ = {
        { "toggle_playback", { MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, VK_F5 } },
        { "volume_up",       { MOD_CONTROL | MOD_SHIFT, VK_PRIOR } },
        { "volume_down",     { MOD_CONTROL | MOD_SHIFT, VK_NEXT } },
        { "next_track",      { MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, VK_RIGHT } },
        { "previous_track",  { MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, VK_LEFT } },
    };
}

std::optional<HotkeyBinding> Config::parse_hotkey(const std::string& str) {
    if (str.empty()) return std::nullopt;

    UINT modifiers = 0;
    UINT vk = 0;

    std::string_view remaining(str);

    while (!remaining.empty()) {
        auto pos = remaining.find('+');
        std::string_view token;
        if (pos == std::string_view::npos) {
            token = remaining;
            remaining = {};
        } else {
            token = remaining.substr(0, pos);
            remaining = remaining.substr(pos + 1);
        }

        // remove ws
        auto start = token.find_first_not_of(' ');
        if (start == std::string_view::npos) continue;
        token = token.substr(start, token.find_last_not_of(' ') - start + 1);

        auto lower = to_lower(token);
        if (lower == "ctrl" || lower == "control") {
            modifiers |= MOD_CONTROL;
        } else if (lower == "shift") {
            modifiers |= MOD_SHIFT;
        } else if (lower == "alt") {
            modifiers |= MOD_ALT;
        } else if (lower == "win") {
            modifiers |= MOD_WIN;
        } else {
            auto resolved = key_name_to_vk(lower);
            if (!resolved) return std::nullopt;
            vk = *resolved;
        }
    }

    if (vk == 0) return std::nullopt;

    // ensure media keys dont get repeated
    if (vk == VK_MEDIA_PLAY_PAUSE || vk == VK_MEDIA_NEXT_TRACK ||
        vk == VK_MEDIA_PREV_TRACK || vk == VK_MEDIA_STOP)
        modifiers |= MOD_NOREPEAT;

    return HotkeyBinding{ modifiers, vk };
}

std::optional<UINT> Config::key_name_to_vk(const std::string& name) {
    // a-z, 0-9
    if (name.size() == 1) {
        char ch = name[0];
        if (ch >= 'a' && ch <= 'z') return static_cast<UINT>(ch - 'a' + 'A');
        if (ch >= '0' && ch <= '9') return static_cast<UINT>(ch);
        return std::nullopt;
    }

    // f1-f12
    if (name[0] == 'f' && name.size() <= 3) {
        int num = std::atoi(name.c_str() + 1);
        if (num >= 1 && num <= 12)
            return static_cast<UINT>(VK_F1 + num - 1);
    }

    static const std::unordered_map<std::string, UINT> table = {
        { "space",     VK_SPACE },
        { "enter",     VK_RETURN },
        { "return",    VK_RETURN },
        { "tab",       VK_TAB },
        { "escape",    VK_ESCAPE },
        { "esc",       VK_ESCAPE },
        { "backspace", VK_BACK },
        { "delete",    VK_DELETE },
        { "del",       VK_DELETE },
        { "insert",    VK_INSERT },
        { "ins",       VK_INSERT },
        { "home",      VK_HOME },
        { "end",       VK_END },
        { "pageup",    VK_PRIOR },
        { "pgup",      VK_PRIOR },
        { "pagedown",  VK_NEXT },
        { "pgdn",      VK_NEXT },
        { "left",      VK_LEFT },
        { "right",     VK_RIGHT },
        { "up",        VK_UP },
        { "down",      VK_DOWN },
        { "plus",      VK_OEM_PLUS },
        { "minus",     VK_OEM_MINUS },
        { "comma",     VK_OEM_COMMA },
        { "period",    VK_OEM_PERIOD },
    };

    if (auto it = table.find(name); it != table.end()) {
        return it->second;
    }

    return std::nullopt;
}
