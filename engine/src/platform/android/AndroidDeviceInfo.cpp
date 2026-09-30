#include "platform/android/AndroidDeviceInfo.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::platform {

// A TV counts as a TV whatever the size of its screen.
SystemInfo AndroidDeviceInfo::toSystemInfo(std::string_view json) {
    const core::Json device = core::Json::parse(json);
    SystemInfo info;
    info.os = SystemInfo::Os::Android;
    if (device.value("television", false)) {
        info.deviceKind = SystemInfo::DeviceKind::Tv;
    } else {
        info.deviceKind = device.value("tablet", false) ? SystemInfo::DeviceKind::Tablet : SystemInfo::DeviceKind::Phone;
    }
    info.osVersion = device.value("osVersion", "");
    info.deviceModel = device.value("deviceModel", "");
    info.manufacturer = device.value("manufacturer", "");
    info.cpuName = device.value("cpuName", "");
    info.cpuCores = device.value("cpuCores", 0);
    info.memoryBytes = device.value("memoryBytes", std::uint64_t{0});
    info.locale = device.value("locale", "");
    info.languages = device.value("languages", std::vector<std::string>());
    info.timeZone = device.value("timeZone", "");
    return info;
}

} // namespace haylen::platform
