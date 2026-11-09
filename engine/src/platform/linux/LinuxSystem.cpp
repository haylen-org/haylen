#include "platform/linux/LinuxSystem.hpp"

#include <sys/utsname.h>
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <thread>

#include "platform/NativeHost.hpp"
#include "platform/SystemState.hpp"
#include "platform/linux/LinuxSystemReader.hpp"

namespace haylen::platform {

SystemInfo LinuxSystem::getInfo() {
    const long pages = sysconf(_SC_PHYS_PAGES);
    const long pageSize = sysconf(_SC_PAGESIZE);
    return {
        .os = SystemInfo::Os::Linux,
        .osVersion = getSystemVersion(),
        .deviceModel = LinuxSystemReader::readValue("/sys/class/dmi/id/product_name"),
        .manufacturer = LinuxSystemReader::readValue("/sys/class/dmi/id/sys_vendor"),
        .deviceKind = SystemInfo::DeviceKind::Desktop,
        .cpuName = LinuxSystemReader::readCpuName(LinuxSystemReader::readText("/proc/cpuinfo")),
        .cpuCores = static_cast<int>(sysconf(_SC_NPROCESSORS_ONLN)),
        .memoryBytes = pages > 0 && pageSize > 0 ? static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(pageSize) : 0,
    };
}

bool LinuxSystem::isTimeZoneFixed() {
    return !getVariable("TZ").empty();
}

void LinuxSystem::reportTimeZone() {
    std::error_code error;
    NativeHost::getSystemState().setTimeZone(LinuxSystemReader::readTimeZone(getVariable("TZ"), std::filesystem::read_symlink(kLocalTime, error).string()));
}

void LinuxSystem::reportLanguages() {
    NativeHost::getSystemState().setLanguages(LinuxSystemReader::readLanguages(getVariable("LANGUAGE"), getVariable("LC_ALL"), getVariable("LC_MESSAGES"), getVariable("LANG")));
}

void LinuxSystem::watchBattery() {
    NativeHost::getSystemState().setBattery(LinuxSystemReader::readBattery(kPowerSupplies));
    // clang-format off
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(kBatteryInterval);
            NativeHost::getSystemState().setBattery(LinuxSystemReader::readBattery(kPowerSupplies));
        }
    }).detach();
    // clang-format on
}

// The distribution names itself in `os-release`, which `/usr/lib` holds where `/etc` has none, and every distribution shares the release of the kernel.
std::string LinuxSystem::getSystemVersion() {
    std::string osRelease = LinuxSystemReader::readText("/etc/os-release");
    if (osRelease.empty()) {
        osRelease = LinuxSystemReader::readText("/usr/lib/os-release");
    }
    utsname kernel{};
    return LinuxSystemReader::readOsVersion(osRelease, uname(&kernel) == 0 ? kernel.release : "");
}

std::string LinuxSystem::getVariable(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

} // namespace haylen::platform
