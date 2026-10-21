#pragma once

#include "haylen/math/Rect.hpp"

namespace haylen::ui {

class Context;

// The fade of an area that scrolls: along each edge where content continues past the area, the content fades into the theme color `scrollFade` over the theme metric `scrollFade`, so the player sees that the area has more. A fade grows in as the content moves away from its end and is gone once the content rests at that end.
class ScrollFade final {
  public:
    // Draws the fades over the content box, where `before` is how far the content continues past its top or left edge and `after` past its bottom or right edge.
    static void draw(Context& context, const math::Rect& box, bool horizontal, double before, double after);
};

} // namespace haylen::ui
