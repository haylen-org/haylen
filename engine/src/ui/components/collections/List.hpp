#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// Rows of items with an optional picture and caption. A draggable list lets the pointer drag its rows to other lists and slot grids and the keyboard, gamepads and remotes carry them.
class List final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "list";
    }

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
    std::vector<ChoiceItem> items;
    std::string selected;
    bool draggable = false;
    std::string dragging;
};

} // namespace haylen::ui
