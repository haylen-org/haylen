#include "platform/linux/LinuxMonitors.hpp"

#include "haylen/platform/Event.hpp"
#include "platform/MemoryWarning.hpp"
#include "platform/NativeHost.hpp"
#include "platform/Player.hpp"
#include "platform/SystemState.hpp"
#include "platform/linux/LinuxGlib.hpp"
#include "platform/linux/LinuxSystem.hpp"

namespace haylen::platform {

void* (*LinuxMonitors::getNetworkMonitor)() = nullptr;
int (*LinuxMonitors::isNetworkAvailable)(void*) = nullptr;
void* (*LinuxMonitors::getMemoryMonitor)() = nullptr;
void* (*LinuxMonitors::getPowerProfileMonitor)() = nullptr;
int (*LinuxMonitors::isPowerSaverEnabled)(void*) = nullptr;
void* (*LinuxMonitors::newFileForPath)(const char*) = nullptr;
void* (*LinuxMonitors::monitorFile)(void*, int, void*, void**) = nullptr;
unsigned long (*LinuxMonitors::connect)(void*, const char*, Callback, void*, void (*)(void*, void*), int) = nullptr;

// The time zone of `TZ` needs no monitor, and every other zone is reported even without GIO. The monitors stay for the process, which keeps their signals.
void LinuxMonitors::observe() {
    LinuxSystem::reportTimeZone();
    void* library = LinuxGlib::open();
    if (library == nullptr || !LinuxGlib::find(library, connect, "g_signal_connect_data")) {
        return;
    }
    observeNetwork(library);
    observeMemory(library);
    observePowerSaver(library);
    if (!LinuxSystem::isTimeZoneFixed()) {
        observeTimeZone(library);
    }
}

void LinuxMonitors::observeNetwork(void* library) {
    if (!LinuxGlib::find(library, getNetworkMonitor, "g_network_monitor_get_default") || !LinuxGlib::find(library, isNetworkAvailable, "g_network_monitor_get_network_available")) {
        return;
    }
    void* monitor = getNetworkMonitor();
    connect(monitor, "network-changed", reinterpret_cast<Callback>(&networkChanged), nullptr, nullptr, kNoFlags);
    networkChanged(monitor, isNetworkAvailable(monitor), nullptr);
}

void LinuxMonitors::observeMemory(void* library) {
    if (!LinuxGlib::find(library, getMemoryMonitor, "g_memory_monitor_dup_default")) {
        return;
    }
    connect(getMemoryMonitor(), "low-memory-warning", reinterpret_cast<Callback>(&memoryWarned), nullptr, nullptr, kNoFlags);
}

void LinuxMonitors::observePowerSaver(void* library) {
    if (!LinuxGlib::find(library, getPowerProfileMonitor, "g_power_profile_monitor_dup_default") || !LinuxGlib::find(library, isPowerSaverEnabled, "g_power_profile_monitor_get_power_saver_enabled")) {
        return;
    }
    void* monitor = getPowerProfileMonitor();
    connect(monitor, "notify::power-saver-enabled", reinterpret_cast<Callback>(&powerSaverChanged), nullptr, nullptr, kNoFlags);
    powerSaverChanged(monitor, nullptr, nullptr);
}

// A new zone replaces the link `/etc/localtime`, which the monitor of the file sees in its folder.
void LinuxMonitors::observeTimeZone(void* library) {
    if (!LinuxGlib::find(library, newFileForPath, "g_file_new_for_path") || !LinuxGlib::find(library, monitorFile, "g_file_monitor_file")) {
        return;
    }
    void* monitor = monitorFile(newFileForPath(LinuxSystem::kLocalTime), kNoFlags, nullptr, nullptr);
    if (monitor != nullptr) {
        connect(monitor, "changed", reinterpret_cast<Callback>(&localTimeChanged), nullptr, nullptr, kNoFlags);
    }
}

void LinuxMonitors::networkChanged(void*, int available, void*) {
    Player::postEvent({.type = Event::Type::NetworkChanged, .online = available != 0});
}

// GIO warns at a low, a medium and a critical level, and only the last one says that the system is about to end processes.
void LinuxMonitors::memoryWarned(void*, std::uint32_t level, void*) {
    MemoryWarning::raise(level >= kCriticalMemory ? Event::MemoryLevel::Critical : Event::MemoryLevel::Warning);
}

void LinuxMonitors::powerSaverChanged(void* monitor, void*, void*) {
    NativeHost::getSystemState().setPowerSaving(isPowerSaverEnabled(monitor) != 0);
}

void LinuxMonitors::localTimeChanged(void*, void*, void*, int, void*) {
    LinuxSystem::reportTimeZone();
}

} // namespace haylen::platform
