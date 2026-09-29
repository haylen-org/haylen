#pragma once

#include <memory>

#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {

struct RenderTargetResource;

// Offscreen color target that canvases can render into and that can be drawn as a texture.
class RenderTarget final {
  public:
    RenderTarget() = default;
    explicit RenderTarget(std::shared_ptr<RenderTargetResource> value) noexcept : resource(std::move(value)) {}

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }
    [[nodiscard]] const Texture& getTexture() const noexcept;
    [[nodiscard]] int getWidth() const noexcept {
        return getTexture().getWidth();
    }
    [[nodiscard]] int getHeight() const noexcept {
        return getTexture().getHeight();
    }
    [[nodiscard]] math::Vec2 getSize() const noexcept {
        return getTexture().getSize();
    }
    [[nodiscard]] const std::shared_ptr<RenderTargetResource>& getResource() const noexcept {
        return resource;
    }
    [[nodiscard]] bool operator==(const RenderTarget& other) const noexcept {
        return resource == other.resource;
    }

  private:
    std::shared_ptr<RenderTargetResource> resource;
};

} // namespace haylen::graphics
