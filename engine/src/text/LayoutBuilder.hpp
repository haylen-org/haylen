#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Font.hpp"
#include "haylen/text/FontFamily.hpp"
#include "haylen/text/RichTextDocument.hpp"
#include "haylen/text/RichTextLayout.hpp"
#include "haylen/text/RichTextOptions.hpp"
#include "haylen/text/RichTextRegistry.hpp"

namespace haylen::text {

// Lays a rich text document out: it measures every piece of every paragraph with its font, breaks paragraphs into lines, sizes tables, and places glyphs, boxes, images and hit areas in reading order.
class LayoutBuilder final {
  public:
    LayoutBuilder(const RichTextDocument& source, const RichTextOptions& layoutOptions, const RichTextRegistry& textRegistry);

    [[nodiscard]] RichTextLayout build();

  private:
    // One unit a line holds: a glyph, an inline image or icon, a line break, or a pause of the reveal before the next character.
    struct Piece {
        enum class Kind : std::uint8_t {
            Glyph,
            Image,
            LineBreak,
            Pause,
        };

        Kind kind = Kind::Glyph;
        std::size_t style = 0;
        std::size_t look = 0;
        char32_t codePoint = 0;
        Font::Glyph glyph;
        float factor = 1.0F;
        float advance = 0.0F;
        float ascent = 0.0F;
        float descent = 0.0F;
        bool space = false;
        graphics::Texture texture;
        math::Rect source{};
        math::Vec2 extent{};
        math::Color tint = math::Color::white();
        RichTextDocument::VerticalAlign align = RichTextDocument::VerticalAlign::Center;
        float lift = 0.0F;
        float pause = 0.0F;
    };

    // The pieces a line holds, the width up to its last visible piece, its ascent and descent, where it starts below the paragraph top, how far a drop cap pushes it, and whether it wrapped, which lets filled lines stretch.
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
    };

    // A paragraph broken into lines, with its drop cap and list marker pieces. The widest segment that cannot wrap is the narrowest the paragraph can get.
    struct Flow {
        std::vector<Piece> pieces;
        std::vector<Line> lines;
        std::vector<Piece> dropCap;
        std::vector<Piece> marker;
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

    // The family, face and size a style draws with.
    struct StyleFont {
        std::shared_ptr<FontFamily> family;
        FontFamily::Selection face;
        float size = 0.0F;
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

    [[nodiscard]] static bool breaksBefore(const std::vector<Piece>& pieces, std::size_t index) noexcept;
    [[nodiscard]] static float alignOffset(TextAlign align, float room, float width) noexcept;

    [[nodiscard]] const StyleFont& getStyleFont(std::size_t style);
    [[nodiscard]] std::size_t getLook(std::size_t style, const FontFamily::Selection& face);
    [[nodiscard]] math::Color getColor(std::size_t style) const;
    [[nodiscard]] float getIndentUnit() const noexcept;

    void appendText(std::vector<Piece>& pieces, std::size_t style, std::u32string_view text);
    void appendImage(std::vector<Piece>& pieces, const RichTextDocument::Inline& item);
    [[nodiscard]] Piece measureObject(std::size_t style, graphics::Texture texture, math::Rect source, math::Vec2 extent, math::Color tint, RichTextDocument::VerticalAlign align);

    [[nodiscard]] std::vector<Block> measureBlocks(const std::vector<RichTextDocument::Paragraph>& paragraphs, float width);
    [[nodiscard]] Block measureParagraph(const RichTextDocument::Paragraph& paragraph, float width);
    [[nodiscard]] Block measureRule(const RichTextDocument::Paragraph& paragraph);
    [[nodiscard]] Block measureTable(const RichTextDocument::Paragraph& paragraph, float width);
    void breakLines(Flow& flow, float available);
    void closeLine(Flow& flow, Line& line, float& top) const;

    float emitBlocks(const std::vector<Block>& blocks, math::Vec2 origin, float width);
    void emitParagraph(const Block& block, math::Vec2 origin, float width);
    void emitRule(const Block& block, math::Vec2 origin, float width);
    void emitTable(const Block& block, math::Vec2 origin);
    void emitPiece(const Piece& piece, math::Vec2 pen, float lineTop, float lineHeight, bool counted);
    void emitDecorations(const Flow& flow, const Line& line, const std::vector<float>& lefts, float baseline);
    void addBox(RichTextLayout::Box::Kind kind, const math::Rect& rect, math::Color color, std::size_t firstCharacter, std::size_t lastCharacter);

    const RichTextDocument& document;
    const RichTextOptions& options;
    const RichTextRegistry& registry;
    RichTextLayout layout;
    std::vector<std::optional<StyleFont>> styleFonts;
    std::map<std::string, std::shared_ptr<FontFamily>, std::less<>> families;
    std::map<std::pair<std::size_t, const Font*>, std::size_t> looks;
    float pendingPause = 0.0F;
};

} // namespace haylen::text
