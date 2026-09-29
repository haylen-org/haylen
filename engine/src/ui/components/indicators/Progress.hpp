#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

// A bar filled up to a value between 0 and 1. Textured themes draw it with their track images, which suits health and loading bars.
class Progress final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "progress";
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    float value = 0.0F;
    Widgets::Tone tone = Widgets::Tone::Accent;
    TextValue text;
};

} // namespace haylen::ui
