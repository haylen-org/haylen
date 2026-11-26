#include "haylen/3d/gltf/Document.hpp"

#include <algorithm>
#include <stdexcept>

#include "haylen/graphics/Image.hpp"

namespace haylen::gltf {

std::span<const std::uint8_t> Document::getImageBytes(std::size_t image) const {
    const Image& found = images.at(image);
    if (!found.bufferView) {
        return found.data;
    }

    const BufferView& view = bufferViews.at(*found.bufferView);
    const Buffer& buffer = buffers.at(view.buffer);
    if (view.byteOffset > buffer.data.size() || view.byteLength > buffer.data.size() - view.byteOffset) {
        throw std::out_of_range("The buffer view of an image lies outside the bytes of its buffer.");
    }

    return std::span(buffer.data).subspan(static_cast<std::size_t>(view.byteOffset), static_cast<std::size_t>(view.byteLength));
}

graphics::Image Document::decodeImage(std::size_t image, int maxSide) const {
    const std::string name = "image " + std::to_string(image) + " of " + file;
    return graphics::Image::decode(getImageBytes(image), maxSide, name);
}

float Document::getDuration(std::size_t animation) const {
    float duration = 0.0F;
    for (const Animation::Sampler& sampler : animations.at(animation).samplers) {
        const Accessor& input = accessors.at(sampler.input);
        if (!input.max.empty()) {
            duration = std::max(duration, static_cast<float>(input.max.front()));
        }
    }

    return duration;
}

} // namespace haylen::gltf
