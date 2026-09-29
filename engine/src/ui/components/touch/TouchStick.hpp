#pragma once

#include <string>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/touch/PointerTracker.hpp"

namespace haylen::ui {

// A virtual analog stick that sets a stick of the action layer. A floating stick centers wherever the finger lands inside its area.
class TouchStick final : public Component {
  public:
    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "touchStick";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context&, float) override;
    void render(Context& context, const math::Rect& bounds) override;
    void drawingStopped(Context&) override;

  private:
    std::string action;
    float radius = 110.0F;
    float deadZone = 0.15F;
    bool floating = false;
    bool touchOnly = false;
    PointerTracker tracker;
};

} // namespace haylen::ui
