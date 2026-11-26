#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace haylen::gpu::d3d12 {

// Measures the time the GPU takes for each frame from a timestamp at the start of its transfer list, which runs first, or of its commands when it copies nothing, to a timestamp at the end of its commands, which the frame resolves into a buffer that the CPU reads once the fence of the frame passed, so it never waits.
class FrameTimer final {
  public:
    FrameTimer(ID3D12Device& device, ID3D12CommandQueue& queue, std::size_t frames);

    void begin(ID3D12GraphicsCommandList& list, std::size_t frame);
    void beginTransfers(ID3D12GraphicsCommandList& list, std::size_t frame);
    void end(ID3D12GraphicsCommandList& list, std::size_t frame);

    // Reads the timestamps of the frame, which the GPU finished.
    void collect(std::size_t frame);

    [[nodiscard]] std::optional<double> getSeconds() const noexcept {
        return seconds;
    }

  private:
    // The timestamps of a frame: the start of its transfers, the start of its commands and their end.
    static constexpr UINT kTimestamps = 3;

    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queries;
    Microsoft::WRL::ComPtr<ID3D12Resource> results;
    const std::uint64_t* mapped = nullptr;
    double frequency = 0.0;
    std::vector<bool> measured;
    std::vector<bool> transferred;
    std::optional<double> seconds;
};

} // namespace haylen::gpu::d3d12
