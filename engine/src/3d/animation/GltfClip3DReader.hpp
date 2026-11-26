#pragma once

#include <cstddef>
#include <string>

#include "haylen/3d/animation/Clip3D.hpp"
#include "haylen/3d/gltf/Document.hpp"

namespace haylen::animation3d {

// Makes the clip of an animation of a glTF document: a track for each channel that targets a node, with the times and values of its sampler as the accessor reader gives them. It checks what it reads again, since a document can come from code as well as from the reader, and names the file and the animation in every refusal.
class GltfClip3DReader final {
  public:
    [[nodiscard]] static Clip3D read(const gltf::Document& document, std::size_t animation);

  private:
    [[nodiscard]] static Track3D readChannel(const gltf::Document& document, std::size_t animation, std::size_t channel, const std::string& target);
    [[noreturn]] static void fail(const gltf::Document& document, std::size_t animation, const std::string& problem);
};

} // namespace haylen::animation3d
