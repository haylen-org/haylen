#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include "gpu/opengl/Gl.hpp"
#include "gpu/opengl/StateCache.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// The framebuffer objects of the attachment sets that passes draw into and resolves copy into, made the first time a set comes and kept until one of its images is destroyed.
class FramebufferCache final {
  public:
    // An image attached at a mip level and a layer. The target is the one its texture binds to, or `Gl::kRenderbuffer` for a multisampled image.
    struct Attachment {
        std::uint32_t image = 0;
        Gl::Uint name = 0;
        Gl::Enum target = Gl::kTexture2d;
        int mipLevel = 0;
        int layer = 0;

        friend bool operator==(const Attachment&, const Attachment&) = default;
    };

    struct Key {
        std::array<Attachment, Device::kMaxColorTargets> colors{};
        std::size_t colorCount = 0;
        Attachment depthStencil;
        bool stencil = false;

        friend bool operator==(const Key&, const Key&) = default;
    };

    FramebufferCache(Gl& functions, StateCache& cache) noexcept : gl(functions), state(cache) {}
    FramebufferCache(const FramebufferCache&) = delete;
    FramebufferCache& operator=(const FramebufferCache&) = delete;
    ~FramebufferCache();

    // The framebuffer of the attachments, made and checked when it is new. It throws `std::runtime_error` when OpenGL cannot draw into the set.
    [[nodiscard]] Gl::Uint get(const Key& key, std::string_view label);

    // Deletes the framebuffers that hold the image.
    void forgetImage(std::uint32_t image);

  private:
    struct Entry {
        Key key;
        Gl::Uint framebuffer = 0;
    };

    void attach(Gl::Enum point, const Attachment& attachment);
    [[nodiscard]] Gl::Uint make(const Key& key, std::string_view label);

    Gl& gl;
    StateCache& state;
    std::vector<Entry> entries;
};

} // namespace haylen::gpu::opengl
