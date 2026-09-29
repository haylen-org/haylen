#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

namespace haylen::graphics {

// The ways a draw combines its colors with the colors already in the target.
class BlendMode final {
  public:
    enum class Type : std::uint8_t {
        Alpha,
        Additive,
        Multiply,
        Screen,
        Premultiplied,
        Opaque,
    };

    // Resolves the names "alpha", "additive", "multiply", "screen", "premultiplied" and "opaque".
    [[nodiscard]] static std::optional<Type> parse(std::string_view text) noexcept;
    [[nodiscard]] static std::string_view name(Type mode) noexcept;

  private:
    static const std::array<std::string_view, 6> kNames;
};

} // namespace haylen::graphics
