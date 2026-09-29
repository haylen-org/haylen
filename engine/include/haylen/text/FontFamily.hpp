#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::text {

class LayoutCache;

// The faces of one typeface: regular, bold, italic, bold italic and mono, plus fallback fonts for the code points a face lacks, such as an Arabic, Devanagari, CJK or symbol font. A style the family has no face for is synthesized from a face it has, which only fonts with a distance field do well.
class FontFamily final {
  public:
    struct Faces {
        std::shared_ptr<Font> regular;
        std::shared_ptr<Font> bold;
        std::shared_ptr<Font> italic;
        std::shared_ptr<Font> boldItalic;
        std::shared_ptr<Font> mono;
        std::vector<std::shared_ptr<Font>> fallbacks;
    };

    // The face that draws a style, and the styles it must synthesize because the family lacks the real face.
    struct Selection {
        Font* font = nullptr;
        bool syntheticBold = false;
        bool syntheticItalic = false;
    };

    explicit FontFamily(Faces familyFaces);
    ~FontFamily();

    FontFamily(const FontFamily&) = delete;
    FontFamily& operator=(const FontFamily&) = delete;

    [[nodiscard]] const Faces& getFaces() const noexcept {
        return faces;
    }

    // Picks the face for a style. Mono text uses the mono face, or the regular faces when the family has none, and the mono face synthesizes bold and italic.
    [[nodiscard]] Selection select(bool bold, bool italic, bool mono) const noexcept;

    // Picks the font that draws one character, a letter with the marks and joiners of its cluster: the selected face when it has every code point that shows, otherwise the first fallback that has them all, which synthesizes the requested styles, then the first font that has the letter itself, and otherwise the selected face, which draws its missing glyph. Spaces, punctuation and digits pass the font of the text before them, which they keep when it is a fallback that has them, so a run of another script keeps its own spaces and punctuation.
    [[nodiscard]] Selection resolve(const Selection& face, std::u32string_view cluster, bool bold, bool italic, Font* previous = nullptr) const;

    // Lays UTF-8 text out like a font does, with the faces the bold and italic of the style pick and the fallbacks for what they lack. Layouts are cached by text and style.
    [[nodiscard]] std::shared_ptr<const Layout> layout(std::string_view text, const Style& style);
    [[nodiscard]] math::Vec2 measure(std::string_view text, const Style& style);

  private:
    static debug::ObjectCounter& counter;

    [[nodiscard]] static bool covers(Font& font, std::u32string_view cluster);

    Faces faces;
    std::unique_ptr<LayoutCache> layouts;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::text
