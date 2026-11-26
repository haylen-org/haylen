#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "haylen/3d/gltf/Accessor.hpp"
#include "haylen/3d/gltf/Animation.hpp"
#include "haylen/3d/gltf/Asset.hpp"
#include "haylen/3d/gltf/Buffer.hpp"
#include "haylen/3d/gltf/BufferView.hpp"
#include "haylen/3d/gltf/Camera.hpp"
#include "haylen/3d/gltf/Image.hpp"
#include "haylen/3d/gltf/Material.hpp"
#include "haylen/3d/gltf/Mesh.hpp"
#include "haylen/3d/gltf/Node.hpp"
#include "haylen/3d/gltf/Sampler.hpp"
#include "haylen/3d/gltf/Scene.hpp"
#include "haylen/3d/gltf/Skin.hpp"
#include "haylen/3d/gltf/Texture.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::graphics {
class Image;
}

namespace haylen::gltf {

// A glTF 2.0 file as CPU-side data that a renderer, physics or a tool reads: every object of the file by its index in the file, with the bytes of its buffers loaded and its images still encoded. The reader checks every rule of the specification before it hands a document out, so every index points at an object, every accessor fits its buffer view and the data obeys the rules of its use.
struct Document {
    // The file as the reader named it, such as `models/ship.glb`, which messages name.
    std::string file;
    Asset asset;

    // The scene to show when the app names none.
    std::optional<std::size_t> scene;
    std::vector<Scene> scenes;
    std::vector<Node> nodes;
    std::vector<Mesh> meshes;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    std::vector<Image> images;
    std::vector<Camera> cameras;
    std::vector<Skin> skins;
    std::vector<Animation> animations;
    std::vector<Accessor> accessors;
    std::vector<BufferView> bufferViews;
    std::vector<Buffer> buffers;
    std::vector<std::string> extensionsUsed;
    std::vector<std::string> extensionsRequired;
    core::Json extras;
    core::Json extensions;

    // The encoded bytes of an image, from its buffer view or from what the reader loaded. Throws `std::out_of_range` for an image the document lacks.
    [[nodiscard]] std::span<const std::uint8_t> getImageBytes(std::size_t image) const;

    // Decodes an image through the image decoder of the engine, which refuses an image longer than `maxSide` on a side before it allocates a pixel. Throws `std::out_of_range` for an image the document lacks and `std::runtime_error` for bytes that do not decode.
    [[nodiscard]] graphics::Image decodeImage(std::size_t image, int maxSide) const;

    // The length of an animation in seconds, the latest time of the inputs of its samplers. Throws `std::out_of_range` for an animation the document lacks.
    [[nodiscard]] float getDuration(std::size_t animation) const;
};

} // namespace haylen::gltf
