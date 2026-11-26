#pragma once

#include <array>
#include <cstddef>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/ShaderProgram.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::gpu::opengl {

// Clears a color attachment of the bound draw framebuffer by drawing the color over every sample of it, for an OpenGL ES driver whose clears miss parts of an attachment that its draws reach, such as a later layer of an array or the other samples of a multisampled image.
class DrawnClear final {
  public:
    DrawnClear(Gl& functions, StateCache& cache);
    DrawnClear(const DrawnClear&) = delete;
    DrawnClear& operator=(const DrawnClear&) = delete;
    ~DrawnClear();

    // Draws the color into the attachment at the index of the framebuffer, which has the count of color attachments and the size in pixels, and leaves its other attachments as they were.
    void clear(std::size_t attachment, std::size_t colorCount, const math::Color& color, int width, int height);

  private:
    static_assert(Device::kMaxColorTargets == 4, "The fragment stage of the drawn clear writes one output for each color attachment of a pass.");

    static const std::array<Device::BlockBinding, 1> kBlocks;
    static const Device::ProgramDesc kProgram;

    Gl& gl;
    StateCache& state;
    ShaderProgram program;
    Gl::Uint vertexArray = 0;
};

} // namespace haylen::gpu::opengl
