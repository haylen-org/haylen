#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Alignment.hpp"
#include "haylen/text/Direction.hpp"

namespace haylen::text {

// How a block of text looks and where it sits. A positive maximum width wraps lines where the Unicode line breaking rules allow, and the anchor is a fraction of the block size that lands on the draw position, where the rotation turns the block and the scale stretches it without laying the text out again. Outlines and blurred shadows need a font with a distance field. Bold and italic pick those faces of a family, synthesized when it lacks them. The direction reads every paragraph left to right, right to left, or from its first strong letter, and the language, a BCP 47 tag such as `ar`, `hi` or `ja`, picks the letter forms and line breaks of its script. Pixel snapping puts the left edge of the block and the baseline of every line on whole pixels of the destination, which keeps small text crisp, when neither the text nor its canvas turns.
struct Style {
    float size = 32.0F;
    math::Color color = math::Color::white();
    float outlineWidth = 0.0F;
    math::Color outlineColor = math::Color::black();
    math::Vec2 shadowOffset{};
    math::Color shadowColor = math::Color::transparent();
    float shadowBlur = 0.0F;
    Alignment align = Alignment::Start;
    float maxWidth = 0.0F;
    float lineSpacing = 1.2F;
    math::Vec2 anchor{};
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};
    bool bold = false;
    bool italic = false;
    Direction direction = Direction::Auto;
    std::string language;
    bool pixelSnap = false;

    // The alignment names `start`, `end`, `left`, `center`, `right` and `fill` and the direction names `auto`, `leftToRight` and `rightToLeft`, which Lua, markup and GUIs share.
    static const std::array<std::pair<std::string_view, Alignment>, 6> kAlignmentNames;
    static const std::array<std::pair<std::string_view, Direction>, 3> kDirectionNames;

    [[nodiscard]] static std::optional<Alignment> alignmentFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view alignmentName(Alignment value) noexcept;
    [[nodiscard]] static std::optional<Direction> directionFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view directionName(Direction value) noexcept;
};

} // namespace haylen::text
