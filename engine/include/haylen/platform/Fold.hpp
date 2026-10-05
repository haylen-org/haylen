#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::platform {

// The fold of a foldable device, or the hinge between the two screens of a dual-screen device, across the window, with its bounds in framebuffer pixels. A vertical fold runs from the top to the bottom of the window and a horizontal one from side to side. A seamless fold has an empty width or height, while a hinge that hides content occludes its bounds. A fold that is half opened or occludes separates the window into two segments, one on each side, which content should not cross.
struct Fold {
    enum class Axis : std::uint8_t {
        Vertical,
        Horizontal,
    };

    enum class State : std::uint8_t {
        Flat,
        HalfOpened,
    };

    // How the device is held: flat without a fold or with a fold opened flat, a tabletop with a horizontal fold half opened, and a book with a vertical fold half opened.
    enum class Posture : std::uint8_t {
        Flat,
        Tabletop,
        Book,
    };

    static constexpr std::array<std::pair<std::string_view, Axis>, 2> kAxisNames{{{"vertical", Axis::Vertical}, {"horizontal", Axis::Horizontal}}};
    static constexpr std::array<std::pair<std::string_view, State>, 2> kStateNames{{{"flat", State::Flat}, {"halfOpened", State::HalfOpened}}};
    static constexpr std::array<std::pair<std::string_view, Posture>, 3> kPostureNames{{{"flat", Posture::Flat}, {"tabletop", Posture::Tabletop}, {"book", Posture::Book}}};

    math::Rect bounds;
    Axis axis = Axis::Vertical;
    State state = State::Flat;
    bool separating = false;
    bool occluding = false;

    [[nodiscard]] static std::string_view axisName(Axis value) noexcept;
    [[nodiscard]] static std::string_view stateName(State value) noexcept;
    [[nodiscard]] static std::string_view postureName(Posture value) noexcept;

    [[nodiscard]] Posture getPosture() const noexcept;

    // Returns the areas of a window of the given size on each side of a separating fold, the left or top one first and without the bounds of the fold, or the whole window when the fold does not separate it.
    [[nodiscard]] std::vector<math::Rect> getSegments(math::Vec2 size) const;

    [[nodiscard]] bool operator==(const Fold&) const = default;
};

} // namespace haylen::platform
