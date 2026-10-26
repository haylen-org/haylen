#pragma once

#import <Metal/Metal.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "haylen/gpu/Device.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::gpu::metal {

// The render command encoder of the pass that records, with the state it holds, so the device sends Metal only what changes between draws.
class EncoderState final {
  public:
    // The entries of the argument tables of every Metal GPU, which bind the buffers, textures and samplers of each stage.
    static constexpr std::size_t kBufferSlots = 31;
    static constexpr std::size_t kTextureSlots = 31;
    static constexpr std::size_t kSamplerSlots = 16;
    static constexpr std::size_t kMaxBytes = 4096;

    // Starts recording a pass into an attachment of the size in pixels, with the state Metal gives a new encoder.
    void begin(id<MTLRenderCommandEncoder> value, int passWidth, int passHeight);
    void end();

    [[nodiscard]] id<MTLRenderCommandEncoder> get() const noexcept {
        return encoder;
    }

    [[nodiscard]] int getWidth() const noexcept {
        return width;
    }

    [[nodiscard]] int getHeight() const noexcept {
        return height;
    }

    // Gives the viewport and the scissor back the whole attachment, as a new pass has them.
    void resetViewport();

    void setPipeline(id<MTLRenderPipelineState> state);
    void setDepthStencil(id<MTLDepthStencilState> state);
    void setRasterizer(MTLCullMode cull, MTLWinding winding, float depthBias, float depthBiasSlope);
    void setBuffer(gpu::Device::Stage stage, id<MTLBuffer> buffer, NSUInteger offset, NSUInteger index);

    // Hands Metal a copy of a few bytes for the buffer index, which costs less than a buffer of the upload ring for up to `kMaxBytes` bytes.
    void setBytes(gpu::Device::Stage stage, std::span<const std::byte> data, NSUInteger index);
    void setTexture(gpu::Device::Stage stage, id<MTLTexture> texture, NSUInteger index);
    void setSampler(gpu::Device::Stage stage, id<MTLSamplerState> sampler, NSUInteger index);
    void setViewport(const math::Rect& pixels);

    // Sets the scissor to the pixels of the rectangle inside the attachment, since Metal refuses a scissor that leaves it.
    void setScissor(const math::Rect& pixels);
    void setStencilReference(std::uint32_t value);
    void setBlendColor(math::Color value);

  private:
    void setViewport(const MTLViewport& value);
    void setScissor(const MTLScissorRect& value);

    struct BoundBuffer {
        const void* buffer = nullptr;
        NSUInteger offset = 0;
    };

    struct StageBindings {
        std::array<BoundBuffer, kBufferSlots> buffers{};
        std::array<const void*, kTextureSlots> textures{};
        std::array<const void*, kSamplerSlots> samplers{};
    };

    struct Rasterizer {
        MTLCullMode cull = MTLCullModeNone;
        MTLWinding winding = MTLWindingClockwise;
        float depthBias = 0.0F;
        float depthBiasSlope = 0.0F;
    };

    id<MTLRenderCommandEncoder> encoder = nil;
    int width = 0;
    int height = 0;
    std::array<StageBindings, 2> stages{};
    const void* pipeline = nullptr;
    const void* depthStencil = nullptr;
    Rasterizer rasterizer;
    MTLViewport viewport{};
    MTLScissorRect scissor{};
    std::uint32_t stencilReference = 0;
    math::Color blendColor = math::Color::transparent();
};

} // namespace haylen::gpu::metal
