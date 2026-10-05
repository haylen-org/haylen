#pragma once

#include <chrono>

#include "sokol_gfx.h"

namespace haylen::graphics {

// Creates Sokol GPU objects with clear errors and picks the shaders of the active backend.
class Gpu final {
  public:
    // Sokol preallocates its resource pools, so these sizes bound how many textures, render targets, baked batches, shaders and pipelines exist at once.
    static constexpr int kImagePoolSize = 4096;
    static constexpr int kViewPoolSize = 2 * kImagePoolSize;
    static constexpr int kBufferPoolSize = 4096;
    static constexpr int kShaderPoolSize = 512;
    static constexpr int kPipelinePoolSize = 2048;

    // Throw a clear error when a pool is full or the backend rejects the object. Errors name a shader or a pipeline by the label of its description.
    [[nodiscard]] static sg_image makeImage(const sg_image_desc& desc);
    [[nodiscard]] static sg_buffer makeBuffer(const sg_buffer_desc& desc);
    [[nodiscard]] static sg_shader makeShader(const sg_shader_desc& desc);
    [[nodiscard]] static sg_pipeline makePipeline(const sg_pipeline_desc& desc);

    // Selects the generated shader description for the active backend. The dummy backend used by tests compiles no shaders, so any description serves.
    [[nodiscard]] static const sg_shader_desc* selectShader(const sg_shader_desc* (*description)(sg_backend));

  private:
    [[nodiscard]] static double millisecondsSince(std::chrono::steady_clock::time_point start) noexcept;
};

} // namespace haylen::graphics
