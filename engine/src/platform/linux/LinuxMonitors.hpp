#pragma once

#include <cstdint>

namespace haylen::platform {

// Follows the network, the memory pressure, the power saver and the time zone of the desktop through the monitors of GIO, which the engine loads at run time: the network monitor, the memory monitor of GLib 2.64 and later, the power profile monitor of GLib 2.70 and later and a file monitor of `/etc/localtime`. A monitor that the GIO of the system lacks leaves its state unknown and never reports. The signals of the monitors arrive while the main context of GLib runs, once per frame on the frame thread.
class LinuxMonitors final {
  public:
    // Reports what each monitor knows now and connects to its changes.
    static void observe();

  private:
    static constexpr std::uint32_t kCriticalMemory = 255;
    static constexpr int kNoFlags = 0;

    using Callback = void (*)();

    static void* (*getNetworkMonitor)();
    static int (*isNetworkAvailable)(void* monitor);
    static void* (*getMemoryMonitor)();
    static void* (*getPowerProfileMonitor)();
    static int (*isPowerSaverEnabled)(void* monitor);
    static void* (*newFileForPath)(const char* path);
    static void* (*monitorFile)(void* file, int flags, void* cancellable, void** error);
    static unsigned long (*connect)(void* instance, const char* signal, Callback handler, void* data, void (*destroy)(void*, void*), int flags);

    static void observeNetwork(void* library);
    static void observeMemory(void* library);
    static void observePowerSaver(void* library);
    static void observeTimeZone(void* library);

    static void networkChanged(void* monitor, int available, void* data);
    static void memoryWarned(void* monitor, std::uint32_t level, void* data);
    static void powerSaverChanged(void* monitor, void* property, void* data);
    static void localTimeChanged(void* monitor, void* file, void* other, int event, void* data);
};

} // namespace haylen::platform
