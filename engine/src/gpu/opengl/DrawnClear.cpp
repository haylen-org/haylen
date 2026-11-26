#include "gpu/opengl/DrawnClear.hpp"

namespace haylen::gpu::opengl {

const std::array<Device::BlockBinding, 1> DrawnClear::kBlocks{{
    {.slot = 0, .stage = Device::Stage::Fragment, .size = 32, .glslName = "clear_values"},
}};

// One triangle covers the viewport at the depth of the block, and every output takes the color, so the draw buffers of the framebuffer choose the attachments it reaches.
const Device::ProgramDesc DrawnClear::kProgram{
    .vertex = {.source = "#version 300 es\n"
                         "layout(std140) uniform clear_values {\n"
                         "    vec4 clear_color;\n"
                         "    vec4 clear_depth;\n"
                         "};\n"
                         "void main() {\n"
                         "    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
                         "    gl_Position = vec4(corner * 2.0 - 1.0, clear_depth.x, 1.0);\n"
                         "}\n"},
    .fragment = {.source = "#version 300 es\n"
                           "precision highp float;\n"
                           "layout(std140) uniform clear_values {\n"
                           "    vec4 clear_color;\n"
                           "    vec4 clear_depth;\n"
                           "};\n"
                           "layout(location = 0) out vec4 color0;\n"
                           "layout(location = 1) out vec4 color1;\n"
                           "layout(location = 2) out vec4 color2;\n"
                           "layout(location = 3) out vec4 color3;\n"
                           "void main() {\n"
                           "    color0 = clear_color;\n"
                           "    color1 = clear_color;\n"
                           "    color2 = clear_color;\n"
                           "    color3 = clear_color;\n"
                           "}\n"},
    .blocks = kBlocks,
    .label = "haylen-drawn-clear",
};

DrawnClear::DrawnClear(Gl& functions, StateCache& cache) : gl(functions), state(cache), program(functions, cache, kProgram) {
    gl.genVertexArrays(1, &vertexArray);
    gl.genBuffers(1, &valueBuffer);
    state.bindBuffer(Gl::kUniformBuffer, valueBuffer);
    gl.bufferData(Gl::kUniformBuffer, static_cast<Gl::SizeiPtr>(sizeof(written)), written.data(), Gl::kDynamicDraw);
}

DrawnClear::~DrawnClear() {
    gl.deleteBuffers(1, &valueBuffer);
    state.forgetBuffer(valueBuffer);
    gl.deleteVertexArrays(1, &vertexArray);
    state.forgetVertexArray(vertexArray);
}

void DrawnClear::clear(std::size_t attachment, std::size_t colorCount, const math::Color& color, int width, int height) {
    state.applyRaster({});

    Values values = written;
    values[0] = color.r;
    values[1] = color.g;
    values[2] = color.b;
    values[3] = color.a;
    write(values);

    std::array<Gl::Enum, Device::kMaxColorTargets> buffers{};
    buffers[attachment] = Gl::kColorAttachment0 + static_cast<Gl::Enum>(attachment);
    draw(buffers, colorCount, width, height);
}

// The draw passes the depth test and the stencil test everywhere and replaces what they guard, through no draw buffer, so no color attachment changes.
void DrawnClear::clearDepthStencil(std::optional<float> depth, std::optional<std::uint8_t> stencil, std::size_t colorCount, int width, int height) {
    const StateCache::StencilFace replace{.compare = Gl::kAlways, .fail = Gl::kReplace, .depthFail = Gl::kReplace, .pass = Gl::kReplace};
    StateCache::Raster raster;
    raster.depthTest = depth.has_value();
    raster.depthWrite = depth.has_value();
    raster.stencilTest = stencil.has_value();
    raster.stencilFront = replace;
    raster.stencilBack = replace;
    state.applyRaster(raster);
    state.applyStencilReference(stencil.value_or(std::uint8_t{0}));

    Values values = written;
    values[4] = depth.value_or(0.0F);
    write(values);

    draw({}, colorCount, width, height);
}

void DrawnClear::write(const Values& values) {
    if (values == written) {
        return;
    }

    state.bindBuffer(Gl::kUniformBuffer, valueBuffer);
    gl.bufferSubData(Gl::kUniformBuffer, 0, static_cast<Gl::SizeiPtr>(sizeof(values)), values.data());
    written = values;
}

// The draw reaches only the attachments that its draw buffers name, and the framebuffer gets back the draw buffer of every color attachment.
void DrawnClear::draw(const std::array<Gl::Enum, Device::kMaxColorTargets>& buffers, std::size_t colorCount, int width, int height) {
    state.useProgram(program.getName());
    state.setViewport({.x = 0, .y = 0, .width = width, .height = height});
    state.setScissorTest(false);
    state.bindVertexArray(vertexArray);

    const auto point = static_cast<std::size_t>(kBlocks[0].slot);
    state.bindUniformRange(point, {.buffer = valueBuffer, .offset = 0, .size = static_cast<Gl::SizeiPtr>(sizeof(written))});

    gl.drawBuffers(static_cast<Gl::Sizei>(colorCount), buffers.data());
    gl.drawArraysInstanced(Gl::kTriangles, 0, 3, 1);

    std::array<Gl::Enum, Device::kMaxColorTargets> every{};
    for (std::size_t index = 0; index < colorCount; ++index) {
        every[index] = Gl::kColorAttachment0 + static_cast<Gl::Enum>(index);
    }

    gl.drawBuffers(static_cast<Gl::Sizei>(colorCount), every.data());
}

} // namespace haylen::gpu::opengl
