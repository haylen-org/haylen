#include "gpu/opengl/DrawnResolve.hpp"

namespace haylen::gpu::opengl {

const std::array<Device::TextureBinding, 1> DrawnResolve::kTextures{{{.slot = 0}}};
const std::array<Device::SamplerBinding, 1> DrawnResolve::kSamplers{{{.slot = 0}}};
const std::array<Device::TexturePair, 1> DrawnResolve::kPairs{{{.texture = 0, .sampler = 0, .glslName = "resolved"}}};

// One triangle covers the viewport, and each pixel takes the texel at its own place, since the flat texture and the layer both keep their bottom row first.
const Device::ProgramDesc DrawnResolve::kProgram{
    .vertex = {.source = "#version 300 es\n"
                         "void main() {\n"
                         "    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
                         "    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
                         "}\n"},
    .fragment = {.source = "#version 300 es\n"
                           "precision highp float;\n"
                           "uniform highp sampler2D resolved;\n"
                           "layout(location = 0) out vec4 color0;\n"
                           "void main() {\n"
                           "    color0 = texelFetch(resolved, ivec2(gl_FragCoord.xy), 0);\n"
                           "}\n"},
    .textures = kTextures,
    .samplers = kSamplers,
    .pairs = kPairs,
    .label = "haylen-drawn-resolve",
};

DrawnResolve::DrawnResolve(Gl& functions, StateCache& cache) : gl(functions), state(cache), program(functions, cache, kProgram) {
    gl.genVertexArrays(1, &vertexArray);
}

DrawnResolve::~DrawnResolve() {
    release();
    gl.deleteVertexArrays(1, &vertexArray);
    state.forgetVertexArray(vertexArray);
}

void DrawnResolve::resolve(Gl::Uint source, std::size_t attachment, Gl::Uint destination, Gl::Enum format, int width, int height) {
    prepare(format, width, height);

    state.prepareClear();
    state.bindReadFramebuffer(source);
    gl.readBuffer(Gl::kColorAttachment0 + static_cast<Gl::Enum>(attachment));
    state.bindDrawFramebuffer(framebuffer);
    gl.blitFramebuffer(0, 0, width, height, 0, 0, width, height, Gl::kColorBufferBit, Gl::kNearest);

    // The draw fetches the texels of the flat texture through the first unit without a sampler, since a fetch takes no filter.
    state.bindDrawFramebuffer(destination);
    state.useProgram(program.getName());
    state.applyRaster({});
    state.setViewport({.x = 0, .y = 0, .width = width, .height = height});
    state.setScissorTest(false);
    state.bindVertexArray(vertexArray);
    state.bindTexture(0, Gl::kTexture2d, texture);
    state.bindSampler(0, 0);
    gl.drawArraysInstanced(Gl::kTriangles, 0, 3, 1);
}

// The flat texture keeps the format and the size of the last resolve, and a resolve of another format or size makes it again, since its storage is immutable. It names its last level itself, as the textures of the device do, so a driver that stores immutable textures without their level count reads it as complete.
void DrawnResolve::prepare(Gl::Enum format, int width, int height) {
    if (texture != 0 && format == textureFormat && width == textureWidth && height == textureHeight) {
        return;
    }

    release();
    gl.genTextures(1, &texture);
    state.bindTexture(0, Gl::kTexture2d, texture);
    gl.texStorage2D(Gl::kTexture2d, 1, format, width, height);
    gl.texParameteri(Gl::kTexture2d, Gl::kTextureMaxLevel, 0);

    gl.genFramebuffers(1, &framebuffer);
    state.bindDrawFramebuffer(framebuffer);
    gl.framebufferTexture2D(Gl::kDrawFramebuffer, Gl::kColorAttachment0, Gl::kTexture2d, texture, 0);

    textureFormat = format;
    textureWidth = width;
    textureHeight = height;
}

void DrawnResolve::release() noexcept {
    if (texture == 0) {
        return;
    }

    gl.deleteFramebuffers(1, &framebuffer);
    state.forgetFramebuffer(framebuffer);
    gl.deleteTextures(1, &texture);
    state.forgetTexture(texture);
    framebuffer = 0;
    texture = 0;
}

} // namespace haylen::gpu::opengl
