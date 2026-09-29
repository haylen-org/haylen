#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/Layout.hpp"
#include "haylen/text/RichTextDocument.hpp"
#include "haylen/text/RichTextOptions.hpp"
#include "haylen/text/RichTextRegistry.hpp"
#include "haylen/text/Style.hpp"
#include "text/Segmenter.hpp"

namespace haylen::text {

class BidiParagraph;

// Lays a rich text document out: it shapes every paragraph into clusters with the fonts of their styles and scripts, breaks paragraphs into lines where the Unicode rules allow, orders every line for display by the Unicode Bidirectional Algorithm, sizes tables, and places glyphs, boxes, images and hit areas. Plain text lays out the same way as a document of one run per line.
class LayoutBuilder final {
  public:
    LayoutBuilder(const RichTextDocument& source, const RichTextOptions& layoutOptions, const RichTextRegistry& textRegistry);

    [[nodiscard]] Layout build();

    // Lays plain text out with a family, or with a lone font when there is no family, where every paragraph separator of the text ends a paragraph and CRLF counts as one.
    [[nodiscard]] static Layout layoutPlainText(std::string_view text, const Style& style, const FontFamily* baseFamily, Font* baseFont);

  private:
    // One unit a line holds in reading order: a cluster of glyphs, an inline image or icon, a line break, or a pause of the reveal before the next character. Code points count in the text of the paragraph, the glyphs of a cluster stand in visual order from its left edge, and the break before it is what the Unicode rules allow there.
    struct Piece {
        enum class Kind : std::uint8_t {
            Cluster,
            Image,
            LineBreak,
            Pause,
        };

        Kind kind = Kind::Cluster;
        std::size_t style = 0;
        std::size_t look = 0;
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t firstGlyph = 0;
        std::size_t glyphCount = 0;
        float advance = 0.0F;
        float ascent = 0.0F;
        float descent = 0.0F;
        std::uint8_t level = 0;
        bool space = false;
        Segmenter::Break breakBefore = Segmenter::Break::Never;
        graphics::Texture texture;
        math::Rect source{};
        math::Vec2 extent{};
        math::Color tint = math::Color::white();
        RichTextDocument::VerticalAlign align = RichTextDocument::VerticalAlign::Center;
        float lift = 0.0F;
        float pause = 0.0F;
    };

    // A glyph of a cluster at the scale of its text, where the offset places its pen from the left edge of the cluster.
    struct PlacedGlyph {
        Font::Glyph glyph;
        std::uint32_t index = 0;
        float factor = 1.0F;
        math::Vec2 offset{};
    };

    // The pieces a line holds, the width up to its last visible piece, its ascent and descent, where it starts below the paragraph top, how far a drop cap pushes it, whether it wrapped, which lets filled lines stretch, and its drawable pieces from left to right.
    struct Line {
        std::size_t begin = 0;
        std::size_t end = 0;
        std::size_t visibleEnd = 0;
        float width = 0.0F;
        float ascent = 0.0F;
        float descent = 0.0F;
        float top = 0.0F;
        float shift = 0.0F;
        bool wrapped = false;
        std::vector<std::size_t> order;
    };

    // The text of a paragraph with the style of every code point, the object behind each object replacement character and the pauses before code points.
    struct Source {
        std::u32string text;
        std::vector<std::size_t> styles;
        std::map<std::size_t, const RichTextDocument::Inline*> objects;
        std::vector<std::pair<std::size_t, float>> pauses;
    };

    // A paragraph broken into lines, with its drop cap and list marker pieces and their visual orders. The widest segment that cannot wrap is the narrowest the paragraph can get.
    struct Flow {
        std::u32string text;
        std::u32string dropCapText;
        bool rightToLeft = false;
        std::vector<Piece> pieces;
        std::vector<PlacedGlyph> glyphs;
        std::vector<Line> lines;
        std::vector<Piece> dropCap;
        std::vector<std::size_t> dropCapOrder;
        std::vector<Piece> marker;
        std::vector<std::size_t> markerOrder;
        float dropCapAscent = 0.0F;
        float dropCapHeight = 0.0F;
        float dropCapShift = 0.0F;
        float emptyAscent = 0.0F;
        float emptyDescent = 0.0F;
        float naturalWidth = 0.0F;
        float minimumWidth = 0.0F;
    };

    struct Block;

    struct TableBlock {
        std::vector<float> columns;
        std::vector<float> rows;
        std::vector<std::vector<Block>> cells;
        bool rightToLeft = false;
    };

    // A laid out paragraph, rule or table with its height and widths. The trailing space is the line spacing below its last line, which the last block of a column leaves out.
    struct Block {
        const RichTextDocument::Paragraph* paragraph = nullptr;
        Flow flow;
        std::optional<TableBlock> table;
        float height = 0.0F;
        float trailing = 0.0F;
        float naturalWidth = 0.0F;
        float minimumWidth = 0.0F;
    };

    // The family, face, styles and size a style draws with, where a lone font has no family and so no fallbacks.
    struct StyleFont {
        const FontFamily* family = nullptr;
        FontFamily::Selection face;
        bool bold = false;
        bool italic = false;
        float size = 0.0F;
    };

