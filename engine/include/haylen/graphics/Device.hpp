#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
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
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, Texture::Options options = {});

    // Creates a texture of one color, checking the size against the device before any pixel exists.
    [[nodiscard]] Texture createTexture(int width, int height, math::Color fill, Texture::Options options = {});

    // Returns the shared 1 by 1 white texture that untextured draws use.
    [[nodiscard]] const Texture& getWhiteTexture() const noexcept;

    // Replaces the pixels and size of an existing texture, which is not dynamic. Every handle to it sees the new contents from the next draw.
    void replaceTexture(const Texture& texture, const Image& image);

    // Creates a texture whose pixels change in place with updateTexture, such as a glyph atlas. Its pixels reach the GPU with the next upload.
    [[nodiscard]] Texture createDynamicTexture(const Image& image, Texture::Options options = {});
    [[nodiscard]] Texture createDynamicTexture(int width, int height, math::Color fill, Texture::Options options = {});
    [[nodiscard]] Texture createDynamicAlphaTexture(int width, int height, std::span<const std::uint8_t> alpha, Texture::Options options = {});

    // Replaces every pixel of a dynamic texture, keeping its size. The GPU only takes whole textures, so the device keeps the last pixels a texture received and sends each changed texture once per frame, however many updates it had.
    void updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels);

    // Sends the pixels of the dynamic textures that changed since the last upload and returns how many bytes they took. The renderer calls it once per frame, before its passes.
    std::size_t uploadTextures();

    [[nodiscard]] std::string_view getBackendName() const noexcept;

    // The name of the GPU the device runs on, as its backend reports it: the name of the Metal device, the description of the DXGI adapter behind Direct3D 11, the GL_RENDERER string of OpenGL and WebGL 2, and the adapter info of WebGPU. It is empty where the backend reports none, such as the dummy backend of tests.
    [[nodiscard]] std::string getAdapterName() const;
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
    static void validateAlpha(int width, int height, std::span<const std::uint8_t> alpha, int limit);

    std::unique_ptr<DeviceState> state;
};

} // namespace haylen::graphics
