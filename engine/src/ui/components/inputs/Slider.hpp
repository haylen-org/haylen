#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

class Slider final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "slider";
    }
    [[nodiscard]] bool usesFocusDirection(FocusDirection direction) const noexcept override {
        return direction == FocusDirection::Left || direction == FocusDirection::Right;
    }

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
    void collectPlayerValues(core::Json& values) const override {
        values["value"] = value;
    }

  private:
    // Left and right move a slider without a step by a twentieth of its range.
    static constexpr double kFocusSteps = 20.0;

    double value = 0.0;
    double minimum = 0.0;
    double maximum = 1.0;
    double step = 0.0;
    bool showValue = false;
    int decimals = 2;
};

} // namespace haylen::ui
