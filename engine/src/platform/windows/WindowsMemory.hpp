#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <mutex>

namespace haylen::platform {

// Follows the memory resource notifications of Windows, which signal when the available physical memory falls below the low threshold of the system and when it rises above the high one. Each fall raises one memory warning, and the next warning waits until the memory recovered, on the thread pool of the system, for the life of the process. Windows tells no critical level.
class WindowsMemory final {
  public:
    static void observe();

  private:
    static void CALLBACK lowReached(void* context, BOOLEAN timedOut);
    static void CALLBACK recovered(void* context, BOOLEAN timedOut);
    static void waitFor(HANDLE notification, WAITORTIMERCALLBACK callback);

    static std::mutex& mutex;
    static HANDLE low;
    static HANDLE high;
    static HANDLE wait;
};

} // namespace haylen::platform
