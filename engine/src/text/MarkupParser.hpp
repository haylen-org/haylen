#pragma once

#include <array>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Color.hpp"
#include "haylen/text/RichTextDocument.hpp"

namespace haylen::text {

// Reads BBCode markup into a rich text document. Tags nest, [lb] and [rb] write brackets, and every paragraph separator, such as a line feed, CRLF or U+2029, ends a paragraph. Malformed markup throws std::invalid_argument naming the line and column of the problem.
class MarkupParser final {
  public:
    explicit MarkupParser(std::string_view source);

    [[nodiscard]] RichTextDocument parse();

    // Reads a color written in markup: a name such as red, #RRGGBB or #AARRGGBB.
    [[nodiscard]] static std::optional<math::Color> parseColor(std::string_view text) noexcept;

    // Tells whether markup has a tag of this name, so no effect may take it.
    [[nodiscard]] static bool isTag(std::string_view name) noexcept;

  private:
    enum class TagKind : std::uint8_t {
        Inline,
        Link,
        Block,
        Table,
        Cell,
        DropCap,
    };

    // A tag as written: its name, the value after the equals sign, its attributes and where it starts.
    struct Tag {
        std::string name;
        std::optional<std::string> value;
        std::map<std::string, std::string, std::less<>> attributes;
        bool closing = false;
        std::size_t offset = 0;
    };

    // The layout a block tag gives the paragraphs inside it. A list numbers its items from one.
    struct Block {
        std::optional<TextAlign> align;
        std::optional<Direction> direction;
        float indent = 0.0F;
        std::optional<RichTextDocument::ListKind> list;
        std::u32string bullet;
        std::size_t items = 0;
    };

    // An open tag with what it replaced, so closing it restores the state before it.
    struct OpenTag {
        std::string name;
        TagKind kind = TagKind::Inline;
        std::size_t offset = 0;
        RichTextDocument::Style style;
        std::optional<std::size_t> styleIndex;
        std::optional<std::size_t> link;
        bool captures = false;
        std::string captured;
    };

    static constexpr std::array<std::pair<std::string_view, std::uint32_t>, 18> kNamedColors{{
        {"black", 0x000000FFU},
        {"white", 0xFFFFFFFFU},
        {"red", 0xFF0000FFU},
        {"green", 0x008000FFU},
        {"lime", 0x00FF00FFU},
        {"blue", 0x0000FFFFU},
        {"yellow", 0xFFFF00FFU},
        {"cyan", 0x00FFFFFFU},
        {"aqua", 0x00FFFFFFU},
        {"magenta", 0xFF00FFFFU},
        {"fuchsia", 0xFF00FFFFU},
        {"gray", 0x808080FFU},
        {"grey", 0x808080FFU},
        {"orange", 0xFFA500FFU},
        {"purple", 0x800080FFU},
        {"pink", 0xFFC0CBFFU},
        {"gold", 0xFFD700FFU},
        {"transparent", 0x00000000U},
    }};
    static constexpr std::array<std::string_view, 33> kTags{"b", "i", "u", "s", "code", "color", "bgcolor", "font", "size", "outline", "shadow", "glow", "alpha", "url", "hint", "speed", "pause", "br", "lb", "rb", "img", "icon", "hr", "p", "center", "left", "right", "fill", "ul", "ol", "table", "cell", "dropcap"};
    static constexpr std::string_view kBullet = "\xE2\x80\xA2";

    [[nodiscard]] static std::u32string listMarker(RichTextDocument::ListKind kind, std::size_t number);
    [[nodiscard]] static std::u32string romanNumeral(std::size_t number, bool upper);

    // Returns the line and column of a byte offset, both counted from one.
    [[nodiscard]] std::pair<std::size_t, std::size_t> locate(std::size_t offset) const;
    [[noreturn]] void fail(std::size_t offset, const std::string& problem) const;
    [[nodiscard]] Tag readTag(std::size_t open, std::size_t close) const;
    [[nodiscard]] float number(const Tag& tag, std::string_view text, std::string_view what) const;
    [[nodiscard]] math::Color color(const Tag& tag, std::string_view text) const;
    [[nodiscard]] math::Vec2 pair(const Tag& tag, std::string_view text) const;
    [[nodiscard]] float size(const Tag& tag, std::string_view text) const;
    [[nodiscard]] RichTextDocument::VerticalAlign verticalAlign(const Tag& tag, std::string_view text) const;
    [[nodiscard]] std::string_view requireValue(const Tag& tag) const;
    void requireNoValue(const Tag& tag) const;
    void checkAttributes(const Tag& tag, std::initializer_list<std::string_view> known) const;

    void handleTag(const Tag& tag);
    void closeTag(const Tag& tag);
    void openInline(const Tag& tag);
    void openBlock(const Tag& tag);
    void openTable(const Tag& tag);
    void openCell(const Tag& tag);
    void openDropCap(const Tag& tag);
    void addImage(const Tag& tag);
    void addIcon(const Tag& tag);
    void addRule(const Tag& tag);

    void pushTag(const Tag& tag, TagKind kind);
    [[nodiscard]] std::size_t currentStyle();
    void appendText(std::u32string_view text);
    void appendInline(RichTextDocument::Inline item);
    void newline();
    void beginParagraph(bool fromNewline);
    void finishParagraph();
    void discardOrFinishParagraph();
    [[nodiscard]] const Block& currentBlock() const noexcept;
    [[nodiscard]] std::vector<RichTextDocument::Paragraph>& container();
    [[nodiscard]] bool insideTableRows() const noexcept;

    std::string_view markup;
    RichTextDocument document;
    RichTextDocument::Style style;
    std::optional<std::size_t> styleIndex;
    std::vector<OpenTag> openTags;
    std::vector<Block> blocks;
    RichTextDocument::Paragraph paragraph;
    bool paragraphFromNewline = false;
    bool skipNewline = false;
    std::optional<std::size_t> table;
    std::optional<RichTextDocument::Cell> cell;
    std::optional<RichTextDocument::DropCap> dropCap;
};

} // namespace haylen::text
