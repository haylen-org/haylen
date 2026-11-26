#pragma once

#include <webgpu/webgpu.h>

#include <array>
#include <cstdint>
#include <map>
#include <string_view>
#include <utility>
#include <vector>

namespace haylen::gpu::webgpu {

// The mip levels of images, which WebGPU does not generate itself, drawn level by level on the GPU. Each layer of a flat image, an array, a cube or a cube array and each slice of a volume of a smaller level is one pass with a triangle that covers it, whose fragments sample a view of the level above at their middle with the linear filter, so each texel is the mean of the 2 by 2 texels above it, and of the 2 by 2 by 2 texels of a volume. The layer or slice that a pass draws comes from a uniform buffer that holds every index at its own offset. An sRGB image samples and draws through views of its own format, so the filter averages linear values.
class MipmapGenerator final {
  public:
    // The view of the level above a mip level of an image, with the bind group that samples it.
    struct Source {
        WGPUTextureView view = nullptr;
        WGPUBindGroup group = nullptr;
    };

    MipmapGenerator(WGPUDevice owner, std::uint32_t uniformAlignment);
    MipmapGenerator(const MipmapGenerator&) = delete;
    MipmapGenerator& operator=(const MipmapGenerator&) = delete;
    ~MipmapGenerator();

    // Makes the source of a mip level of a texture of the layers given, which the image keeps until it goes, when the caller releases its view and its group.
    [[nodiscard]] Source makeSource(WGPUTexture texture, bool volume, int level, int layers) const;

    // The pipeline that draws the levels of images of the format, made the first time and kept, with the indices of this many layers or slices in the uniform buffer.
    [[nodiscard]] WGPURenderPipeline prepare(WGPUTextureFormat format, bool volume, int count);

    // Records the pass that draws one layer, or one slice of a volume through the depth slice of its attachment, of a mip level from its source.
    void draw(WGPUCommandEncoder encoder, WGPURenderPipeline pipeline, const Source& source, WGPUTextureView target, std::uint32_t depthSlice, int index) const;

    // Destroys the uniform buffers that a larger one replaced, once the frame whose commands read them was submitted.
    void releaseOutgrown() noexcept;

  private:
    // The bytes of the uniform of a pass, the index of its layer or slice in the first four.
    static constexpr std::uint32_t kIndexBytes = 16;

    static constexpr std::string_view kLayersSource = R"(
struct Destination {
    index: u32,
}

@group(0) @binding(0) var above: texture_2d_array<f32>;
@group(0) @binding(1) var aboveSampler: sampler;
@group(1) @binding(0) var<uniform> destination: Destination;

@vertex fn vertexMain(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let corner = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    return vec4f(corner * 2.0 - 1.0, 0.0, 1.0);
}

@fragment fn fragmentMain(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let size = max(textureDimensions(above) >> vec2u(1u), vec2u(1u));
    let place = position.xy / vec2f(size);
    return textureSampleLevel(above, aboveSampler, place, destination.index, 0.0);
}
)";

    static constexpr std::string_view kVolumeSource = R"(
struct Destination {
    index: u32,
}

@group(0) @binding(0) var above: texture_3d<f32>;
@group(0) @binding(1) var aboveSampler: sampler;
@group(1) @binding(0) var<uniform> destination: Destination;

@vertex fn vertexMain(@builtin(vertex_index) index: u32) -> @builtin(position) vec4f {
    let corner = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    return vec4f(corner * 2.0 - 1.0, 0.0, 1.0);
}

@fragment fn fragmentMain(@builtin(position) position: vec4f) -> @location(0) vec4f {
    let size = max(textureDimensions(above) >> vec3u(1u), vec3u(1u));
    let place = vec3f(position.xy, f32(destination.index) + 0.5) / vec3f(size);
    return textureSampleLevel(above, aboveSampler, place, 0.0);
}
)";

    // The module and the layouts of the variant for layers or for volumes.
    struct Variant {
        WGPUShaderModule module = nullptr;
        WGPUBindGroupLayout sources = nullptr;
        WGPUPipelineLayout layout = nullptr;
    };

    [[nodiscard]] Variant makeVariant(std::string_view source, WGPUTextureViewDimension dimension) const;
    void reserveIndices(std::uint32_t count);

    WGPUDevice device;
    std::uint32_t stride;
    WGPUSampler sampler = nullptr;
    WGPUBindGroupLayout indexLayout = nullptr;
    std::array<Variant, 2> variants{};
    std::map<std::pair<WGPUTextureFormat, bool>, WGPURenderPipeline> pipelines;
    WGPUBuffer indices = nullptr;
    WGPUBindGroup indexGroup = nullptr;
    std::uint32_t capacity = 0;
    std::vector<WGPUBuffer> outgrown;
};

} // namespace haylen::gpu::webgpu
