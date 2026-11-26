#include "gpu/opengl/ReadbackBuffer.hpp"

#include <cstring>
#include <format>
#include <stdexcept>

namespace haylen::gpu::opengl {

ReadbackBuffer::ReadbackBuffer(const std::shared_ptr<Context>& owner, Gl::Enum bufferTarget, std::size_t bytes) : context(owner), target(bufferTarget), size(bytes) {
    owner->gl.genBuffers(1, &buffer);
    owner->state.bindBuffer(target, buffer);
    owner->gl.bufferData(target, static_cast<Gl::SizeiPtr>(size), nullptr, Gl::kStreamRead);
}

ReadbackBuffer::~ReadbackBuffer() {
    if (const std::shared_ptr<Context> owner = context.lock()) {
        release(*owner);
    }
}

void ReadbackBuffer::release(Context& owner) noexcept {
    if (sync != nullptr) {
        owner.gl.deleteSync(sync);
        sync = nullptr;
    }

    if (buffer != 0) {
        owner.gl.deleteBuffers(1, &buffer);
        owner.state.forgetBuffer(buffer);
        buffer = 0;
    }
}

void ReadbackBuffer::fence(Context& owner, std::string_view subject) {
    sync = owner.gl.fenceSync(Gl::kSyncGpuCommandsComplete, 0);
    owner.gl.flush();

    if (sync == nullptr) {
        throw std::runtime_error(std::format("The OpenGL context could not fence the copy of {}.", subject));
    }
}

// A lost context may never signal the fence, so a copy ends once its device found the context lost.
std::optional<std::vector<std::uint8_t>> ReadbackBuffer::poll(std::string_view subject) {
    const std::shared_ptr<Context> owner = context.lock();
    if (owner == nullptr || sync == nullptr) {
        return std::nullopt;
    }

    if (owner->lost) {
        release(*owner);
        throw std::runtime_error(std::format("The OpenGL device was lost before it copied {}.", subject));
    }

    Gl::Int status = 0;
    owner->gl.getSynciv(sync, Gl::kSyncStatus, 1, nullptr, &status);
    if (static_cast<Gl::Enum>(status) != Gl::kSignaled) {
        return std::nullopt;
    }

    std::optional<std::vector<std::uint8_t>> bytes = copyOut(*owner);
    release(*owner);

    if (!bytes) {
        throw std::runtime_error(std::format("The GPU could not hand over {}.", subject));
    }

    return bytes;
}

std::optional<std::vector<std::uint8_t>> ReadbackBuffer::copyOut(Context& owner) const {
    Gl& gl = owner.gl;
    std::vector<std::uint8_t> bytes(size);
    owner.state.bindBuffer(target, buffer);

    bool copied = true;
    if (gl.getBufferSubData != nullptr) {
        gl.getBufferSubData(target, 0, static_cast<Gl::SizeiPtr>(size), bytes.data());
    } else if (const void* mapped = gl.mapBufferRange(target, 0, static_cast<Gl::SizeiPtr>(size), Gl::kMapReadBit)) {
        std::memcpy(bytes.data(), mapped, size);
        gl.unmapBuffer(target);
    } else {
        copied = false;
    }

    owner.state.bindBuffer(target, 0);

    if (!copied) {
        return std::nullopt;
    }

    return bytes;
}

} // namespace haylen::gpu::opengl
