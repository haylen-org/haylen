#pragma once

#include <memory>
#include <vector>

#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/text/Font.hpp"

namespace haylen::text {

// The faces of one typeface: regular, bold, italic, bold italic and mono, plus fallback fonts for the code points a face lacks, such as a CJK or symbol font. A style the family has no face for is synthesized from a face it has, which only fonts with a distance field do well.
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

    [[nodiscard]] const Faces& getFaces() const noexcept {
        return faces;
    }

    // Picks the face for a style. Mono text uses the mono face, or the regular faces when the family has none, and the mono face synthesizes bold and italic.
    [[nodiscard]] Selection select(bool bold, bool italic, bool mono) const noexcept;

    // Picks the font that draws a code point in a style: the selected face when it has the glyph, otherwise the first fallback that has it, which synthesizes the requested styles, and otherwise the selected face, which draws its missing glyph.
    [[nodiscard]] Selection resolve(const Selection& face, char32_t codePoint, bool bold, bool italic) const;

  private:
    static debug::ObjectCounter counter;

    Faces faces;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::text
