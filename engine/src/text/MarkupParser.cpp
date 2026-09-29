#include "text/MarkupParser.hpp"

#include <algorithm>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

#include <fast_float/fast_float.h>

#include "haylen/core/Utf8.hpp"
#include "haylen/text/Style.hpp"
#include "text/Segmenter.hpp"

namespace haylen::text {

MarkupParser::MarkupParser(std::string_view source) : markup(source) {}

std::pair<std::size_t, std::size_t> MarkupParser::locate(std::size_t offset) const {
    const std::string_view before = markup.substr(0, offset);
    const std::size_t lineStart = before.rfind('\n');
    const auto line = static_cast<std::size_t>(std::count(before.begin(), before.end(), '\n')) + 1;
    return {line, core::Utf8::countCodePoints(before.substr(lineStart == std::string_view::npos ? 0 : lineStart + 1)) + 1};
}

void MarkupParser::fail(std::size_t offset, const std::string& problem) const {
    const auto [line, column] = locate(offset);
    throw std::invalid_argument("Rich text markup at line " + std::to_string(line) + ", column " + std::to_string(column) + ": " + problem);
}

// Reads the tag between the brackets. The value of url and hint is the rest of the tag, so it may hold spaces, and other values and attributes may be quoted.
MarkupParser::Tag MarkupParser::readTag(std::size_t open, std::size_t close) const {
    Tag tag;
    tag.offset = open;
    std::string_view content = markup.substr(open + 1, close - open - 1);
    if (content.starts_with('/')) {
        tag.closing = true;
        tag.name = std::string(content.substr(1));
        if (tag.name.empty() || tag.name.find_first_of(" =") != std::string::npos) {
            fail(open, "[" + std::string(content) + "] is not a closing tag.");
        }
        return tag;
    }

    const std::size_t nameEnd = std::min(content.find_first_of(" ="), content.size());
    tag.name = std::string(content.substr(0, nameEnd));
    if (tag.name.empty()) {
        fail(open, "[" + std::string(content) + "] has no tag name.");
    }
    content.remove_prefix(nameEnd);

    // clang-format off
    const auto token = [&](std::string_view& rest) {
        if (rest.starts_with('"')) {
            const std::size_t end = rest.find('"', 1);
            if (end == std::string_view::npos) {
                fail(open, "[" + tag.name + "] has a quote that never ends.");
            }
            std::string value(rest.substr(1, end - 1));
            rest.remove_prefix(end + 1);
            return value;
        }
        const std::size_t end = std::min(rest.find(' '), rest.size());
        std::string value(rest.substr(0, end));
        rest.remove_prefix(end);
        return value;
    };
    // clang-format on

    if (content.starts_with('=')) {
        content.remove_prefix(1);
        if (tag.name == "url" || tag.name == "hint") {
            const bool quoted = content.size() >= 2 && content.starts_with('"') && content.ends_with('"');
            tag.value = std::string(quoted ? content.substr(1, content.size() - 2) : content);
            return tag;
        }
        tag.value = token(content);
    }

    while (!content.empty()) {
        if (content.front() == ' ') {
            content.remove_prefix(1);
            continue;
        }
        const std::size_t equals = content.find('=');
        const std::size_t space = std::min(content.find(' '), content.size());
        if (equals == std::string_view::npos || equals > space) {
            fail(open, "The attribute " + std::string(content.substr(0, space)) + " of [" + tag.name + "] needs a value.");
        }
        std::string key(content.substr(0, equals));
        content.remove_prefix(equals + 1);
        tag.attributes.insert_or_assign(std::move(key), token(content));
    }
    return tag;
}

float MarkupParser::number(const Tag& tag, std::string_view text, std::string_view what) const {
    float value = 0.0F;
    const auto [end, error] = fast_float::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size() || text.empty()) {
        fail(tag.offset, std::string(text) + " is not a number for the " + std::string(what) + " of [" + tag.name + "].");
    }
    return value;
}

std::optional<math::Color> MarkupParser::parseColor(std::string_view text) noexcept {
    for (const auto& [name, rgba] : kNamedColors) {
        if (name == text) {
            return math::Color::fromHex(rgba);
        }
    }
    return math::Color::parse(text);
}

