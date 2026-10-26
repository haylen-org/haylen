#include "gpu/metal/EncoderState.hpp"

#include <algorithm>
#include <cmath>

namespace haylen::gpu::metal {

void EncoderState::begin(id<MTLRenderCommandEncoder> value, int passWidth, int passHeight) {
    encoder = value;
    width = passWidth;
    height = passHeight;
    stages = {};
    pipeline = nullptr;
    depthStencil = nullptr;
    rasterizer = {};
    viewport = {.originX = 0.0, .originY = 0.0, .width = static_cast<double>(width), .height = static_cast<double>(height), .znear = 0.0, .zfar = 1.0};
    scissor = {.x = 0, .y = 0, .width = static_cast<NSUInteger>(width), .height = static_cast<NSUInteger>(height)};
    stencilReference = 0;
    blendColor = math::Color::transparent();
}

void EncoderState::end() {
    @autoreleasepool {
        [encoder endEncoding];
        encoder = nil;
    }
}

void EncoderState::resetViewport() {
    setViewport(MTLViewport{.originX = 0.0, .originY = 0.0, .width = static_cast<double>(width), .height = static_cast<double>(height), .znear = 0.0, .zfar = 1.0});
    setScissor(MTLScissorRect{.x = 0, .y = 0, .width = static_cast<NSUInteger>(width), .height = static_cast<NSUInteger>(height)});
}

void EncoderState::setPipeline(id<MTLRenderPipelineState> state) {
    const void* identity = (__bridge const void*)state;
    if (identity != pipeline) {
        pipeline = identity;
        [encoder setRenderPipelineState:state];
    }
}

void EncoderState::setDepthStencil(id<MTLDepthStencilState> state) {
    const void* identity = (__bridge const void*)state;
    if (identity != depthStencil) {
        depthStencil = identity;
        [encoder setDepthStencilState:state];
    }
}

void EncoderState::setRasterizer(MTLCullMode cull, MTLWinding winding, float depthBias, float depthBiasSlope) {
    if (cull != rasterizer.cull) {
        rasterizer.cull = cull;
        [encoder setCullMode:cull];
    }
    if (winding != rasterizer.winding) {
        rasterizer.winding = winding;
        [encoder setFrontFacingWinding:winding];
    }
    if (depthBias != rasterizer.depthBias || depthBiasSlope != rasterizer.depthBiasSlope) {
        rasterizer.depthBias = depthBias;
        rasterizer.depthBiasSlope = depthBiasSlope;
        [encoder setDepthBias:depthBias slopeScale:depthBiasSlope clamp:0.0F];
    }
}

// A buffer that stays bound at the index moves to the new offset alone, which costs Metal less than binding it again.
void EncoderState::setBuffer(gpu::Device::Stage stage, id<MTLBuffer> buffer, NSUInteger offset, NSUInteger index) {
    BoundBuffer& bound = stages[static_cast<std::size_t>(stage)].buffers[index];
    const void* identity = (__bridge const void*)buffer;
    if (identity == bound.buffer && offset == bound.offset) {
        return;
    }

    const bool moved = identity == bound.buffer;
    bound = {.buffer = identity, .offset = offset};
    if (stage == gpu::Device::Stage::Vertex) {
        if (moved) {
            [encoder setVertexBufferOffset:offset atIndex:index];
        } else {
            [encoder setVertexBuffer:buffer offset:offset atIndex:index];
        }
        return;
    }
    if (moved) {
        [encoder setFragmentBufferOffset:offset atIndex:index];
    } else {
        [encoder setFragmentBuffer:buffer offset:offset atIndex:index];
    }
}

// The copy takes the index from whatever buffer the encoder bound there, so the next buffer at the index binds again.
void EncoderState::setBytes(gpu::Device::Stage stage, std::span<const std::byte> data, NSUInteger index) {
    stages[static_cast<std::size_t>(stage)].buffers[index] = {};
    if (stage == gpu::Device::Stage::Vertex) {
        [encoder setVertexBytes:data.data() length:data.size() atIndex:index];
    } else {
        [encoder setFragmentBytes:data.data() length:data.size() atIndex:index];
    }
}

void EncoderState::setTexture(gpu::Device::Stage stage, id<MTLTexture> texture, NSUInteger index) {
    const void*& bound = stages[static_cast<std::size_t>(stage)].textures[index];
    const void* identity = (__bridge const void*)texture;
    if (identity == bound) {
        return;
    }
    bound = identity;
    if (stage == gpu::Device::Stage::Vertex) {
        [encoder setVertexTexture:texture atIndex:index];
    } else {
        [encoder setFragmentTexture:texture atIndex:index];
    }
}

void EncoderState::setSampler(gpu::Device::Stage stage, id<MTLSamplerState> sampler, NSUInteger index) {
    const void*& bound = stages[static_cast<std::size_t>(stage)].samplers[index];
    const void* identity = (__bridge const void*)sampler;
    if (identity == bound) {
        return;
    }
    bound = identity;
    if (stage == gpu::Device::Stage::Vertex) {
        [encoder setVertexSamplerState:sampler atIndex:index];
    } else {
        [encoder setFragmentSamplerState:sampler atIndex:index];
    }
}

void EncoderState::setViewport(const math::Rect& pixels) {
    setViewport({.originX = pixels.x, .originY = pixels.y, .width = pixels.width, .height = pixels.height, .znear = 0.0, .zfar = 1.0});
}

void EncoderState::setScissor(const math::Rect& pixels) {
    const long left = std::clamp(std::lround(pixels.x), 0L, static_cast<long>(width));
    const long top = std::clamp(std::lround(pixels.y), 0L, static_cast<long>(height));
    const long right = std::clamp(std::lround(pixels.x + pixels.width), left, static_cast<long>(width));
    const long bottom = std::clamp(std::lround(pixels.y + pixels.height), top, static_cast<long>(height));
    setScissor(MTLScissorRect{.x = static_cast<NSUInteger>(left), .y = static_cast<NSUInteger>(top), .width = static_cast<NSUInteger>(right - left), .height = static_cast<NSUInteger>(bottom - top)});
}

void EncoderState::setViewport(const MTLViewport& value) {
    if (value.originX == viewport.originX && value.originY == viewport.originY && value.width == viewport.width && value.height == viewport.height) {
        return;
    }
    viewport = value;
    [encoder setViewport:value];
}

void EncoderState::setScissor(const MTLScissorRect& value) {
    if (value.x == scissor.x && value.y == scissor.y && value.width == scissor.width && value.height == scissor.height) {
        return;
    }
    scissor = value;
    [encoder setScissorRect:value];
}

void EncoderState::setStencilReference(std::uint32_t value) {
    if (value != stencilReference) {
        stencilReference = value;
        [encoder setStencilReferenceValue:value];
    }
}

void EncoderState::setBlendColor(math::Color value) {
    if (value.r == blendColor.r && value.g == blendColor.g && value.b == blendColor.b && value.a == blendColor.a) {
        return;
    }
    blendColor = value;
    [encoder setBlendColorRed:value.r green:value.g blue:value.b alpha:value.a];
}

} // namespace haylen::gpu::metal
