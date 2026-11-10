#include "haylen/text/FontFamily.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "text/LayoutBuilder.hpp"
#include "text/LayoutCache.hpp"
#include "text/Segmenter.hpp"
#include "text/TextFit.hpp"

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

template <typename Accept> const std::shared_ptr<Font>* FontFamily::findFallback(Accept&& accept) const {
    for (const std::shared_ptr<Font>& font : faces.fallbacks) {
        if (accept(*font)) {
            return &font;
        }
    }
    if (!faces.systemFonts) {
        return nullptr;
    }
    for (const SystemFonts::Face& face : faces.systemFonts->getFaces()) {
        if (accept(*face.font)) {
            return &face.font;
        }
    }
    return nullptr;
}

FontFamily::Selection FontFamily::resolve(const Selection& face, std::u32string_view cluster, bool bold, bool italic, Font* previous) const {
    const auto fallback = [bold, italic](const std::shared_ptr<Font>* font) { return Selection{.font = font->get(), .syntheticBold = bold, .syntheticItalic = italic}; };
    if (previous != nullptr && previous != face.font && covers(*previous, cluster)) {
        if (const std::shared_ptr<Font>* kept = findFallback([previous](Font& font) { return &font == previous; })) {
            return fallback(kept);
        }
    }

    // A character that shows as an emoji by default or through U+FE0F takes the first font that draws it as a color glyph, and one that U+FE0E asks to show as text the first font that draws it in the color of the text.
    const bool text = std::ranges::find(cluster, kTextPresentation) != cluster.end();
    const bool emoji = !text && !cluster.empty() && (std::ranges::find(cluster, kEmojiPresentation) != cluster.end() || Segmenter::hasEmojiPresentation(cluster.front()));
    if (emoji || text) {
        if (covers(*face.font, cluster) && face.font->hasColorGlyph(cluster.front()) == emoji) {
            return face;
        }
        if (const std::shared_ptr<Font>* presented = findFallback([cluster, emoji](Font& font) { return covers(font, cluster) && font.hasColorGlyph(cluster.front()) == emoji; })) {
            return fallback(presented);
        }
    }

    if (covers(*face.font, cluster)) {
        return face;
    }
    if (const std::shared_ptr<Font>* covering = findFallback([cluster](Font& font) { return covers(font, cluster); })) {
        return fallback(covering);
    }
    if (cluster.empty() || face.font->hasGlyph(cluster.front())) {
        return face;
    }
    if (const std::shared_ptr<Font>* letter = findFallback([cluster](Font& font) { return font.hasGlyph(cluster.front()); })) {
        return fallback(letter);
    }

    // No font holds the letter, so the family asks the fonts of the system for one and draws the missing glyph of the face until it arrives.
    Selection missing = face;
    missing.waiting = faces.systemFonts && faces.systemFonts->request(cluster.front());
    return missing;
}

std::shared_ptr<const Layout> FontFamily::layout(std::string_view text, const Style& style) {
    if (faces.systemFonts && faces.systemFonts->getRevision() != systemRevision) {
        systemRevision = faces.systemFonts->getRevision();
        layouts->dropWaitingForFonts();
    }
    if (std::shared_ptr<const Layout> cached = layouts->find(text, style)) {
        return cached;
    }
    return layouts->store(text, style, TextFit::layout(style, [this, text](const Style& laid) { return LayoutBuilder::layoutPlainText(text, laid, this, nullptr); }));
}

math::Vec2 FontFamily::measure(std::string_view text, const Style& style) {
    return layout(text, style)->size * style.scale;
}

} // namespace haylen::text
