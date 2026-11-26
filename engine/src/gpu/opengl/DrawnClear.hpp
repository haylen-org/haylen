#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/ShaderProgram.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::gpu::opengl {

// Clears attachments of the bound draw framebuffer by drawing over every sample of them, for an OpenGL ES driver whose clears miss parts of an attachment that its draws reach, such as a later layer of an array or the other samples of a multisampled image. A color attachment takes a color, and the depth stencil attachment a depth, a stencil value or both. The values reach the draw through a uniform buffer of its own, which takes new values only when they differ from the ones it holds.
class DrawnClear final {
  public:
    DrawnClear(Gl& functions, StateCache& cache);
    DrawnClear(const DrawnClear&) = delete;
    DrawnClear& operator=(const DrawnClear&) = delete;
    ~DrawnClear();

    // Draws the color into the attachment at the index of the framebuffer, which has the count of color attachments and the size in pixels, and leaves its other attachments as they were.
    void clear(std::size_t attachment, std::size_t colorCount, const math::Color& color, int width, int height);

    // Draws the depth, the stencil value or both into the depth stencil attachment of the framebuffer, which has the count of color attachments and the size in pixels, and leaves its color attachments as they were. The depth is the depth of clip space that reaches the depth the attachment clears to.
    void clearDepthStencil(std::optional<float> depth, std::optional<std::uint8_t> stencil, std::size_t colorCount, int width, int height);

  private:
    static_assert(Device::kMaxColorTargets == 4, "The fragment stage of the drawn clear writes one output for each color attachment of a pass.");

    // The color and then the depth of clip space, as the uniform block of the program lays them out.
    using Values = std::array<float, 8>;

    static const std::array<Device::BlockBinding, 1> kBlocks;
    static const Device::ProgramDesc kProgram;

    void write(const Values& values);
    void draw(const std::array<Gl::Enum, Device::kMaxColorTargets>& buffers, std::size_t colorCount, int width, int height);

    Gl& gl;
    StateCache& state;
    ShaderProgram program;
    Gl::Uint vertexArray = 0;
    Gl::Uint valueBuffer = 0;
    Values written{};
};

} // namespace haylen::gpu::opengl
