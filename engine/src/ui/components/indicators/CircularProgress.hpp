#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/TextValue.hpp"
#include "ui/Widgets.hpp"

namespace haylen::ui {

// A value between 0 and 1 drawn around a circle: a ring that fills clockwise, or a shade over a picture that shrinks as the value falls, like the cooldown of an ability.
class CircularProgress final : public Component {
  public:
    enum class Variant : std::uint8_t {
        Ring,
        Cooldown,
    };

    [[nodiscard]] std::string_view getKind() const noexcept override {
        return "circularProgress";
    }

  protected:
    [[nodiscard]] Alignment getDefaultAlignment() const noexcept override {
        return Alignment::Start;
    }

    void readProperties(PropertyReader& reader) override;
    [[nodiscard]] math::Vec2 measureContent(Context& context, float) override;
    void render(Context& context, const math::Rect& bounds) override;

  private:
    static constexpr std::array<std::pair<std::string_view, Variant>, 2> kVariants{{
        {"ring", Variant::Ring},
        {"cooldown", Variant::Cooldown},
    }};
    static constexpr int kSegments = 64;

    void drawRing(Context& context, math::Vec2 center, float radius) const;
    void drawCooldown(Context& context, const math::Rect& bounds, math::Vec2 center, float radius) const;

    float value = 0.0F;
    float size = 0.0F;
    float thickness = 0.0F;
    Variant variant = Variant::Ring;
    Widgets::Tone tone = Widgets::Tone::Accent;
    TextValue text;
    std::string image;
};

} // namespace haylen::ui