    // A drawable piece placed on its line, from left to right.
    struct Placed {
        std::size_t piece = 0;
        float x = 0.0F;
        std::size_t character = 0;
    };

    // Synthetic bold adds this fraction of the text size to each side of a stroke, and synthetic italic leans glyphs by this shift per unit of height.
    static constexpr float kSyntheticWeight = 0.03F;
    static constexpr float kSyntheticSkew = 0.2F;

    // One indent level, the gap between a list marker and its item and the space around a rule, as fractions of the base text size.
    static constexpr float kIndentEms = 1.5F;
    static constexpr float kMarkerGapEms = 0.4F;
    static constexpr float kRuleMarginEms = 0.4F;

    // Underlines and strikes sit at these fractions of the text size below and above the baseline, with this thickness.
    static constexpr float kUnderlineOffset = 0.1F;
    static constexpr float kStrikeOffset = 0.3F;
    static constexpr float kDecorationThickness = 0.06F;

    // An object replacement character stands for an image or an icon, and a line separator for a line break inside a paragraph.
    static constexpr char32_t kObject = U'\U0000FFFC';
    static constexpr char32_t kLineSeparator = U'\U00002028';

    LayoutBuilder(const RichTextDocument& source, const RichTextOptions& layoutOptions, const RichTextRegistry* textRegistry, const FontFamily* baseFamily, Font* baseFont);

    [[nodiscard]] static Segmenter::Break breakBefore(const std::vector<Piece>& pieces, std::size_t index) noexcept;
    [[nodiscard]] static Alignment resolveAlign(Alignment align, bool rightToLeft, bool lastLine) noexcept;
    [[nodiscard]] static float alignOffset(Alignment align, float room, float width) noexcept;
    [[nodiscard]] static std::vector<std::size_t> orderPieces(const std::vector<Piece>& pieces, std::size_t begin, std::size_t end, const BidiParagraph& bidi);

    [[nodiscard]] const StyleFont& getStyleFont(std::size_t style);
    [[nodiscard]] std::size_t getLook(std::size_t style, const FontFamily::Selection& face);
    [[nodiscard]] math::Color getColor(std::size_t style) const;
    [[nodiscard]] float getIndentUnit() const noexcept;
    [[nodiscard]] Direction getDirection(const RichTextDocument::Paragraph& paragraph) const noexcept;

    [[nodiscard]] std::vector<Piece> shape(const Source& source, const BidiParagraph& bidi, std::vector<PlacedGlyph>& glyphs);
    void shapeRun(const Source& source, std::size_t begin, std::size_t end, const FontFamily::Selection& face, std::uint8_t level, std::uint32_t script, std::vector<Piece>& pieces, std::vector<PlacedGlyph>& glyphs);
    [[nodiscard]] Piece measureImage(const RichTextDocument::Inline& item);
    [[nodiscard]] Piece measureObject(std::size_t style, graphics::Texture texture, math::Rect source, math::Vec2 extent, math::Color tint, RichTextDocument::VerticalAlign align);
    void shapeAside(Flow& flow, std::u32string_view text, std::size_t style, std::vector<Piece>& pieces, std::vector<std::size_t>& order);

    [[nodiscard]] std::vector<Block> measureBlocks(const std::vector<RichTextDocument::Paragraph>& paragraphs, float width);
    [[nodiscard]] Block measureParagraph(const RichTextDocument::Paragraph& paragraph, float width);
    [[nodiscard]] Block measureRule(const RichTextDocument::Paragraph& paragraph);
    [[nodiscard]] Block measureTable(const RichTextDocument::Paragraph& paragraph, float width);
    void breakLines(Flow& flow, float available);
    void closeLine(Flow& flow, Line& line, float& top) const;

    float emitBlocks(const std::vector<Block>& blocks, math::Vec2 origin, float width);
    void emitParagraph(const Block& block, math::Vec2 origin, float width);
    void emitRule(const Block& block, math::Vec2 origin, float width);
    void emitTable(const Block& block, math::Vec2 origin, float width);
    void emitPiece(const Flow& flow, std::u32string_view text, const Piece& piece, math::Vec2 pen, float lineTop, float lineHeight, std::size_t character);
    void emitDecorations(const Flow& flow, const Line& line, const std::vector<Placed>& placed, float baseline);
    void addBox(Layout::Box::Kind kind, const math::Rect& rect, math::Color color, std::size_t firstCharacter, std::size_t lastCharacter, bool rightToLeft = false);
    [[nodiscard]] std::size_t addCharacter(const Piece& piece, std::size_t offset, bool rightToLeft);

    const RichTextDocument& document;
    const RichTextOptions& options;
    const RichTextRegistry* registry;
    const FontFamily* family;
    Font* loneFont;
    Layout layout;
    std::vector<std::optional<StyleFont>> styleFonts;
    std::map<std::string, std::shared_ptr<FontFamily>, std::less<>> families;
    std::map<std::pair<std::size_t, const Font*>, std::size_t> looks;
    std::vector<Font::ShapedGlyph> shapedGlyphs;
    float pendingPause = 0.0F;
    std::size_t textOffset = 0;
};

} // namespace haylen::text
