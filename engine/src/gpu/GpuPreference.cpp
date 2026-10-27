#include "gpu/GpuPreference.hpp"

#if defined(_WIN32)
#include <windows.h>
#endif

namespace haylen::gpu {

// The variables live in the executable, which reaches them by the names it exports, so a library of the engine needs no symbol of the executable to link.
void GpuPreference::request([[maybe_unused]] bool lowPower) noexcept {
#if defined(_WIN32)
    const HMODULE executable = GetModuleHandleW(nullptr);
    if (auto* nvidia = reinterpret_cast<DWORD*>(reinterpret_cast<void*>(GetProcAddress(executable, "NvOptimusEnablement")))) {
        *nvidia = lowPower ? 0U : 1U;
    }
    if (auto* amd = reinterpret_cast<int*>(reinterpret_cast<void*>(GetProcAddress(executable, "AmdPowerXpressRequestHighPerformance")))) {
        *amd = lowPower ? 0 : 1;
    }
#endif
}

} // namespace haylen::gpu
