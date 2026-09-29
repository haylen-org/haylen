#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "ui/components/buttons/ButtonBase.hpp"

namespace haylen::ui {

class Button final : public ButtonBase {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "button";
    }

  protected:
    void readMore(PropertyReader& reader) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    bool checked = false;
};

} // namespace haylen::ui
