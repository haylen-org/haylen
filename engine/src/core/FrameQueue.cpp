#include "haylen/core/FrameQueue.hpp"

#include <exception>
#include <utility>

namespace haylen::core {

void FrameQueue::post(std::function<void()> call) {
    const std::scoped_lock lock(mutex);
    calls.push_back(std::move(call));
}

void FrameQueue::flush() {
    {
        const std::scoped_lock lock(mutex);
        std::swap(calls, running);
    }

    // The running buffer keeps its capacity between frames, so a steady flow of calls does not allocate every frame.
    std::exception_ptr failure;
    for (std::function<void()>& call : running) {
        try {
            call();
        } catch (...) {
            if (!failure) {
                failure = std::current_exception();
            }
        }
    }
    running.clear();
    if (failure) {
        std::rethrow_exception(failure);
    }
}

void FrameQueue::clear() noexcept {
    std::vector<std::function<void()>> dropped;
    {
        const std::scoped_lock lock(mutex);
        dropped = std::exchange(calls, {});
    }
    running.clear();
}

std::size_t FrameQueue::size() const {
    const std::scoped_lock lock(mutex);
    return calls.size();
}

} // namespace haylen::core
