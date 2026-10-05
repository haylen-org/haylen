#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <optional>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"
#include "haylen/ui/Component.hpp"
#include "ui/components/touch/PointerTracker.hpp"

namespace haylen::ui {

// A virtual analog stick that sets a stick of the action layer. A fixed stick stays at the center of its area, a floating stick centers wherever the finger lands inside it, and a following stick also moves after a finger that leaves its ring.
class TouchStick final : public Component {
  public:
    enum class Mode : std::uint8_t {
        Fixed,
        Floating,
        Following,
    };

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
    static constexpr std::array<std::pair<std::string_view, Mode>, 3> kModes{{
        {"fixed", Mode::Fixed},
        {"floating", Mode::Floating},
        {"following", Mode::Following},
    }};

    [[nodiscard]] math::Vec2 placeCenter(const math::Rect& bounds, math::Vec2 pointer);

    std::string action;
    float radius = 110.0F;
    float deadZone = 0.15F;
    Mode mode = Mode::Fixed;
    bool touchOnly = false;
    PointerTracker tracker;

    // The center of the ring while a finger holds the stick.
    std::optional<math::Vec2> held;
};

} // namespace haylen::ui
