#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "gpu/opengl/Context.hpp"

namespace haylen::gpu::opengl {

// A buffer that a copy of the GPU fills behind a fence, which the readbacks of render targets and of buffers copy into. Polling reads the status of the fence without blocking, which WebGL2 allows on the main thread, and copies the bytes out once the GPU passed it, by mapping the buffer on OpenGL ES, which reads buffers back only that way, and with `glGetBufferSubData` on OpenGL and WebGL2.
class ReadbackBuffer final {
  public:
    // Makes the buffer of the size on the target, where it stays bound for the copy that fills it.
    ReadbackBuffer(const std::shared_ptr<Context>& owner, Gl::Enum bufferTarget, std::size_t bytes);
    ReadbackBuffer(const ReadbackBuffer&) = delete;
    ReadbackBuffer& operator=(const ReadbackBuffer&) = delete;
    ~ReadbackBuffer();

    [[nodiscard]] Gl::Uint getName() const noexcept {
        return buffer;
    }

    // Fences the copy that the context queued into the buffer and sends it to the GPU. Throws `std::runtime_error` that names what the copy holds, such as the pixels of a render target, when the context cannot fence it.
    void fence(Context& owner, std::string_view subject);

    // Returns the bytes once the GPU finished the copy, and nothing before or after. Throws `std::runtime_error` that names what the copy holds when the device was lost before the copy finished or when the context could not hand the bytes over.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll(std::string_view subject);

  private:
    // Returns the bytes of the buffer, or nothing when the context could not hand them over.
    [[nodiscard]] std::optional<std::vector<std::uint8_t>> copyOut(Context& owner) const;
    void release(Context& owner) noexcept;

    std::weak_ptr<Context> context;
    Gl::Enum target = Gl::kCopyWriteBuffer;
    Gl::Uint buffer = 0;
    Gl::Sync sync = nullptr;
    std::size_t size = 0;
};

} // namespace haylen::gpu::opengl
