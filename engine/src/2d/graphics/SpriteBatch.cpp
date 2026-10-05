#include "haylen/2d/graphics/SpriteBatch.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"

namespace haylen::graphics2d {

SpriteBatch::SpriteBatch(graphics::Texture image) : texture(std::move(image)) {
    if (!texture.isValid()) {
        throw std::invalid_argument("A sprite batch needs a texture.");
    }
}

std::size_t SpriteBatch::add(const SpriteInstance& sprite) {
    sprites.push_back(sprite);
    if (!partColors.empty()) {
        partColors.emplace_back();
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
}

void SpriteBatch::clear() noexcept {
    sprites.clear();
    partColors.clear();
}

void SpriteBatch::reserve(std::size_t count) {
    sprites.reserve(count);
}

void SpriteBatch::resize(std::size_t count, const SpriteInstance& sprite) {
    sprites.resize(count, sprite);
    if (!partColors.empty()) {
        partColors.resize(count);
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

void SpriteBatch::writeFields(std::span<const float> values, const SpriteLayout& layout, std::size_t first) {
    if (first > sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    const std::size_t count = std::min(layout.getCount(values), sprites.size() - first);
    for (std::size_t index = 0; index < count; ++index) {
        layout.apply(values, index, sprites[first + index]);
    }
}

void SpriteBatch::readFields(std::span<float> values, const SpriteLayout& layout, std::size_t first) const {
    if (first > sprites.size()) {
        throw std::out_of_range("Sprite batch index is out of range.");
    }
    const std::size_t count = std::min(layout.getCount(values), sprites.size() - first);
    for (std::size_t index = 0; index < count; ++index) {
        layout.store(sprites[first + index], values, index);
    }
}

void SpriteBatch::draw(Renderer& renderer, const DrawOrder& order) const {
    renderer.drawBatch(texture, sprites, order, partColors);
}

StaticSpriteBatch SpriteBatch::bake(Renderer& renderer) const {
    return renderer.createStaticBatch(texture, sprites);
}

} // namespace haylen::graphics2d
