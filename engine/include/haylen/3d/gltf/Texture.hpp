#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// An image with the sampler that reads it. A texture without a sampler repeats and lets the renderer choose its filters, and a texture without a source takes its image from an extension.
struct Texture {
    std::string name;
    std::optional<std::size_t> sampler;
    std::optional<std::size_t> source;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf
