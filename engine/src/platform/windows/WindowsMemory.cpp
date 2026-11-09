#include "platform/windows/WindowsMemory.hpp"

#include "haylen/platform/Event.hpp"
#include "platform/MemoryWarning.hpp"

namespace haylen::platform {

std::mutex& WindowsMemory::mutex = *new std::mutex();
HANDLE WindowsMemory::low = nullptr;
HANDLE WindowsMemory::high = nullptr;
HANDLE WindowsMemory::wait = nullptr;

void WindowsMemory::observe() {
    low = CreateMemoryResourceNotification(LowMemoryResourceNotification);
    high = CreateMemoryResourceNotification(HighMemoryResourceNotification);
    if (low == nullptr || high == nullptr) {
        return;
    }
    waitFor(low, &lowReached);
}

void CALLBACK WindowsMemory::lowReached(void*, BOOLEAN) {
    MemoryWarning::raise(Event::MemoryLevel::Warning);
    waitFor(high, &recovered);
}

void CALLBACK WindowsMemory::recovered(void*, BOOLEAN) {
    waitFor(low, &lowReached);
}

// A wait that runs once still needs its handle released, which its own callback may do while it runs. The lock keeps a callback from releasing the wait before the registration that started it has stored its handle.
void WindowsMemory::waitFor(HANDLE notification, WAITORTIMERCALLBACK callback) {
    const std::scoped_lock lock(mutex);
    if (wait != nullptr) {
        UnregisterWait(wait);
    }
    if (!RegisterWaitForSingleObject(&wait, notification, callback, nullptr, INFINITE, WT_EXECUTEONLYONCE)) {
        wait = nullptr;
    }
}

} // namespace haylen::platform
