#pragma once

#include <cstddef>

namespace haylen::test {

// Watches the allocations of the current thread while it lives, which the test program sees through its own global allocation functions, so tests can show that an operation never allocates a whole file or package. Trackers nest, and only the innermost one counts.
class AllocationTracker final {
  public:
    AllocationTracker() noexcept;
    ~AllocationTracker();

    AllocationTracker(const AllocationTracker&) = delete;
    AllocationTracker& operator=(const AllocationTracker&) = delete;

    [[nodiscard]] std::size_t getLargest() const noexcept {
        return largest;
    }
    [[nodiscard]] std::size_t getTotal() const noexcept {
        return total;
    }

    // Called by the global allocation functions of the test program for every allocation.
    static void record(std::size_t size) noexcept;

  private:
    static thread_local AllocationTracker* active;

    AllocationTracker* previous;
    std::size_t largest = 0;
    std::size_t total = 0;
};

} // namespace haylen::test
