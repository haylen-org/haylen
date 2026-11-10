#pragma once

#include <array>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/ShaderProgram.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::opengl {

// The drawable of a surface that its window shows at a larger size, as when the app caps its pixel density. The default framebuffer of a window keeps the size of the window, so the swapchain passes of the surface draw into a framebuffer of the size of the drawable with the samples of the window, which `present` draws over the default framebuffer, stretched with the linear filter. A drawing rather than a blit scales it, since OpenGL blits into a multisampled framebuffer only at the same size.
class ScaledDrawable final {
  public:
    ScaledDrawable(Gl& functions, StateCache& cache, const Surface::Size& size, int sampleCount);
    ScaledDrawable(const ScaledDrawable&) = delete;
    ScaledDrawable& operator=(const ScaledDrawable&) = delete;
    ~ScaledDrawable();

    // Makes the framebuffer of the drawable again for a new size.
    void resize(const Surface::Size& value);

    [[nodiscard]] const Surface::Size& getSize() const noexcept {
        return size;
    }

    [[nodiscard]] Gl::Uint getFramebuffer() const noexcept {
        return framebuffer;
    }

    // Draws what the passes drew over the default framebuffer of the current drawable, at the size of its window.
    void present();

  private:
    static const std::array<Device::TextureBinding, 1> kTextures;
    static const std::array<Device::SamplerBinding, 1> kSamplers;
    static const std::array<Device::TexturePair, 1> kPairs;
    static const Device::ProgramDesc kProgram;

    void makeTargets();
    void destroyTargets() noexcept;
    [[nodiscard]] Gl::Uint makeRenderbuffer(Gl::Enum format) const;

    Gl& gl;
    StateCache& state;
    ShaderProgram program;
    Surface::Size size;
    int samples = 1;
    Gl::Uint vertexArray = 0;
    Gl::Uint sampler = 0;
    Gl::Uint framebuffer = 0;
    Gl::Uint colorBuffer = 0;
    Gl::Uint depthStencil = 0;
    Gl::Uint resolveFramebuffer = 0;
    Gl::Uint texture = 0;
};

} // namespace haylen::gpu::opengl
