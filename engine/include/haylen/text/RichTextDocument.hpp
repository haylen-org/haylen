#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Alignment.hpp"
#include "haylen/text/Direction.hpp"

namespace haylen::text {

// What rich text markup says, before any font measures it: paragraphs of styled text runs and inline objects in reading order, the styles they use and the links, hints and effects the styles point to. Sizes are in pixels at a scale of 1.
struct RichTextDocument {
    // Where an inline image or icon sits against the text of its line.
    enum class VerticalAlign : std::uint8_t {
        Top,
        Center,
        Baseline,
        Bottom,
    };

    // A tag the parser did not know, which names an effect such as `[wave amp=20]`, and where it opened in the markup.
    struct Effect {
        std::string name;
        std::map<std::string, std::string, std::less<>> parameters;
        std::size_t line = 0;
        std::size_t column = 0;
    };

    struct Shadow {
        math::Vec2 offset{};
        math::Color color = math::Color::black();
        float blur = 0.0F;
    };

    struct Glow {
        float width = 0.0F;
        math::Color color = math::Color::white();
    };

    // How a run of text looks. The size multiplies the base size of the text when it is relative, and effects list indices into the effects in nesting order.
    struct Style {
        bool bold = false;
        bool italic = false;
        bool underline = false;
        bool strike = false;
        bool mono = false;
        std::optional<math::Color> color;
        std::optional<math::Color> background;
        std::string font;
        std::optional<float> size;
        float sizeFactor = 1.0F;
        float outlineWidth = 0.0F;
        math::Color outlineColor = math::Color::black();
        std::optional<Shadow> shadow;
        std::optional<Glow> glow;
        float alpha = 1.0F;
        std::optional<std::size_t> link;
        std::optional<std::size_t> hint;
        std::vector<std::size_t> effects;
        float revealSpeed = 1.0F;
    };

    struct Image {
        std::string path;
        std::optional<float> width;
        std::optional<float> height;
        std::optional<math::Rect> region;
        math::Color color = math::Color::white();
        VerticalAlign align = VerticalAlign::Center;
    };

    struct Icon {
        std::string name;
        std::optional<float> width;
        std::optional<float> height;
        math::Color color = math::Color::white();
        VerticalAlign align = VerticalAlign::Center;
    };

    // One piece of a paragraph. Text holds code points, images and icons index their lists, a line break wraps without ending the paragraph, and a pause holds the reveal for some seconds before the next character.
    struct Inline {
        enum class Kind : std::uint8_t {
            Text,
            Image,
            Icon,
            LineBreak,
            Pause,
        };

        Kind kind = Kind::Text;
        std::size_t style = 0;
        std::u32string text;
        std::size_t object = 0;
        float seconds = 0.0F;
    };

    // A large first letter or word that spans several lines, which the lines next to it flow around.
    struct DropCap {
        std::u32string text;
        std::size_t style = 0;
        float margin = 0.0F;
    };

    // The markers of list items: bullets or numbers counted in decimal, letters or roman numerals.
    enum class ListKind : std::uint8_t {
        Bullet,
        Decimal,
        LowerAlpha,
        UpperAlpha,
        LowerRoman,
        UpperRoman,
    };

    // A block of the document: a paragraph of text, a horizontal rule, or a table. The indent counts levels on the side the paragraph starts, and a list item carries the marker drawn in the indent before its first line.
    struct Paragraph {
        enum class Kind : std::uint8_t {
            Text,
            Rule,
            Table,
        };

        Kind kind = Kind::Text;
        std::optional<Alignment> align;
        std::optional<Direction> direction;
        float indent = 0.0F;
        std::u32string marker;
        std::size_t markerStyle = 0;
        std::vector<Inline> inlines;
        std::optional<DropCap> dropCap;

        // A rule spans a fraction of the width with its thickness and color, drawn in the text color when it has none.
        float ruleWidth = 1.0F;
        float ruleThickness = 2.0F;
        std::optional<math::Color> ruleColor;
        std::size_t table = 0;
    };

    struct Cell {
        std::vector<Paragraph> paragraphs;
        std::optional<math::Color> background;
        std::optional<math::Color> border;
        float padding = 4.0F;
    };

    struct Table {
        std::size_t columns = 1;
        std::vector<Cell> cells;
    };

    std::vector<Paragraph> paragraphs;
    std::vector<Style> styles;
    std::vector<Image> images;
    std::vector<Icon> icons;
    std::vector<Table> tables;
    std::vector<std::string> links;
    std::vector<std::string> hints;
    std::vector<Effect> effects;
};

} // namespace haylen::text
