#pragma once

#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

namespace haylen::core {

// Calls that run on the frame thread at the end of the frame, after rendering. Any thread may post, and the engine flushes the queue once per frame. Deferred signal slots and queued events travel through it.
class FrameQueue final {
  public:
    void post(std::function<void()> call);

    // Runs the calls posted before the flush started. Calls posted while it runs wait for the next flush, and every call runs even when an earlier one throws, after which the first exception is thrown again.
    void flush();

    // Drops every pending call without running it.
    void clear() noexcept;

    [[nodiscard]] std::size_t size() const;

  private:
    mutable std::mutex mutex;
    std::vector<std::function<void()>> calls;
    std::vector<std::function<void()>> running;
};

} // namespace haylen::core
