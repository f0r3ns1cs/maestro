#include "session_volume.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <vector>

using Microsoft::WRL::ComPtr;

std::vector<DWORD> SessionVolume::find_process_ids(std::wstring_view name)
{
    std::vector<DWORD> pids;

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) [[unlikely]] {
        return pids;
    }

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (name == entry.szExeFile) {
                pids.push_back(entry.th32ProcessID);
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return pids;
}

bool SessionVolume::attach_to_proc(std::wstring_view name)
{
    detach();

    auto pids = find_process_ids(name);
    if (pids.empty()) [[unlikely]] {
        return false;
    }

    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
        CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) [[unlikely]] {
        return false;
    }

    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eMultimedia, &device))) [[unlikely]] {
        return false;
    }

    ComPtr<IAudioSessionManager2> mgr;
    if (FAILED(device->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL,
        nullptr, reinterpret_cast<void**>(mgr.GetAddressOf())))) [[unlikely]] {
        return false;
    }

    ComPtr<IAudioSessionEnumerator> sessions;
    if (FAILED(mgr->GetSessionEnumerator(&sessions))) [[unlikely]] {
        return false;
    }

    int count = 0;
    if (FAILED(sessions->GetCount(&count))) [[unlikely]] {
        return false;
    }

    for (int i = 0; i < count; ++i) {
        ComPtr<IAudioSessionControl> ctl;
        if (FAILED(sessions->GetSession(i, &ctl))) [[unlikely]] {
            continue;
        }

        ComPtr<IAudioSessionControl2> ctl2;
        if (FAILED(ctl.As(&ctl2))) [[unlikely]] {
            continue;
        }

        DWORD session_pid = 0;
        if (FAILED(ctl2->GetProcessId(&session_pid))) [[unlikely]] {
            continue;
        }

        if (std::find(pids.begin(), pids.end(), session_pid) != pids.end()) {
            if (SUCCEEDED(ctl2.As(&volume_))) {
                return true;
            }
        }
    }
    return false;
}

void SessionVolume::detach() noexcept
{
    volume_.Reset();
}

bool SessionVolume::is_attached() const noexcept
{
    return volume_ != nullptr;
}

std::optional<float> SessionVolume::get_level() const
{
    if (!volume_) [[unlikely]] {
        return std::nullopt;
    }

    float level = 0.0f;
    if (SUCCEEDED(volume_->GetMasterVolume(&level))) [[likely]]
    {
        return level;
    }
    return std::nullopt;
}

bool SessionVolume::set_level(float level)
{
    if (!volume_) [[unlikely]] {
        return false;
    }

    level = std::clamp(level, 0.0f, 1.0f);
    return SUCCEEDED(volume_->SetMasterVolume(level, nullptr));
}

bool SessionVolume::nudge(float delta)
{
    auto current = get_level();
    if (!current) [[unlikely]] {
        return false;
    }
    return set_level(*current + delta);
}
