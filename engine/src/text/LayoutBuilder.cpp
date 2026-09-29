#include "text/LayoutBuilder.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "text/BreakRules.hpp"

namespace haylen::text {

LayoutBuilder::LayoutBuilder(const RichTextDocument& source, const RichTextOptions& layoutOptions, const RichTextRegistry& textRegistry) : document(source), options(layoutOptions), registry(textRegistry), styleFonts(source.styles.size()) {}

float LayoutBuilder::getIndentUnit() const noexcept {
    return options.size * options.scale * kIndentEms;
}

float LayoutBuilder::alignOffset(TextAlign align, float room, float width) noexcept {
    switch (align) {
    case TextAlign::Center:
        return (room - width) * 0.5F;
    case TextAlign::Right:
        return room - width;
    case TextAlign::Left:
    case TextAlign::Fill:
        break;
    }
    return 0.0F;
}

const LayoutBuilder::StyleFont& LayoutBuilder::getStyleFont(std::size_t style) {
    std::optional<StyleFont>& cached = styleFonts[style];
    if (cached) {
        return *cached;
    }

    const RichTextDocument::Style& described = document.styles[style];
    std::shared_ptr<FontFamily> family = options.family;
    if (!described.font.empty()) {
        auto found = families.find(described.font);
        if (found == families.end()) {
            std::shared_ptr<FontFamily> named = options.fonts ? options.fonts(described.font) : nullptr;
            if (!named) {
                throw std::invalid_argument("The rich text uses the font " + described.font + ", which is not registered.");
            }
            layout.families.push_back(named);
            found = families.emplace(described.font, std::move(named)).first;
        }
        family = found->second;
    }
    const float size = described.size.value_or(options.size) * described.sizeFactor * options.scale;
    cached = StyleFont{.family = family, .face = family->select(described.bold, described.italic, described.mono), .size = size};
    return *cached;
}

// Synthetic bold widens the strokes of a distance field font through its weight, and draws a bitmap font twice one native pixel apart.
std::size_t LayoutBuilder::getLook(std::size_t style, const FontFamily::Selection& face) {
    const auto key = std::make_pair(style, static_cast<const Font*>(face.font));
    if (const auto found = looks.find(key); found != looks.end()) {
        return found->second;
    }

    const RichTextDocument::Style& described = document.styles[style];
    const float size = getStyleFont(style).size;
    const bool field = face.font->isDistanceField();
    RichTextLayout::Look look{
        .font = face.font,
        .size = size,
        .outlineWidth = described.outlineWidth * options.scale,
        .outlineColor = described.outlineColor,
        .weight = face.syntheticBold && field ? size * kSyntheticWeight : 0.0F,
        .emboldenOffset = face.syntheticBold && !field ? size / face.font->getNativeSize() : 0.0F,
        .skew = face.syntheticItalic ? kSyntheticSkew : 0.0F,
    };
    if (described.shadow) {
        RichTextDocument::Shadow shadow = *described.shadow;
        shadow.offset = shadow.offset * options.scale;
        shadow.blur *= options.scale;
        look.shadow = shadow;
    }
    if (described.glow) {
        RichTextDocument::Glow glow = *described.glow;
        glow.width *= options.scale;
        look.glow = glow;
    }

    layout.looks.push_back(look);
    looks.emplace(key, layout.looks.size() - 1);
    return layout.looks.size() - 1;
}

math::Color LayoutBuilder::getColor(std::size_t style) const {
    const RichTextDocument::Style& described = document.styles[style];
    math::Color color = described.color.value_or(options.color);
    color.a *= described.alpha;
    return color;
}

void LayoutBuilder::appendText(std::vector<Piece>& pieces, std::size_t style, std::u32string_view text) {
    const StyleFont& font = getStyleFont(style);
    const RichTextDocument::Style& described = document.styles[style];
    for (std::size_t index = 0; index < text.size(); ++index) {
        const char32_t codePoint = text[index];
        const FontFamily::Selection face = font.family->resolve(font.face, codePoint, described.bold, described.italic);
        Font& drawing = *face.font;
        const std::size_t look = getLook(style, face);
        const RichTextLayout::Look& shape = layout.looks[look];

        Piece piece{.kind = Piece::Kind::Glyph, .style = style, .look = look, .codePoint = codePoint, .glyph = drawing.getGlyph(codePoint), .factor = font.size / drawing.getNativeSize(), .space = BreakRules::isSpace(codePoint)};
        piece.advance = piece.glyph.advance * piece.factor + shape.weight * 2.0F + shape.emboldenOffset;
        if (index + 1 < text.size() && font.family->resolve(font.face, text[index + 1], described.bold, described.italic).font == &drawing) {
            piece.advance += drawing.getKerning(codePoint, text[index + 1]) * piece.factor;
        }
        piece.ascent = drawing.getAscent(font.size);
        piece.descent = drawing.getLineHeight(font.size) - piece.ascent;
        pieces.push_back(std::move(piece));
    }
}

// Centered objects center on the middle of the text of their style, and top and bottom ones take their place once the line knows its height.
LayoutBuilder::Piece LayoutBuilder::measureObject(std::size_t style, graphics::Texture texture, math::Rect source, math::Vec2 extent, math::Color tint, RichTextDocument::VerticalAlign align) {
    const StyleFont& font = getStyleFont(style);
    const float ascent = font.face.font->getAscent(font.size);
    const float descent = font.face.font->getLineHeight(font.size) - ascent;
    Piece piece{.kind = Piece::Kind::Image, .style = style, .advance = extent.x, .texture = std::move(texture), .source = source, .extent = extent, .tint = tint, .align = align};

    using Align = RichTextDocument::VerticalAlign;
    if (align == Align::Baseline) {
        piece.ascent = extent.y;
    } else if (align == Align::Center) {
        piece.lift = (ascent - descent) * 0.5F;
        piece.ascent = piece.lift + extent.y * 0.5F;
        piece.descent = extent.y * 0.5F - piece.lift;
    }
    return piece;
}

// An image keeps the shape of its region when the markup gives only one side, and an icon without a size of its own is as tall as its text.
void LayoutBuilder::appendImage(std::vector<Piece>& pieces, const RichTextDocument::Inline& item) {
    // clang-format off
    const auto fit = [this](math::Vec2 natural, std::optional<float> width, std::optional<float> height) {
        const float ratio = natural.y > 0.0F ? natural.x / natural.y : 1.0F;
        if (width && height) {
            return math::Vec2{*width, *height} * options.scale;
        }
        if (width) {
            return math::Vec2{*width, ratio > 0.0F ? *width / ratio : 0.0F} * options.scale;
        }
        if (height) {
            return math::Vec2{*height * ratio, *height} * options.scale;
        }
        return natural;
    };
    // clang-format on

    if (item.kind == RichTextDocument::Inline::Kind::Image) {
        const RichTextDocument::Image& image = document.images[item.object];
        if (!options.images) {
            throw std::invalid_argument("The rich text has no image source for [img=" + image.path + "].");
        }
        graphics::Texture texture = options.images(image.path);
        if (!texture.isValid()) {
            layout.waitingForImages = true;
        }
        const math::Rect source = image.region.value_or(texture.isValid() ? math::Rect{0.0F, 0.0F, texture.getSize().x, texture.getSize().y} : math::Rect{});
        const math::Vec2 extent = fit(source.getSize() * options.scale, image.width, image.height);
        pieces.push_back(measureObject(item.style, std::move(texture), source, extent, image.color, image.align));
        return;
    }

    const RichTextDocument::Icon& icon = document.icons[item.object];
    const RichTextRegistry::Icon* registered = registry.findIcon(icon.name);
    if (registered == nullptr) {
        throw std::invalid_argument("The rich text uses the icon " + icon.name + ", which is not registered.");
    }
    const math::Vec2 shape = registered->source.getSize();
    const float textSize = getStyleFont(item.style).size;
    const math::Vec2 natural = registered->size.isZero() ? math::Vec2{shape.y > 0.0F ? textSize * shape.x / shape.y : textSize, textSize} : registered->size * options.scale;
    pieces.push_back(measureObject(item.style, registered->texture, registered->source, fit(natural, icon.width, icon.height), icon.color, icon.align));
}

// A line may wrap before a piece where the text allows it and around images, never before a space, and a pause stays with what follows it.
bool LayoutBuilder::breaksBefore(const std::vector<Piece>& pieces, std::size_t index) noexcept {
    const Piece& current = pieces[index];
    if (current.kind == Piece::Kind::Pause || current.space) {
        return false;
    }
    std::size_t previous = index;
    while (previous > 0 && pieces[previous - 1].kind == Piece::Kind::Pause) {
        --previous;
    }
    if (previous == 0) {
        return false;
    }
    const Piece& before = pieces[previous - 1];
    if (before.kind != Piece::Kind::Glyph || current.kind != Piece::Kind::Glyph) {
        return true;
    }
    return BreakRules::canBreakBetween(before.codePoint, current.codePoint);
}

void LayoutBuilder::closeLine(Flow& flow, Line& line, float& top) const {
    using Align = RichTextDocument::VerticalAlign;
    float ascent = 0.0F;
    float descent = 0.0F;
    bool measured = false;
    for (std::size_t index = line.begin; index < line.end; ++index) {
        const Piece& piece = flow.pieces[index];
        if (piece.kind == Piece::Kind::Glyph || (piece.kind == Piece::Kind::Image && piece.align != Align::Top && piece.align != Align::Bottom)) {
            ascent = std::max(ascent, piece.ascent);
            descent = std::max(descent, piece.descent);
            measured = true;
        }
    }
    if (!measured) {
        ascent = flow.emptyAscent;
        descent = flow.emptyDescent;
    }
    for (std::size_t index = line.begin; index < line.end; ++index) {
        const Piece& piece = flow.pieces[index];
        if (piece.kind == Piece::Kind::Image && piece.align == Align::Top) {
            descent = std::max(descent, piece.extent.y - ascent);
        } else if (piece.kind == Piece::Kind::Image && piece.align == Align::Bottom) {
            ascent = std::max(ascent, piece.extent.y - descent);
        }
    }

    line.ascent = ascent;
    line.descent = descent;
    line.top = top;
    top += (ascent + descent) * options.lineSpacing;
    flow.lines.push_back(line);
}

// Greedy wrapping between segments, the runs of pieces from one wrap opportunity to the next. A segment wider than a whole line wraps between its pieces instead, and every line keeps at least one piece. Lines beside a drop cap are shorter.
void LayoutBuilder::breakLines(Flow& flow, float available) {
    const std::vector<Piece>& pieces = flow.pieces;
    const bool bounded = available > 0.0F;
    float top = 0.0F;
    float penX = 0.0F;
    Line line;

    // clang-format off
    const auto start = [&](std::size_t begin) {
        line = Line{.begin = begin, .end = begin, .visibleEnd = begin, .top = top, .shift = top < flow.dropCapHeight ? flow.dropCapShift : 0.0F};
        penX = 0.0F;
    };
    const auto isVisible = [](const Piece& piece) {
        return (piece.kind == Piece::Kind::Glyph && !piece.space) || piece.kind == Piece::Kind::Image;
    };
    // clang-format on

    start(0);
    std::size_t cursor = 0;
    while (cursor < pieces.size()) {
        if (pieces[cursor].kind == Piece::Kind::LineBreak) {
            line.end = cursor + 1;
            closeLine(flow, line, top);
            start(cursor + 1);
            ++cursor;
            continue;
        }

        std::size_t segmentEnd = cursor + 1;
        while (segmentEnd < pieces.size() && pieces[segmentEnd].kind != Piece::Kind::LineBreak && !breaksBefore(pieces, segmentEnd)) {
            ++segmentEnd;
        }
        float total = 0.0F;
        float visible = 0.0F;
        std::size_t visibleEnd = cursor;
        for (std::size_t index = cursor; index < segmentEnd; ++index) {
            total += pieces[index].advance;
            if (isVisible(pieces[index])) {
                visible = total;
                visibleEnd = index + 1;
            }
        }
        flow.minimumWidth = std::max(flow.minimumWidth, visible);

        if (bounded && line.end > line.begin && penX + visible > available - line.shift) {
            line.wrapped = true;
            closeLine(flow, line, top);
            start(cursor);
        }
        if (bounded && line.end == line.begin && visible > available - line.shift) {
            std::size_t fit = cursor + 1;
            float width = pieces[cursor].advance;
            while (fit < segmentEnd && width + pieces[fit].advance <= available - line.shift) {
                width += pieces[fit].advance;
                ++fit;
            }
            if (fit < segmentEnd) {
                line.end = fit;
                line.visibleEnd = fit;
                line.width = width;
                line.wrapped = true;
                closeLine(flow, line, top);
                start(fit);
                cursor = fit;
                continue;
            }
        }

        line.end = segmentEnd;
        if (visible > 0.0F) {
            line.width = penX + visible;
            line.visibleEnd = visibleEnd;
        }
        penX += total;
        cursor = segmentEnd;
    }
    closeLine(flow, line, top);
}

std::vector<LayoutBuilder::Block> LayoutBuilder::measureBlocks(const std::vector<RichTextDocument::Paragraph>& paragraphs, float width) {
    std::vector<Block> blocks;
    blocks.reserve(paragraphs.size());
    for (const RichTextDocument::Paragraph& paragraph : paragraphs) {
        switch (paragraph.kind) {
        case RichTextDocument::Paragraph::Kind::Text:
            blocks.push_back(measureParagraph(paragraph, width));
            break;
        case RichTextDocument::Paragraph::Kind::Rule:
            blocks.push_back(measureRule(paragraph));
            break;
        case RichTextDocument::Paragraph::Kind::Table:
            blocks.push_back(measureTable(paragraph, width));
            break;
        }
    }
    return blocks;
}

LayoutBuilder::Block LayoutBuilder::measureParagraph(const RichTextDocument::Paragraph& paragraph, float width) {
    Block block{.paragraph = &paragraph};
    Flow& flow = block.flow;
    for (const RichTextDocument::Inline& item : paragraph.inlines) {
        switch (item.kind) {
        case RichTextDocument::Inline::Kind::Text:
            appendText(flow.pieces, item.style, item.text);
            break;
        case RichTextDocument::Inline::Kind::Image:
        case RichTextDocument::Inline::Kind::Icon:
            appendImage(flow.pieces, item);
            break;
        case RichTextDocument::Inline::Kind::LineBreak:
            flow.pieces.push_back({.kind = Piece::Kind::LineBreak, .style = item.style});
            break;
        case RichTextDocument::Inline::Kind::Pause:
            flow.pieces.push_back({.kind = Piece::Kind::Pause, .style = item.style, .pause = item.seconds});
            break;
        }
    }

    // An empty line is as tall as the text of its paragraph would be, or as the base text when it has none.
    const Font* emptyFont = options.family->getFaces().regular.get();
    float emptySize = options.size * options.scale;
    if (!paragraph.inlines.empty()) {
        const StyleFont& first = getStyleFont(paragraph.inlines.front().style);
        emptyFont = first.face.font;
        emptySize = first.size;
    }
    flow.emptyAscent = emptyFont->getAscent(emptySize);
    flow.emptyDescent = emptyFont->getLineHeight(emptySize) - flow.emptyAscent;

    if (paragraph.dropCap) {
        appendText(flow.dropCap, paragraph.dropCap->style, paragraph.dropCap->text);
        float dropCapWidth = 0.0F;
        float descent = 0.0F;
        for (const Piece& piece : flow.dropCap) {
            dropCapWidth += piece.advance;
            flow.dropCapAscent = std::max(flow.dropCapAscent, piece.ascent);
            descent = std::max(descent, piece.descent);
        }
        flow.dropCapHeight = flow.dropCapAscent + descent;
        flow.dropCapShift = dropCapWidth + paragraph.dropCap->margin * options.scale;
    }
    if (!paragraph.marker.empty()) {
        appendText(flow.marker, paragraph.markerStyle, paragraph.marker);
    }

    const float indent = paragraph.indent * getIndentUnit();
    breakLines(flow, width > 0.0F ? std::max(width - indent, 1.0F) : 0.0F);
    for (const Line& line : flow.lines) {
        flow.naturalWidth = std::max(flow.naturalWidth, line.shift + line.width);
    }

    const Line& last = flow.lines.back();
    const float linesHeight = last.top + (last.ascent + last.descent) * options.lineSpacing;
    block.height = std::max(linesHeight, flow.dropCapHeight);
    block.trailing = linesHeight >= flow.dropCapHeight ? (last.ascent + last.descent) * (options.lineSpacing - 1.0F) : 0.0F;
    block.naturalWidth = indent + flow.naturalWidth;
    block.minimumWidth = indent + std::max(flow.minimumWidth, flow.dropCapShift);
    return block;
}

LayoutBuilder::Block LayoutBuilder::measureRule(const RichTextDocument::Paragraph& paragraph) {
    const float margin = options.size * options.scale * kRuleMarginEms;
    return {.paragraph = &paragraph, .height = paragraph.ruleThickness * options.scale + margin * 2.0F};
}

// Columns take the width their content wants. When the table would be wider than the text, each column gives up room in proportion to how far it can shrink before its widest word no longer fits.
LayoutBuilder::Block LayoutBuilder::measureTable(const RichTextDocument::Paragraph& paragraph, float width) {
    const RichTextDocument::Table& table = document.tables[paragraph.table];
    const std::size_t columns = table.columns;
    std::vector<float> natural(columns, 0.0F);
    std::vector<float> minimum(columns, 0.0F);
    for (std::size_t index = 0; index < table.cells.size(); ++index) {
        const RichTextDocument::Cell& cell = table.cells[index];
        const float padding = cell.padding * options.scale * 2.0F;
        for (const Block& block : measureBlocks(cell.paragraphs, 0.0F)) {
            natural[index % columns] = std::max(natural[index % columns], block.naturalWidth + padding);
            minimum[index % columns] = std::max(minimum[index % columns], block.minimumWidth + padding);
        }
    }

    const float indent = paragraph.indent * getIndentUnit();
    const float available = width > 0.0F ? width - indent : 0.0F;
    const float naturalSum = std::accumulate(natural.begin(), natural.end(), 0.0F);
    const float minimumSum = std::accumulate(minimum.begin(), minimum.end(), 0.0F);
    TableBlock laid{.columns = natural, .rows = std::vector<float>((table.cells.size() + columns - 1) / columns, 0.0F)};
    if (available > 0.0F && naturalSum > available) {
        const float share = minimumSum < available ? (available - minimumSum) / (naturalSum - minimumSum) : 0.0F;
        for (std::size_t column = 0; column < columns; ++column) {
            laid.columns[column] = minimum[column] + (natural[column] - minimum[column]) * share;
        }
    }

    for (std::size_t index = 0; index < table.cells.size(); ++index) {
        const RichTextDocument::Cell& cell = table.cells[index];
        const float padding = cell.padding * options.scale * 2.0F;
        std::vector<Block> blocks = measureBlocks(cell.paragraphs, std::max(laid.columns[index % columns] - padding, 1.0F));
        float height = padding;
        for (const Block& block : blocks) {
            height += block.height;
        }
        if (!blocks.empty()) {
            height -= blocks.back().trailing;
        }
        laid.rows[index / columns] = std::max(laid.rows[index / columns], height);
        laid.cells.push_back(std::move(blocks));
    }

    Block block{.paragraph = &paragraph};
    block.height = std::accumulate(laid.rows.begin(), laid.rows.end(), 0.0F);
    block.naturalWidth = indent + std::accumulate(laid.columns.begin(), laid.columns.end(), 0.0F);
    block.minimumWidth = indent + minimumSum;
    block.table = std::move(laid);
    return block;
}

RichTextLayout LayoutBuilder::build() {
    layout.families.push_back(options.family);

    const std::vector<Block> blocks = measureBlocks(document.paragraphs, options.maxWidth);
    float width = std::max(options.maxWidth, 0.0F);
    if (options.maxWidth <= 0.0F) {
        for (const Block& block : blocks) {
            width = std::max(width, block.naturalWidth);
        }
    }
    for (const Block& block : blocks) {
        layout.lineCount += block.table ? block.table->rows.size() : std::max<std::size_t>(block.flow.lines.size(), 1);
    }

    layout.size = {width, emitBlocks(blocks, {}, width)};
    return std::move(layout);
}

float LayoutBuilder::emitBlocks(const std::vector<Block>& blocks, math::Vec2 origin, float width) {
    float y = origin.y;
    for (const Block& block : blocks) {
        switch (block.paragraph->kind) {
        case RichTextDocument::Paragraph::Kind::Text:
            emitParagraph(block, {origin.x, y}, width);
            break;
        case RichTextDocument::Paragraph::Kind::Rule:
            emitRule(block, {origin.x, y}, width);
            break;
        case RichTextDocument::Paragraph::Kind::Table:
            emitTable(block, {origin.x, y});
            break;
        }
        y += block.height;
    }
    return y - origin.y - (blocks.empty() ? 0.0F : blocks.back().trailing);
}

void LayoutBuilder::emitParagraph(const Block& block, math::Vec2 origin, float width) {
    const RichTextDocument::Paragraph& paragraph = *block.paragraph;
    const Flow& flow = block.flow;
    const float indent = paragraph.indent * getIndentUnit();
    const TextAlign align = paragraph.align.value_or(options.align);
    const float left = origin.x + indent;
    const float room = std::max(width - indent, 0.0F);

    // The drop cap reads first and stands at the top of the paragraph.
    float dropX = left;
    for (const Piece& piece : flow.dropCap) {
        emitPiece(piece, {dropX, origin.y + flow.dropCapAscent}, origin.y, flow.dropCapHeight, true);
        dropX += piece.advance;
    }

    for (std::size_t lineIndex = 0; lineIndex < flow.lines.size(); ++lineIndex) {
        const Line& line = flow.lines[lineIndex];
        const float lineRoom = room - line.shift;
        float x = left + line.shift + alignOffset(align, lineRoom, line.width);
        float spacing = 0.0F;
        if (align == TextAlign::Fill && line.wrapped) {
            const auto spaces = std::count_if(flow.pieces.begin() + static_cast<std::ptrdiff_t>(line.begin), flow.pieces.begin() + static_cast<std::ptrdiff_t>(line.visibleEnd), [](const Piece& piece) { return piece.space; });
            spacing = spaces > 0 ? (lineRoom - line.width) / static_cast<float>(spaces) : 0.0F;
        }
        const float lineTop = origin.y + line.top;
        const float baseline = lineTop + line.ascent;
        const float lineHeight = line.ascent + line.descent;

        // A list marker ends a small gap before the indent on the first baseline, and shows with the first character of its item.
        if (lineIndex == 0 && !flow.marker.empty()) {
            float markerWidth = 0.0F;
            for (const Piece& piece : flow.marker) {
                markerWidth += piece.advance;
            }
            float markerX = left - markerWidth - options.size * options.scale * kMarkerGapEms;
            for (const Piece& piece : flow.marker) {
                emitPiece(piece, {markerX, baseline}, lineTop, lineHeight, false);
                markerX += piece.advance;
            }
        }

        std::vector<float> lefts(line.end - line.begin + 1, x);
        for (std::size_t index = line.begin; index < line.end; ++index) {
            const Piece& piece = flow.pieces[index];
            lefts[index - line.begin] = x;
            if (piece.kind == Piece::Kind::Pause) {
                pendingPause += piece.pause;
                continue;
            }
            if (piece.kind == Piece::Kind::LineBreak) {
                continue;
            }
            emitPiece(piece, {x, baseline}, lineTop, lineHeight, true);
            x += piece.advance;
            if (piece.space && index < line.visibleEnd) {
                x += spacing;
            }
        }
        lefts.back() = x;
        emitDecorations(flow, line, lefts, baseline);
    }
}

void LayoutBuilder::emitPiece(const Piece& piece, math::Vec2 pen, float lineTop, float lineHeight, bool counted) {
    const std::size_t character = layout.characters.size();
    if (counted) {
        layout.characters.push_back({.box = {pen.x, lineTop, piece.advance, lineHeight}, .pause = std::exchange(pendingPause, 0.0F), .speed = document.styles[piece.style].revealSpeed});
    }

    if (piece.kind == Piece::Kind::Image) {
        using Align = RichTextDocument::VerticalAlign;
        float top = pen.y - piece.lift - piece.extent.y * 0.5F;
        if (piece.align == Align::Top) {
            top = lineTop;
        } else if (piece.align == Align::Bottom) {
            top = lineTop + lineHeight - piece.extent.y;
        } else if (piece.align == Align::Baseline) {
            top = pen.y - piece.extent.y;
        }
        if (piece.texture.isValid() && !piece.extent.isZero()) {
            math::Color tint = piece.tint;
            tint.a *= document.styles[piece.style].alpha;
            layout.images.push_back({.texture = piece.texture, .rect = {pen.x, top, piece.extent.x, piece.extent.y}, .source = piece.source, .color = tint, .character = character});
        }
        return;
    }
    if (!piece.glyph.visible) {
        return;
    }

    const RichTextLayout::Look& look = layout.looks[piece.look];
    layout.glyphs.push_back({
        .look = piece.look,
        .style = piece.style,
        .character = character,
        .codePoint = piece.codePoint,
        .position = {pen.x + look.weight + piece.glyph.offset.x * piece.factor, pen.y + piece.glyph.offset.y * piece.factor},
        .size = piece.glyph.source.getSize() * piece.factor,
        .baseline = pen.y,
        .source = piece.glyph.source,
        .page = piece.glyph.page,
        .color = getColor(piece.style),
    });
}

// Backgrounds, underlines, strikes, links and hints each merge the neighbouring pieces of a line that share them into one rectangle, up to the last visible piece.
void LayoutBuilder::emitDecorations(const Flow& flow, const Line& line, const std::vector<float>& lefts, float baseline) {
    const float lineTop = baseline - line.ascent;
    const float lineHeight = line.ascent + line.descent;
    std::vector<std::size_t> characters(line.end - line.begin, layout.characters.size());
    std::size_t next = layout.characters.size();
    for (std::size_t index = line.end; index > line.begin; --index) {
        const Piece& piece = flow.pieces[index - 1];
        if (piece.kind == Piece::Kind::Glyph || piece.kind == Piece::Kind::Image) {
            --next;
        }
        characters[index - 1 - line.begin] = next;
    }

    // clang-format off
    const auto runs = [&](const auto& keyOf, const auto& emit) {
        std::size_t begin = line.begin;
        while (begin < line.visibleEnd) {
            const auto key = keyOf(flow.pieces[begin]);
            std::size_t end = begin + 1;
            while (end < line.visibleEnd && keyOf(flow.pieces[end]) == key) {
                ++end;
            }
            if (key) {
                const float x = lefts[begin - line.begin];
                const float right = lefts[end - 1 - line.begin] + flow.pieces[end - 1].advance;
                emit(*key, math::Rect{x, lineTop, right - x, lineHeight}, characters[begin - line.begin], characters[end - 1 - line.begin], flow.pieces[begin]);
            }
            begin = end;
        }
    };
    // clang-format on

    const auto styleOf = [this](const Piece& piece) -> const RichTextDocument::Style& { return document.styles[piece.style]; };
    runs([&](const Piece& piece) { return styleOf(piece).background; }, [&](math::Color color, const math::Rect& rect, std::size_t first, std::size_t last, const Piece&) { addBox(RichTextLayout::Box::Kind::Background, rect, color, first, last); });

    // clang-format off
    const auto decorate = [&](bool underline) {
        runs([&](const Piece& piece) -> std::optional<std::size_t> {
            const RichTextDocument::Style& style = styleOf(piece);
            const bool shown = underline ? style.underline || (style.link && options.underlineLinks) : style.strike;
            return shown ? std::optional<std::size_t>(piece.style) : std::nullopt;
        }, [&](std::size_t style, const math::Rect& rect, std::size_t first, std::size_t last, const Piece&) {
            const float size = getStyleFont(style).size;
            const float thickness = std::max(1.0F, size * kDecorationThickness);
            const float y = underline ? baseline + size * kUnderlineOffset : baseline - size * kStrikeOffset;
            addBox(underline ? RichTextLayout::Box::Kind::Underline : RichTextLayout::Box::Kind::Strike, {rect.x, y - thickness * 0.5F, rect.width, thickness}, getColor(style), first, last);
        });
    };
    // clang-format on
    decorate(true);
    decorate(false);

    runs([&](const Piece& piece) { return styleOf(piece).link; }, [&](std::size_t link, const math::Rect& rect, std::size_t, std::size_t, const Piece&) { layout.links.push_back({.rect = rect, .index = link}); });
    runs([&](const Piece& piece) { return styleOf(piece).hint; }, [&](std::size_t hint, const math::Rect& rect, std::size_t, std::size_t, const Piece&) { layout.hints.push_back({.rect = rect, .index = hint}); });
}

void LayoutBuilder::addBox(RichTextLayout::Box::Kind kind, const math::Rect& rect, math::Color color, std::size_t firstCharacter, std::size_t lastCharacter) {
    layout.boxes.push_back({.kind = kind, .rect = rect, .color = color, .firstCharacter = firstCharacter, .lastCharacter = lastCharacter});
}

// A rule shows once the reveal reaches it, centered unless its paragraph aligns it.
void LayoutBuilder::emitRule(const Block& block, math::Vec2 origin, float width) {
    const RichTextDocument::Paragraph& paragraph = *block.paragraph;
    const float indent = paragraph.indent * getIndentUnit();
    const float room = std::max(width - indent, 0.0F);
    const float length = room * paragraph.ruleWidth;
    const float x = origin.x + indent + alignOffset(paragraph.align.value_or(TextAlign::Center), room, length);
    const float margin = options.size * options.scale * kRuleMarginEms;
    const std::size_t next = layout.characters.size();
    addBox(RichTextLayout::Box::Kind::Rule, {x, origin.y + margin, length, paragraph.ruleThickness * options.scale}, paragraph.ruleColor.value_or(options.color), next, next);
}

void LayoutBuilder::emitTable(const Block& block, math::Vec2 origin) {
    const RichTextDocument::Paragraph& paragraph = *block.paragraph;
    const RichTextDocument::Table& table = document.tables[paragraph.table];
    const TableBlock& laid = *block.table;
    const float left = origin.x + paragraph.indent * getIndentUnit();
    const float border = std::max(1.0F, options.scale);

    for (std::size_t index = 0; index < table.cells.size(); ++index) {
        const RichTextDocument::Cell& cell = table.cells[index];
        const std::size_t column = index % table.columns;
        const std::size_t row = index / table.columns;
        const float x = left + std::accumulate(laid.columns.begin(), laid.columns.begin() + static_cast<std::ptrdiff_t>(column), 0.0F);
        const float y = origin.y + std::accumulate(laid.rows.begin(), laid.rows.begin() + static_cast<std::ptrdiff_t>(row), 0.0F);
        const math::Rect area{x, y, laid.columns[column], laid.rows[row]};
        const std::size_t next = layout.characters.size();

        if (cell.background) {
            addBox(RichTextLayout::Box::Kind::CellBackground, area, *cell.background, next, next);
        }
        if (cell.border) {
            for (const math::Rect& edge : {math::Rect{area.x, area.y, area.width, border}, math::Rect{area.x, area.getBottom() - border, area.width, border}, math::Rect{area.x, area.y, border, area.height}, math::Rect{area.getRight() - border, area.y, border, area.height}}) {
                addBox(RichTextLayout::Box::Kind::CellBorder, edge, *cell.border, next, next);
            }
        }
        const float padding = cell.padding * options.scale;
        emitBlocks(laid.cells[index], {area.x + padding, area.y + padding}, std::max(area.width - padding * 2.0F, 0.0F));
    }
}

} // namespace haylen::text
