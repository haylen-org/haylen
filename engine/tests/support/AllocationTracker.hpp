#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#if defined(__ANDROID__)
#include <unwind.h>
#endif

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

    // Called by the global allocation functions of the test program for every allocation, with the address of the code that asked for it.
    static void record(std::size_t size, const void* caller) noexcept;

  private:
    // The bytes of the stack of an allocation that the web reads to see whether the WebGPU of the browser asked for it.
    static constexpr std::size_t kStackBytes = 16384;

    // Whether the code at the address belongs to the GPU driver of the system or to the validation layer of its graphics API, which allocate for every command buffer that a device records, beyond what the engine can change.
    [[nodiscard]] static bool isSystemGraphics(const void* caller) noexcept;
#if defined(__linux__)
    // The frames of the stack of an allocation that Linux reads to find the code that asked for it.
    static constexpr std::size_t kStackDepth = 24;

    [[nodiscard]] static bool isLinuxGraphics(const void* caller) noexcept;
    [[nodiscard]] static std::string_view getLibraryName(const void* address) noexcept;
    [[nodiscard]] static int captureStack(std::array<void*, kStackDepth>& frames) noexcept;
#endif
#if defined(__ANDROID__)
    // The frames that the unwinder of Android walked so far.
    struct StackWalk {
        std::array<void*, kStackDepth>* frames = nullptr;
        int depth = 0;
    };

    static _Unwind_Reason_Code addFrame(_Unwind_Context* context, void* walk) noexcept;
#endif

    static thread_local AllocationTracker* active;

    AllocationTracker* previous;
    std::size_t largest = 0;
    std::size_t total = 0;
};

} // namespace haylen::test
