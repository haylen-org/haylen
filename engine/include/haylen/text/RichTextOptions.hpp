#pragma once

#include <functional>
#include <memory>
#include <string_view>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/TextAlign.hpp"

namespace haylen::text {

// How rich text lays out: the family and size of text without [font] or [size], the color, the width it wraps at, the alignment of paragraphs without their own, the scale of every size in the markup and options, and how many characters per second the reveal shows, where zero shows everything at once. Fonts resolves the names of [font] tags and images the paths of [img] tags, which may return an empty texture while an image still loads.
struct RichTextOptions {
    std::shared_ptr<FontFamily> family;
    float size = 32.0F;
    math::Color color = math::Color::white();
    float maxWidth = 0.0F;
    TextAlign align = TextAlign::Left;
    float lineSpacing = 1.2F;
    float scale = 1.0F;
    float revealSpeed = 0.0F;
    bool underlineLinks = true;
    std::function<std::shared_ptr<FontFamily>(std::string_view name)> fonts;
    std::function<graphics::Texture(std::string_view path)> images;
};

} // namespace haylen::text
