#include "haylen/text/Font.hpp"

#include "text/LayoutBuilder.hpp"
#include "text/LayoutCache.hpp"

namespace haylen::text {

debug::ObjectCounter& Font::counter = *new debug::ObjectCounter("Font", debug::ObjectCounter::Kind::Native);

Font::Font(Metrics fontMetrics) : metrics(fontMetrics), layouts(std::make_unique<LayoutCache>()) {}

Font::~Font() = default;

float Font::getLineHeight(float size) const noexcept {
    return metrics.lineHeight * size / metrics.nativeSize;
}

float Font::getAscent(float size) const noexcept {
    return metrics.ascent * size / metrics.nativeSize;
}

float Font::toDistance(float pixels, float size) const noexcept {
    if (metrics.spread <= 0.0F) {
        return 0.0F;
    }
    // The field falls from 1 to 0 across twice the spread at the native size, with the edge at 0.5.
    return pixels * metrics.nativeSize / size / (metrics.spread * 2.0F);
}

std::shared_ptr<const Layout> Font::layout(std::string_view text, const Style& style) {
    return layouts->get(text, style, [&] { return LayoutBuilder::layoutPlainText(text, style, nullptr, this); });
}

math::Vec2 Font::measure(std::string_view text, const Style& style) {
    return layout(text, style)->size * style.scale;
}

} // namespace haylen::text
