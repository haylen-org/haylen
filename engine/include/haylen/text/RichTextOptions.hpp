#pragma once

#include <functional>
#include <memory>
#include <string>
#include <string_view>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/text/Alignment.hpp"
#include "haylen/text/Direction.hpp"
#include "haylen/text/FontFamily.hpp"

namespace haylen::text {

// How rich text lays out: the family and size of text without [font] or [size], whether all of it is bold or italic as if inside [b] or [i], the color, the width it wraps at, the alignment and direction of paragraphs without their own, the language of the text, the scale of every size in the markup and options, and how many characters per second the reveal shows, where zero shows everything at once. Fonts resolves the names of [font] tags and images the paths of [img] tags, which may return an empty texture while an image still loads.
struct RichTextOptions {
    std::shared_ptr<FontFamily> family;
    float size = 32.0F;
    bool bold = false;
    bool italic = false;
    math::Color color = math::Color::white();
    float maxWidth = 0.0F;
    Alignment align = Alignment::Start;
    Direction direction = Direction::Auto;
    std::string language;
    float lineSpacing = 1.2F;
    float scale = 1.0F;
    float revealSpeed = 0.0F;
    bool underlineLinks = true;
    std::function<std::shared_ptr<FontFamily>(std::string_view name)> fonts;
    std::function<graphics::Texture(std::string_view path)> images;
};

} // namespace haylen::text
