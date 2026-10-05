#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// A track with two knobs that pick a range, such as a price filter. The pointer drags the nearer knob. With the focus, left and right move the active knob and accept switches to the other one.
class RangeSlider final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "rangeSlider";
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
        values["low"] = low;
        values["high"] = high;
    }

  private:
    // Left and right move a range without a step by a twentieth of its span.
    static constexpr double kFocusSteps = 20.0;

    [[nodiscard]] float measureLabel(Context& context) const;
    [[nodiscard]] float toPosition(const Context& context, double amount, const math::Rect& track) const noexcept;
    [[nodiscard]] bool follow(Context& context, const math::Rect& bounds, const math::Rect& track);
    void drawKnob(Context& context, const math::Rect& bounds, float x, bool active, bool pressed) const;

    double low = 0.0;
    double high = 1.0;
    double minimum = 0.0;
    double maximum = 1.0;
    double step = 0.0;
    bool showValue = false;
    int decimals = 2;
    bool highActive = false;
};

} // namespace haylen::ui
