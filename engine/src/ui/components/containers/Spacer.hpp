#pragma once

#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

class Spacer final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "spacer";
    }

  protected:
    void readProperties(PropertyReader&) override {}
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context&, const math::Rect&) override {}
};

} // namespace haylen::ui
