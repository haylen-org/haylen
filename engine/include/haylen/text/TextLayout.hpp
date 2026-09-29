#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/RichTextDocument.hpp"

namespace haylen::text {

class Font;
class FontFamily;

// Text shaped and placed in a block whose top-left is the origin, plain text and rich text alike: every glyph with how it draws, the filled boxes of backgrounds, underlines, strikes, rules and table borders, the inline images, the boxes of links and hints, the characters and the lines. Glyphs sit in visual order, so right-to-left runs read from the right. Characters are the clusters of the text, a letter with its marks or the letters a ligature joins, and they count in reading order, one more per image or icon, so a reveal shows them in the order they are read.
struct TextLayout {
    // How the glyphs of one style draw with one font besides their color. Lengths are pixels: the outline, the weight a synthetic bold adds to each side of a stroke of a font with a distance field, and the offset of the second copy a synthetic bold draws with a bitmap font. The skew leans a synthetic italic.
    struct Look {
        Font* font = nullptr;
        float size = 0.0F;
        float outlineWidth = 0.0F;
        math::Color outlineColor = math::Color::black();
        float weight = 0.0F;
        float emboldenOffset = 0.0F;
        float skew = 0.0F;
        std::optional<RichTextDocument::Shadow> shadow;
        std::optional<RichTextDocument::Glow> glow;
    };

    // A glyph with its index in its font, the top-left and size of its quad, its baseline, which effects and the skew move it around, and the page region it shows. The code point is the first one of its character.
    struct Glyph {
        std::size_t look = 0;
        std::size_t style = 0;
        std::size_t character = 0;
        std::uint32_t index = 0;
        char32_t codePoint = 0;
        math::Vec2 position{};
        math::Vec2 size{};
        float baseline = 0.0F;
        math::Rect source{};
        std::uint16_t page = 0;
        math::Color color = math::Color::white();
        bool visible = true;
    };

    // A filled rectangle. Backgrounds, underlines and strikes cover the characters of their text and grow with the reveal from the side their text starts, while rules and the backgrounds and borders of table cells show once the reveal reaches the character after them.
    struct Box {
        enum class Kind : std::uint8_t {
            Background,
            Underline,
            Strike,
            Rule,
            CellBackground,
            CellBorder,
        };

        Kind kind = Kind::Background;
        math::Rect rect{};
        math::Color color = math::Color::white();
        std::size_t firstCharacter = 0;
        std::size_t lastCharacter = 0;
        bool rightToLeft = false;
        bool visible = true;
    };

    struct Image {
        graphics::Texture texture;
        math::Rect rect{};
        math::Rect source{};
        math::Color color = math::Color::white();
        std::size_t character = 0;
        bool visible = true;
    };

    // One rectangle a link or a hint covers, several when it wraps or its text changes direction, with the index of its link or hint in the document.
    struct Area {
        math::Rect rect{};
        std::size_t index = 0;
    };

    // Where each character sits and which code points of the text it draws, counted in the text without markup where paragraphs end with a line break, whether it reads right to left, and how the reveal reaches it: the pause before it and the speed of its style.
    struct Character {
        math::Rect box{};
        std::size_t begin = 0;
        std::size_t end = 0;
        bool rightToLeft = false;
        float pause = 0.0F;
        float speed = 1.0F;
    };

    // A line of text: the box its characters fill, which is empty where the line starts when it has none, its baseline, the range of its characters, the range of the code points of the text without markup it holds and the direction of its paragraph.
    struct Line {
        math::Rect box{};
        float baseline = 0.0F;
        std::size_t firstCharacter = 0;
        std::size_t endCharacter = 0;
        std::size_t begin = 0;
        std::size_t end = 0;
        bool rightToLeft = false;
    };

    std::vector<Look> looks;
    std::vector<Glyph> glyphs;
    std::vector<Box> boxes;
    std::vector<Image> images;
    std::vector<Area> links;
    std::vector<Area> hints;
    std::vector<Character> characters;
    std::vector<Line> lines;
    math::Vec2 size{};

    // Every line of text, and every row of a table, which counts as one.
    std::size_t lineCount = 0;

    // An image that was still loading took no room, so the layout must be built again once it arrives.
    bool waitingForImages = false;

    // The families of the fonts the looks of rich text point to, which stay alive as long as the layout. A layout of plain text points to the fonts of the font or family that made it, which must outlive it.
    std::vector<std::shared_ptr<FontFamily>> families;
};

} // namespace haylen::text
