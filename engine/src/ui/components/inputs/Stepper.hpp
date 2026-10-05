#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// A value between two arrows that step it, a number or one of a list of options, the way console settings pick a difficulty. Left and right step it while it has the focus, and accept or a tap on the value moves to the next one.
class Stepper final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "stepper";
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
        if (items.empty()) {
            values["value"] = value;
        } else {
            values["selected"] = selected;
        }
    }

  private:
    [[nodiscard]] std::string getShownText(Context& context) const;
    [[nodiscard]] bool canStep(int direction) const;
    void step(Context& context, int direction);

    double value = 0.0;
    double minimum = 0.0;
    double maximum = 10.0;
    double increment = 1.0;
    int decimals = 0;
    bool wrap = false;
    std::vector<ChoiceItem> items;
    std::string selected;
};

} // namespace haylen::ui
