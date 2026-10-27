#include "gpu/webgpu/PassTimeline.hpp"

#include <algorithm>

namespace haylen::gpu::webgpu {

std::optional<double> PassTimeline::addFrame(std::uint64_t frame, std::span<const std::uint64_t> times) noexcept {
    const bool follows = lastFrame && *lastFrame + 1U == frame;
    std::uint64_t reached = follows ? lastEnd : 0U;
    std::uint64_t busy = 0;
    bool written = true;
    for (std::size_t pass = 0; pass + 1U < times.size(); pass += 2U) {
        const std::uint64_t begin = times[pass];
        const std::uint64_t end = times[pass + 1U];
        written = written && begin != 0 && end >= begin;

        const std::uint64_t start = std::max(begin, reached);
        busy += end > start ? end - start : 0U;
        reached = std::max(reached, end);
    }

    // The end of a frame with timestamps that cannot be trusted bounds nothing, so the frame after it counts its first pass whole.
    lastFrame = written ? std::optional(frame) : std::nullopt;
    lastEnd = reached;
    if (!written) {
        return std::nullopt;
    }
    return static_cast<double>(busy) * 1e-9;
}

} // namespace haylen::gpu::webgpu
