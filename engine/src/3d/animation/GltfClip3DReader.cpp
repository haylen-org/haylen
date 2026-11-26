#include "3d/animation/GltfClip3DReader.hpp"

#include <format>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "3d/animation/GltfNodeNames3D.hpp"
#include "haylen/3d/gltf/AccessorReader.hpp"

namespace haylen::animation3d {

void GltfClip3DReader::fail(const gltf::Document& document, std::size_t animation, const std::string& problem) {
    throw std::invalid_argument(std::format("The animation {} of the glTF file \"{}\" does not make a clip, since {}. Export the model again from the tool that made it, so it follows glTF 2.0.", animation, document.file, problem));
}

Clip3D GltfClip3DReader::read(const gltf::Document& document, std::size_t animation) {
    if (animation >= document.animations.size()) {
        throw std::out_of_range(std::format("The glTF file \"{}\" has {} animations, so it has no animation {}.", document.file, document.animations.size(), animation));
    }

    const gltf::Animation& source = document.animations[animation];
    const std::vector<std::string> names = GltfNodeNames3D::of(document);
    std::vector<Track3D> tracks;
    tracks.reserve(source.channels.size());
    for (std::size_t channel = 0; channel < source.channels.size(); ++channel) {
        // A channel without a node targets what an extension names, which this reader does not take.
        const std::optional<std::size_t> node = source.channels[channel].node;
        if (!node) {
            continue;
        }

        if (*node >= document.nodes.size()) {
            fail(document, animation, std::format("channel {} names node {}, which the file lacks", channel, *node));
        }

        tracks.push_back(readChannel(document, animation, channel, names[*node]));
    }

    try {
        return Clip3D({.name = source.name, .tracks = std::move(tracks)});
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("The animation {} of the glTF file \"{}\" does not make a clip. {}", animation, document.file, error.what()));
    }
}

Track3D GltfClip3DReader::readChannel(const gltf::Document& document, std::size_t animation, std::size_t channel, const std::string& target) {
    const gltf::Animation& source = document.animations[animation];
    const gltf::Animation::Channel& entry = source.channels[channel];
    if (entry.sampler >= source.samplers.size()) {
        fail(document, animation, std::format("channel {} names sampler {}, which the animation lacks", channel, entry.sampler));
    }

    const gltf::Animation::Sampler& sampler = source.samplers[entry.sampler];
    if (sampler.input >= document.accessors.size() || sampler.output >= document.accessors.size()) {
        fail(document, animation, std::format("sampler {} names an accessor that the file lacks", entry.sampler));
    }

    if (document.accessors[sampler.input].type != gltf::Accessor::Type::Scalar) {
        fail(document, animation, std::format("the times of sampler {} are not single numbers", entry.sampler));
    }

    Track3D::Description description{.target = target};
    gltf::Accessor::Type expected = gltf::Accessor::Type::Vec3;
    switch (entry.path) {
    case gltf::Animation::Path::Translation:
        description.property = "position";
        description.type = Track3D::Type::Vec3;
        break;
    case gltf::Animation::Path::Rotation:
        description.property = "rotation";
        description.type = Track3D::Type::Quat;
        expected = gltf::Accessor::Type::Vec4;
        break;
    case gltf::Animation::Path::Scale:
        description.property = "scale";
        description.type = Track3D::Type::Vec3;
        break;
    case gltf::Animation::Path::Weights:
        description.property = "weights";
        description.type = Track3D::Type::Weights;
        expected = gltf::Accessor::Type::Scalar;
        break;
    }

    if (document.accessors[sampler.output].type != expected) {
        fail(document, animation, std::format("the values of sampler {} are not of the type that the property \"{}\" of channel {} takes", entry.sampler, description.property, channel));
    }

    switch (sampler.interpolation) {
    case gltf::Animation::Interpolation::Step:
        description.interpolation = Track3D::Interpolation::Step;
        break;
    case gltf::Animation::Interpolation::Linear:
        description.interpolation = Track3D::Interpolation::Linear;
        break;
    case gltf::Animation::Interpolation::CubicSpline:
        description.interpolation = Track3D::Interpolation::CubicSpline;
        break;
    }

    // The weights of a channel are the morph targets of the mesh of its node, one for each target at each key.
    std::size_t targets = 0;
    if (entry.path == gltf::Animation::Path::Weights) {
        const std::optional<std::size_t> mesh = document.nodes[*entry.node].mesh;
        targets = mesh && *mesh < document.meshes.size() ? document.meshes[*mesh].getTargetCount() : 0;
        if (targets == 0) {
            fail(document, animation, std::format("channel {} animates the morph weights of node {}, whose mesh has no morph targets", channel, *entry.node));
        }
    }

    std::optional<Track3D> track;
    try {
        description.times = gltf::AccessorReader::readFloats(document, sampler.input);
        description.values = gltf::AccessorReader::readFloats(document, sampler.output);
        track.emplace(std::move(description));
    } catch (const std::invalid_argument& error) {
        throw std::invalid_argument(std::format("The animation {} of the glTF file \"{}\" does not make a clip. {}", animation, document.file, error.what()));
    }

    if (targets != 0 && track->getComponentCount() != targets) {
        fail(document, animation, std::format("channel {} gives {} weights to each key, while the mesh of node {} has {} morph targets", channel, track->getComponentCount(), *entry.node, targets));
    }

    return std::move(*track);
}

} // namespace haylen::animation3d
