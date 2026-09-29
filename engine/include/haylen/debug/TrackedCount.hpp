#pragma once

#include <cstddef>

#include "haylen/debug/ObjectCounter.hpp"

namespace haylen::debug {

// Counts the changing number of objects that one owner holds, such as the particles of an emitter or the bodies of a physics world, for as long as the owner lives. The owner reports its count when it changes, and the difference counts as created or destroyed objects.
class TrackedCount final {
  public:
    explicit TrackedCount(ObjectCounter& kind) noexcept : counter(&kind) {}
    TrackedCount(const TrackedCount& other) noexcept : counter(other.counter) {
        set(other.count);
    }
    TrackedCount& operator=(const TrackedCount& other) noexcept {
        set(other.count);
        return *this;
    }
    ~TrackedCount() {
        set(0);
    }

    void set(std::size_t value) noexcept {
        if (value > count) {
            counter->add(value - count);
        } else if (value < count) {
            counter->remove(count - value);
        }
        count = value;
    }
    [[nodiscard]] std::size_t get() const noexcept {
        return count;
    }

  private:
    ObjectCounter* counter;
    std::size_t count = 0;
};

} // namespace haylen::debug
