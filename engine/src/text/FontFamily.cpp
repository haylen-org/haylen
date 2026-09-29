#include "haylen/text/FontFamily.hpp"

#include <stdexcept>
#include <utility>

namespace haylen::text {

debug::ObjectCounter FontFamily::counter("FontFamily", debug::ObjectCounter::Kind::Native);

FontFamily::FontFamily(Faces familyFaces) : faces(std::move(familyFaces)) {
    if (!faces.regular) {
        throw std::invalid_argument("A font family needs a regular face.");
    }
    for (const std::shared_ptr<Font>& fallback : faces.fallbacks) {
        if (!fallback) {
            throw std::invalid_argument("A font family fallback must be a font.");
        }
    }
}

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

FontFamily::Selection FontFamily::resolve(const Selection& face, char32_t codePoint, bool bold, bool italic) const {
    if (face.font->hasGlyph(codePoint)) {
        return face;
    }
    for (const std::shared_ptr<Font>& fallback : faces.fallbacks) {
        if (fallback->hasGlyph(codePoint)) {
            return {.font = fallback.get(), .syntheticBold = bold, .syntheticItalic = italic};
        }
    }
    return face;
}

} // namespace haylen::text
