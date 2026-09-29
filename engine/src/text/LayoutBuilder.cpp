#include "text/LayoutBuilder.hpp"

#include <algorithm>
#include <numeric>
#include <stdexcept>
#include <utility>

#include "haylen/core/Utf8.hpp"
#include "text/BidiParagraph.hpp"

namespace haylen::text {

LayoutBuilder::LayoutBuilder(const RichTextDocument& source, const RichTextOptions& layoutOptions, const RichTextRegistry& textRegistry) : LayoutBuilder(source, layoutOptions, &textRegistry, layoutOptions.family.get(), nullptr) {}

LayoutBuilder::LayoutBuilder(const RichTextDocument& source, const RichTextOptions& layoutOptions, const RichTextRegistry* textRegistry, const FontFamily* baseFamily, Font* baseFont) : document(source), options(layoutOptions), registry(textRegistry), family(baseFamily), loneFont(baseFont), styleFonts(source.styles.size()) {}

// The carriage return of CRLF stays at the end of its paragraph, where it draws nothing, so the characters keep counting the code points of the text.
TextLayout LayoutBuilder::layoutPlainText(std::string_view text, const TextStyle& style, const FontFamily* baseFamily, Font* baseFont) {
    RichTextDocument plain{.styles = {RichTextDocument::Style{}}};
    const std::u32string codePoints = core::Utf8::decode(text);
    // clang-format off
    const auto endsParagraph = [&codePoints](std::size_t index) {
        const bool crlf = codePoints[index] == U'\r' && index + 1 < codePoints.size() && codePoints[index + 1] == U'\n';
        return Segmenter::isParagraphSeparator(codePoints[index]) && !crlf;
    };
    // clang-format on

    std::size_t start = 0;
    while (start <= codePoints.size()) {
        std::size_t end = start;
        while (end < codePoints.size() && !endsParagraph(end)) {
            ++end;
        }
        RichTextDocument::Paragraph& paragraph = plain.paragraphs.emplace_back();
        if (end > start) {
            paragraph.inlines.push_back({.kind = RichTextDocument::Inline::Kind::Text, .text = codePoints.substr(start, end - start)});
        }
        start = end + 1;
    }

    const RichTextOptions plainOptions{.size = style.size, .bold = style.bold, .italic = style.italic, .color = style.color, .maxWidth = style.maxWidth, .align = style.align, .direction = style.direction, .language = style.language, .lineSpacing = style.lineSpacing};
    return LayoutBuilder(plain, plainOptions, nullptr, baseFamily, baseFont).build();
}

float LayoutBuilder::getIndentUnit() const noexcept {
    return options.size * options.scale * kIndentEms;
}

Direction LayoutBuilder::getDirection(const RichTextDocument::Paragraph& paragraph) const noexcept {
    return paragraph.direction.value_or(options.direction);
}

TextAlign LayoutBuilder::resolveAlign(TextAlign align, bool rightToLeft, bool lastLine) noexcept {
    switch (align) {
    case TextAlign::Start:
        return rightToLeft ? TextAlign::Right : TextAlign::Left;
    case TextAlign::End:
        return rightToLeft ? TextAlign::Left : TextAlign::Right;
    case TextAlign::Fill:
        return lastLine ? resolveAlign(TextAlign::Start, rightToLeft, false) : TextAlign::Fill;
    case TextAlign::Left:
    case TextAlign::Center:
    case TextAlign::Right:
        break;
    }
    return align;
}

float LayoutBuilder::alignOffset(TextAlign align, float room, float width) noexcept {
    switch (align) {
    case TextAlign::Center:
        return (room - width) * 0.5F;
    case TextAlign::Right:
        return room - width;
    default:
        return 0.0F;
    }
}

const LayoutBuilder::StyleFont& LayoutBuilder::getStyleFont(std::size_t style) {
    std::optional<StyleFont>& cached = styleFonts[style];
    if (cached) {
        return *cached;
    }

    const RichTextDocument::Style& described = document.styles[style];
    const bool bold = described.bold || options.bold;
    const bool italic = described.italic || options.italic;
    const float size = described.size.value_or(options.size) * described.sizeFactor * options.scale;
    if (described.font.empty() && family == nullptr) {
        cached = StyleFont{.face = {.font = loneFont, .syntheticBold = bold, .syntheticItalic = italic}, .bold = bold, .italic = italic, .size = size};
        return *cached;
    }

    const FontFamily* chosen = family;
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
        chosen = found->second.get();
    }
    cached = StyleFont{.family = chosen, .face = chosen->select(bold, italic, described.mono), .bold = bold, .italic = italic, .size = size};
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
    TextLayout::Look look{
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

// Shapes one run of a font, script and direction. The glyphs come in visual order, so the clusters of a right-to-left run count down, and every cluster becomes a piece in reading order that keeps its glyphs in visual order. A cluster whose code points draw nothing, such as a tab or another control character, keeps no glyph and takes no room.
void LayoutBuilder::shapeRun(const Source& source, std::size_t begin, std::size_t end, const FontFamily::Selection& face, std::uint8_t level, std::uint32_t script, std::vector<Piece>& pieces, std::vector<PlacedGlyph>& glyphs) {
    const std::size_t style = source.styles[begin];
    const StyleFont& font = getStyleFont(style);
    Font& drawing = *face.font;
    const std::size_t look = getLook(style, face);
    const TextLayout::Look& drawn = layout.looks[look];
    const float factor = font.size / drawing.getNativeSize();

    shapedGlyphs.clear();
    drawing.shape({.text = source.text, .begin = begin, .end = end, .script = script, .language = options.language, .rightToLeft = level % 2 != 0}, shapedGlyphs);
    std::vector<std::size_t> byCluster(shapedGlyphs.size());
    std::iota(byCluster.begin(), byCluster.end(), std::size_t{0});
    std::ranges::stable_sort(byCluster, {}, [this](std::size_t index) { return shapedGlyphs[index].cluster; });

    std::size_t next = 0;
    while (next < byCluster.size()) {
        const std::size_t cluster = shapedGlyphs[byCluster[next]].cluster;
        std::size_t last = next;
        while (last < byCluster.size() && shapedGlyphs[byCluster[last]].cluster == cluster) {
            ++last;
        }
        Piece piece{.kind = Piece::Kind::Cluster, .style = style, .look = look, .begin = cluster, .end = last < byCluster.size() ? shapedGlyphs[byCluster[last]].cluster : end, .firstGlyph = glyphs.size(), .level = level};
        const auto codePoints = std::u32string_view(source.text).substr(piece.begin, piece.end - piece.begin);
        const bool visible = !std::ranges::all_of(codePoints, &Segmenter::isInvisible);

        float pen = 0.0F;
        for (; visible && next < last; ++next) {
            const Font::ShapedGlyph& shaped = shapedGlyphs[byCluster[next]];
            glyphs.push_back({.glyph = drawing.getGlyph(shaped.index), .index = shaped.index, .factor = factor, .offset = math::Vec2{pen, 0.0F} + shaped.offset * factor});
            pen += shaped.advance * factor;
        }
        next = last;
        piece.glyphCount = glyphs.size() - piece.firstGlyph;
        piece.advance = visible ? pen + drawn.weight * 2.0F + drawn.emboldenOffset : 0.0F;
        piece.ascent = drawing.getAscent(font.size);
        piece.descent = drawing.getLineHeight(font.size) - piece.ascent;
        piece.space = std::ranges::all_of(codePoints, &Segmenter::isSpace);
        pieces.push_back(std::move(piece));
    }
}

// Every grapheme cluster picks the font that draws it, spaces and punctuation keeping the font before them, and runs of one style, font, level and script shape together with the whole paragraph as their context. Objects and line breaks stand alone, and a pause stays with the piece after it, so a line never wraps between them.
std::vector<LayoutBuilder::Piece> LayoutBuilder::shape(const Source& source, const BidiParagraph& bidi, std::vector<PlacedGlyph>& glyphs) {
    const std::u32string_view text = source.text;
    const std::vector<std::uint32_t> scripts = Segmenter::getScripts(text);
    const std::vector<bool> graphemes = Segmenter::getGraphemeStarts(text);
    const std::vector<Segmenter::Break> breaks = Segmenter::getLineBreaks(text, options.language);

    std::vector<Piece> pieces;
    std::size_t nextPause = 0;
    // clang-format off
    const auto add = [&](Piece piece) {
        piece.breakBefore = piece.begin > 0 ? breaks[piece.begin - 1] : Segmenter::Break::Never;
        const std::size_t firstPause = pieces.size();
        for (; nextPause < source.pauses.size() && source.pauses[nextPause].first <= piece.begin; ++nextPause) {
            pieces.push_back({.kind = Piece::Kind::Pause, .style = piece.style, .begin = piece.begin, .end = piece.begin, .pause = source.pauses[nextPause].second});
        }
        if (pieces.size() > firstPause) {
            pieces[firstPause].breakBefore = std::exchange(piece.breakBefore, Segmenter::Break::Never);
        }
        pieces.push_back(std::move(piece));
    };
    // clang-format on

    std::vector<Piece> shaped;
    std::size_t runBegin = 0;
    FontFamily::Selection runFace;
    Font* previous = nullptr;
    std::size_t index = 0;
    while (index < text.size()) {
        std::size_t clusterEnd = index + 1;
        while (clusterEnd < text.size() && !graphemes[clusterEnd]) {
            ++clusterEnd;
        }
        const std::u32string_view cluster = text.substr(index, clusterEnd - index);
        const bool object = text[index] == kObject && source.objects.contains(index);
        const bool lineBreak = text[index] == kLineSeparator;

        FontFamily::Selection face;
        if (!object && !lineBreak) {
            const StyleFont& font = getStyleFont(source.styles[index]);
            const bool common = std::ranges::all_of(cluster, &Segmenter::isCommon);
            face = font.family != nullptr ? font.family->resolve(font.face, cluster, font.bold, font.italic, common ? previous : nullptr) : font.face;
            previous = face.font;
        }

        // A run ends before an object, a line break, or a change of style, font, level or script.
        const bool continues = index > runBegin && !object && !lineBreak && face.font == runFace.font && source.styles[index] == source.styles[runBegin] && bidi.getLevel(index) == bidi.getLevel(runBegin) && scripts[index] == scripts[runBegin];
        if (!continues && index > runBegin && runFace.font != nullptr) {
            shaped.clear();
            shapeRun(source, runBegin, index, runFace, bidi.getLevel(runBegin), scripts[runBegin], shaped, glyphs);
            for (Piece& piece : shaped) {
                add(std::move(piece));
            }
        }
        if (!continues) {
            runBegin = index;
            runFace = face;
        }

        if (object) {
            Piece piece = measureImage(*source.objects.at(index));
            piece.begin = index;
            piece.end = clusterEnd;
            piece.level = bidi.getLevel(index);
            add(std::move(piece));
            runBegin = clusterEnd;
            runFace = {};
        } else if (lineBreak) {
            add({.kind = Piece::Kind::LineBreak, .style = source.styles[index], .begin = index, .end = clusterEnd, .level = bidi.getLevel(index)});
            runBegin = clusterEnd;
            runFace = {};
        }
        index = clusterEnd;
    }
    if (runFace.font != nullptr && runBegin < text.size()) {
        shaped.clear();
        shapeRun(source, runBegin, text.size(), runFace, bidi.getLevel(runBegin), scripts[runBegin], shaped, glyphs);
        for (Piece& piece : shaped) {
            add(std::move(piece));
        }
    }
    for (; nextPause < source.pauses.size(); ++nextPause) {
        pieces.push_back({.kind = Piece::Kind::Pause, .begin = text.size(), .end = text.size(), .pause = source.pauses[nextPause].second});
    }
    return pieces;
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
LayoutBuilder::Piece LayoutBuilder::measureImage(const RichTextDocument::Inline& item) {
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
        return measureObject(item.style, std::move(texture), source, extent, image.color, image.align);
    }

    const RichTextDocument::Icon& icon = document.icons[item.object];
    const RichTextRegistry::Icon* registered = registry != nullptr ? registry->findIcon(icon.name) : nullptr;
    if (registered == nullptr) {
        throw std::invalid_argument("The rich text uses the icon " + icon.name + ", which is not registered.");
    }
    const math::Vec2 shape = registered->source.getSize();
    const float textSize = getStyleFont(item.style).size;
    const math::Vec2 natural = registered->size.isZero() ? math::Vec2{shape.y > 0.0F ? textSize * shape.x / shape.y : textSize, textSize} : registered->size * options.scale;
    return measureObject(item.style, registered->texture, registered->source, fit(natural, icon.width, icon.height), icon.color, icon.align);
}

// A drop cap or a list marker reads in the direction of its paragraph and never wraps, so it keeps its pieces in reading order and the order they stand in from the left.
void LayoutBuilder::shapeAside(Flow& flow, std::u32string_view text, std::size_t style, std::vector<Piece>& pieces, std::vector<std::size_t>& order) {
    const Source source{.text = std::u32string(text), .styles = std::vector<std::size_t>(text.size(), style)};
    const BidiParagraph bidi(source.text, flow.rightToLeft ? Direction::RightToLeft : Direction::LeftToRight);
    pieces = shape(source, bidi, flow.glyphs);
    order = orderPieces(pieces, 0, pieces.size(), bidi);
}

// A line may wrap, or must end, before a piece where the Unicode line breaking rules say so, and a pause stays with what follows it, so the line breaks before the pause.
Segmenter::Break LayoutBuilder::breakBefore(const std::vector<Piece>& pieces, std::size_t index) noexcept {
    return index > 0 && pieces[index - 1].kind != Piece::Kind::Pause ? pieces[index].breakBefore : Segmenter::Break::Never;
}

// The drawable pieces of a line from left to right: the runs of the line in the order the bidirectional algorithm shows them, each read forward or backward by the direction of its level.
std::vector<std::size_t> LayoutBuilder::orderPieces(const std::vector<Piece>& pieces, std::size_t begin, std::size_t end, const BidiParagraph& bidi) {
    std::vector<std::size_t> order;
    std::size_t first = end;
    std::size_t last = begin;
    for (std::size_t index = begin; index < end; ++index) {
        if (pieces[index].kind != Piece::Kind::Pause) {
            first = std::min(first, index);
            last = index + 1;
        }
    }
    if (first >= last) {
        return order;
    }

    for (const BidiParagraph::Run& run : bidi.getVisualRuns(pieces[first].begin, pieces[last - 1].end)) {
        const std::size_t start = order.size();
        for (std::size_t index = first; index < last; ++index) {
            const Piece& piece = pieces[index];
            const bool drawable = piece.kind == Piece::Kind::Cluster || piece.kind == Piece::Kind::Image;
            if (drawable && piece.begin >= run.begin && piece.begin < run.end) {
                order.push_back(index);
            }
        }
        if (run.level % 2 != 0) {
            std::reverse(order.begin() + static_cast<std::ptrdiff_t>(start), order.end());
        }
    }
    return order;
}

void LayoutBuilder::closeLine(Flow& flow, Line& line, float& top) const {
    using Align = RichTextDocument::VerticalAlign;
    float ascent = 0.0F;
    float descent = 0.0F;
    bool measured = false;
    for (std::size_t index = line.begin; index < line.end; ++index) {
        const Piece& piece = flow.pieces[index];
        if (piece.kind == Piece::Kind::Cluster || (piece.kind == Piece::Kind::Image && piece.align != Align::Top && piece.align != Align::Bottom)) {
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

// Greedy wrapping between segments, the runs of pieces from one break opportunity to the next. A segment wider than a whole line wraps between its clusters instead, and every line keeps at least one piece. A mandatory break, such as the one after a vertical tab or a form feed, ends the line. Lines beside a drop cap are shorter.
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
        return (piece.kind == Piece::Kind::Cluster && !piece.space) || piece.kind == Piece::Kind::Image;
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

        if (line.end > line.begin && breakBefore(pieces, cursor) == Segmenter::Break::Mandatory) {
            closeLine(flow, line, top);
            start(cursor);
        }

        std::size_t segmentEnd = cursor + 1;
        while (segmentEnd < pieces.size() && pieces[segmentEnd].kind != Piece::Kind::LineBreak && breakBefore(pieces, segmentEnd) == Segmenter::Break::Never) {
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

    // The paragraph becomes one text, where an object replacement character stands for each image or icon and a line separator for each line break.
    Source source;
    for (const RichTextDocument::Inline& item : paragraph.inlines) {
        switch (item.kind) {
        case RichTextDocument::Inline::Kind::Text:
            source.text += item.text;
            source.styles.insert(source.styles.end(), item.text.size(), item.style);
            break;
        case RichTextDocument::Inline::Kind::Image:
        case RichTextDocument::Inline::Kind::Icon:
            source.objects.emplace(source.text.size(), &item);
            source.text += kObject;
            source.styles.push_back(item.style);
            break;
        case RichTextDocument::Inline::Kind::LineBreak:
            source.text += kLineSeparator;
            source.styles.push_back(item.style);
            break;
        case RichTextDocument::Inline::Kind::Pause:
            source.pauses.emplace_back(source.text.size(), item.seconds);
            break;
        }
    }
    const BidiParagraph bidi(source.text, getDirection(paragraph));
    flow.rightToLeft = bidi.isRightToLeft();
    flow.pieces = shape(source, bidi, flow.glyphs);
    flow.text = source.text;

    // An empty line is as tall as the text of its paragraph would be, or as the base text when it has none.
    const Font* emptyFont = family != nullptr ? family->getFaces().regular.get() : loneFont;
    float emptySize = options.size * options.scale;
    if (!paragraph.inlines.empty()) {
        const StyleFont& first = getStyleFont(paragraph.inlines.front().style);
        emptyFont = first.face.font;
        emptySize = first.size;
    }
    flow.emptyAscent = emptyFont->getAscent(emptySize);
    flow.emptyDescent = emptyFont->getLineHeight(emptySize) - flow.emptyAscent;

    if (paragraph.dropCap) {
        flow.dropCapText = paragraph.dropCap->text;
        shapeAside(flow, paragraph.dropCap->text, paragraph.dropCap->style, flow.dropCap, flow.dropCapOrder);
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
        shapeAside(flow, paragraph.marker, paragraph.markerStyle, flow.marker, flow.markerOrder);
    }

    const float indent = paragraph.indent * getIndentUnit();
    breakLines(flow, width > 0.0F ? std::max(width - indent, 1.0F) : 0.0F);
    for (Line& line : flow.lines) {
        line.order = orderPieces(flow.pieces, line.begin, line.end, bidi);
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

// Columns take the width their content wants. When the table would be wider than the text, each column gives up room in proportion to how far it can shrink before its widest word no longer fits. A table reads in the direction of its first paragraph unless it has its own, so its first column stands at the right of a right-to-left table.
LayoutBuilder::Block LayoutBuilder::measureTable(const RichTextDocument::Paragraph& paragraph, float width) {
    const RichTextDocument::Table& table = document.tables[paragraph.table];
    const std::size_t columns = table.columns;
    std::vector<float> natural(columns, 0.0F);
    std::vector<float> minimum(columns, 0.0F);
    std::optional<bool> firstRightToLeft;
    for (std::size_t index = 0; index < table.cells.size(); ++index) {
        const RichTextDocument::Cell& cell = table.cells[index];
        const float padding = cell.padding * options.scale * 2.0F;
        for (const Block& block : measureBlocks(cell.paragraphs, 0.0F)) {
            natural[index % columns] = std::max(natural[index % columns], block.naturalWidth + padding);
            minimum[index % columns] = std::max(minimum[index % columns], block.minimumWidth + padding);
            if (!firstRightToLeft && block.paragraph->kind == RichTextDocument::Paragraph::Kind::Text) {
                firstRightToLeft = block.flow.rightToLeft;
            }
        }
    }

    const float indent = paragraph.indent * getIndentUnit();
    const float available = width > 0.0F ? width - indent : 0.0F;
    const float naturalSum = std::accumulate(natural.begin(), natural.end(), 0.0F);
    const float minimumSum = std::accumulate(minimum.begin(), minimum.end(), 0.0F);
    const Direction direction = getDirection(paragraph);
    TableBlock laid{.columns = natural, .rows = std::vector<float>((table.cells.size() + columns - 1) / columns, 0.0F), .rightToLeft = direction == Direction::Auto ? firstRightToLeft.value_or(false) : direction == Direction::RightToLeft};
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

TextLayout LayoutBuilder::build() {
    if (options.family) {
        layout.families.push_back(options.family);
    }

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
            emitTable(block, {origin.x, y}, width);
            break;
        }
        y += block.height;
    }
    return y - origin.y - (blocks.empty() ? 0.0F : blocks.back().trailing);
}

std::size_t LayoutBuilder::addCharacter(const Piece& piece, std::size_t offset, bool rightToLeft) {
    layout.characters.push_back({.begin = offset + piece.begin, .end = offset + piece.end, .style = piece.style, .rightToLeft = rightToLeft, .pause = std::exchange(pendingPause, 0.0F), .speed = document.styles[piece.style].revealSpeed});
    return layout.characters.size() - 1;
}

// Characters count in reading order while every line draws its pieces from left to right. The indent, the drop cap and the list marker stand on the side the paragraph starts, the left of left-to-right text and the right of right-to-left text.
void LayoutBuilder::emitParagraph(const Block& block, math::Vec2 origin, float width) {
    const RichTextDocument::Paragraph& paragraph = *block.paragraph;
    const Flow& flow = block.flow;
    const bool rightToLeft = flow.rightToLeft;
    const float indent = paragraph.indent * getIndentUnit();
    const TextAlign align = paragraph.align.value_or(options.align);
    const float room = std::max(width - indent, 0.0F);
    const float left = rightToLeft ? origin.x : origin.x + indent;
    const std::size_t textStart = textOffset + flow.dropCapText.size();

    // The drop cap reads first and stands at the top of the paragraph.
    std::vector<std::size_t> dropCharacters(flow.dropCap.size());
    float dropWidth = 0.0F;
    for (std::size_t index = 0; index < flow.dropCap.size(); ++index) {
        dropCharacters[index] = addCharacter(flow.dropCap[index], textOffset, rightToLeft);
        dropWidth += flow.dropCap[index].advance;
    }
    float dropX = rightToLeft ? left + room - dropWidth : left;
    for (const std::size_t index : flow.dropCapOrder) {
        const Piece& piece = flow.dropCap[index];
        emitPiece(flow, flow.dropCapText, piece, {dropX, origin.y + flow.dropCapAscent}, origin.y, flow.dropCapHeight, dropCharacters[index]);
        layout.characters[dropCharacters[index]].box = {dropX, origin.y, piece.advance, flow.dropCapHeight};
        dropX += piece.advance;
    }

    std::size_t lineStart = 0;
    for (std::size_t lineIndex = 0; lineIndex < flow.lines.size(); ++lineIndex) {
        const Line& line = flow.lines[lineIndex];
        const bool lastLine = lineIndex + 1 == flow.lines.size() || !line.wrapped;
        const TextAlign resolved = resolveAlign(align, rightToLeft, lastLine);
        const float lineRoom = room - line.shift;
        const float lineTop = origin.y + line.top;
        const float baseline = lineTop + line.ascent;
        const float lineHeight = line.ascent + line.descent;

        // Characters take their numbers in reading order before the line places them.
        std::vector<std::size_t> characters(line.end - line.begin, 0);
        const std::size_t firstCharacter = layout.characters.size();
        for (std::size_t index = line.begin; index < line.end; ++index) {
            const Piece& piece = flow.pieces[index];
            if (piece.kind == Piece::Kind::Pause) {
                pendingPause += piece.pause;
            } else if (piece.kind == Piece::Kind::Cluster || piece.kind == Piece::Kind::Image) {
                characters[index - line.begin] = addCharacter(piece, textStart, piece.level % 2 != 0);
            }
        }

        float spacing = 0.0F;
        if (resolved == TextAlign::Fill) {
            const auto spaces = std::count_if(flow.pieces.begin() + static_cast<std::ptrdiff_t>(line.begin), flow.pieces.begin() + static_cast<std::ptrdiff_t>(line.visibleEnd), [](const Piece& piece) { return piece.space; });
            spacing = spaces > 0 ? (lineRoom - line.width) / static_cast<float>(spaces) : 0.0F;
        }

        // The spaces a wrapped line ends with stand past its end, which is the left of a right-to-left line, so the visible text keeps its alignment.
        float trailing = 0.0F;
        for (std::size_t index = line.visibleEnd; index < line.end; ++index) {
            trailing += flow.pieces[index].kind == Piece::Kind::Cluster ? flow.pieces[index].advance : 0.0F;
        }
        const float contentLeft = left + (rightToLeft ? 0.0F : line.shift) + alignOffset(resolved, lineRoom, line.width);
        float x = rightToLeft ? contentLeft - trailing : contentLeft;

        // A list marker stands a small gap before the start of the first line, and shows with the first character of its item.
        if (lineIndex == 0 && !flow.marker.empty()) {
            float markerWidth = 0.0F;
            for (const Piece& piece : flow.marker) {
                markerWidth += piece.advance;
            }
            const float gap = options.size * options.scale * kMarkerGapEms;
            float markerX = rightToLeft ? left + room + gap : left - markerWidth - gap;
            for (const std::size_t index : flow.markerOrder) {
                emitPiece(flow, paragraph.marker, flow.marker[index], {markerX, baseline}, lineTop, lineHeight, firstCharacter);
                markerX += flow.marker[index].advance;
            }
        }

        std::vector<Placed> placed;
        placed.reserve(line.order.size());
        for (const std::size_t index : line.order) {
            const Piece& piece = flow.pieces[index];
            const std::size_t character = characters[index - line.begin];
            emitPiece(flow, flow.text, piece, {x, baseline}, lineTop, lineHeight, character);
            const float advance = piece.advance + (piece.space && index < line.visibleEnd ? spacing : 0.0F);
            layout.characters[character].box = {x, lineTop, advance, lineHeight};
            placed.push_back({.piece = index, .x = x, .character = character});
            x += advance;
        }

        // The line holds the code points from where the one before it ended to the end of its last piece.
        std::size_t lineEnd = lineStart;
        for (std::size_t index = line.begin; index < line.end; ++index) {
            lineEnd = std::max(lineEnd, flow.pieces[index].end);
        }
        layout.lines.push_back({.box = {contentLeft, lineTop, resolved == TextAlign::Fill ? lineRoom : line.width, lineHeight}, .baseline = baseline, .firstCharacter = firstCharacter, .endCharacter = layout.characters.size(), .begin = textStart + lineStart, .end = textStart + lineEnd, .rightToLeft = rightToLeft});
        lineStart = lineEnd;
        emitDecorations(flow, line, placed, baseline);
    }
    textOffset = textStart + flow.text.size() + 1;
}

void LayoutBuilder::emitPiece(const Flow& flow, std::u32string_view text, const Piece& piece, math::Vec2 pen, float lineTop, float lineHeight, std::size_t character) {
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

    const TextLayout::Look& look = layout.looks[piece.look];
    for (std::size_t index = piece.firstGlyph; index < piece.firstGlyph + piece.glyphCount; ++index) {
        const PlacedGlyph& placed = flow.glyphs[index];
        if (!placed.glyph.visible) {
            continue;
        }
        layout.glyphs.push_back({
            .look = piece.look,
            .style = piece.style,
            .character = character,
            .index = placed.index,
            .codePoint = text[piece.begin],
            .position = {pen.x + look.weight + placed.offset.x + placed.glyph.offset.x * placed.factor, pen.y + placed.offset.y + placed.glyph.offset.y * placed.factor},
            .size = placed.glyph.source.getSize() * placed.factor,
            .baseline = pen.y + placed.offset.y,
            .source = placed.glyph.source,
            .page = placed.glyph.page,
            .color = getColor(piece.style),
        });
    }
}

// Backgrounds, underlines, strikes, links and hints each merge the neighbouring pieces of a line that share them into one rectangle, up to the last visible piece. Backgrounds and lines also split where the direction changes, so each grows with the reveal from the side its text starts.
void LayoutBuilder::emitDecorations(const Flow& flow, const Line& line, const std::vector<Placed>& placed, float baseline) {
    const float lineTop = baseline - line.ascent;
    const float lineHeight = line.ascent + line.descent;

    // clang-format off
    const auto runs = [&](const auto& keyOf, bool byDirection, const auto& emit) {
        std::size_t begin = 0;
        while (begin < placed.size()) {
            const Piece& first = flow.pieces[placed[begin].piece];
            const auto key = keyOf(first);
            std::size_t end = begin + 1;
            std::size_t lowest = placed[begin].character;
            std::size_t highest = placed[begin].character;
            while (end < placed.size() && placed[end].piece < line.visibleEnd && keyOf(flow.pieces[placed[end].piece]) == key && (!byDirection || flow.pieces[placed[end].piece].level == first.level)) {
                lowest = std::min(lowest, placed[end].character);
                highest = std::max(highest, placed[end].character);
                ++end;
            }
            if (key && placed[begin].piece < line.visibleEnd) {
                const float x = placed[begin].x;
                const float right = placed[end - 1].x + flow.pieces[placed[end - 1].piece].advance;
                emit(*key, math::Rect{x, lineTop, right - x, lineHeight}, lowest, highest, first.level % 2 != 0);
            }
            begin = end;
        }
    };
    // clang-format on

    const auto styleOf = [this](const Piece& piece) -> const RichTextDocument::Style& { return document.styles[piece.style]; };
    runs([&](const Piece& piece) { return styleOf(piece).background; }, true, [&](math::Color color, const math::Rect& rect, std::size_t first, std::size_t last, bool rightToLeft) { addBox(TextLayout::Box::Kind::Background, rect, color, first, last, rightToLeft); });

    // clang-format off
    const auto decorate = [&](bool underline) {
        runs([&](const Piece& piece) -> std::optional<std::size_t> {
            const RichTextDocument::Style& style = styleOf(piece);
            const bool shown = underline ? style.underline || (style.link && options.underlineLinks) : style.strike;
            return shown ? std::optional<std::size_t>(piece.style) : std::nullopt;
        }, true, [&](std::size_t style, const math::Rect& rect, std::size_t first, std::size_t last, bool rightToLeft) {
            const float size = getStyleFont(style).size;
            const float thickness = std::max(1.0F, size * kDecorationThickness);
            const float y = underline ? baseline + size * kUnderlineOffset : baseline - size * kStrikeOffset;
            addBox(underline ? TextLayout::Box::Kind::Underline : TextLayout::Box::Kind::Strike, {rect.x, y - thickness * 0.5F, rect.width, thickness}, getColor(style), first, last, rightToLeft);
        });
    };
    // clang-format on
    decorate(true);
    decorate(false);

    runs([&](const Piece& piece) { return styleOf(piece).link; }, false, [&](std::size_t link, const math::Rect& rect, std::size_t, std::size_t, bool) { layout.links.push_back({.rect = rect, .index = link}); });
    runs([&](const Piece& piece) { return styleOf(piece).hint; }, false, [&](std::size_t hint, const math::Rect& rect, std::size_t, std::size_t, bool) { layout.hints.push_back({.rect = rect, .index = hint}); });
}

void LayoutBuilder::addBox(TextLayout::Box::Kind kind, const math::Rect& rect, math::Color color, std::size_t firstCharacter, std::size_t lastCharacter, bool rightToLeft) {
    layout.boxes.push_back({.kind = kind, .rect = rect, .color = color, .firstCharacter = firstCharacter, .lastCharacter = lastCharacter, .rightToLeft = rightToLeft});
}

// A rule shows once the reveal reaches it, centered unless its paragraph aligns it, and its indent stands on the side its direction starts.
void LayoutBuilder::emitRule(const Block& block, math::Vec2 origin, float width) {
    const RichTextDocument::Paragraph& paragraph = *block.paragraph;
    const bool rightToLeft = getDirection(paragraph) == Direction::RightToLeft;
    const float indent = paragraph.indent * getIndentUnit();
    const float room = std::max(width - indent, 0.0F);
    const float length = room * paragraph.ruleWidth;
    const float left = rightToLeft ? origin.x : origin.x + indent;
    const float x = left + alignOffset(resolveAlign(paragraph.align.value_or(TextAlign::Center), rightToLeft, false), room, length);
    const float margin = options.size * options.scale * kRuleMarginEms;
    const std::size_t next = layout.characters.size();
    addBox(TextLayout::Box::Kind::Rule, {x, origin.y + margin, length, paragraph.ruleThickness * options.scale}, paragraph.ruleColor.value_or(options.color), next, next);
}

void LayoutBuilder::emitTable(const Block& block, math::Vec2 origin, float width) {
    const RichTextDocument::Paragraph& paragraph = *block.paragraph;
    const RichTextDocument::Table& table = document.tables[paragraph.table];
    const TableBlock& laid = *block.table;
    const float indent = paragraph.indent * getIndentUnit();
    const float tableWidth = std::accumulate(laid.columns.begin(), laid.columns.end(), 0.0F);
    const float left = laid.rightToLeft ? origin.x + std::max(width - indent, tableWidth) - tableWidth : origin.x + indent;
    const float border = std::max(1.0F, options.scale);

    for (std::size_t index = 0; index < table.cells.size(); ++index) {
        const RichTextDocument::Cell& cell = table.cells[index];
        const std::size_t column = index % table.columns;
        const std::size_t row = index / table.columns;
        const float before = std::accumulate(laid.columns.begin(), laid.columns.begin() + static_cast<std::ptrdiff_t>(column), 0.0F);
        const float x = laid.rightToLeft ? left + tableWidth - before - laid.columns[column] : left + before;
        const float y = origin.y + std::accumulate(laid.rows.begin(), laid.rows.begin() + static_cast<std::ptrdiff_t>(row), 0.0F);
        const math::Rect area{x, y, laid.columns[column], laid.rows[row]};
        const std::size_t next = layout.characters.size();

        if (cell.background) {
            addBox(TextLayout::Box::Kind::CellBackground, area, *cell.background, next, next);
        }
        if (cell.border) {
            for (const math::Rect& edge : {math::Rect{area.x, area.y, area.width, border}, math::Rect{area.x, area.getBottom() - border, area.width, border}, math::Rect{area.x, area.y, border, area.height}, math::Rect{area.getRight() - border, area.y, border, area.height}}) {
                addBox(TextLayout::Box::Kind::CellBorder, edge, *cell.border, next, next);
            }
        }
        const float padding = cell.padding * options.scale;
        emitBlocks(laid.cells[index], {area.x + padding, area.y + padding}, std::max(area.width - padding * 2.0F, 0.0F));
    }
}

} // namespace haylen::text
