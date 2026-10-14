#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/SpriteData.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics2d {

class Renderer;

// Many sprites drawn with a single batch from one texture, or from up to `SpriteData::kMaxTextures` textures with the same filter, wrap and mipmaps. Scripts keep the data in C++ and change only what moves. Every sprite has part colors, white until set, which recolor it when the batch draws with a part mask, and data, the first texture and zeros until set, which picks its texture and hands four numbers to the material of the draw, and a batch keeps room for each only once a sprite has some.
class SpriteBatch final {
  public:
    static constexpr std::size_t kMaxSprites = std::size_t{1} << 24U;

    explicit SpriteBatch(graphics::Texture image);

    explicit SpriteBatch(std::vector<graphics::Texture> images);

    // Throws `std::invalid_argument` for textures that cannot draw one batch: none, an invalid one, more than `SpriteData::kMaxTextures` or textures that sample differently.
    static void checkTextures(std::span<const graphics::Texture> images);

    std::size_t add(const SpriteInstance& sprite);
    void set(std::size_t index, const SpriteInstance& sprite);
    [[nodiscard]] const SpriteInstance& get(std::size_t index) const;
    void remove(std::size_t index);
    void clear() noexcept;
    // Make room for a number of sprites or set it, filling new sprites with the sprite. A count above `kMaxSprites` throws `std::invalid_argument`.
    void reserve(std::size_t count);
    void resize(std::size_t count, const SpriteInstance& sprite = {});

    void setPartColors(std::size_t index, const PartColors& colors);
    [[nodiscard]] PartColors getPartColors(std::size_t index) const;

    // Throws `std::invalid_argument` when the data names a texture the batch lacks.
    void setData(std::size_t index, const SpriteData& value);
    [[nodiscard]] SpriteData getData(std::size_t index) const;

    // Copies the fields of the layout from the values into the sprites and their data from the one at `first` on, for as many sprites as both hold, which moves every sprite of a large batch in one call. Throws `std::out_of_range` when `first` is past the end, and `std::invalid_argument` when a value names a texture the batch lacks, which leaves the sprites as they were.
    void writeFields(std::span<const float> values, const SpriteLayout& layout, std::size_t first = 0);

    // Copies the fields of the layout from the sprites into the values, the other way around.
    void readFields(std::span<float> values, const SpriteLayout& layout, std::size_t first = 0) const;

    [[nodiscard]] std::size_t size() const noexcept {
        return sprites.size();
    }
    [[nodiscard]] const graphics::Texture& getTexture() const noexcept {
        return textures.front();
    }
    [[nodiscard]] std::span<const graphics::Texture> getTextures() const noexcept {
        return textures;
    }
    [[nodiscard]] std::span<const SpriteInstance> getSprites() const noexcept {
        return sprites;
    }

    void draw(Renderer& renderer, const DrawOrder& order = {}) const;
    // Throws `std::logic_error` for a batch with several textures or with data, which a baked batch cannot draw.
    [[nodiscard]] StaticSpriteBatch bake(Renderer& renderer) const;

  private:
    static void checkCount(std::size_t count);
    void checkTexture(std::uint8_t texture) const;

    std::vector<graphics::Texture> textures;
    std::vector<SpriteInstance> sprites;
    std::vector<PartColors> partColors;
    std::vector<SpriteData> data;
};

} // namespace haylen::graphics2d
