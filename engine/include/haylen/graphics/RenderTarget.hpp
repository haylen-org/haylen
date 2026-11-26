#pragma once

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {

struct RenderTargetResource;

// An offscreen target that passes draw into and whose textures draws sample later: up to four colors, an optional depth with or without stencil, and samples that resolve into the textures of one sample when a pass ends. Canvases draw into its first color.
class RenderTarget final {
  public:
    // The attachments of a render target. Each color takes a format that the device draws into and filters, and the first one is the color that canvases draw into. A `depthFormat` adds a depth attachment, which `depthTexture` keeps as a texture that draws sample, resolved by `depthResolve` when the target has several samples. With several samples each color draws into samples that resolve into its texture when a pass ends. A memoryless target keeps its samples and a depth without a texture only inside each pass, which a tile GPU holds in the memory of its tiles, so every pass into it clears them and none keeps them.
    struct Options {
        std::vector<gpu::Device::PixelFormat> colors{gpu::Device::PixelFormat::Rgba8};
        std::optional<gpu::Device::PixelFormat> depthFormat{};
        int samples = 1;
        bool depthTexture = false;
        gpu::Device::DepthResolve depthResolve = gpu::Device::DepthResolve::SampleZero;
        bool memoryless = false;
    };

    RenderTarget() = default;
    explicit RenderTarget(std::shared_ptr<RenderTargetResource> value) noexcept : resource(std::move(value)) {}

    [[nodiscard]] bool isValid() const noexcept {
        return resource != nullptr;
    }

    // The texture of the first color, which canvases draw into.
    [[nodiscard]] const Texture& getTexture() const noexcept;

    // The texture of the color at the index, from 0 up to the count of colors.
    [[nodiscard]] const Texture& getTexture(std::size_t index) const;
    [[nodiscard]] std::size_t getColorCount() const noexcept;

    // The texture of the depth when the target keeps one, and an invalid texture otherwise.
    [[nodiscard]] const Texture& getDepthTexture() const noexcept;

    [[nodiscard]] const Options& getOptions() const noexcept;
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
