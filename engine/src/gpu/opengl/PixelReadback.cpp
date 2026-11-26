#include "gpu/opengl/PixelReadback.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace haylen::gpu::opengl {

PixelReadback::PixelReadback(const std::shared_ptr<Context>& owner, Gl::Uint framebuffer, int width, int height, bool floats) : copy(owner, Gl::kPixelPackBuffer, getBytes(width, height, floats)), pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)), floating(floats) {
    StateCache& state = owner->state;
    state.bindReadFramebuffer(framebuffer);
    owner->gl.readPixels(0, 0, width, height, Gl::kRgba, floating ? Gl::kFloat : Gl::kUnsignedByte, nullptr);
    state.bindBuffer(Gl::kPixelPackBuffer, 0);
    copy.fence(*owner, kSubject);
}

std::size_t PixelReadback::getBytes(int width, int height, bool floats) noexcept {
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * (floats ? 16 : 4);
}

std::optional<std::vector<std::uint8_t>> PixelReadback::poll() {
    std::optional<std::vector<std::uint8_t>> read = copy.poll(kSubject);
    if (!read || !floating) {
        return read;
    }

    std::vector<std::uint8_t> result(pixels * 4);
    for (std::size_t index = 0; index < result.size(); ++index) {
        float value = 0.0F;
        std::memcpy(&value, read->data() + index * sizeof(float), sizeof(float));
        result[index] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    }

    return result;
}

} // namespace haylen::gpu::opengl