bool MarkupParser::isTag(std::string_view name) noexcept {
    return std::find(kTags.begin(), kTags.end(), name) != kTags.end();
}

math::Color MarkupParser::color(const Tag& tag, std::string_view text) const {
    const std::optional<math::Color> parsed = parseColor(text);
    if (!parsed) {
        fail(tag.offset, std::string(text) + " is not a color. Use a name such as red or #RRGGBB or #AARRGGBB.");
    }
    return *parsed;
}

math::Vec2 MarkupParser::pair(const Tag& tag, std::string_view text) const {
    const std::size_t comma = text.find(',');
    if (comma == std::string_view::npos) {
        fail(tag.offset, std::string(text) + " is not a pair of numbers like 2,3 for [" + tag.name + "].");
    }
    return {number(tag, text.substr(0, comma), "x"), number(tag, text.substr(comma + 1), "y")};
}

float MarkupParser::size(const Tag& tag, std::string_view text) const {
    const float value = number(tag, text, "size");
    if (!(value > 0.0F)) {
        fail(tag.offset, "The size of [" + tag.name + "] must be positive.");
    }
    return value;
}

RichTextDocument::VerticalAlign MarkupParser::verticalAlign(const Tag& tag, std::string_view text) const {
    using Align = RichTextDocument::VerticalAlign;
    constexpr std::array<std::pair<std::string_view, Align>, 4> kAligns{{{"top", Align::Top}, {"center", Align::Center}, {"baseline", Align::Baseline}, {"bottom", Align::Bottom}}};
    for (const auto& [name, align] : kAligns) {
        if (name == text) {
            return align;
        }
    }
    fail(tag.offset, "The valign of [" + tag.name + "] must be top, center, baseline or bottom.");
}

std::string_view MarkupParser::requireValue(const Tag& tag) const {
    if (!tag.value || tag.value->empty()) {
        fail(tag.offset, "[" + tag.name + "] needs a value, as in [" + tag.name + "=...].");
    }
    return *tag.value;
}

void MarkupParser::requireNoValue(const Tag& tag) const {
    if (tag.value) {
        fail(tag.offset, "[" + tag.name + "] takes no value.");
    }
}

void MarkupParser::checkAttributes(const Tag& tag, std::initializer_list<std::string_view> known) const {
    for (const auto& [key, value] : tag.attributes) {
        if (std::find(known.begin(), known.end(), key) == known.end()) {
            fail(tag.offset, "[" + tag.name + "] has no attribute named " + key + ".");
        }
    }
}

std::u32string MarkupParser::romanNumeral(std::size_t number, bool upper) {
    constexpr std::array<std::pair<std::size_t, std::string_view>, 13> kNumerals{{{1000, "m"}, {900, "cm"}, {500, "d"}, {400, "cd"}, {100, "c"}, {90, "xc"}, {50, "l"}, {40, "xl"}, {10, "x"}, {9, "ix"}, {5, "v"}, {4, "iv"}, {1, "i"}}};
    std::u32string result;
    for (const auto& [value, letters] : kNumerals) {
        while (number >= value) {
            for (const char letter : letters) {
                result += static_cast<char32_t>(upper ? letter - 'a' + 'A' : letter);
            }
            number -= value;
        }
    }
    return result;
}

std::u32string MarkupParser::listMarker(RichTextDocument::ListKind kind, std::size_t number) {
    using Kind = RichTextDocument::ListKind;
    std::u32string label;
    switch (kind) {
    case Kind::Bullet:
        return core::Utf8::decode(kBullet);
    case Kind::Decimal:
        for (const char digit : std::to_string(number)) {
            label += static_cast<char32_t>(digit);
        }
        break;
    case Kind::LowerAlpha:
    case Kind::UpperAlpha:
        // Letters count like spreadsheet columns, so z is followed by aa.
        for (std::size_t rest = number; rest > 0; rest = (rest - 1) / 26) {
            label.insert(label.begin(), static_cast<char32_t>((kind == Kind::UpperAlpha ? U'A' : U'a') + (rest - 1) % 26));
        }
        break;
    case Kind::LowerRoman:
    case Kind::UpperRoman:
        label = romanNumeral(number, kind == Kind::UpperRoman);
        break;
    }
    return label + U".";
}

