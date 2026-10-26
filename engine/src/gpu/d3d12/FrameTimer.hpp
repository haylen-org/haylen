#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace haylen::gpu::d3d12 {

// Measures the time the GPU takes for each frame with two timestamps around the commands of the frame, which the frame resolves into a buffer that the CPU reads once the fence of the frame passed, so it never waits.
class FrameTimer final {
  public:
    FrameTimer(ID3D12Device& device, ID3D12CommandQueue& queue, std::size_t frames);

    void begin(ID3D12GraphicsCommandList& list, std::size_t frame);
    void end(ID3D12GraphicsCommandList& list, std::size_t frame);

    // Reads the timestamps of the frame, which the GPU finished.
    void collect(std::size_t frame);

    [[nodiscard]] std::optional<double> getSeconds() const noexcept {
        return seconds;
    }

  private:
    Microsoft::WRL::ComPtr<ID3D12QueryHeap> queries;
    Microsoft::WRL::ComPtr<ID3D12Resource> results;
    const std::uint64_t* mapped = nullptr;
    double frequency = 0.0;
    std::vector<bool> measured;
    std::optional<double> seconds;
};

} // namespace haylen::gpu::d3d12
