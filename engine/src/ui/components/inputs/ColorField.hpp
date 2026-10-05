#pragma once

#include <string_view>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// A swatch with the color in text that opens a color picker.
class ColorField final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "colorField";
    }

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
    void collectPlayerValues(core::Json& values) const override {
        values["value"] = value.toHex();
    }

  private:
    math::Color value = math::Color::white();
    bool alpha = true;
};

} // namespace haylen::ui
