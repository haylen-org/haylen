#pragma once

#include <webgpu/webgpu.h>

#include <array>
#include <cstdint>
#include <optional>

#include "gpu/webgpu/PassTimeline.hpp"

namespace haylen::gpu::webgpu {

// The time the GPU takes for the passes of each frame, which timestamp queries at the start and the end of every pass measure and `PassTimeline` turns into the time of the frame. The queries of a frame resolve into a buffer that the CPU maps a few frames later, so measuring never waits for the GPU, and a frame whose buffer is still mapped goes unmeasured. A buffer that is destroyed ends its mapping before the call returns, so the callbacks of the mappings never outlive the timer.
class FrameTimer final {
  public:
    // The passes of one frame that the queries measure, and the frames whose queries can wait for their mapping at once.
    static constexpr std::uint32_t kMaxPasses = 32;
    static constexpr std::uint32_t kSlots = 3;

    explicit FrameTimer(WGPUDevice device);
    FrameTimer(const FrameTimer&) = delete;
    FrameTimer& operator=(const FrameTimer&) = delete;
    ~FrameTimer();

    // The timestamp writes of the next pass of the frame, or none when the frame measures no more passes.
    [[nodiscard]] const WGPUPassTimestampWrites* nextPass() noexcept;

    // Resolves the queries of the frame into its buffer, before the commands of the frame are finished.
    void resolve(WGPUCommandEncoder encoder) noexcept;

    // Maps the buffer of the frame once the frame is submitted, and moves on to the next frame.
    void submitted() noexcept;

    [[nodiscard]] std::optional<double> getSeconds() const noexcept;

  private:
    enum class State : std::uint8_t {
        Free,
        Mapping,
        Mapped,
    };

    // The passes that the frame of the slot measures, those that its mapped buffer holds, and the number of the submission of that frame.
    struct Slot {
        WGPUBuffer readback = nullptr;
        std::uint32_t passes = 0;
        std::uint32_t mapped = 0;
        std::uint64_t frame = 0;
    };

    static constexpr std::uint64_t kSlotBytes = std::uint64_t{kMaxPasses} * 2U * sizeof(std::uint64_t);

    static void receiveMapping(WGPUMapAsyncStatus status, WGPUStringView message, void* first, void* second);

    // Reads the times of the slots whose mapping finished in the order of their frames, which frees them.
    void collect() noexcept;
    void read(Slot& slot) noexcept;

    WGPUQuerySet queries = nullptr;
    WGPUBuffer resolved = nullptr;
    std::array<Slot, kSlots> slots{};
    std::array<State, kSlots> states{};
    std::uint32_t current = 0;
    std::uint64_t submissions = 0;
    PassTimeline timeline;
    bool started = false;
    bool measuring = false;
    WGPUPassTimestampWrites writes = WGPU_PASS_TIMESTAMP_WRITES_INIT;
    std::optional<double> seconds;
};

} // namespace haylen::gpu::webgpu
