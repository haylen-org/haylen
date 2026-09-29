#include "haylen/text/FontFamily.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "text/LayoutBuilder.hpp"
#include "text/LayoutCache.hpp"
#include "text/Segmenter.hpp"

namespace haylen::text {

debug::ObjectCounter& FontFamily::counter = *new debug::ObjectCounter("FontFamily", debug::ObjectCounter::Kind::Native);

FontFamily::FontFamily(Faces familyFaces) : faces(std::move(familyFaces)), layouts(std::make_unique<LayoutCache>()) {
    if (!faces.regular) {
        throw std::invalid_argument("A font family needs a regular face.");
    }
    for (const std::shared_ptr<Font>& fallback : faces.fallbacks) {
        if (!fallback) {
            throw std::invalid_argument("A font family fallback must be a font.");
        }
    }
}

FontFamily::~FontFamily() = default;

FontFamily::Selection FontFamily::select(bool bold, bool italic, bool mono) const noexcept {
    if (mono && faces.mono) {
        return {.font = faces.mono.get(), .syntheticBold = bold, .syntheticItalic = italic};
    }
    if (bold && italic) {
        if (faces.boldItalic) {
            return {.font = faces.boldItalic.get()};
        }
        if (faces.bold) {
            return {.font = faces.bold.get(), .syntheticItalic = true};
        }
        if (faces.italic) {
            return {.font = faces.italic.get(), .syntheticBold = true};
        }
        return {.font = faces.regular.get(), .syntheticBold = true, .syntheticItalic = true};
    }
    if (bold) {
        return faces.bold ? Selection{.font = faces.bold.get()} : Selection{.font = faces.regular.get(), .syntheticBold = true};
    }
    if (italic) {
        return faces.italic ? Selection{.font = faces.italic.get()} : Selection{.font = faces.regular.get(), .syntheticItalic = true};
    }
    return {.font = faces.regular.get()};
}

// Joiners, variation selectors and direction marks draw nothing, so a font covers a cluster without them.
bool FontFamily::covers(Font& font, std::u32string_view cluster) {
    return std::ranges::all_of(cluster, [&font](char32_t codePoint) { return Segmenter::isInvisible(codePoint) || font.hasGlyph(codePoint); });
}

FontFamily::Selection FontFamily::resolve(const Selection& face, std::u32string_view cluster, bool bold, bool italic, Font* previous) const {
    const auto fallback = [bold, italic](const std::shared_ptr<Font>& font) { return Selection{.font = font.get(), .syntheticBold = bold, .syntheticItalic = italic}; };
    if (previous != nullptr && previous != face.font && covers(*previous, cluster)) {
        for (const std::shared_ptr<Font>& font : faces.fallbacks) {
            if (font.get() == previous) {
                return fallback(font);
            }
        }
    }
    if (covers(*face.font, cluster)) {
        return face;
    }
    for (const std::shared_ptr<Font>& font : faces.fallbacks) {
        if (covers(*font, cluster)) {
            return fallback(font);
        }
    }
    if (cluster.empty() || face.font->hasGlyph(cluster.front())) {
        return face;
    }
    for (const std::shared_ptr<Font>& font : faces.fallbacks) {
        if (font->hasGlyph(cluster.front())) {
            return fallback(font);
        }
    }
    return face;
}

std::shared_ptr<const Layout> FontFamily::layout(std::string_view text, const Style& style) {
    return layouts->get(text, style, [&] { return LayoutBuilder::layoutPlainText(text, style, this, nullptr); });
}

math::Vec2 FontFamily::measure(std::string_view text, const Style& style) {
    return layout(text, style)->size;
}

} // namespace haylen::text
