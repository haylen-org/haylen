#pragma once

#include <optional>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Context;

// The `iconColor` of a button or an item: a theme color that tints its picture, `false`, which keeps the colors of the picture, or nothing, which leaves the tint to what draws it.
class IconColor final {
  public:
    // Reads the value of the property at a path such as `button.iconColor` or `list.items.iconColor`, which starts its error.
    [[nodiscard]] static IconColor fromJson(const core::Json& value, std::string_view path);

    // Returns the tint of the picture against the theme around it, or the fallback when nothing was set.
    [[nodiscard]] math::Color resolve(const Context& context, math::Color fallback) const noexcept;

  private:
    std::optional<Theme::Color> role;
    bool ownColors = false;
};

} // namespace haylen::ui
