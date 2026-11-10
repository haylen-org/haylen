#include "gpu/opengl/ScaledDrawable.hpp"

#include <stdexcept>

namespace haylen::gpu::opengl {

const std::array<Device::TextureBinding, 1> ScaledDrawable::kTextures{{{.slot = 0}}};
const std::array<Device::SamplerBinding, 1> ScaledDrawable::kSamplers{{{.slot = 0}}};
const std::array<Device::TexturePair, 1> ScaledDrawable::kPairs{{{.texture = 0, .sampler = 0, .glslName = "drawable"}}};

// One triangle covers the viewport and reads the drawable at the same place, since the drawable and the default framebuffer both keep their bottom row first.
const Device::ProgramDesc ScaledDrawable::kProgram{
    .vertex = {.source = "#version 300 es\n"
                         "out vec2 place;\n"
                         "void main() {\n"
                         "    vec2 corner = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));\n"
                         "    place = corner;\n"
                         "    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);\n"
                         "}\n"},
    .fragment = {.source = "#version 300 es\n"
                           "precision highp float;\n"
                           "uniform highp sampler2D drawable;\n"
                           "in vec2 place;\n"
                           "layout(location = 0) out vec4 color0;\n"
                           "void main() {\n"
                           "    color0 = texture(drawable, place);\n"
                           "}\n"},
    .textures = kTextures,
    .samplers = kSamplers,
    .pairs = kPairs,
    .label = "haylen-scaled-drawable",
};

ScaledDrawable::ScaledDrawable(Gl& functions, StateCache& cache, const Surface::Size& value, int sampleCount) : gl(functions), state(cache), program(functions, cache, kProgram), size(value), samples(sampleCount) {
    gl.genVertexArrays(1, &vertexArray);
    gl.genSamplers(1, &sampler);
    gl.samplerParameteri(sampler, Gl::kTextureMinFilter, static_cast<Gl::Int>(Gl::kLinear));
    gl.samplerParameteri(sampler, Gl::kTextureMagFilter, static_cast<Gl::Int>(Gl::kLinear));
    gl.samplerParameteri(sampler, Gl::kTextureWrapS, static_cast<Gl::Int>(Gl::kClampToEdge));
    gl.samplerParameteri(sampler, Gl::kTextureWrapT, static_cast<Gl::Int>(Gl::kClampToEdge));
    makeTargets();
}

ScaledDrawable::~ScaledDrawable() {
    destroyTargets();
    gl.deleteSamplers(1, &sampler);
    state.forgetSampler(sampler);
    gl.deleteVertexArrays(1, &vertexArray);
    state.forgetVertexArray(vertexArray);
}

void ScaledDrawable::resize(const Surface::Size& value) {
    if (value == size) {
        return;
    }
    destroyTargets();
    size = value;
    makeTargets();
}

// A window of one sample draws straight into the texture that `present` reads, and a multisampled one resolves into it first.
void ScaledDrawable::makeTargets() {
    gl.genTextures(1, &texture);
    state.bindTexture(0, Gl::kTexture2d, texture);
    gl.texStorage2D(Gl::kTexture2d, 1, Gl::kRgba8, size.width, size.height);
    depthStencil = makeRenderbuffer(Gl::kDepth24Stencil8);

    gl.genFramebuffers(1, &framebuffer);
    state.bindDrawFramebuffer(framebuffer);
    if (samples > 1) {
        colorBuffer = makeRenderbuffer(Gl::kRgba8);
        gl.framebufferRenderbuffer(Gl::kDrawFramebuffer, Gl::kColorAttachment0, Gl::kRenderbuffer, colorBuffer);
    } else {
        gl.framebufferTexture2D(Gl::kDrawFramebuffer, Gl::kColorAttachment0, Gl::kTexture2d, texture, 0);
    }
    gl.framebufferRenderbuffer(Gl::kDrawFramebuffer, Gl::kDepthStencilAttachment, Gl::kRenderbuffer, depthStencil);
    if (gl.checkFramebufferStatus(Gl::kDrawFramebuffer) != Gl::kFramebufferComplete) {
        throw std::runtime_error("The OpenGL device cannot draw into a framebuffer of the size of the drawable, so it cannot show the window with fewer pixels than it has.");
    }

    if (samples > 1) {
        gl.genFramebuffers(1, &resolveFramebuffer);
        state.bindDrawFramebuffer(resolveFramebuffer);
        gl.framebufferTexture2D(Gl::kDrawFramebuffer, Gl::kColorAttachment0, Gl::kTexture2d, texture, 0);
    }
}

Gl::Uint ScaledDrawable::makeRenderbuffer(Gl::Enum format) const {
    Gl::Uint name = 0;
    gl.genRenderbuffers(1, &name);
    gl.bindRenderbuffer(Gl::kRenderbuffer, name);
    gl.renderbufferStorageMultisample(Gl::kRenderbuffer, samples > 1 ? samples : 0, format, size.width, size.height);
    gl.bindRenderbuffer(Gl::kRenderbuffer, 0);
    return name;
}

void ScaledDrawable::destroyTargets() noexcept {
    for (Gl::Uint* made : {&framebuffer, &resolveFramebuffer}) {
        if (*made != 0) {
            gl.deleteFramebuffers(1, made);
            state.forgetFramebuffer(*made);
            *made = 0;
        }
    }
    for (Gl::Uint* made : {&colorBuffer, &depthStencil}) {
        if (*made != 0) {
            gl.deleteRenderbuffers(1, made);
            *made = 0;
        }
    }
    if (texture != 0) {
        gl.deleteTextures(1, &texture);
        state.forgetTexture(texture);
        texture = 0;
    }
}

void ScaledDrawable::present() {
    if (samples > 1) {
        state.prepareClear();
        state.bindReadFramebuffer(framebuffer);
        state.bindDrawFramebuffer(resolveFramebuffer);
        gl.blitFramebuffer(0, 0, size.width, size.height, 0, 0, size.width, size.height, Gl::kColorBufferBit, Gl::kNearest);
    }

    state.bindDrawFramebuffer(0);
    state.useProgram(program.getName());
    state.applyRaster({});
    state.setViewport({.x = 0, .y = 0, .width = size.windowWidth, .height = size.windowHeight});
    state.setScissorTest(false);
    state.bindVertexArray(vertexArray);
    state.bindTexture(0, Gl::kTexture2d, texture);
    state.bindSampler(0, sampler);
    gl.drawArraysInstanced(Gl::kTriangles, 0, 3, 1);
}

} // namespace haylen::gpu::opengl
