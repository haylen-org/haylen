#include "haylen/platform/SystemInfo.hpp"

#include <algorithm>

namespace haylen::platform {

const std::array<std::pair<std::string_view, SystemInfo::Os>, 8> SystemInfo::kOsNames{{{"macOs", Os::MacOs}, {"windows", Os::Windows}, {"linux", Os::Linux}, {"ios", Os::Ios}, {"ipadOs", Os::IpadOs}, {"tvOs", Os::TvOs}, {"android", Os::Android}, {"web", Os::Web}}};
const std::array<std::pair<std::string_view, SystemInfo::DeviceKind>, 5> SystemInfo::kDeviceKindNames{{{"desktop", DeviceKind::Desktop}, {"phone", DeviceKind::Phone}, {"tablet", DeviceKind::Tablet}, {"tv", DeviceKind::Tv}, {"browser", DeviceKind::Browser}}};

core::Json SystemInfo::toJson() const {
    core::Json json{{"os", osName(os)}, {"deviceKind", deviceKindName(deviceKind)}, {"languages", languages}};
    const std::array<std::pair<const char*, const std::string*>, 7> texts{{{"osVersion", &osVersion}, {"deviceModel", &deviceModel}, {"manufacturer", &manufacturer}, {"cpuName", &cpuName}, {"gpuName", &gpuName}, {"locale", &locale}, {"timeZone", &timeZone}}};
    for (const auto& [key, text] : texts) {
        if (!text->empty()) {
            json[key] = *text;
        }
    }
    if (cpuCores > 0) {
        json["cpuCores"] = cpuCores;
    }
    if (memoryBytes > 0) {
        json["memoryBytes"] = memoryBytes;
    }
    return json;
}

std::string_view SystemInfo::osName(Os value) noexcept {
    return std::ranges::find(kOsNames, value, &std::pair<std::string_view, Os>::second)->first;
}

std::string_view SystemInfo::deviceKindName(DeviceKind value) noexcept {
    return std::ranges::find(kDeviceKindNames, value, &std::pair<std::string_view, DeviceKind>::second)->first;
}

} // namespace haylen::platform
