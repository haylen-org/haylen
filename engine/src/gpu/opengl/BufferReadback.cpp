#include "gpu/opengl/BufferReadback.hpp"

namespace haylen::gpu::opengl {

// The targets of copies take a buffer of any kind, even a buffer of indices on WebGL2, so the copy reads the source through them.
BufferReadback::BufferReadback(const std::shared_ptr<Context>& owner, Gl::Uint source, std::size_t offset, std::size_t size) : copy(owner, Gl::kCopyWriteBuffer, size) {
    owner->state.bindBuffer(Gl::kCopyReadBuffer, source);
    owner->gl.copyBufferSubData(Gl::kCopyReadBuffer, Gl::kCopyWriteBuffer, static_cast<Gl::IntPtr>(offset), 0, static_cast<Gl::SizeiPtr>(size));
    copy.fence(*owner, kSubject);
}

std::optional<std::vector<std::uint8_t>> BufferReadback::poll() {
    return copy.poll(kSubject);
}

} // namespace haylen::gpu::opengl
