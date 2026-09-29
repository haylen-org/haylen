#pragma once

#include "haylen/math/Rect.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

class Context;
struct ChoiceItem;

// The row every list-like kind draws: its background for the state and the picture, text and caption of an item.
class ListRow final {
  public:
    static constexpr float kPadding = 16.0F;

    // Paints the row background for its state and reports a press.
    static Widgets::Interaction draw(Context& context, const math::Rect& bounds, bool selected);

    // Draws the picture, text and caption of an item inside a row, unless the row is out of view.
    static void drawContent(Context& context, const math::Rect& bounds, const ChoiceItem& item);

    // Returns the width a row needs to show the picture, text and caption of an item whole.
    [[nodiscard]] static float measure(Context& context, const ChoiceItem& item);
};

} // namespace haylen::ui
