#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::gpu {
class Device;
}

namespace haylen::graphics {

struct DeviceState;

// Creates textures and render targets on the GPU device of the backend, which it owns. Every method must be called on the frame thread.
class Device final {
  public:
    // Receives the pixels of a render target, or the reason they could not be read.
    using ReadCallback = std::function<void(std::optional<Image> image, const std::string& error)>;

    // How many objects of one kind the GPU context holds, out of the room it preallocated for them.
    struct Pool {
        std::string_view name;
        int used = 0;
        int size = 0;
    };

    explicit Device(std::unique_ptr<gpu::Device> device);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] Texture createTexture(const Image& image, Texture::Options options = {});
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, Texture::Options options = {});

    // Creates a texture of one color, checking the size against the device before any pixel exists.
    [[nodiscard]] Texture createTexture(int width, int height, math::Color fill, Texture::Options options = {});

    // Returns the shared 1 by 1 white texture that untextured draws use.
    [[nodiscard]] const Texture& getWhiteTexture() const noexcept;

    // Replaces the pixels and size of an existing texture. Every handle to it sees the new contents from the next draw, and a dynamic texture stays dynamic.
    void replaceTexture(const Texture& texture, const Image& image);

    // Creates a texture whose pixels change in place with `updateTexture`, such as a glyph atlas. Its pixels reach the GPU with the next upload.
    [[nodiscard]] Texture createDynamicTexture(const Image& image, Texture::Options options = {});
    [[nodiscard]] Texture createDynamicTexture(int width, int height, math::Color fill, Texture::Options options = {});
    [[nodiscard]] Texture createDynamicAlphaTexture(int width, int height, std::span<const std::uint8_t> alpha, Texture::Options options = {});

    // Replaces every pixel of a dynamic texture, keeping its size. The GPU only takes whole textures, so the device keeps the last pixels a texture received and sends each changed texture once per frame, however many updates it had.
    void updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels);

    // Sends the pixels of the dynamic textures that changed since the last upload and returns how many bytes they took, together with the images that the frame fills itself, such as the shadow atlas of the lights, since the GPU device takes the images of a frame in one call. The renderer calls it once per frame, before its passes.
    std::size_t uploadTextures(std::span<const gpu::Device::ImageWrite> frameImages = {});

    // The name of the graphics API the device draws through, from `gpu::Device::kApiNames`: `metal`, `direct3D12`, `direct3D11`, `vulkan`, `openGl`, `openGlEs`, `webGl2`, `webGpu`, or `none` where nothing draws, such as the headless host of tests.
    [[nodiscard]] std::string_view getApiName() const noexcept;

    // The seconds the GPU took for the last frame that it finished, which lags the frame that runs by a frame or two, or nothing where the graphics API does not measure it. Metal measures it, and the other APIs do not.
    [[nodiscard]] std::optional<double> getGpuFrameSeconds() const noexcept;

    // The name of the GPU the device runs on, as its graphics API reports it: the name of the Metal device, the description of the DXGI adapter behind Direct3D 11, the `GL_RENDERER` string of OpenGL and WebGL 2, and the adapter info of WebGPU. It is empty where the API reports none, such as on the headless host of tests.
    [[nodiscard]] std::string getAdapterName() const;
    [[nodiscard]] int getMaxTextureSize() const noexcept;

    // Lists how many images, buffers, samplers, programs and pipelines exist, out of the room the GPU device holds for them.
    [[nodiscard]] std::vector<Pool> getPools() const;

    // Destroys GPU resources whose last handle was released, at a point where no pending draw can use them.
    void collectGarbage();

    // Reads the pixels of a render target as the frame that asks for them renders it, with straight colors and the top row first. The copy starts once that frame is submitted and the GPU finishes it without the frame thread waiting, where the graphics API allows, and the callback receives the image on the frame thread in a later frame, after the task pool turned the copy into straight colors.
    void readTarget(const RenderTarget& target, ReadCallback done);

    // Starts the copies the frame asked for and hands over the finished ones. The engine calls it once per frame, after the frame was submitted.
    void processReads(core::JobSystem& jobs);

    // Drops every read that has not reached its callback, which never runs, such as when the app stops.
    void cancelReads() noexcept;

    // Returns how many reads have not reached their callback yet.
    [[nodiscard]] std::size_t getPendingReadCount() const noexcept;

    [[nodiscard]] gpu::Device& getGpu() noexcept;

    [[nodiscard]] DeviceState& getState() noexcept {
        return *state;
    }

  private:
    static void validateSize(int width, int height, int limit);
    static void validateAlpha(int width, int height, std::span<const std::uint8_t> alpha, int limit);

    std::unique_ptr<DeviceState> state;
};

} // namespace haylen::graphics
