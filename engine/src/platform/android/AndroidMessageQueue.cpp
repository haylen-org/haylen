#include "platform/android/AndroidMessageQueue.hpp"

#include <sys/eventfd.h>
#include <unistd.h>

#include <stdexcept>
#include <utility>

namespace haylen::platform {

AndroidMessageQueue::AndroidMessageQueue() : items(kInitialCapacity), descriptor(eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK)) {
    if (descriptor < 0) {
        throw std::runtime_error("The frame thread of the activity could not create the descriptor that wakes it.");
    }
}

AndroidMessageQueue::~AndroidMessageQueue() {
    close(descriptor);
}

// Only the first message of an empty queue wakes the frame thread, which takes every message that waits.
std::uint64_t AndroidMessageQueue::push(const Payload& payload) {
    std::uint64_t sequence = 0;
    bool wasEmpty = false;
    {
        const std::scoped_lock lock(mutex);
        if (count == items.size()) {
            grow();
        }
        sequence = ++pushed;
        wasEmpty = count == 0;
        items[(head + count) % items.size()] = {.sequence = sequence, .payload = payload};
        ++count;
    }
    if (wasEmpty) {
        const std::uint64_t wake = 1;
        (void)write(descriptor, &wake, sizeof(wake));
    }
    return sequence;
}

void AndroidMessageQueue::quiet() noexcept {
    std::uint64_t wakes = 0;
    (void)read(descriptor, &wakes, sizeof(wakes));
}

bool AndroidMessageQueue::pop(Message& message) {
    const std::scoped_lock lock(mutex);
    if (count == 0) {
        return false;
    }
    message = std::move(items[head]);
    head = (head + 1) % items.size();
    --count;
    return true;
}

void AndroidMessageQueue::markHandled(std::uint64_t sequence) {
    {
        const std::scoped_lock lock(mutex);
        handled = sequence;
    }
    handledChanged.notify_all();
}

void AndroidMessageQueue::waitHandled(std::uint64_t sequence) {
    std::unique_lock lock(mutex);
    handledChanged.wait(lock, [this, sequence] { return handled >= sequence; });
}

void AndroidMessageQueue::grow() {
    std::vector<Message> grown(items.size() * 2);
    for (std::size_t index = 0; index < count; ++index) {
        grown[index] = std::move(items[(head + index) % items.size()]);
    }
    items = std::move(grown);
    head = 0;
}

} // namespace haylen::platform
