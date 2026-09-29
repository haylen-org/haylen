#include "haylen/graphics/Texture.hpp"

#include "graphics/TextureResource.hpp"

namespace haylen::graphics {

std::optional<Texture::Filter> Texture::filterFromName(std::string_view name) noexcept {
    if (name == "nearest") {
        return Filter::Nearest;
    }
    if (name == "linear") {
        return Filter::Linear;
    }
    return std::nullopt;
}

std::optional<Texture::Wrap> Texture::wrapFromName(std::string_view name) noexcept {
    if (name == "clamp") {
        return Wrap::Clamp;
    }
    if (name == "repeat") {
        return Wrap::Repeat;
    }
    if (name == "mirror") {
        return Wrap::Mirror;
    }
    return std::nullopt;
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
