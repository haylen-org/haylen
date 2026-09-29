#pragma once

#include <string>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"

namespace haylen::ui {

// A friendly placeholder for a view with nothing to show yet, with an optional picture, title and message.
class EmptyState final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "emptyState";
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    std::string image;
    TextValue title;
    TextValue message;
    float imageSize = 128.0F;
};

} // namespace haylen::ui
