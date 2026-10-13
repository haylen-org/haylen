#include "platform/windows/WindowsRumble.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <xinput.h>

#include <cmath>

namespace haylen::platform {

std::array<std::optional<std::chrono::steady_clock::time_point>, WindowsRumble::kControllers> WindowsRumble::ends{};

void WindowsRumble::set(std::size_t index, float low, float high) {
    XINPUT_VIBRATION motors{.wLeftMotorSpeed = static_cast<WORD>(std::lround(low * 65535.0F)), .wRightMotorSpeed = static_cast<WORD>(std::lround(high * 65535.0F))};
    XInputSetState(static_cast<DWORD>(index), &motors);
}

void WindowsRumble::start(std::size_t index, float low, float high, float seconds) {
    static_assert(kControllers == XUSER_MAX_COUNT);
    if (index >= ends.size()) {
        return;
    }
    const bool stops = low <= 0.0F && high <= 0.0F;
    set(index, low, high);
    ends[index] = stops ? std::nullopt : std::optional(std::chrono::steady_clock::now() + std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<float>(seconds)));
}

void WindowsRumble::update() {
    const auto now = std::chrono::steady_clock::now();
    for (std::size_t index = 0; index < ends.size(); ++index) {
        if (ends[index] && *ends[index] <= now) {
            set(index, 0.0F, 0.0F);
            ends[index].reset();
        }
    }
}

} // namespace haylen::platform
