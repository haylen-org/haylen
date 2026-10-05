#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/core/JsonNumber.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Two children beside each other, or stacked when vertical, with a handle between them that the player can drag. The handle takes the focus, and the arrows along the split move it.
class Splitter final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "splitter";
    }
    [[nodiscard]] bool usesFocusDirection(FocusDirection direction) const noexcept override {
        return vertical ? direction == FocusDirection::Up || direction == FocusDirection::Down : direction == FocusDirection::Left || direction == FocusDirection::Right;
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 2;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
    void collectPlayerValues(core::Json& values) const override {
        values["ratio"] = core::JsonNumber::fromFloat(ratio);
    }

  private:
    static constexpr float kFocusStep = 0.05F;

    float ratio = 0.5F;
    bool vertical = false;
    bool dragging = false;
};

} // namespace haylen::ui
