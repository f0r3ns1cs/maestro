#ifndef APPLICATION_H
#define APPLICATION_H

#include <windows.h>
#include <functional>
#include <unordered_map>

#include "config.h"
#include "session_volume.h"

class Application {
public:
    Application() noexcept;
    ~Application() noexcept;

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    [[nodiscard]] bool initialize(HINSTANCE instance);

    [[nodiscard]] int run();

private:
    static LRESULT CALLBACK router_proc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handle_msg(HWND, UINT, WPARAM, LPARAM);

    [[nodiscard]] bool create_msg_wnd(HINSTANCE instance);
    [[nodiscard]] bool setup_tray_icon();
    void tear_down_tray_icon() noexcept;
    void show_context_menu() noexcept;

    void register_hotkeys();
    void unregister_hotkeys() noexcept;
    void dispatch_hotkey(int id);
    void ensure_session() noexcept;

    static void send_key_press(WORD vk) noexcept;

    HWND                    hwnd_ = nullptr;
    HINSTANCE               instance_ = nullptr;
    UINT                    tray_msg_ = 0;
    NOTIFYICONDATAW         nid_ = {};
    Config                  config_;
    SessionVolume           volume_;

    struct Binding
    {
        UINT modifiers;
        UINT vk;
        std::function<void()> action;
    };
    std::unordered_map<int, Binding> hotkeys_;
};

#endif // APPLICATION_H
