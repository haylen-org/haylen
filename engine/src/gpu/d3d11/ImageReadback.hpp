#pragma once

#include <d3d11_1.h>
#include <wrl/client.h>

#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::d3d11 {

// The copy of a render target into a staging texture behind an event query, which `poll` maps only once the query says the GPU finished the copy, so it never waits.
class ImageReadback final : public Device::Readback {
  public:
    ImageReadback(Microsoft::WRL::ComPtr<ID3D11DeviceContext> owner, Microsoft::WRL::ComPtr<ID3D11Texture2D> copy, Microsoft::WRL::ComPtr<ID3D11Query> finished, Device::PixelFormat pixelFormat, int imageWidth, int imageHeight);

    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

  private:
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    Microsoft::WRL::ComPtr<ID3D11Texture2D> staging;
    Microsoft::WRL::ComPtr<ID3D11Query> done;
    Device::PixelFormat format = Device::PixelFormat::Rgba8;
    int width = 0;
    int height = 0;
};

} // namespace haylen::gpu::d3d11
