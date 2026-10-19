#pragma once

#include <functional>

#include "haylen/text/Layout.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::text {

// Lays plain text out in its style, at the largest size that fits the block in its box when the minimum scale of the style is below 1, as `Style` describes.
class TextFit final {
  public:
    using Builder = std::function<Layout(const Style& style)>;

    [[nodiscard]] static Layout layout(const Style& style, const Builder& build);

  private:
    // The halvings of the range between the smallest size and the size, which settle the size that fits within a fraction of a unit.
    static constexpr int kSteps = 8;

    [[nodiscard]] static bool fits(const Layout& layout, const Style& style) noexcept;
};

} // namespace haylen::text
