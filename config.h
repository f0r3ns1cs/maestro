#ifndef CONFIG_H
#define CONFIG_H

#include <windows.h>

#include <optional>
#include <string>
#include <unordered_map>

struct HotkeyBinding {
    UINT modifiers = 0;
    UINT vk = 0;
};

class Config {
public:
    Config();

    [[nodiscard]] const std::wstring& target_process() const noexcept;
    [[nodiscard]] float volume_step() const noexcept;

    [[nodiscard]] std::optional<HotkeyBinding> hotkey(const std::string& action) const;

    [[nodiscard]] bool startup_enabled() const noexcept;
    void set_startup_enabled(bool enabled);

private:
    [[nodiscard]] static std::wstring config_dir();
    [[nodiscard]] static std::wstring config_path();

    void load();
    void write_defaults(const std::wstring& path);

    [[nodiscard]] static std::optional<HotkeyBinding> parse_hotkey(const std::string& str);
    [[nodiscard]] static std::optional<UINT> key_name_to_vk(const std::string& name);

    std::wstring target_process_ = L"Spotify.exe";
    float volume_step_ = 0.02f;
    std::unordered_map<std::string, HotkeyBinding> hotkeys_;
};

#endif // CONFIG_H
