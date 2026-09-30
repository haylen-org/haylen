#include "platform/linux/LinuxSystem.hpp"

#include <spawn.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <thread>
#include <utility>

#include "platform/SystemState.hpp"
#include "platform/linux/LinuxSystemReader.hpp"
#include "platform/sokol/SokolHost.hpp"

namespace haylen::platform {

SystemInfo LinuxSystem::getInfo() {
    std::error_code error;
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
        .locale = LinuxSystemReader::toLanguageTag(getVariable("LANG")),
        .languages = LinuxSystemReader::readLanguages(getVariable("LANGUAGE"), getVariable("LANG")),
        .timeZone = LinuxSystemReader::readTimeZone(getVariable("TZ"), std::filesystem::read_symlink("/etc/localtime", error).string()),
    };
}

// Starts `xdg-open` without blocking the frame thread, and a helper thread waits for it to tell whether an application took the url.
void LinuxSystem::openUrl(const std::string& url, std::function<void(bool opened)> callback) {
    pid_t child = 0;
    const std::array<char*, 3> arguments{const_cast<char*>("xdg-open"), const_cast<char*>(url.c_str()), nullptr};
    if (posix_spawnp(&child, "xdg-open", nullptr, nullptr, arguments.data(), environ) != 0) {
        callback(false);
        return;
    }
    // clang-format off
    std::thread([child, done = std::move(callback)] {
        int status = 0;
        const bool exited = waitpid(child, &status, 0) == child;
        done(exited && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    }).detach();
    // clang-format on
}

void LinuxSystem::watchBattery() {
    SokolHost::getSystemState().setBattery(LinuxSystemReader::readBattery(kPowerSupplies));
    // clang-format off
    std::thread([] {
        for (;;) {
            std::this_thread::sleep_for(kBatteryInterval);
            SokolHost::getSystemState().setBattery(LinuxSystemReader::readBattery(kPowerSupplies));
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
