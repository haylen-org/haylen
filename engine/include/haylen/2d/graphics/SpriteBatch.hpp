#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics2d {

class Renderer;

// Many sprites that share one texture and are drawn with a single batch. Scripts keep the data in C++ and change only what moves. Every sprite has part colors, white until set, which recolor it when the batch draws with a part mask, and a batch keeps room for them only once one is set.
class SpriteBatch final {
  public:
    explicit SpriteBatch(graphics::Texture image);

    std::size_t add(const SpriteInstance& sprite);
    void set(std::size_t index, const SpriteInstance& sprite);
    [[nodiscard]] const SpriteInstance& get(std::size_t index) const;
    void remove(std::size_t index);
    void clear() noexcept;
    void reserve(std::size_t count);

    // Sets the number of sprites, filling new ones with the sprite.
    void resize(std::size_t count, const SpriteInstance& sprite = {});

    void setPartColors(std::size_t index, const PartColors& colors);
    [[nodiscard]] PartColors getPartColors(std::size_t index) const;

    // Copies the fields of the layout from the values into the sprites from the one at `first` on, for as many sprites as both hold, which moves every sprite of a large batch in one call. Throws `std::out_of_range` when `first` is past the end.
    void writeFields(std::span<const float> values, const SpriteLayout& layout, std::size_t first = 0);

    // Copies the fields of the layout from the sprites into the values, the other way around.
    void readFields(std::span<float> values, const SpriteLayout& layout, std::size_t first = 0) const;

    [[nodiscard]] std::size_t size() const noexcept {
        return sprites.size();
    }
    [[nodiscard]] const graphics::Texture& getTexture() const noexcept {
        return texture;
    }
    [[nodiscard]] std::span<const SpriteInstance> getSprites() const noexcept {
        return sprites;
    }

    void draw(Renderer& renderer, const DrawOrder& order = {}) const;
    [[nodiscard]] StaticSpriteBatch bake(Renderer& renderer) const;

  private:
    graphics::Texture texture;
    std::vector<SpriteInstance> sprites;
    std::vector<PartColors> partColors;
};

} // namespace haylen::graphics2d
