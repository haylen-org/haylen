#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "ui/components/buttons/ButtonBase.hpp"

namespace haylen::ui {

// A button that opens a floating panel holding its child.
class Popover final : public ButtonBase {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "popover";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 1;
    }

  protected:
    void readMore(PropertyReader& reader) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    float contentWidth = 480.0F;
};

} // namespace haylen::ui
