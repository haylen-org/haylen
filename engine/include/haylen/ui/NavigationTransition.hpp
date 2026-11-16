#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>

#include "haylen/math/Easing.hpp"
#include "haylen/math/EasingCurve.hpp"

namespace haylen::ui {

// How screens change places in the area they share: a slide along the reading direction, as a stack pushes and pops, a cross fade, a rise from the bottom of the screen, as a sheet arrives, or a change at once. The screens move on real time, and while the system asks for reduced motion a slide and a rise cross fade instead.
struct NavigationTransition {
    enum class Effect : std::uint8_t {
        None,
        Slide,
        Fade,
        Rise,
    };

    static constexpr float kDuration = 0.3F;

    Effect effect = Effect::Slide;
    float duration = kDuration;
    math::EasingCurve ease{math::Easing::Type::CubicOut};

    // Returns the transition that the system setting of reduced motion leaves.
    [[nodiscard]] NavigationTransition reduced(bool reducedMotion) const;

    // Whether the change happens at once, without screens that move.
    [[nodiscard]] bool isInstant() const noexcept {
        return effect == Effect::None || duration <= 0.0F;
    }

    [[nodiscard]] static std::optional<Effect> effectFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view effectName(Effect value) noexcept;

    static constexpr std::array<std::pair<std::string_view, Effect>, 4> kEffectNames{{{"none", Effect::None}, {"slide", Effect::Slide}, {"fade", Effect::Fade}, {"rise", Effect::Rise}}};
};

} // namespace haylen::ui
