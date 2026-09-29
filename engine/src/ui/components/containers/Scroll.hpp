#pragma once

#include <cstddef>
#include <string_view>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"

namespace haylen::ui {

// Shows one child in an area that scrolls up and down or sideways. The wheel, the scrollbar and a finger dragging the content scroll it, the focus scrolls to the focused control, and with snapping it settles on the start of the nearest item of its child.
class Scroll final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "scroll";
    }
    [[nodiscard]] std::size_t getChildLimit() const noexcept override {
        return 1;
    }

  protected:
    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float availableWidth) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    static constexpr float kSnapDelay = 0.12F;
    static constexpr float kSnapSpeed = 14.0F;

    // A finger that drags the content takes over from the control it pressed, so a list of buttons still scrolls on a touch screen.
    void followFinger();
    void settle(Context& context, float position, float limit);

    bool scrollbar = true;
    bool horizontal = false;
    bool snap = false;
    float idle = 0.0F;
    std::vector<float> points;
};

} // namespace haylen::ui
