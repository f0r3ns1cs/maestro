#include <windows.h>

#include <string_view>

#include "application.h"
#include "ComRaii.h"

namespace {
    constexpr std::wstring_view kMutexName = L"mMaestro";
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, LPWSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName.data());
    const DWORD err = GetLastError();

    if (err == ERROR_ALREADY_EXISTS) [[unlikely]] {
        MessageBoxW(nullptr, L"Maestro is already running.",
            L"Maestro", MB_OK | MB_ICONINFORMATION);
        if (mutex) CloseHandle(mutex); // still need to close it
        return 1;
    }
    if (!mutex) [[unlikely]] {
        MessageBoxW(nullptr, L"Failed to create mutex.",
            L"Maestro", MB_OK | MB_ICONERROR);
        return 1;
    }

    ComRaii com;
    if (!com) [[unlikely]] {
        MessageBoxW(nullptr, L"Failed to initialize COM.",
                    L"Maestro", MB_OK | MB_ICONERROR);
        return 1;
    }

    Application app;
    if (!app.initialize(instance)) [[unlikely]] {
        MessageBoxW(nullptr, L"Failed to start the application.",
                    L"Maestro", MB_OK | MB_ICONERROR);
        return 1;
    }

    const int exit_code = app.run();

    if (mutex) [[likely]] {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }

    return exit_code;
}