RichTextDocument MarkupParser::parse() {
    blocks.emplace_back();
    beginParagraph(false);

    // Every paragraph separator ends a paragraph, and the carriage return of CRLF belongs to its line feed, so markup with CRLF line endings reads like markup with LF ones.
    std::size_t textStart = 0;
    // clang-format off
    const auto flushText = [&](std::size_t end) {
        const std::u32string text = core::Utf8::decode(markup.substr(textStart, end - textStart));
        std::u32string pending;
        for (std::size_t index = 0; index < text.size(); ++index) {
            const char32_t codePoint = text[index];
            if (codePoint == U'\r' && index + 1 < text.size() && text[index + 1] == U'\n') {
                continue;
            }
            const bool separator = Segmenter::isParagraphSeparator(codePoint);
            if (std::exchange(skipNewline, false) && separator) {
                continue;
            }
            if (insideTableRows()) {
                if (codePoint != U' ' && codePoint != U'\t' && !separator) {
                    fail(textStart, "Text inside a [table] must be inside a [cell].");
                }
                continue;
            }
            if (separator) {
                appendText(std::exchange(pending, {}));
                newline();
                continue;
            }
            pending += codePoint;
        }
        appendText(pending);
    };
    // clang-format on

    for (std::size_t offset = 0; offset < markup.size();) {
        if (markup[offset] != '[') {
            ++offset;
            continue;
        }
        flushText(offset);
        const std::size_t close = markup.find(']', offset + 1);
        if (close == std::string_view::npos) {
            fail(offset, "A tag has no closing bracket.");
        }
        skipNewline = false;
        handleTag(readTag(offset, close));
        offset = close + 1;
        textStart = offset;
    }
    flushText(markup.size());

    if (!openTags.empty()) {
        fail(openTags.back().offset, "[" + openTags.back().name + "] is never closed.");
    }
    // A last paragraph that a newline began is an empty line, like a trailing newline of plain text.
    if (!paragraph.inlines.empty() || paragraph.dropCap || paragraphFromNewline || container().empty()) {
        finishParagraph();
    }
    return std::move(document);
}

void MarkupParser::handleTag(const Tag& tag) {
    if (tag.closing) {
        closeTag(tag);
        return;
    }
    if (tag.name == "lb" || tag.name == "rb") {
        requireNoValue(tag);
        checkAttributes(tag, {});
        appendText(tag.name == "lb" ? U"[" : U"]");
        return;
    }
    if (insideTableRows() && tag.name != "cell") {
        fail(tag.offset, "Only [cell] tags go directly inside a [table], not [" + tag.name + "].");
    }
    if (dropCap) {
        fail(tag.offset, "A [dropcap] holds plain text, not [" + tag.name + "].");
    }

    if (tag.name == "br") {
        requireNoValue(tag);
        checkAttributes(tag, {});
        appendInline({.kind = RichTextDocument::Inline::Kind::LineBreak, .style = currentStyle()});
    } else if (tag.name == "pause") {
        const float seconds = number(tag, requireValue(tag), "seconds");
        checkAttributes(tag, {});
        if (seconds < 0.0F) {
            fail(tag.offset, "A [pause] cannot last a negative time.");
        }
        appendInline({.kind = RichTextDocument::Inline::Kind::Pause, .style = currentStyle(), .seconds = seconds});
    } else if (tag.name == "img") {
        addImage(tag);
    } else if (tag.name == "icon") {
        addIcon(tag);
    } else if (tag.name == "hr") {
        addRule(tag);
    } else if (tag.name == "p" || tag.name == "center" || tag.name == "left" || tag.name == "right" || tag.name == "fill" || tag.name == "ul" || tag.name == "ol") {
        openBlock(tag);
    } else if (tag.name == "table") {
        openTable(tag);
    } else if (tag.name == "cell") {
        openCell(tag);
    } else if (tag.name == "dropcap") {
        openDropCap(tag);
    } else {
        openInline(tag);
    }
}

void MarkupParser::pushTag(const Tag& tag, TagKind kind) {
    openTags.push_back({.name = tag.name, .kind = kind, .offset = tag.offset, .style = style, .styleIndex = styleIndex});
}

