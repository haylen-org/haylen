#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

class Context;
class Popup;

// The themed list that combos, menu buttons and context menus open in a popup, with one row per item for the pointer, the keyboard, gamepads and remotes.
class PopupList final {
  public:
    // Draws the popup next to its anchor when it is open, and returns the item picked this frame, which also closes it. The current item starts with the focus.
    [[nodiscard]] static std::optional<std::string> draw(Context& context, Popup& popup, std::string_view name, const std::vector<ChoiceItem>& items, std::string_view current, const math::Rect& anchor, float minimumWidth);

  private:
    [[nodiscard]] static float measureWidth(Context& context, const std::vector<ChoiceItem>& items);
};

} // namespace haylen::ui
