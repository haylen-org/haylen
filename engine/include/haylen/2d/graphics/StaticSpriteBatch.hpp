#pragma once

#include <cstddef>
#include <memory>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::graphics2d {

struct StaticBatchResource;

// Immutable GPU copy of many sprites that share one texture, drawn every frame without uploading instance data again.
class StaticSpriteBatch final {
  public:
    StaticSpriteBatch() = default;
    explicit StaticSpriteBatch(std::shared_ptr<StaticBatchResource> value) noexcept : resource(std::move(value)) {}

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] math::Rect getBounds() const noexcept;
    [[nodiscard]] const graphics::Texture& getTexture() const noexcept;
    [[nodiscard]] const std::shared_ptr<StaticBatchResource>& getResource() const noexcept {
        return resource;
    }

  private:
    std::shared_ptr<StaticBatchResource> resource;
};

} // namespace haylen::graphics2d
