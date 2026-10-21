#pragma once

#include <optional>
#include <string_view>

#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// The part of a GUI where the game shows, as a place the focus can go. While it has the focus, the directions, accept and menu reach the action map instead of the UI, and it never takes the pointer, so clicks and touches on it reach the game too. A play area with a radius rounds its focus ring and covers the corners of the game outside its curve with the color around it, since the game draws below every GUI.
class PlayArea final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "playArea";
    }
    [[nodiscard]] bool describe(Context& context, platform::AccessibilityElement& element) const override;

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override {
        return {};
    }
    void render(Context& context, const math::Rect& bounds) override;

  private:
    void drawCorners(Context& context, const math::Rect& bounds) const;

    float radius = 0.0F;
    std::optional<math::Color> cornerColor;
};

} // namespace haylen::ui
