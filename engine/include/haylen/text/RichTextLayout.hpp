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
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichTextDocument.hpp"

namespace haylen::text {

// Rich text placed in a block whose top-left is the origin: every glyph with how it draws, the filled boxes of backgrounds, underlines, strikes, rules and table borders, the inline images and the boxes of links and hints. Characters count in reading order, one per code point and one per image or icon, and each drawable knows the characters it belongs to so a reveal can show them in turn.
struct RichTextLayout {
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

    // A glyph with the top-left and size of its quad, its baseline, which effects and the skew move it around, and the page region it shows.
    struct Glyph {
        std::size_t look = 0;
        std::size_t style = 0;
        std::size_t character = 0;
        char32_t codePoint = 0;
        math::Vec2 position{};
        math::Vec2 size{};
        float baseline = 0.0F;
        math::Rect source{};
        std::uint16_t page = 0;
        math::Color color = math::Color::white();
        bool visible = true;
    };

    // A filled rectangle. Backgrounds, underlines and strikes cover the characters of their text and grow with the reveal, while rules and the backgrounds and borders of table cells show once the reveal reaches the character after them.
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

    // One rectangle a link or a hint covers, several when it wraps, with the index of its link or hint in the document.
    struct Area {
        math::Rect rect{};
        std::size_t index = 0;
    };

    // Where each character sits, and how the reveal reaches it: the pause before it and the speed of its style.
    struct Character {
        math::Rect box{};
        float pause = 0.0F;
        float speed = 1.0F;
    };

    std::vector<Look> looks;
    std::vector<Glyph> glyphs;
    std::vector<Box> boxes;
    std::vector<Image> images;
    std::vector<Area> links;
    std::vector<Area> hints;
    std::vector<Character> characters;
    math::Vec2 size{};
    std::size_t lineCount = 0;

    // An image that was still loading took no room, so the layout must be built again once it arrives.
    bool waitingForImages = false;

    // The families of the fonts the looks point to, which stay alive as long as the layout.
    std::vector<std::shared_ptr<FontFamily>> families;
};

} // namespace haylen::text
