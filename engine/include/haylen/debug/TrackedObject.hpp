#pragma once

#include <cstddef>
#include <cstdint>

#include "haylen/debug/ObjectCounter.hpp"

namespace haylen::debug {

// Counts one object in a counter for as long as it lives, together with the memory it reports. A class counts its objects by holding one as a member. A copy counts as a new object that holds no memory yet, and assignment changes nothing, since the object stays the same.
class TrackedObject final {
  public:
    explicit TrackedObject(ObjectCounter& kind) noexcept : counter(&kind) {
        counter->add();
    }
    TrackedObject(const TrackedObject& other) noexcept : counter(other.counter) {
        counter->add();
    }
    TrackedObject& operator=(const TrackedObject&) noexcept {
        return *this;
    }
    ~TrackedObject() {
        counter->addBytes(-bytes);
        counter->remove();
    }

    // Reports how much memory the object holds now, replacing what it reported before.
    void setBytes(std::size_t value) noexcept {
        const auto next = static_cast<std::int64_t>(value);
        counter->addBytes(next - bytes);
        bytes = next;
    }
    [[nodiscard]] std::size_t getBytes() const noexcept {
        return static_cast<std::size_t>(bytes);
    }

  private:
    ObjectCounter* counter;
    std::int64_t bytes = 0;
};

} // namespace haylen::debug
