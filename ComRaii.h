#ifndef COM_RAII_H
#define COM_RAII_H

#include <objbase.h>

class ComRaii {
public:
    explicit ComRaii(DWORD concurrency = COINIT_APARTMENTTHREADED) noexcept
        : ok_(SUCCEEDED(CoInitializeEx(nullptr, concurrency))) {}

    ~ComRaii() noexcept {
        if (ok_) [[likely]]
            CoUninitialize();
    }

    ComRaii(const ComRaii&) = delete;
    ComRaii& operator=(const ComRaii&) = delete;
    ComRaii(ComRaii&&) = delete;
    ComRaii& operator=(ComRaii&&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept { return ok_; }

private:
    const bool ok_;
};

#endif // COM_RAII_H
