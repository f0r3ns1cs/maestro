#include "application.h"

#include <shellapi.h>
#include <strsafe.h>

namespace {
    namespace hotkey_id {
        constexpr int kTogglePlayback = 0x10;
        constexpr int kRaiseVolume    = 0x11;
        constexpr int kLowerVolume    = 0x12;
        constexpr int kSkipForward    = 0x13;
        constexpr int kSkipBackward   = 0x14;
    }
    constexpr UINT   kTrayIconUid  = 42;
    constexpr UINT   kCmdExit      = 40001;
    constexpr UINT   kCmdStartup   = 40002;
    constexpr std::wstring_view kClassName = L"MaestroMsgWnd";
}

Application::Application() noexcept = default;

Application::~Application() noexcept
{
    unregister_hotkeys();
    tear_down_tray_icon();
    if (hwnd_) [[likely]] {
        DestroyWindow(hwnd_);
    }
}

bool Application::initialize(HINSTANCE instance)
{
    instance_ = instance;

    if (!create_msg_wnd(instance)) [[unlikely]] {
        return false;
    }

    if (!setup_tray_icon()) [[unlikely]] {
        return false;
    }

    volume_.attach_to_proc(config_.target_process());
    register_hotkeys();
    return true;
}

int Application::run()
{
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        if (msg.message == WM_HOTKEY) [[likely]] {
            dispatch_hotkey(static_cast<int>(msg.wParam));
        }
        else {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }
    return static_cast<int>(msg.wParam);
}

bool Application::create_msg_wnd(HINSTANCE instance)
{
    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = router_proc;
    wc.hInstance      = instance;
    wc.lpszClassName  = kClassName.data();

    if (!RegisterClassExW(&wc)) [[unlikely]] {
        return false;
    }

    hwnd_ = CreateWindowExW(
        0, kClassName.data(), nullptr, 0,
        0, 0, 0, 0,
        HWND_MESSAGE, nullptr, instance, this
    );
    return hwnd_ != nullptr;
}

LRESULT CALLBACK Application::router_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    Application* self = nullptr;

    if (msg == WM_NCCREATE) [[unlikely]] {
        auto cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<Application*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    else [[likely]] {
        self = reinterpret_cast<Application*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (self) [[likely]] {
        return self->handle_msg(hwnd, msg, wp, lp);
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Application::handle_msg(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    if (tray_msg_ != 0 && msg == tray_msg_) {
        if (LOWORD(lp) == WM_RBUTTONUP) {
            show_context_menu();
        }
        return 0;
    }

    switch (msg) {
    case WM_COMMAND:
        if (LOWORD(wp) == kCmdExit) {
            PostQuitMessage(0);
        }
        else if (LOWORD(wp) == kCmdStartup) {
            config_.set_startup_enabled(!config_.startup_enabled());
        }
        return 0;

    case WM_DESTROY:
        tear_down_tray_icon();
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
}

bool Application::setup_tray_icon()
{
    HICON icon = LoadIconW(nullptr, IDI_APPLICATION);

    tray_msg_ = RegisterWindowMessageW(L"MaestroTrayMsg");
    if (tray_msg_ == 0) [[unlikely]] {
        return false;
    }

    nid_.cbSize           = sizeof(nid_);
    nid_.hWnd             = hwnd_;
    nid_.uID              = kTrayIconUid;
    nid_.uFlags           = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    nid_.uCallbackMessage = tray_msg_;
    nid_.hIcon            = icon;
    StringCchCopyW(nid_.szTip, ARRAYSIZE(nid_.szTip), L"Maestro - right-click to exit");

    return Shell_NotifyIconW(NIM_ADD, &nid_) != FALSE;
}

void Application::tear_down_tray_icon() noexcept
{
    Shell_NotifyIconW(NIM_DELETE, &nid_);
}

void Application::show_context_menu() noexcept
{
    HMENU menu = CreatePopupMenu();
    if (!menu) [[unlikely]] {
        return;
    }

    UINT startup_flags = MF_STRING;
    if (config_.startup_enabled()) {
        startup_flags |= MF_CHECKED;
    }
    AppendMenuW(menu, startup_flags, kCmdStartup, L"Run at startup");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kCmdExit, L"Exit");

    POINT pt{};
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd_, nullptr);
    DestroyMenu(menu);
}

void Application::register_hotkeys()
{
    auto bind = [&](int id, const std::string& action, const HotkeyBinding& fallback,
                    std::function<void()> fn) {
        auto binding = config_.hotkey(action);
        hotkeys_[id] = {
            binding ? binding->modifiers : fallback.modifiers,
            binding ? binding->vk        : fallback.vk,
            std::move(fn)
        };
    };

    float step = config_.volume_step();

    bind(hotkey_id::kTogglePlayback, "toggle_playback",
         { MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, VK_F5 },
         [this] { send_key_press(VK_MEDIA_PLAY_PAUSE); });

    bind(hotkey_id::kRaiseVolume, "volume_up",
         { MOD_CONTROL | MOD_SHIFT, VK_PRIOR },
         [this, step] { ensure_session(); volume_.nudge(step); });

    bind(hotkey_id::kLowerVolume, "volume_down",
         { MOD_CONTROL | MOD_SHIFT, VK_NEXT },
         [this, step] { ensure_session(); volume_.nudge(-step); });

    bind(hotkey_id::kSkipForward, "next_track",
         { MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, VK_RIGHT },
         [this] { send_key_press(VK_MEDIA_NEXT_TRACK); });

    bind(hotkey_id::kSkipBackward, "previous_track",
         { MOD_NOREPEAT | MOD_CONTROL | MOD_SHIFT, VK_LEFT },
         [this] { send_key_press(VK_MEDIA_PREV_TRACK); });

    bool all_ok = true;
    for (auto& [id, hk] : hotkeys_)
        all_ok &= (::RegisterHotKey(nullptr, id, hk.modifiers, hk.vk) != 0);

    if (!all_ok) {
        MessageBoxW(nullptr,
            L"Some hotkeys could not be registered.\n"
            L"They may conflict with another application.",
            L"Maestro", MB_OK | MB_ICONWARNING);
    }
}

void Application::unregister_hotkeys() noexcept {
    for (auto& [id, _] : hotkeys_)
        ::UnregisterHotKey(nullptr, id);
}

void Application::dispatch_hotkey(int id) {
    if (auto it = hotkeys_.find(id); it != hotkeys_.end())
        it->second.action();
}

void Application::ensure_session() noexcept {
    if (!volume_.is_attached()) [[unlikely]]
        volume_.attach_to_proc(config_.target_process());
}

void Application::send_key_press(WORD vk) noexcept {
    INPUT down{};
    down.type   = INPUT_KEYBOARD;
    down.ki.wVk = vk;

    INPUT up      = down;
    up.ki.dwFlags = KEYEVENTF_KEYUP;

    INPUT sequence[] = { down, up };
    SendInput(ARRAYSIZE(sequence), sequence, sizeof(INPUT));
}
