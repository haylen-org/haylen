#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/ChoiceItem.hpp"

namespace haylen::ui {

class RadioGroup final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "radioGroup";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    [[nodiscard]] std::string getFocusTarget() const;

    std::vector<ChoiceItem> items;
    std::string selected;
    bool horizontal = false;
};

} // namespace haylen::ui
