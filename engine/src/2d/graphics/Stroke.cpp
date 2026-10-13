#include "haylen/2d/graphics/Stroke.hpp"

#include <algorithm>

namespace haylen::graphics2d {

std::optional<Stroke::Join> Stroke::joinFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kJoinNames, name, &std::pair<std::string_view, Join>::first);
    return found != kJoinNames.end() ? std::optional<Join>(found->second) : std::nullopt;
}

std::string_view Stroke::joinName(Join value) noexcept {
    const auto found = std::ranges::find(kJoinNames, value, &std::pair<std::string_view, Join>::second);
    return found != kJoinNames.end() ? found->first : std::string_view{};
}

std::optional<Stroke::Cap> Stroke::capFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kCapNames, name, &std::pair<std::string_view, Cap>::first);
    return found != kCapNames.end() ? std::optional<Cap>(found->second) : std::nullopt;
}

std::string_view Stroke::capName(Cap value) noexcept {
    const auto found = std::ranges::find(kCapNames, value, &std::pair<std::string_view, Cap>::second);
    return found != kCapNames.end() ? found->first : std::string_view{};
}

std::optional<Stroke::TextureMode> Stroke::textureModeFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kTextureModeNames, name, &std::pair<std::string_view, TextureMode>::first);
    return found != kTextureModeNames.end() ? std::optional<TextureMode>(found->second) : std::nullopt;
}

std::string_view Stroke::textureModeName(TextureMode value) noexcept {
    const auto found = std::ranges::find(kTextureModeNames, value, &std::pair<std::string_view, TextureMode>::second);
    return found != kTextureModeNames.end() ? found->first : std::string_view{};
}

} // namespace haylen::graphics2d
