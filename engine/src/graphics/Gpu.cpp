#include "graphics/Gpu.hpp"

#include <chrono>
#include <format>
#include <stdexcept>

#include "haylen/core/Log.hpp"

namespace haylen::graphics {

double Gpu::millisecondsSince(std::chrono::steady_clock::time_point start) noexcept {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

sg_image Gpu::makeImage(const sg_image_desc& desc) {
    const sg_image image = sg_make_image(&desc);
    if (image.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another texture. At most {} textures and render targets can exist at once.", kImagePoolSize));
    }
    if (sg_query_image_state(image) != SG_RESOURCESTATE_VALID) {
        sg_destroy_image(image);
        throw std::runtime_error(std::format("The graphics device could not create a {}x{} texture.", desc.width, desc.height));
    }
    return image;
}

sg_buffer Gpu::makeBuffer(const sg_buffer_desc& desc) {
    const sg_buffer buffer = sg_make_buffer(&desc);
    if (buffer.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another buffer. At most {} baked sprite batches and draw buffers can exist at once.", kBufferPoolSize));
    }
    if (sg_query_buffer_state(buffer) != SG_RESOURCESTATE_VALID) {
        sg_destroy_buffer(buffer);
        throw std::runtime_error(std::format("The graphics device could not create a buffer of {} bytes.", desc.size == 0 ? desc.data.size : desc.size));
    }
    return buffer;
}

// Backends compile shader sources when the program is made, which the debug log times.
sg_shader Gpu::makeShader(const sg_shader_desc& desc) {
    const auto started = std::chrono::steady_clock::now();
    const sg_shader shader = sg_make_shader(&desc);
    if (shader.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another shader. At most {} shader programs can exist at once.", kShaderPoolSize));
    }
    if (sg_query_shader_state(shader) != SG_RESOURCESTATE_VALID) {
        sg_destroy_shader(shader);
        throw std::runtime_error(std::format("The graphics device could not create the shader \"{}\".", desc.label));
    }
    core::Log::debug("The graphics device made the shader \"{}\" in {:.2f} ms.", desc.label, millisecondsSince(started));
    return shader;
}

sg_pipeline Gpu::makePipeline(const sg_pipeline_desc& desc) {
    const auto started = std::chrono::steady_clock::now();
    const sg_pipeline pipeline = sg_make_pipeline(&desc);
    if (pipeline.id == SG_INVALID_ID) {
        throw std::runtime_error(std::format("The graphics device has no room for another pipeline. At most {} pipelines can exist at once.", kPipelinePoolSize));
    }
    if (sg_query_pipeline_state(pipeline) != SG_RESOURCESTATE_VALID) {
        sg_destroy_pipeline(pipeline);
        throw std::runtime_error(std::format("The graphics device could not create the pipeline \"{}\".", desc.label));
    }
    core::Log::debug("The graphics device made the pipeline \"{}\" in {:.2f} ms.", desc.label, millisecondsSince(started));
    return pipeline;
}

const sg_shader_desc* Gpu::selectShader(const sg_shader_desc* (*description)(sg_backend)) {
    const sg_backend backend = sg_query_backend();
    return description(backend == SG_BACKEND_DUMMY ? SG_BACKEND_GLCORE : backend);
}

} // namespace haylen::graphics
