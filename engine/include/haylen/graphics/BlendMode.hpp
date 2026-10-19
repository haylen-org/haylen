#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace haylen::graphics {

// The ways a draw combines its colors with the colors already in the target. The modes from `Overlay` on mix each color with the color below it, which the blending of the GPU cannot reach, so groups alone take them: the group renders first, and its image blends with a copy of what its canvas drew below it.
class BlendMode final {
  public:
    enum class Type : std::uint8_t {
        Alpha,
        Additive,
        Multiply,
        Screen,
        Premultiplied,
        Opaque,
        Overlay,
        Darken,
        Lighten,
        ColorDodge,
        ColorBurn,
        HardLight,
        SoftLight,
        Difference,
        Exclusion,
    };

    // Resolves the names of the modes, `alpha` to `opaque` and `overlay`, `darken`, `lighten`, `colorDodge`, `colorBurn`, `hardLight`, `softLight`, `difference` and `exclusion`.
    [[nodiscard]] static std::optional<Type> parse(std::string_view text) noexcept;
    [[nodiscard]] static std::string_view name(Type mode) noexcept;
    [[nodiscard]] static bool readsBackdrop(Type mode) noexcept {
        return mode >= Type::Overlay;
    }

  private:
    static const std::array<std::string_view, 15> kNames;
};

} // namespace haylen::graphics
