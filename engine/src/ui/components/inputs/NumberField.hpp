#pragma once

#include <string>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/TextInput.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// A number between a minimum and a maximum, changed with the minus and plus buttons or typed into the middle.
class NumberField final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "numberField";
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
        values["value"] = value;
    }

  private:
    // The arm of a sign as a share of the height of its button, and its weight as a multiple of the stroke width of the theme.
    static constexpr float kSignArm = 0.16F;
    static constexpr float kSignWeight = 1.5F;

    void drawSign(Context& context, const math::Rect& button, bool plus);

    double value = 0.0;
    double minimum = 0.0;
    double maximum = 100.0;
    double step = 1.0;
    int decimals = 0;
    platform::TextInput::ReturnKey returnKey = platform::TextInput::ReturnKey::Default;
    std::string text = "0";
};

} // namespace haylen::ui
