#include "gpu/GpuPreference.hpp"

#include <windows.h>

namespace haylen::gpu {

// The variables live in the executable, which reaches them by the names it exports, so a library of the engine needs no symbol of the executable to link.
void GpuPreference::request(bool lowPower) noexcept {
    const HMODULE executable = GetModuleHandleW(nullptr);
    if (auto* nvidia = reinterpret_cast<DWORD*>(reinterpret_cast<void*>(GetProcAddress(executable, "NvOptimusEnablement")))) {
        *nvidia = lowPower ? 0U : 1U;
    }

    if (auto* amd = reinterpret_cast<int*>(reinterpret_cast<void*>(GetProcAddress(executable, "AmdPowerXpressRequestHighPerformance")))) {
        *amd = lowPower ? 0 : 1;
    }
}

} // namespace haylen::gpu
