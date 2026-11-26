#pragma once

#include <string>

#include "haylen/core/Json.hpp"

namespace haylen::gltf {

// What a glTF file says about itself: the version of glTF it targets, the oldest version that reads it, the tool that wrote it and its copyright.
struct Asset {
    std::string version;
    std::string minVersion;
    std::string generator;
    std::string copyright;
    core::Json extras;
    core::Json extensions;
};

} // namespace haylen::gltf
