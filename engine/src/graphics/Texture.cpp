#include "haylen/graphics/Texture.hpp"

#include <algorithm>

#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

const std::array<std::pair<std::string_view, Texture::Filter>, 2> Texture::kFilterNames{{{"nearest", Filter::Nearest}, {"linear", Filter::Linear}}};
const std::array<std::pair<std::string_view, Texture::Wrap>, 3> Texture::kWrapNames{{{"clamp", Wrap::Clamp}, {"repeat", Wrap::Repeat}, {"mirror", Wrap::Mirror}}};

std::optional<Texture::Filter> Texture::filterFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kFilterNames, name, &std::pair<std::string_view, Filter>::first);
    return found != kFilterNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Texture::filterName(Filter value) noexcept {
    return std::ranges::find(kFilterNames, value, &std::pair<std::string_view, Filter>::second)->first;
}

std::optional<Texture::Wrap> Texture::wrapFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kWrapNames, name, &std::pair<std::string_view, Wrap>::first);
    return found != kWrapNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Texture::wrapName(Wrap value) noexcept {
    return std::ranges::find(kWrapNames, value, &std::pair<std::string_view, Wrap>::second)->first;
}

int Texture::getWidth() const noexcept {
    return resource ? resource->width : 0;
}

int Texture::getHeight() const noexcept {
    return resource ? resource->height : 0;
}

math::Vec2 Texture::getSize() const noexcept {
    return {static_cast<float>(getWidth()), static_cast<float>(getHeight())};
}

std::uint32_t Texture::getId() const noexcept {
    return resource ? resource->id : 0;
}

Texture::Options Texture::getOptions() const noexcept {
    return resource ? resource->options : Options{};
}

} // namespace haylen::graphics
