#include "gpu/opengl/DrawnClear.hpp"

namespace haylen::gpu::opengl {

const std::array<Device::BlockBinding, 1> DrawnClear::kBlocks{{
    {.slot = 0, .stage = Device::Stage::Fragment, .size = 16, .glslName = "clear_color"},
}};

// One triangle covers the viewport, and every output takes the color, so the draw buffers of the framebuffer choose the attachment it reaches.
const Device::ProgramDesc DrawnClear::kProgram{
    .vertex = {.source = "#version 300 es\n"
                         "void main() {\n"
                         "    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
                         "    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
                         "}\n"},
    .fragment = {.source = "#version 300 es\n"
                           "precision highp float;\n"
                           "layout(std140) uniform clear_color {\n"
                           "    vec4 clear_value;\n"
                           "};\n"
                           "layout(location = 0) out vec4 color0;\n"
                           "layout(location = 1) out vec4 color1;\n"
                           "layout(location = 2) out vec4 color2;\n"
                           "layout(location = 3) out vec4 color3;\n"
                           "void main() {\n"
                           "    color0 = clear_value;\n"
                           "    color1 = clear_value;\n"
                           "    color2 = clear_value;\n"
                           "    color3 = clear_value;\n"
                           "}\n"},
    .blocks = kBlocks,
    .label = "haylen-drawn-clear",
};

DrawnClear::DrawnClear(Gl& functions, StateCache& cache) : gl(functions), state(cache), program(functions, cache, kProgram) {
    gl.genVertexArrays(1, &vertexArray);
    gl.genBuffers(1, &colorBuffer);
    state.bindBuffer(Gl::kUniformBuffer, colorBuffer);
    gl.bufferData(Gl::kUniformBuffer, static_cast<Gl::SizeiPtr>(sizeof(written)), written.data(), Gl::kDynamicDraw);
}

DrawnClear::~DrawnClear() {
    gl.deleteBuffers(1, &colorBuffer);
    state.forgetBuffer(colorBuffer);
    gl.deleteVertexArrays(1, &vertexArray);
    state.forgetVertexArray(vertexArray);
}

void DrawnClear::clear(std::size_t attachment, std::size_t colorCount, const math::Color& color, int width, int height) {
    state.useProgram(program.getName());
    state.applyRaster({});
    state.setViewport({.x = 0, .y = 0, .width = width, .height = height});
    state.setScissorTest(false);
    state.bindVertexArray(vertexArray);

    const std::array<float, 4> value{color.r, color.g, color.b, color.a};
    if (value != written) {
        state.bindBuffer(Gl::kUniformBuffer, colorBuffer);
        gl.bufferSubData(Gl::kUniformBuffer, 0, static_cast<Gl::SizeiPtr>(sizeof(value)), value.data());
        written = value;
    }

    const auto point = static_cast<std::size_t>(kBlocks[0].slot);
    state.bindUniformRange(point, {.buffer = colorBuffer, .offset = 0, .size = static_cast<Gl::SizeiPtr>(sizeof(written))});

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
