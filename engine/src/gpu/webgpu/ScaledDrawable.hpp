#pragma once

#include <webgpu/webgpu.h>

#include <string_view>

#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::webgpu {

// The drawable of a canvas that the page shows at a larger size, as when the app caps its pixel density. The page sizes the drawing buffer of the canvas after the window, so the swapchain passes draw into a texture of the size of the drawable, which `record` draws over the texture of the canvas, stretched with the linear filter, since WebGPU has no copy that scales.
class ScaledDrawable final {
  public:
    ScaledDrawable(WGPUDevice owner, WGPUTextureFormat canvasFormat, const Surface::Size& size);
    ScaledDrawable(const ScaledDrawable&) = delete;
    ScaledDrawable& operator=(const ScaledDrawable&) = delete;
    ~ScaledDrawable();

    // Makes the texture of the drawable again for a new size.
    void resize(const Surface::Size& value);

    [[nodiscard]] const Surface::Size& getSize() const noexcept {
        return size;
    }

    [[nodiscard]] WGPUTextureView getView() const noexcept {
        return view;
    }

    // Records a pass that draws the drawable over the texture of the canvas of the frame.
    void record(WGPUCommandEncoder encoder, WGPUTextureView canvas) const;

  private:
    static constexpr std::string_view kSource = R"(
struct Corner {
    @builtin(position) position: vec4f,
    @location(0) place: vec2f,
}

@group(0) @binding(0) var drawable: texture_2d<f32>;
@group(0) @binding(1) var drawableSampler: sampler;

@vertex fn vertexMain(@builtin(vertex_index) index: u32) -> Corner {
    let corner = vec2f(f32((index << 1u) & 2u), f32(index & 2u));
    var made: Corner;
    made.position = vec4f(corner * 2.0 - 1.0, 0.0, 1.0);
    made.place = vec2f(corner.x, 1.0 - corner.y);
    return made;
}

@fragment fn fragmentMain(corner: Corner) -> @location(0) vec4f {
    return textureSample(drawable, drawableSampler, corner.place);
}
)";

    void makeTexture();
    void releaseTexture() noexcept;

    WGPUDevice device;
    WGPUTextureFormat format;
    Surface::Size size;
    WGPUShaderModule module = nullptr;
    WGPURenderPipeline pipeline = nullptr;
    WGPUSampler sampler = nullptr;
    WGPUTexture texture = nullptr;
    WGPUTextureView view = nullptr;
    WGPUBindGroup group = nullptr;
};

} // namespace haylen::gpu::webgpu
