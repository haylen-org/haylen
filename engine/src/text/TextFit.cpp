#include "text/TextFit.hpp"

#include <utility>

namespace haylen::text {

// Text that fits at its size keeps it, text that does not fit even at the smallest size takes that size, and otherwise the range between the two halves until the largest size that fits is known closely.
Layout TextFit::layout(const Style& style, const Builder& build) {
    Style laid = style;
    if (!style.wrap) {
        laid.maxWidth = 0.0F;
    }
    Layout full = build(laid);
    if (style.minScale >= 1.0F || fits(full, style)) {
        return full;
    }

    float low = style.size * style.minScale;
    float high = style.size;
    laid.size = low;
    Layout best = build(laid);
    if (!fits(best, style)) {
        return best;
    }
    for (int step = 0; step < kSteps; ++step) {
        laid.size = (low + high) * 0.5F;
        Layout tried = build(laid);
        if (fits(tried, style)) {
            low = laid.size;
            best = std::move(tried);
        } else {
            high = laid.size;
        }
    }
    return best;
}

bool TextFit::fits(const Layout& layout, const Style& style) noexcept {
    if (style.maxHeight > 0.0F && layout.size.y > style.maxHeight) {
        return false;
    }
    return style.wrap || style.maxWidth <= 0.0F || layout.size.x <= style.maxWidth;
}

} // namespace haylen::text
