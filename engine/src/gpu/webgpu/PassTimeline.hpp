#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace haylen::gpu::webgpu {

// The time the GPU works on each frame, from the timestamps that queries take at the start and the end of its passes. WebGPU lets a timestamp at the start of a pass be taken before the work submitted ahead of the pass finished, as the tile-based GPUs of Apple do while they still shade the pass before, so adding up the spans of the passes counts the same work more than once. The queue runs the passes in the order they were submitted, so the work of a pass counts only from the end of the pass before it, which for the first pass of a frame is the last pass of the frame before when that frame was measured. A frame with a timestamp that the queries never wrote, which reads zero, or that runs backwards gives no time.
class PassTimeline final {
  public:
    // Takes the timestamps in nanoseconds of the passes of the frame with the number, the start and the end of each pass in their order, in the order the frames were submitted, and returns the seconds the GPU worked on the frame, or none where the timestamps cannot tell.
    [[nodiscard]] std::optional<double> addFrame(std::uint64_t frame, std::span<const std::uint64_t> times) noexcept;

  private:
    std::optional<std::uint64_t> lastFrame;
    std::uint64_t lastEnd = 0;
};

} // namespace haylen::gpu::webgpu
