#include "support/AllocationTracker.hpp"

#include <algorithm>
#include <cstdlib>
#include <new>

namespace haylen::test {

thread_local AllocationTracker* AllocationTracker::active = nullptr;

AllocationTracker::AllocationTracker() noexcept : previous(active) {
    active = this;
}

AllocationTracker::~AllocationTracker() {
    active = previous;
}

void AllocationTracker::record(std::size_t size) noexcept {
    if (active != nullptr) {
        active->largest = std::max(active->largest, size);
        active->total += size;
    }
}

} // namespace haylen::test

// The standard names the replaceable allocation functions, so the program replaces them here, and they only forward to the tracker and to `malloc` and `free`. The array and nothrow forms call these by default, and the aligned forms keep their own pair.
void* operator new(std::size_t size) {
    haylen::test::AllocationTracker::record(size);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}
