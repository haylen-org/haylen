#pragma once

#include <array>
#include <cstddef>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/ShaderProgram.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::gpu::opengl {

// Clears a color attachment of the bound draw framebuffer by drawing the color over it, for an OpenGL ES driver whose clears reach only the first layer of an array or a volume texture while its draws reach the layer the framebuffer attaches.
class LayerClear final {
  public:
    LayerClear(Gl& functions, StateCache& cache);
    LayerClear(const LayerClear&) = delete;
    LayerClear& operator=(const LayerClear&) = delete;
    ~LayerClear();

    // Draws the color into the attachment at the index of the framebuffer, which has the count of color attachments and the size in pixels, and leaves its other attachments as they were.
    void clear(std::size_t attachment, std::size_t colorCount, const math::Color& color, int width, int height);

  private:
    static_assert(Device::kMaxColorTargets == 4, "The fragment stage of the layer clear writes one output for each color attachment of a pass.");

    static const std::array<Device::BlockBinding, 1> kBlocks;
    static const Device::ProgramDesc kProgram;

    Gl& gl;
    StateCache& state;
    ShaderProgram program;
    Gl::Uint vertexArray = 0;
};

} // namespace haylen::gpu::opengl
