#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

class Checkbox final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "checkbox";
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
    void collectPlayerValues(core::Json& values) const override {
        values["checked"] = checked;
    }

  private:
    TextValue text;
    bool checked = false;
};

} // namespace haylen::ui
