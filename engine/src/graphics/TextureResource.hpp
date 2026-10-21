#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "graphics/ResourceGraveyard.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {

// The GPU objects behind a texture handle, buried in the device graveyard when the last handle goes away. The debug statistics count textures and render target images apart, with the bytes of their pixels.
struct TextureResource {
    static debug::ObjectCounter& textures;
    static debug::ObjectCounter& targets;

    gpu::Device::Image image;
    gpu::Device::Sampler sampler;
    int width = 0;
    int height = 0;
    std::uint32_t id = 0;
    Texture::Options options{};
    gpu::Device::PixelFormat format = gpu::Device::PixelFormat::Rgba8;
    bool flipped = false;

    // The path of a texture the assets loaded from the package, which debug drawings name it by.
    std::string label;

    // The submission of the 2D renderer that last kept the texture alive until it ends, which keeps it once however many draws use it.
    std::uint64_t retainEpoch = 0;

    // A dynamic texture changes in place, and keeps the pixels it received since the last upload until the device sends them. The staging memory stays while the texture changes every frame, such as a video, and goes after a frame without a change.
    bool dynamic = false;
    bool pending = false;
    std::uint64_t uploadedAt = 0;
    std::vector<std::uint8_t> staged;
    std::weak_ptr<ResourceGraveyard> graveyard;
    debug::TrackedObject tracked;

    explicit TextureResource(debug::ObjectCounter& kind = textures) noexcept : tracked(kind) {}
    TextureResource(const TextureResource&) = delete;
    TextureResource& operator=(const TextureResource&) = delete;
    ~TextureResource();
};

} // namespace haylen::graphics
