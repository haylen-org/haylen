#pragma once

#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d12 {

// The copy of a render target into a buffer of a readback heap, which `poll` maps once the fence passed the frame that copied it, so it never waits.
class ImageReadback final : public Device::Readback {
  public:
    ImageReadback(Microsoft::WRL::ComPtr<ID3D12Resource> copy, Microsoft::WRL::ComPtr<ID3D12Fence> frames, std::uint64_t frameValue, UINT pitch, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    Microsoft::WRL::ComPtr<ID3D12Resource> buffer;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    std::uint64_t value = 0;
    UINT rowPitch = 0;
    Device::PixelFormat format = Device::PixelFormat::Rgba8;
    int width = 0;
    int height = 0;
};

} // namespace haylen::gpu::d3d12
