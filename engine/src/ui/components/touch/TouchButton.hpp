#pragma once

#include <string>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/components/touch/PointerTracker.hpp"

namespace haylen::ui {

// A virtual button of the action layer, held while a finger or the mouse presses it. It also reports press and release events.
class TouchButton final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "touchButton";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect& bounds) override;
    void drawingStopped(Context& context) override;
    [[nodiscard]] bool reportsPresses() const noexcept override {
        return true;
    }

  private:
    std::string action;
    TextValue text;
    std::string image;
    float size = 120.0F;
    bool touchOnly = false;
    bool down = false;
    PointerTracker tracker;
};

} // namespace haylen::ui
