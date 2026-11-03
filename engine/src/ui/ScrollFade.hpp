#pragma once

#include "haylen/math/Rect.hpp"

namespace haylen::ui {

class Context;

// The fade of an area that scrolls: along each edge where content continues past the area, the content fades out to transparent over the theme metric `scrollFade`, so whatever lies below it shows through and the player sees that the area has more. A fade grows in as the content moves away from its end and is gone once the content rests at that end.
class ScrollFade final {
  public:
    // Fades what the window being built draws until `end` over the box of the content, where `before` is how far the content continues past its top or left edge and `after` past its bottom or right edge. Returns whether it faded anything, which only then needs `end`, so content that rests at both ends costs nothing.
    [[nodiscard]] static bool begin(Context& context, const math::Rect& box, bool horizontal, double before, double after);
    static void end(Context& context);
};

} // namespace haylen::ui
