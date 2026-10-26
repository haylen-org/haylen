#pragma once

#include <webgpu/webgpu.h>

#include <array>
#include <cstdint>
#include <memory>
#include <optional>

namespace haylen::gpu::webgpu {

// The time the GPU takes for the passes of each frame, which timestamp queries at the start and the end of every pass measure. The queries of a frame resolve into a buffer that the CPU maps a few frames later, so measuring never waits for the GPU, and a frame whose buffer is still mapped goes unmeasured.
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

    // The passes that the frame of the slot measures, and those that its mapped buffer holds.
    struct Slot {
        WGPUBuffer readback = nullptr;
        std::uint32_t passes = 0;
        std::uint32_t mapped = 0;
    };

    static constexpr std::uint64_t kSlotBytes = std::uint64_t{kMaxPasses} * 2U * sizeof(std::uint64_t);

    static void receiveMapping(WGPUMapAsyncStatus status, WGPUStringView message, void* first, void* second);

    // Reads the times of the slots whose mapping finished, which frees them.
    void collect() noexcept;

    WGPUQuerySet queries = nullptr;
    WGPUBuffer resolved = nullptr;
    std::array<Slot, kSlots> slots{};
    std::shared_ptr<std::array<State, kSlots>> states = std::make_shared<std::array<State, kSlots>>();
    std::uint32_t current = 0;
    bool started = false;
    bool measuring = false;
    WGPUPassTimestampWrites writes = WGPU_PASS_TIMESTAMP_WRITES_INIT;
    std::optional<double> seconds;
};

} // namespace haylen::gpu::webgpu
