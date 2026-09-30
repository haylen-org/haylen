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

    Os os = Os::MacOs;
    std::string osVersion;
    std::string deviceModel;
    std::string manufacturer;
    DeviceKind deviceKind = DeviceKind::Desktop;
    std::string cpuName;
    int cpuCores = 0;

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

  private:
    static const std::array<std::pair<std::string_view, Os>, 8> kOsNames;
    static const std::array<std::pair<std::string_view, DeviceKind>, 5> kDeviceKindNames;
};

} // namespace haylen::platform
