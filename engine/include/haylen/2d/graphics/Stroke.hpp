#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::graphics2d {

// How a line through points draws: its width and color, given once or for each point, how its segments join and its ends cap, an image stretched or repeated along its length and a pattern of dashes. The spans point at values the caller keeps until the draw returns.
struct Stroke {
    enum class Join : std::uint8_t {
        Miter,
        Round,
        Bevel,
    };

    enum class Cap : std::uint8_t {
        Butt,
        Round,
        Square,
    };

    enum class TextureMode : std::uint8_t {
        Stretch,
        Tile,
    };

    float width = 1.0F;

    // One width for each point, which replaces `width` and blends along each segment.
    std::span<const float> widths;
    math::Color color = math::Color::white();

    // One color for each point, which replaces `color` and blends along each segment.
    std::span<const math::Color> colors;
    Join join = Join::Miter;
    Cap cap = Cap::Butt;

    // A miter join longer than this many widths becomes a bevel.
    float miterLimit = 4.0F;
    bool closed = false;
    graphics::Texture texture;
    TextureMode textureMode = TextureMode::Stretch;

    // The length one copy of the texture covers in tile mode, or the width times the aspect of the texture when 0.
    float textureLength = 0.0F;

    // Lengths drawn and left out in turn along the stroke, repeated, where a list of an odd count repeats twice to pair up, starting `dashOffset` into the pattern.
    std::span<const float> dashes;
    float dashOffset = 0.0F;

    [[nodiscard]] static std::optional<Join> joinFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view joinName(Join value) noexcept;
    [[nodiscard]] static std::optional<Cap> capFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view capName(Cap value) noexcept;
    [[nodiscard]] static std::optional<TextureMode> textureModeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view textureModeName(TextureMode value) noexcept;

    static constexpr std::array<std::pair<std::string_view, Join>, 3> kJoinNames{{{"miter", Join::Miter}, {"round", Join::Round}, {"bevel", Join::Bevel}}};
    static constexpr std::array<std::pair<std::string_view, Cap>, 3> kCapNames{{{"butt", Cap::Butt}, {"round", Cap::Round}, {"square", Cap::Square}}};
    static constexpr std::array<std::pair<std::string_view, TextureMode>, 2> kTextureModeNames{{{"stretch", TextureMode::Stretch}, {"tile", TextureMode::Tile}}};
};

} // namespace haylen::graphics2d
