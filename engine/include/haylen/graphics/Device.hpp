#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::graphics {

struct DeviceSetup;
struct DeviceState;

// Owns the GPU context and creates textures and render targets. Every method must be called on the frame thread.
class Device final {
  public:
    // How many objects of one kind the GPU context holds, out of the room it preallocated for them.
    struct Pool {
        std::string_view name;
        int used = 0;
        int size = 0;
    };

    explicit Device(const DeviceSetup& setup);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] Texture createTexture(const Image& image, Texture::Options options = {});
    [[nodiscard]] Texture createAlphaTexture(int width, int height, std::span<const std::uint8_t> alpha, Texture::Options options = {});
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, Texture::Options options = {});

    // Returns the shared 1 by 1 white texture that untextured draws use.
    [[nodiscard]] const Texture& getWhiteTexture() const noexcept;

    // Replaces the pixels and size of an existing texture. Every handle to it sees the new contents from the next draw.
    void replaceTexture(const Texture& texture, const Image& image);
    void replaceAlphaTexture(const Texture& texture, int width, int height, std::span<const std::uint8_t> alpha);

    [[nodiscard]] std::string_view getBackendName() const noexcept;
    [[nodiscard]] int getMaxTextureSize() const noexcept;

    // Lists the pools of images, views, buffers, samplers, shaders and pipelines.
    [[nodiscard]] std::vector<Pool> getPools() const;

    // Destroys GPU resources whose last handle was released, at a point where no pending draw can use them.
    void collectGarbage();

    [[nodiscard]] DeviceState& getState() noexcept {
        return *state;
    }

  private:
    static void validateSize(int width, int height, int limit);

    std::unique_ptr<DeviceState> state;
};

} // namespace haylen::graphics