// Applies an inline tag to the style of the text after it. Tags the parser does not know are effects, which the rich text resolves by name.
void MarkupParser::openInline(const Tag& tag) {
    pushTag(tag, TagKind::Inline);
    const std::string& name = tag.name;
    if (name == "b" || name == "i" || name == "u" || name == "s" || name == "code") {
        requireNoValue(tag);
        checkAttributes(tag, {});
        style.bold = style.bold || name == "b";
        style.italic = style.italic || name == "i";
        style.underline = style.underline || name == "u";
        style.strike = style.strike || name == "s";
        style.mono = style.mono || name == "code";
    } else if (name == "color") {
        checkAttributes(tag, {});
        style.color = color(tag, requireValue(tag));
    } else if (name == "bgcolor") {
        checkAttributes(tag, {});
        style.background = color(tag, requireValue(tag));
    } else if (name == "font") {
        checkAttributes(tag, {});
        style.font = std::string(requireValue(tag));
    } else if (name == "size") {
        checkAttributes(tag, {});
        const std::string_view value = requireValue(tag);
        if (value.ends_with('%')) {
            style.sizeFactor *= size(tag, value.substr(0, value.size() - 1)) / 100.0F;
        } else {
            style.size = size(tag, value);
            style.sizeFactor = 1.0F;
        }
    } else if (name == "outline") {
        checkAttributes(tag, {"color"});
        style.outlineWidth = number(tag, requireValue(tag), "width");
        style.outlineColor = tag.attributes.contains("color") ? color(tag, tag.attributes.at("color")) : math::Color::black();
    } else if (name == "shadow") {
        checkAttributes(tag, {"color", "blur"});
        RichTextDocument::Shadow shadow{.offset = tag.value ? pair(tag, *tag.value) : math::Vec2{2.0F, 2.0F}, .color = math::Color{0.0F, 0.0F, 0.0F, 0.5F}};
        if (tag.attributes.contains("color")) {
            shadow.color = color(tag, tag.attributes.at("color"));
        }
        if (tag.attributes.contains("blur")) {
            shadow.blur = number(tag, tag.attributes.at("blur"), "blur");
        }
        style.shadow = shadow;
    } else if (name == "glow") {
        checkAttributes(tag, {"color"});
        style.glow = RichTextDocument::Glow{.width = tag.value ? number(tag, *tag.value, "width") : 4.0F, .color = tag.attributes.contains("color") ? color(tag, tag.attributes.at("color")) : math::Color::white()};
    } else if (name == "alpha") {
        checkAttributes(tag, {});
        const float alpha = number(tag, requireValue(tag), "alpha");
        if (alpha < 0.0F || alpha > 1.0F) {
            fail(tag.offset, "The alpha of [alpha] must be from 0 to 1.");
        }
        style.alpha *= alpha;
    } else if (name == "speed") {
        checkAttributes(tag, {});
        const float speed = number(tag, requireValue(tag), "speed");
        if (!(speed > 0.0F)) {
            fail(tag.offset, "The speed of [speed] must be positive.");
        }
        style.revealSpeed *= speed;
    } else if (name == "url") {
        checkAttributes(tag, {});
        openTags.back().kind = TagKind::Link;
        openTags.back().link = document.links.size();
        openTags.back().captures = !tag.value;
        document.links.push_back(tag.value.value_or(""));
        style.link = openTags.back().link;
    } else if (name == "hint") {
        style.hint = document.hints.size();
        document.hints.emplace_back(requireValue(tag));
    } else {
        RichTextDocument::Effect effect{.name = name, .parameters = tag.attributes};
        if (tag.value) {
            effect.parameters.insert_or_assign("value", *tag.value);
        }
        std::tie(effect.line, effect.column) = locate(tag.offset);
        style.effects.push_back(document.effects.size());
        document.effects.push_back(std::move(effect));
    }
    styleIndex.reset();
}

