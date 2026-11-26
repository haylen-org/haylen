#include "platform/ProcessorCheck.hpp"

#include <cstdio>

#if defined(_M_X64)
#include <intrin.h>
#elif defined(__x86_64__)
#include <cpuid.h>
#endif

namespace haylen::platform {

const char* ProcessorCheck::describeMissing(std::uint32_t features) noexcept {
    const bool sse42 = (features & kSse42) != 0;
    const bool popcnt = (features & kPopcnt) != 0;
    if (!sse42 && !popcnt) {
        return kMissingBoth;
    }

    if (!sse42) {
        return kMissingSse42;
    }

    return popcnt ? nullptr : kMissingPopcnt;
}

std::uint32_t ProcessorCheck::readFeatures() noexcept {
#if defined(_M_X64)
    int registers[4]{};
    __cpuid(registers, 1);
    return static_cast<std::uint32_t>(registers[2]);
#elif defined(__x86_64__)
    unsigned int eax = 0;
    unsigned int ebx = 0;
    unsigned int ecx = 0;
    unsigned int edx = 0;
    __get_cpuid(1, &eax, &ebx, &ecx, &edx);
    return ecx;
#else
    return kSse42 | kPopcnt;
#endif
}

bool ProcessorCheck::require() noexcept {
    const char* missing = describeMissing(readFeatures());
    if (missing == nullptr) {
        return true;
    }

    std::fputs(missing, stderr);
    std::fputc('\n', stderr);
    return false;
}

} // namespace haylen::platform
