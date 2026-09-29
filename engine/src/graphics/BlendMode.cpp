#include "haylen/graphics/BlendMode.hpp"

namespace haylen::graphics {

const std::array<std::string_view, 6> BlendMode::kNames = {"alpha", "additive", "multiply", "screen", "premultiplied", "opaque"};

std::optional<BlendMode::Type> BlendMode::parse(std::string_view text) noexcept {
    for (std::size_t index = 0; index < kNames.size(); ++index) {
        if (kNames[index] == text) {
            return static_cast<Type>(index);
        }
    }
    return std::nullopt;
}

std::string_view BlendMode::name(Type mode) noexcept {
    return kNames[static_cast<std::size_t>(mode)];
}

} // namespace haylen::graphics