// Block tags end the paragraph before them and lay out the paragraphs inside them, and the newline right after the tag belongs to the markup rather than the text.
void MarkupParser::openBlock(const Tag& tag) {
    using Kind = RichTextDocument::ListKind;
    Block block = blocks.back();
    block.list.reset();
    block.items = 0;
    const std::string& name = tag.name;
    if (name == "p") {
        requireNoValue(tag);
        checkAttributes(tag, {"align", "dir", "indent"});
        if (tag.attributes.contains("align")) {
            const std::optional<Alignment> align = Style::alignmentFromName(tag.attributes.at("align"));
            if (!align) {
                fail(tag.offset, "The align of [p] must be start, end, left, center, right or fill.");
            }
            block.align = *align;
        }
        if (tag.attributes.contains("dir")) {
            const std::optional<Direction> direction = Style::directionFromName(tag.attributes.at("dir"));
            if (!direction) {
                fail(tag.offset, "The dir of [p] must be auto, leftToRight or rightToLeft.");
            }
            block.direction = *direction;
        }
        if (tag.attributes.contains("indent")) {
            block.indent += number(tag, tag.attributes.at("indent"), "indent");
        }
    } else if (name == "ul") {
        requireNoValue(tag);
        checkAttributes(tag, {"bullet"});
        block.list = Kind::Bullet;
        block.bullet = core::Utf8::decode(tag.attributes.contains("bullet") ? std::string_view(tag.attributes.at("bullet")) : kBullet);
        block.indent += 1.0F;
    } else if (name == "ol") {
        requireNoValue(tag);
        checkAttributes(tag, {"type"});
        const std::string type = tag.attributes.contains("type") ? tag.attributes.at("type") : "1";
        constexpr std::array<std::pair<std::string_view, Kind>, 5> kTypes{{{"1", Kind::Decimal}, {"a", Kind::LowerAlpha}, {"A", Kind::UpperAlpha}, {"i", Kind::LowerRoman}, {"I", Kind::UpperRoman}}};
        const auto found = std::find_if(kTypes.begin(), kTypes.end(), [&](const auto& entry) { return entry.first == type; });
        if (found == kTypes.end()) {
            fail(tag.offset, "The type of [ol] must be 1, a, A, i or I.");
        }
        block.list = found->second;
        block.indent += 1.0F;
    } else {
        requireNoValue(tag);
        checkAttributes(tag, {});
        block.align = name == "center" ? Alignment::Center : (name == "right" ? Alignment::Right : (name == "fill" ? Alignment::Fill : Alignment::Left));
    }

    discardOrFinishParagraph();
    pushTag(tag, TagKind::Block);
    blocks.push_back(std::move(block));
    beginParagraph(false);
    skipNewline = true;
}

void MarkupParser::openTable(const Tag& tag) {
    if (cell) {
        fail(tag.offset, "Tables cannot be nested.");
    }
    checkAttributes(tag, {});
    const float columns = number(tag, requireValue(tag), "column count");
    if (columns < 1.0F || columns != static_cast<float>(static_cast<std::size_t>(columns))) {
        fail(tag.offset, "A [table] needs a whole number of columns of at least 1.");
    }

    discardOrFinishParagraph();
    pushTag(tag, TagKind::Table);
    table = document.tables.size();
    document.tables.push_back({.columns = static_cast<std::size_t>(columns)});
    container().push_back({.kind = RichTextDocument::Paragraph::Kind::Table, .direction = currentBlock().direction, .table = *table});
    skipNewline = true;
}

void MarkupParser::openCell(const Tag& tag) {
    if (!table || cell) {
        fail(tag.offset, "A [cell] goes directly inside a [table].");
    }
    requireNoValue(tag);
    checkAttributes(tag, {"bg", "border", "padding"});
    RichTextDocument::Cell opened;
    if (tag.attributes.contains("bg")) {
        opened.background = color(tag, tag.attributes.at("bg"));
    }
    if (tag.attributes.contains("border")) {
        opened.border = color(tag, tag.attributes.at("border"));
    }
    if (tag.attributes.contains("padding")) {
        opened.padding = number(tag, tag.attributes.at("padding"), "padding");
    }

    pushTag(tag, TagKind::Cell);
    cell = std::move(opened);
    blocks.push_back({});
    beginParagraph(false);
    skipNewline = true;
}

