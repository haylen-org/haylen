#include "haylen/audio/Session.hpp"

#include <algorithm>

namespace haylen::audio {

const std::array<std::pair<std::string_view, Session::Category>, 3> Session::kCategoryNames{{{"ambient", Category::Ambient}, {"soloAmbient", Category::SoloAmbient}, {"playback", Category::Playback}}};

std::optional<Session::Category> Session::categoryFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kCategoryNames, name, &std::pair<std::string_view, Category>::first);
    return found != kCategoryNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Session::categoryName(Category value) noexcept {
    return std::ranges::find(kCategoryNames, value, &std::pair<std::string_view, Category>::second)->first;
}

} // namespace haylen::audio
