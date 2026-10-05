#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Keeps its child inside the safe area, for GUIs that cover the whole screen but hold controls the notch must not hide.
class SafeArea final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "safeArea";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 1;
    }

  protected:
    void readProperties(PropertyReader&) override {}
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;
};

} // namespace haylen::ui
