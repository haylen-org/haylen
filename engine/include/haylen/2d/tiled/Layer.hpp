#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/tiled/Object.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// A tile, object, image or group layer. Each kind uses its own fields, and group layers hold their child layers.
struct Layer {
    enum class Kind : std::uint8_t {
        Tile,
        Object,
        Image,
        Group,
    };

    struct Chunk {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::vector<std::uint32_t> gids;
    };

    std::uint32_t id = 0;
    std::string name;
    std::string type;
    Kind kind = Kind::Tile;
    bool visible = true;
    float opacity = 1.0F;
    graphics::BlendMode::Type blend = graphics::BlendMode::Type::Alpha;
    math::Vec2 offset{};
    math::Vec2 parallax{1.0F, 1.0F};
    math::Color tint = math::Color::white();
    Properties properties;

    int width = 0;
    int height = 0;
    std::vector<std::uint32_t> gids;
    std::vector<Chunk> chunks;

    // Whether the layer keeps its cells in chunks, as every tile layer of an infinite map does, which holds even while it has none.
    bool infinite = false;

    std::vector<Object> objects;
    bool indexDrawOrder = false;

    // The color Tiled shows the objects of an object layer in, which apps may use for their own drawings of the layer.
    std::optional<math::Color> color;

    std::string image;
    std::optional<math::Color> transparentColor;
    math::Vec2 imageSize{};
    bool repeatX = false;
    bool repeatY = false;
    graphics::Texture texture;

    std::vector<Layer> layers;

    [[nodiscard]] static std::string_view kindName(Kind value) noexcept;

    // Returns the global tile id at a cell, including flip flags, or zero outside the layer.
    [[nodiscard]] std::uint32_t getGid(int column, int row) const noexcept;

    // Tells whether a cell lies inside the layer, or inside one of its chunks on an infinite map.
    [[nodiscard]] bool contains(int column, int row) const noexcept;
    void setGid(int column, int row, std::uint32_t value);
};

} // namespace haylen::tiled
