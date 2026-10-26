#include "gpu/opengl/FramebufferCache.hpp"

#include <format>
#include <stdexcept>

namespace haylen::gpu::opengl {

FramebufferCache::~FramebufferCache() {
    for (const Entry& entry : entries) {
        gl.deleteFramebuffers(1, &entry.framebuffer);
        state.forgetFramebuffer(entry.framebuffer);
    }
}

Gl::Uint FramebufferCache::get(const Key& key, std::string_view label) {
    for (const Entry& entry : entries) {
        if (entry.key == key) {
            return entry.framebuffer;
        }
    }
    const Gl::Uint framebuffer = make(key, label);
    entries.push_back({.key = key, .framebuffer = framebuffer});
    return framebuffer;
}

void FramebufferCache::attach(Gl::Enum point, const Attachment& attachment) {
    switch (attachment.target) {
    case Gl::kRenderbuffer:
        gl.framebufferRenderbuffer(Gl::kDrawFramebuffer, point, Gl::kRenderbuffer, attachment.name);
        return;
    case Gl::kTextureCubeMap:
        gl.framebufferTexture2D(Gl::kDrawFramebuffer, point, Gl::kTextureCubeMapPositiveX + static_cast<Gl::Enum>(attachment.layer), attachment.name, attachment.mipLevel);
        return;
    case Gl::kTexture2dArray:
    case Gl::kTexture3d:
        gl.framebufferTextureLayer(Gl::kDrawFramebuffer, point, attachment.name, attachment.mipLevel, attachment.layer);
        return;
    default:
        gl.framebufferTexture2D(Gl::kDrawFramebuffer, point, Gl::kTexture2d, attachment.name, attachment.mipLevel);
        return;
    }
}

Gl::Uint FramebufferCache::make(const Key& key, std::string_view label) {
    Gl::Uint framebuffer = 0;
    gl.genFramebuffers(1, &framebuffer);
    state.bindDrawFramebuffer(framebuffer);

    std::array<Gl::Enum, Device::kMaxColorTargets> drawBuffers{};
    for (std::size_t index = 0; index < key.colorCount; ++index) {
        drawBuffers[index] = Gl::kColorAttachment0 + static_cast<Gl::Enum>(index);
        attach(drawBuffers[index], key.colors[index]);
    }
    if (key.depthStencil.image != 0) {
        attach(key.stencil ? Gl::kDepthStencilAttachment : Gl::kDepthAttachment, key.depthStencil);
    }
    gl.drawBuffers(static_cast<Gl::Sizei>(key.colorCount), drawBuffers.data());

    const Gl::Enum status = gl.checkFramebufferStatus(Gl::kDrawFramebuffer);
    if (status != Gl::kFramebufferComplete) {
        gl.deleteFramebuffers(1, &framebuffer);
        state.forgetFramebuffer(framebuffer);
        throw std::runtime_error(std::format("OpenGL cannot draw into the attachments of the pass \"{}\", whose framebuffer has the status 0x{:04X}.", label, status));
    }
    return framebuffer;
}

void FramebufferCache::forgetImage(std::uint32_t image) {
    // clang-format off
    std::erase_if(entries, [&](const Entry& entry) {
        bool holds = entry.key.depthStencil.image == image;
        for (std::size_t index = 0; index < entry.key.colorCount; ++index) {
            holds = holds || entry.key.colors[index].image == image;
        }
        if (holds) {
            gl.deleteFramebuffers(1, &entry.framebuffer);
            state.forgetFramebuffer(entry.framebuffer);
        }
        return holds;
    });
    // clang-format on
}

} // namespace haylen::gpu::opengl
