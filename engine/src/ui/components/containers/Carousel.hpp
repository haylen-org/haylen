#pragma once

#include <cstddef>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Pages shown one at a time that slide sideways, one child per page, such as a tutorial or a level picker. A swipe or drag, the arrows and the page dots change the page, and so do left and right on the dots while they have the focus.
class Carousel final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "carousel";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return kUnlimitedChildren;
    }
    [[nodiscard]] bool usesFocusDirection(FocusDirection direction) const noexcept override {
        return direction == FocusDirection::Left || direction == FocusDirection::Right;
    }

  protected:
    [[nodiscard]] bool isFocusable() const noexcept override {
        return true;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    // How much of the way to the page a slide covers per transition duration of the theme.
    static constexpr float kSlideRate = 1.5F;
    static constexpr float kSwipeShare = 0.2F;

    [[nodiscard]] float getIndicatorHeight(Context& context) const;
    void drawPages(Context& context, const math::Rect& area);
    [[nodiscard]] int drawArrows(Context& context, const math::Rect& area, std::size_t count);
    [[nodiscard]] int drawIndicators(Context& context, const math::Rect& row, std::size_t count);
    void turn(Context& context, int target, std::size_t count);

    int page = 1;
    bool loop = false;
    bool indicators = true;
    bool arrows = true;
    float interval = 0.0F;
    float shown = 0.0F;
    float waited = 0.0F;
    float dragged = 0.0F;
};

} // namespace haylen::ui
