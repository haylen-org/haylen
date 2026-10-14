#include "haylen/2d/graphics/SpriteBatch.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>
#include <utility>

#include "graphics/TextureResource.hpp"
#include "haylen/2d/graphics/Renderer.hpp"

namespace haylen::graphics2d {

SpriteBatch::SpriteBatch(graphics::Texture image) : SpriteBatch(std::vector<graphics::Texture>{std::move(image)}) {}

SpriteBatch::SpriteBatch(std::vector<graphics::Texture> images) : textures(std::move(images)) {
    checkTextures(textures);
}

void SpriteBatch::checkTextures(std::span<const graphics::Texture> images) {
    if (images.empty() || !std::ranges::all_of(images, &graphics::Texture::isValid)) {
        throw std::invalid_argument("A sprite batch needs a texture.");
    }
    if (images.size() > SpriteData::kMaxTextures) {
        throw std::invalid_argument(std::format("A sprite batch draws from at most {} textures.", SpriteData::kMaxTextures));
    }
    const graphics::Texture::Options& options = images.front().getResource()->options;
    if (!std::ranges::all_of(images, [&options](const graphics::Texture& image) { return image.getResource()->options == options; })) {
        throw std::invalid_argument("The textures of a sprite batch share their filter, wrap and mipmaps, since they draw with one sampler.");
    }
}

void SpriteBatch::checkTexture(std::uint8_t texture) const {
    if (texture >= textures.size()) {
        throw std::invalid_argument(std::format("A sprite of a batch names a texture past the {} textures of the batch.", textures.size()));
    }
}

std::size_t SpriteBatch::add(const SpriteInstance& sprite) {
    sprites.push_back(sprite);
    if (!partColors.empty()) {
        partColors.emplace_back();
    }
    if (!data.empty()) {
        data.emplace_back();
    }
    return sprites.size() - 1;
}

void SpriteBatch::set(std::size_t index, const SpriteInstance& sprite) {
    sprites.at(index) = sprite;
}

const SpriteInstance& SpriteBatch::get(std::size_t index) const {
    return sprites.at(index);
}

void SpriteBatch::remove(std::size_t index) {
    if (index >= sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    sprites.erase(sprites.begin() + static_cast<std::ptrdiff_t>(index));
    if (!partColors.empty()) {
        partColors.erase(partColors.begin() + static_cast<std::ptrdiff_t>(index));
    }
    if (!data.empty()) {
        data.erase(data.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void SpriteBatch::clear() noexcept {
    sprites.clear();
    partColors.clear();
    data.clear();
}

void SpriteBatch::checkCount(std::size_t count) {
    if (count > kMaxSprites) {
        throw std::invalid_argument("A sprite batch holds at most 16777216 sprites.");
    }
}

void SpriteBatch::reserve(std::size_t count) {
    checkCount(count);
    sprites.reserve(count);
}

void SpriteBatch::resize(std::size_t count, const SpriteInstance& sprite) {
    checkCount(count);
    sprites.resize(count, sprite);
    if (!partColors.empty()) {
        partColors.resize(count);
    }
    if (!data.empty()) {
        data.resize(count);
    }
}

void SpriteBatch::setPartColors(std::size_t index, const PartColors& colors) {
    if (index >= sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    partColors.resize(sprites.size());
    partColors[index] = colors;
}

PartColors SpriteBatch::getPartColors(std::size_t index) const {
    if (index >= sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    return partColors.empty() ? PartColors{} : partColors[index];
}

void SpriteBatch::setData(std::size_t index, const SpriteData& value) {
    if (index >= sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    checkTexture(value.texture);
    data.resize(sprites.size());
    data[index] = value;
}

SpriteData SpriteBatch::getData(std::size_t index) const {
    if (index >= sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    return data.empty() ? SpriteData{} : data[index];
}

void SpriteBatch::writeFields(std::span<const float> values, const SpriteLayout& layout, std::size_t first) {
    if (first > sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    const std::size_t count = std::min(layout.getCount(values), sprites.size() - first);
    if (!layout.hasDataFields()) {
        SpriteData unused;
        for (std::size_t index = 0; index < count; ++index) {
            layout.apply(values, index, sprites[first + index], unused);
        }
        return;
    }

    // The textures the values name are checked before any sprite changes, so a wrong value leaves the batch whole.
    for (std::size_t index = 0; index < count; ++index) {
        checkTexture(layout.makeData(values, index).texture);
    }
    data.resize(sprites.size());
    for (std::size_t index = 0; index < count; ++index) {
        layout.apply(values, index, sprites[first + index], data[first + index]);
    }
}

void SpriteBatch::readFields(std::span<float> values, const SpriteLayout& layout, std::size_t first) const {
    if (first > sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    const std::size_t count = std::min(layout.getCount(values), sprites.size() - first);
    for (std::size_t index = 0; index < count; ++index) {
        layout.store(sprites[first + index], data.empty() ? SpriteData{} : data[first + index], values, index);
    }
}

void SpriteBatch::draw(Renderer& renderer, const DrawOrder& order) const {
    renderer.drawBatch(textures, sprites, data, order, partColors);
}

StaticSpriteBatch SpriteBatch::bake(Renderer& renderer) const {
    if (textures.size() > 1 || !data.empty()) {
        throw std::logic_error("A sprite batch with several textures or sprite data does not bake. Draw it as a sprite batch instead.");
    }
    return renderer.createStaticBatch(textures.front(), sprites);
}

} // namespace haylen::graphics2d
