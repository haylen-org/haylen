#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::platform {

// What the device and its operating system are, which the platform reports once when the app starts. Text the platform does not report stays empty and numbers stay zero, so nothing is guessed.
struct SystemInfo {
    enum class Os : std::uint8_t {
        MacOs,
        Windows,
        Linux,
        Ios,
        IpadOs,
        TvOs,
        Android,
        Web,
    };

    enum class DeviceKind : std::uint8_t {
        Desktop,
        Phone,
        Tablet,
        Tv,
        Browser,
    };

    enum class Architecture : std::uint8_t {
        Arm64,
        Arm,
        X64,
        X86,
        Wasm32,
        Wasm64,
    };

    Os os = Os::MacOs;
    std::string osVersion;
    std::string deviceModel;
    std::string manufacturer;
    DeviceKind deviceKind = DeviceKind::Desktop;
    std::string cpuName;
    int cpuCores = 0;

    // The processor architecture the process runs on and the width of its pointers, which the build of the engine decides, so a 32-bit Android library on a 64-bit phone reports `Arm` and 32.
    Architecture architecture = processArchitecture();
    int bits = static_cast<int>(sizeof(void*) * 8);

    // The total memory of the device.
    std::uint64_t memoryBytes = 0;
    std::string gpuName;

    // The language of the user as a BCP 47 tag, such as `pt-BR`, and the languages the user prefers, the most preferred first.
    std::string locale;
    std::vector<std::string> languages;

    // The IANA name of the time zone, such as `America/Sao_Paulo`.
    std::string timeZone;

    // The info as `haylen.system` reports it, with the enums as their names and every value the platform left empty left out, except the list of languages.
    [[nodiscard]] core::Json toJson() const;

    [[nodiscard]] static std::string_view osName(Os value) noexcept;
    [[nodiscard]] static std::string_view deviceKindName(DeviceKind value) noexcept;
    [[nodiscard]] static std::string_view architectureName(Architecture value) noexcept;

    [[nodiscard]] static constexpr Architecture processArchitecture() noexcept {
#if defined(__aarch64__) || defined(_M_ARM64)
        return Architecture::Arm64;
#elif defined(__arm__) || defined(_M_ARM)
        return Architecture::Arm;
#elif defined(__x86_64__) || defined(_M_X64)
        return Architecture::X64;
#elif defined(__i386__) || defined(_M_IX86)
        return Architecture::X86;
#elif defined(__wasm64__)
        return Architecture::Wasm64;
#elif defined(__wasm32__)
        return Architecture::Wasm32;
#else
#error "The engine does not know the processor architecture of this build."
#endif
    }

  private:
    static const std::array<std::pair<std::string_view, Os>, 8> kOsNames;
    static const std::array<std::pair<std::string_view, DeviceKind>, 5> kDeviceKindNames;
    static const std::array<std::pair<std::string_view, Architecture>, 6> kArchitectureNames;
};

} // namespace haylen::platform
