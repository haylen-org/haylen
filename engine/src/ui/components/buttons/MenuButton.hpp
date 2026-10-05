#pragma once

#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "ui/Popup.hpp"
#include "ui/components/ChoiceItem.hpp"
#include "ui/components/buttons/ButtonBase.hpp"

namespace haylen::ui {

class MenuButton final : public ButtonBase {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "menuButton";
    }

  protected:
    void readMore(PropertyReader& reader) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    std::vector<ChoiceItem> items;
    Popup popup;
};

} // namespace haylen::ui
