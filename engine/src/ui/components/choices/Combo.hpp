#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

// A field showing the selected item that opens a list of the others.
class Combo final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "combo";
    }

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;
    void collectPlayerValues(core::Json& values) const override {
        values["selected"] = selected;
    }

  private:
    std::vector<ChoiceItem> items;
    std::string selected;
    TextValue placeholder;
};

} // namespace haylen::ui
