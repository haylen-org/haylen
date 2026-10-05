#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

class Alert final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "alert";
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    Widgets::Tone tone = Widgets::Tone::Information;
    TextValue title;
    TextValue message;
};

} // namespace haylen::ui
