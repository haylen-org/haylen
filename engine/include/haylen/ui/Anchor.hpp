#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Alignment.hpp"

namespace haylen::ui {

// Places a node against an area of the screen instead of inside its parent: at a corner, at an edge, at the center, or stretched along one or both axes.
struct Anchor {
    // The area an anchored node is placed in: the safe area, away from notches, rounded corners and system bars, or the whole visible screen.
    enum class Area : std::uint8_t {
        Safe,
        Screen,
    };

    struct Preset {
        std::string_view name;
        Alignment horizontal;
        Alignment vertical;
    };

    static constexpr std::array<Preset, 16> kPresets{{
        {"topLeft", Alignment::Start, Alignment::Start},
        {"top", Alignment::Center, Alignment::Start},
        {"topRight", Alignment::End, Alignment::Start},
        {"left", Alignment::Start, Alignment::Center},
        {"center", Alignment::Center, Alignment::Center},
        {"right", Alignment::End, Alignment::Center},
        {"bottomLeft", Alignment::Start, Alignment::End},
        {"bottom", Alignment::Center, Alignment::End},
        {"bottomRight", Alignment::End, Alignment::End},
        {"stretch", Alignment::Stretch, Alignment::Stretch},
        {"stretchTop", Alignment::Stretch, Alignment::Start},
        {"stretchBottom", Alignment::Stretch, Alignment::End},
        {"stretchLeft", Alignment::Start, Alignment::Stretch},
        {"stretchRight", Alignment::End, Alignment::Stretch},
        {"stretchHorizontal", Alignment::Stretch, Alignment::Center},
        {"stretchVertical", Alignment::Center, Alignment::Stretch},
    }};

    Alignment horizontal = Alignment::Start;
    Alignment vertical = Alignment::Start;

    [[nodiscard]] static std::optional<Anchor> fromName(std::string_view name) noexcept;

    // Returns where a node of the measured size goes inside the area, taking the whole area along stretched axes.
    [[nodiscard]] math::Rect place(math::Vec2 size, const math::Rect& area) const noexcept;
};

} // namespace haylen::ui
