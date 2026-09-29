#pragma once

#include <optional>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class BusyIndicator final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "busyIndicator";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Center;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    float size = 48.0F;
    std::optional<Theme::Color> color;
};

} // namespace haylen::ui
