#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// Rows of items with an optional picture and caption, of which only the rows in view draw. A draggable list lets the pointer drag its rows to other lists and slot grids and the keyboard, gamepads and remotes choose and carry them.
class List final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "list";
    }
    [[nodiscard]] bool describe(Context& context, platform::AccessibilityElement& element) const override;

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
    void collectPlayerValues(core::Json& values) const override {
        values["selected"] = selected;
    }
    void drawingStopped(Context& context) override;

  private:
    [[nodiscard]] std::pair<std::size_t, std::size_t> findRowRange(const Context& context, const math::Rect& bounds, float height) const;
    void drawRow(Context& context, const math::Rect& bounds, float height, std::size_t index, std::string_view focused);

    std::vector<ChoiceItem> items;
    std::string selected;
    bool draggable = false;
    std::string dragging;

    // The row that had the focus in the last frame, which draws even outside the view so the focus stays on it.
    std::optional<std::size_t> remembered;
};

} // namespace haylen::ui
