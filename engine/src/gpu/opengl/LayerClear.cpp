#include "gpu/opengl/LayerClear.hpp"

#include <span>

namespace haylen::gpu::opengl {

const std::array<Device::BlockBinding, 1> LayerClear::kBlocks{{
    {.slot = 0, .stage = Device::Stage::Fragment, .size = 16, .glslName = "clear_color", .glslCount = 1},
}};

// One triangle covers the viewport, and every output takes the color, so the draw buffers of the framebuffer choose the attachment it reaches.
const Device::ProgramDesc LayerClear::kProgram{
    .vertex = {.source = "#version 300 es\n"
                         "void main() {\n"
                         "    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
                         "    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
                         "}\n"},
    .fragment = {.source = "#version 300 es\n"
                           "precision highp float;\n"
                           "uniform vec4 clear_color[1];\n"
                           "layout(location = 0) out vec4 color0;\n"
                           "layout(location = 1) out vec4 color1;\n"
                           "layout(location = 2) out vec4 color2;\n"
                           "layout(location = 3) out vec4 color3;\n"
                           "void main() {\n"
                           "    color0 = clear_color[0];\n"
                           "    color1 = clear_color[0];\n"
                           "    color2 = clear_color[0];\n"
                           "    color3 = clear_color[0];\n"
                           "}\n"},
    .blocks = kBlocks,
    .label = "haylen-layer-clear",
};

LayerClear::LayerClear(Gl& functions, StateCache& cache) : gl(functions), state(cache), program(functions, cache, kProgram) {
    gl.genVertexArrays(1, &vertexArray);
}

LayerClear::~LayerClear() {
    gl.deleteVertexArrays(1, &vertexArray);
    state.forgetVertexArray(vertexArray);
}

void LayerClear::clear(std::size_t attachment, std::size_t colorCount, const math::Color& color, int width, int height) {
    state.useProgram(program.getName());
    state.applyRaster({});
    state.setViewport({.x = 0, .y = 0, .width = width, .height = height});
    state.setScissorTest(false);
    state.bindVertexArray(vertexArray);
    const std::array<float, 4> value{color.r, color.g, color.b, color.a};
    program.applyUniforms(0, std::as_bytes(std::span(value)));

    // The draw reaches only the attachment that its draw buffer names, and the framebuffer gets back the draw buffer of every attachment.
    std::array<Gl::Enum, Device::kMaxColorTargets> buffers{};
    buffers[attachment] = Gl::kColorAttachment0 + static_cast<Gl::Enum>(attachment);
    gl.drawBuffers(static_cast<Gl::Sizei>(colorCount), buffers.data());
    gl.drawArraysInstanced(Gl::kTriangles, 0, 3, 1);
    for (std::size_t index = 0; index < colorCount; ++index) {
        buffers[index] = Gl::kColorAttachment0 + static_cast<Gl::Enum>(index);
    }
    gl.drawBuffers(static_cast<Gl::Sizei>(colorCount), buffers.data());
}

} // namespace haylen::gpu::opengl
