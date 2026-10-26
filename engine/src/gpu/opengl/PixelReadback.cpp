#include "gpu/opengl/PixelReadback.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace haylen::gpu::opengl {

PixelReadback::PixelReadback(const std::shared_ptr<Context>& owner, Gl::Uint framebuffer, int width, int height, bool floats) : context(owner), pixels(static_cast<std::size_t>(width) * static_cast<std::size_t>(height)), floating(floats) {
    Gl& gl = owner->gl;
    StateCache& state = owner->state;
    const std::size_t bytes = pixels * (floating ? 16 : 4);

    gl.genBuffers(1, &buffer);
    state.bindBuffer(Gl::kPixelPackBuffer, buffer);
    gl.bufferData(Gl::kPixelPackBuffer, static_cast<Gl::SizeiPtr>(bytes), nullptr, Gl::kStreamRead);
    state.bindReadFramebuffer(framebuffer);
    gl.readPixels(0, 0, width, height, Gl::kRgba, floating ? Gl::kFloat : Gl::kUnsignedByte, nullptr);
    state.bindBuffer(Gl::kPixelPackBuffer, 0);

    fence = gl.fenceSync(Gl::kSyncGpuCommandsComplete, 0);
    gl.flush();
    if (fence == nullptr) {
        release(*owner);
        throw std::runtime_error("The OpenGL context could not fence the copy of a render target.");
    }
}

PixelReadback::~PixelReadback() {
    if (const std::shared_ptr<Context> owner = context.lock()) {
        release(*owner);
    }
}

void PixelReadback::release(Context& owner) noexcept {
    if (fence != nullptr) {
        owner.gl.deleteSync(fence);
        fence = nullptr;
    }
    if (buffer != 0) {
        owner.gl.deleteBuffers(1, &buffer);
        owner.state.forgetBuffer(buffer);
        buffer = 0;
    }
}

std::optional<std::vector<std::uint8_t>> PixelReadback::poll() {
    const std::shared_ptr<Context> owner = context.lock();
    if (owner == nullptr || fence == nullptr) {
        return std::nullopt;
    }
    Gl::Int status = 0;
    owner->gl.getSynciv(fence, Gl::kSyncStatus, 1, nullptr, &status);
    if (static_cast<Gl::Enum>(status) != Gl::kSignaled) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> result = copyOut(*owner);
    release(*owner);
    return result;
}

// OpenGL ES reads a buffer back only by mapping it, and OpenGL and WebGL2 copy it out.
std::vector<std::uint8_t> PixelReadback::copyOut(Context& owner) const {
    Gl& gl = owner.gl;
    const std::size_t bytes = pixels * (floating ? 16 : 4);
    std::vector<std::byte> read(bytes);
    owner.state.bindBuffer(Gl::kPixelPackBuffer, buffer);
    if (gl.getBufferSubData != nullptr) {
        gl.getBufferSubData(Gl::kPixelPackBuffer, 0, static_cast<Gl::SizeiPtr>(bytes), read.data());
    } else if (const void* mapped = gl.mapBufferRange(Gl::kPixelPackBuffer, 0, static_cast<Gl::SizeiPtr>(bytes), Gl::kMapReadBit)) {
        std::memcpy(read.data(), mapped, bytes);
        gl.unmapBuffer(Gl::kPixelPackBuffer);
    }
    owner.state.bindBuffer(Gl::kPixelPackBuffer, 0);

    std::vector<std::uint8_t> result(pixels * 4);
    if (!floating) {
        std::memcpy(result.data(), read.data(), bytes);
        return result;
    }
    for (std::size_t index = 0; index < result.size(); ++index) {
        float value = 0.0F;
        std::memcpy(&value, read.data() + index * sizeof(float), sizeof(float));
        result[index] = static_cast<std::uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    }
    return result;
}

} // namespace haylen::gpu::opengl
