#pragma once

#include <optional>

#include "haylen/math/Rect.hpp"

namespace haylen::ui {

class Context;

// A themed scroll bar in a lane of its own along the end edge of an area that scrolls, the right edge or the left one of a right-to-left UI for a vertical area and the bottom edge for a horizontal one. The lane holds the gap that keeps the bar away from the content, the bar and the inset that keeps it away from the edges and the rounded corners of the area, so the bar never covers or touches the content. Its thumb shows which part of the content is in view, a drag of the thumb moves the content and a press on the track moves it by one view.
class Scrollbar final {
  public:
    // The fewest points of the screen between the content and the bar, whatever scale shrinks the UI.
    static constexpr float kMinimumGap = 4.0F;

    // Returns the gap between the content and the bar in UI units: the theme metric, or the units that span the fewest points when the metric spans fewer on this screen.
    [[nodiscard]] static float getGap(float metric, float pointsPerUnit) noexcept;
    [[nodiscard]] static float getGap(const Context& context);

    // Returns the room the bar takes across its axis: the gap, the bar and the inset.
    [[nodiscard]] static float getLane(const Context& context);

    // Returns the room content gives up while the bar shows when it ends `padding` away from the edge of the area, which is the part of the lane its padding leaves.
    [[nodiscard]] static float getReserve(const Context& context, float padding);

    // Returns the part of a box inside the area of a bar that lies outside the lane of the bar, which is where content goes while the bar shows.
    [[nodiscard]] static math::Rect getContentBox(const Context& context, const math::Rect& area, const math::Rect& box, bool horizontal);

    // Places the bar in the lane of the area and takes the pointer there, and returns the offset the player asks for. A reversed bar starts at its far end, as a horizontal bar of a right-to-left UI does, and the ends of the track keep clear of corners of the area rounded by `radius`.
    [[nodiscard]] std::optional<double> interact(Context& context, const math::Rect& area, bool horizontal, bool reversed, double offset, double maximum, float viewLength, float radius = 0.0F);

    // Draws the track, when the theme paints it with an image, and the thumb, faded by `alpha`.
    void draw(Context& context, float alpha) const;

  private:
    static constexpr float kMinThumb = 32.0F;

    math::Rect track;
    math::Rect thumb;
    bool hovered = false;
    bool held = false;
    bool grabbing = false;
    float grab = 0.0F;
};

} // namespace haylen::ui