void MarkupParser::openDropCap(const Tag& tag) {
    if (!paragraph.inlines.empty() || paragraph.dropCap) {
        fail(tag.offset, "A [dropcap] must start its paragraph.");
    }
    requireNoValue(tag);
    checkAttributes(tag, {"font", "size", "color", "margin"});
    pushTag(tag, TagKind::DropCap);
    if (tag.attributes.contains("font")) {
        style.font = tag.attributes.at("font");
    }
    if (tag.attributes.contains("size")) {
        style.size = size(tag, tag.attributes.at("size"));
        style.sizeFactor = 1.0F;
    } else {
        style.sizeFactor *= 3.0F;
    }
    if (tag.attributes.contains("color")) {
        style.color = color(tag, tag.attributes.at("color"));
    }
    styleIndex.reset();
    dropCap = RichTextDocument::DropCap{.style = currentStyle(), .margin = tag.attributes.contains("margin") ? number(tag, tag.attributes.at("margin"), "margin") : 4.0F};
}

void MarkupParser::addImage(const Tag& tag) {
    checkAttributes(tag, {"width", "height", "region", "color", "valign"});
    RichTextDocument::Image image{.path = std::string(requireValue(tag))};
    if (tag.attributes.contains("width")) {
        image.width = size(tag, tag.attributes.at("width"));
    }
    if (tag.attributes.contains("height")) {
        image.height = size(tag, tag.attributes.at("height"));
    }
    if (tag.attributes.contains("region")) {
        const std::string& region = tag.attributes.at("region");
        const std::size_t middle = region.find(',', region.find(',') + 1);
        if (middle == std::string::npos) {
            fail(tag.offset, "The region of [img] needs x,y,width,height.");
        }
        const math::Vec2 origin = pair(tag, std::string_view(region).substr(0, middle));
        const math::Vec2 extent = pair(tag, std::string_view(region).substr(middle + 1));
        image.region = math::Rect{origin.x, origin.y, extent.x, extent.y};
    }
    if (tag.attributes.contains("color")) {
        image.color = color(tag, tag.attributes.at("color"));
    }
    if (tag.attributes.contains("valign")) {
        image.align = verticalAlign(tag, tag.attributes.at("valign"));
    }
    document.images.push_back(std::move(image));
    appendInline({.kind = RichTextDocument::Inline::Kind::Image, .style = currentStyle(), .object = document.images.size() - 1});
}

void MarkupParser::addIcon(const Tag& tag) {
    checkAttributes(tag, {"width", "height", "color", "valign"});
    RichTextDocument::Icon icon{.name = std::string(requireValue(tag))};
    if (tag.attributes.contains("width")) {
        icon.width = size(tag, tag.attributes.at("width"));
    }
    if (tag.attributes.contains("height")) {
        icon.height = size(tag, tag.attributes.at("height"));
    }
    if (tag.attributes.contains("color")) {
        icon.color = color(tag, tag.attributes.at("color"));
    }
    if (tag.attributes.contains("valign")) {
        icon.align = verticalAlign(tag, tag.attributes.at("valign"));
    }
    document.icons.push_back(std::move(icon));
    appendInline({.kind = RichTextDocument::Inline::Kind::Icon, .style = currentStyle(), .object = document.icons.size() - 1});
}

void MarkupParser::addRule(const Tag& tag) {
    requireNoValue(tag);
    checkAttributes(tag, {"width", "height", "color"});
    RichTextDocument::Paragraph rule{.kind = RichTextDocument::Paragraph::Kind::Rule, .align = currentBlock().align, .direction = currentBlock().direction, .indent = currentBlock().indent};
    if (tag.attributes.contains("width")) {
        const std::string& width = tag.attributes.at("width");
        if (!width.ends_with('%')) {
            fail(tag.offset, "The width of [hr] is a percentage such as 50%.");
        }
        rule.ruleWidth = std::clamp(number(tag, std::string_view(width).substr(0, width.size() - 1), "width") / 100.0F, 0.0F, 1.0F);
    }
    if (tag.attributes.contains("height")) {
        rule.ruleThickness = size(tag, tag.attributes.at("height"));
    }
    if (tag.attributes.contains("color")) {
        rule.ruleColor = color(tag, tag.attributes.at("color"));
    }

    discardOrFinishParagraph();
    container().push_back(std::move(rule));
    beginParagraph(false);
    skipNewline = true;
}

