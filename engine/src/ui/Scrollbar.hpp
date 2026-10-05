#pragma once

#include <optional>

#include "haylen/math/Rect.hpp"

namespace haylen::ui {

class Context;

// A themed scrollbar along the end edge of an area that scrolls itself: its thumb shows which part of the content is in view, a drag of the thumb moves the content and a press on the track moves it by one view.
class Scrollbar final {
  public:
    // Takes the pointer before the content of the area does, so the bar wins over the items under it, and returns the offset the player asks for. A reversed bar starts at its far end, as a horizontal bar of a right-to-left UI does.
    [[nodiscard]] std::optional<double> interact(const math::Rect& area, bool horizontal, bool reversed, double offset, double maximum, float viewLength);

    // Draws the thumb over the content, faded by `alpha`.
    void draw(Context& context, float alpha) const;

  private:
    static constexpr float kMinThumb = 32.0F;
    static constexpr float kInset = 3.0F;

    math::Rect thumb;
    bool horizontalBar = false;
    bool hovered = false;
    bool held = false;
    bool grabbing = false;
    float grab = 0.0F;
};

} // namespace haylen::ui
