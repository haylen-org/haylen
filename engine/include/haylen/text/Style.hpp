#pragma once

#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Alignment.hpp"
#include "haylen/text/Balance.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/text/Gradient.hpp"
#include "haylen/text/LineHeight.hpp"

namespace haylen::text {

// How a block of text looks and where it sits. A positive maximum width wraps lines where the Unicode line breaking rules allow, unless wrapping is off, where every line stays whole, and the balance decides how the wrapped lines of a paragraph share its words, which by default keeps the last line from holding one word alone. Each line is as tall as its fonts and the ink of its glyphs, or every line as tall as the tallest one with a uniform line height, and the line spacing multiplies that height. A minimum scale below 1 fits the block in its box, the maximum width and a positive maximum height, by laying it out at the largest size down to that share of the size where it fits: wrapped text until its lines fit the height, and text that does not wrap until its widest line fits the width too, while text that fits at the size keeps it, and the anchor is a fraction of the block size that lands on the draw position, where the rotation turns the block and the scale stretches it without laying the text out again. Outlines, blurred shadows and gradients need a font with a distance field. Bold and italic pick those faces of a family, synthesized when it lacks them. The direction reads every paragraph left to right, right to left, or from its first strong letter, and the language, a BCP 47 tag such as `ar`, `hi` or `ja`, picks the letter forms and line breaks of its script. Pixel snapping puts the left edge and the baseline of every line on whole pixels of the destination, which keeps small text crisp, when neither the text nor its canvas turns. Ligatures join letters such as f and i into one glyph where the font draws them, which suits prose, and turning them off keeps a glyph for every letter, which code and identifiers need. Code keeps a glyph for every letter too and breaks its lines only between its tokens, never inside a string or the indentation of a line. Any text keeps a path, an address or a hyphenated name whole while it fits on a line.
struct Style {
    float size = 32.0F;
    math::Color color = math::Color::white();
    float outlineWidth = 0.0F;
    math::Color outlineColor = math::Color::black();
    math::Vec2 shadowOffset{};
    math::Color shadowColor = math::Color::transparent();
    float shadowBlur = 0.0F;

    // Fills the glyphs with colors that blend along a direction across each line, multiplied by the color.
    std::optional<Gradient> gradient;
    Alignment align = Alignment::Start;
    float maxWidth = 0.0F;
    float maxHeight = 0.0F;
    float minScale = 1.0F;
    bool wrap = true;
    Balance balance = Balance::LastLine;
    float lineSpacing = 1.2F;
    LineHeight lineHeight = LineHeight::Natural;
    math::Vec2 anchor{};
    float rotation = 0.0F;
    math::Vec2 scale{1.0F, 1.0F};
    bool bold = false;
    bool italic = false;

    // Draws a line under the text or through it, in its color.
    bool underline = false;
    bool strike = false;
    Direction direction = Direction::Auto;
    std::string language;
    bool pixelSnap = false;
    bool ligatures = true;
    bool code = false;

    // The alignment names `start`, `end`, `left`, `center`, `right` and `fill`, the direction names `auto`, `leftToRight` and `rightToLeft`, the balance names `none`, `lastLine` and `allLines`, and the line height names `natural` and `uniform`, which Lua, markup and GUIs share.
    static const std::array<std::pair<std::string_view, Alignment>, 6> kAlignmentNames;
    static const std::array<std::pair<std::string_view, Direction>, 3> kDirectionNames;
    static const std::array<std::pair<std::string_view, Balance>, 3> kBalanceNames;
    static const std::array<std::pair<std::string_view, LineHeight>, 2> kLineHeightNames;

    [[nodiscard]] static std::optional<Alignment> alignmentFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view alignmentName(Alignment value) noexcept;
    [[nodiscard]] static std::optional<Direction> directionFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view directionName(Direction value) noexcept;
    [[nodiscard]] static std::optional<Balance> balanceFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view balanceName(Balance value) noexcept;
    [[nodiscard]] static std::optional<LineHeight> lineHeightFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view lineHeightName(LineHeight value) noexcept;
};

} // namespace haylen::text