void MarkupParser::closeTag(const Tag& tag) {
    if (openTags.empty()) {
        fail(tag.offset, "[/" + tag.name + "] closes nothing.");
    }
    if (openTags.back().name != tag.name) {
        fail(tag.offset, "[/" + tag.name + "] closes [" + openTags.back().name + "], which is still open.");
    }

    OpenTag closed = std::move(openTags.back());
    openTags.pop_back();
    switch (closed.kind) {
    case TagKind::Inline:
        break;
    case TagKind::Link:
        if (closed.captures) {
            document.links[*closed.link] = std::move(closed.captured);
        }
        break;
    case TagKind::DropCap:
        if (dropCap->text.empty()) {
            fail(tag.offset, "A [dropcap] needs some text.");
        }
        paragraph.dropCap = std::move(dropCap);
        dropCap.reset();
        break;
    case TagKind::Block:
        discardOrFinishParagraph();
        blocks.pop_back();
        beginParagraph(false);
        skipNewline = true;
        break;
    case TagKind::Cell:
        discardOrFinishParagraph();
        blocks.pop_back();
        document.tables[*table].cells.push_back(std::move(*cell));
        cell.reset();
        skipNewline = true;
        break;
    case TagKind::Table:
        table.reset();
        beginParagraph(false);
        skipNewline = true;
        break;
    }
    style = std::move(closed.style);
    styleIndex = closed.styleIndex;
}

std::size_t MarkupParser::currentStyle() {
    if (!styleIndex) {
        styleIndex = document.styles.size();
        document.styles.push_back(style);
    }
    return *styleIndex;
}

void MarkupParser::appendText(std::u32string_view text) {
    if (text.empty()) {
        return;
    }
    if (dropCap) {
        dropCap->text += text;
        return;
    }
    for (OpenTag& tag : openTags) {
        if (tag.captures) {
            for (const char32_t codePoint : text) {
                core::Utf8::append(tag.captured, codePoint);
            }
        }
    }

    const std::size_t index = currentStyle();
    if (!paragraph.inlines.empty() && paragraph.inlines.back().kind == RichTextDocument::Inline::Kind::Text && paragraph.inlines.back().style == index) {
        paragraph.inlines.back().text += text;
        return;
    }
    paragraph.inlines.push_back({.kind = RichTextDocument::Inline::Kind::Text, .style = index, .text = std::u32string(text)});
}

void MarkupParser::appendInline(RichTextDocument::Inline item) {
    paragraph.inlines.push_back(std::move(item));
}

void MarkupParser::newline() {
    if (dropCap) {
        fail(openTags.back().offset, "A [dropcap] cannot hold a line break.");
    }
    finishParagraph();
    beginParagraph(true);
}

void MarkupParser::beginParagraph(bool fromNewline) {
    paragraph = {.align = currentBlock().align, .direction = currentBlock().direction, .indent = currentBlock().indent};
    paragraphFromNewline = fromNewline;
}

// A paragraph inside a list becomes its next item once it has content, so blank lines between items take no number.
void MarkupParser::finishParagraph() {
    Block& block = blocks.back();
    if (block.list && (!paragraph.inlines.empty() || paragraph.dropCap)) {
        ++block.items;
        paragraph.marker = *block.list == RichTextDocument::ListKind::Bullet ? block.bullet : listMarker(*block.list, block.items);
        paragraph.markerStyle = paragraph.inlines.empty() ? currentStyle() : paragraph.inlines.front().style;
    }
    container().push_back(std::move(paragraph));
    paragraph = {};
}

// An empty paragraph at a block boundary only held the newline before the tag, so it leaves no empty line.
void MarkupParser::discardOrFinishParagraph() {
    if (paragraph.inlines.empty() && !paragraph.dropCap) {
        paragraph = {};
        return;
    }
    finishParagraph();
}

const MarkupParser::Block& MarkupParser::currentBlock() const noexcept {
    return blocks.back();
}

std::vector<RichTextDocument::Paragraph>& MarkupParser::container() {
    return cell ? cell->paragraphs : document.paragraphs;
}

bool MarkupParser::insideTableRows() const noexcept {
    return table.has_value() && !cell.has_value();
}

} // namespace haylen::text
