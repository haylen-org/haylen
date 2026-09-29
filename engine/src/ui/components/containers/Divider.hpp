#pragma once

#include <optional>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Theme.hpp"

namespace haylen::ui {

class Divider final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "divider";
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    bool vertical = false;
    std::optional<Theme::Color> color;
};

} // namespace haylen::ui
