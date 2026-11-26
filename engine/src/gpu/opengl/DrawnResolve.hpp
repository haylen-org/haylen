#pragma once

#include <array>
#include <cstddef>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/ShaderProgram.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// Resolves a multisampled color attachment into a later layer of an array or a cube array by drawing, for an OpenGL ES driver whose blits miss the layers past the first that a framebuffer attaches while its draws reach them. A blit resolves the attachment into a flat texture of its format and size, and a draw copies that texture texel by texel into the layer. The flat texture stays for the next resolve of the same format and size.
class DrawnResolve final {
  public:
    DrawnResolve(Gl& functions, StateCache& cache);
    DrawnResolve(const DrawnResolve&) = delete;
    DrawnResolve& operator=(const DrawnResolve&) = delete;
    ~DrawnResolve();

    // Resolves the color attachment at the index of the multisampled framebuffer into the framebuffer that attaches the layer as its one color attachment, both of the size in pixels and of the internal format.
    void resolve(Gl::Uint source, std::size_t attachment, Gl::Uint destination, Gl::Enum format, int width, int height);

  private:
    static const std::array<Device::TextureBinding, 1> kTextures;
    static const std::array<Device::SamplerBinding, 1> kSamplers;
    static const std::array<Device::TexturePair, 1> kPairs;
    static const Device::ProgramDesc kProgram;

    void prepare(Gl::Enum format, int width, int height);
    void release() noexcept;

    Gl& gl;
    StateCache& state;
    ShaderProgram program;
    Gl::Uint vertexArray = 0;
    Gl::Uint texture = 0;
    Gl::Uint framebuffer = 0;
    Gl::Enum textureFormat = Gl::kNone;
    int textureWidth = 0;
    int textureHeight = 0;
};

} // namespace haylen::gpu::opengl
