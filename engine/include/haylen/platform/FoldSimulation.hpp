#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>

#include "haylen/core/Json.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/platform/Fold.hpp"

namespace haylen::platform {

// A fold the engine uses instead of the one the device reports, to test layouts around folds and hinges without the device. The fold lies across the middle of the window, and a hinge of a width in window points hides what is under it.
class FoldSimulation final {
  public:
    // A posture the simulation imitates by name.
    struct Preset {
        std::string_view name;
        Fold::Axis axis;
        Fold::State state;
        float hinge;
    };

    static constexpr std::array<Preset, 4> kPresets{{
        {"book", Fold::Axis::Vertical, Fold::State::HalfOpened, 0.0F},
        {"tabletop", Fold::Axis::Horizontal, Fold::State::HalfOpened, 0.0F},
        {"flat", Fold::Axis::Vertical, Fold::State::Flat, 0.0F},
        {"dualScreen", Fold::Axis::Vertical, Fold::State::Flat, 34.0F},
    }};

    // Reads a preset name such as `book`, or an object with the `axis`, the `state` and the `hinge` width in window points. Throws `std::invalid_argument` for anything else.
    [[nodiscard]] static FoldSimulation fromJson(const core::Json& value);

    // Returns the fold in framebuffer pixels for a framebuffer of the given size and pixels per point. A half opened fold and a hinge separate the window.
    [[nodiscard]] Fold getFold(math::Vec2 framebuffer, float dpiScale) const noexcept;

    // Writes the preset name, or the object of the axis, the state and the hinge.
    [[nodiscard]] core::Json toJson() const;

    [[nodiscard]] bool operator==(const FoldSimulation&) const = default;

  private:
    std::optional<std::size_t> preset;
    Fold::Axis axis = Fold::Axis::Vertical;
    Fold::State state = Fold::State::Flat;
    float hinge = 0.0F;
};

} // namespace haylen::platform
