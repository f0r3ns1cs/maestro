#ifndef SESSION_VOLUME_H
#define SESSION_VOLUME_H

#include <audioclient.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

#include <optional>
#include <string_view>
#include <vector>

class SessionVolume {
public:
    SessionVolume() noexcept = default;
    ~SessionVolume() noexcept = default;

    SessionVolume(const SessionVolume&) = delete;
    SessionVolume& operator=(const SessionVolume&) = delete;
    SessionVolume(SessionVolume&&) = delete;
    SessionVolume& operator=(SessionVolume&&) = delete;

    bool attach_to_proc(std::wstring_view name);

    void detach() noexcept;

    [[nodiscard]] bool is_attached() const noexcept;

    [[nodiscard]] std::optional<float> get_level() const;

    [[nodiscard]] bool set_level(float level);

    bool nudge(float delta);

private:
    Microsoft::WRL::ComPtr<ISimpleAudioVolume> volume_;

    [[nodiscard]] static std::vector<DWORD> find_process_ids(std::wstring_view name);
};

#endif // SESSION_VOLUME_H
