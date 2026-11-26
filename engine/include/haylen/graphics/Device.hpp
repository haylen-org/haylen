#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/Buffer.hpp"
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

// Creates textures and render targets on the GPU device of the platform, which it owns. Every method must be called on the frame thread.
class Device final {
  public:
    // Receives the pixels of a render target, or the reason they could not be read.
    using ReadCallback = std::function<void(std::optional<Image> image, const std::string& error)>;

    // Receives the bytes of a range of a buffer, or the reason they could not be read.
    using BufferReadCallback = std::function<void(std::optional<std::vector<std::uint8_t>> bytes, const std::string& error)>;

    // Receives the path of the file that the capture tool of the GPU device wrote, or the reason the capture failed.
    using CaptureCallback = std::function<void(std::optional<std::filesystem::path> path, const std::string& error)>;

    // How many objects of one kind the GPU context holds, out of the room it preallocated for them.
    using Pool = gpu::Device::Pool;

    // A rectangle of the pixels of a texture in whole pixels, from its top row and its left column.
    using Region = gpu::Device::Region;

    // The pixels of a texture of a type in a pixel format of the GPU device, such as the blocks of a compressed format that an app ships for its target, with one span for each mip level from the largest, each holding the rows of blocks of every layer or slice of its level tightly packed, the first layer first. The layers count the layers of an array, the six faces of a cube and six faces for each cube of a cube array, in the order +X, -X, +Y, -Y, +Z and -Z, and the depth counts the slices of a volume.
    struct TextureData {
        gpu::Device::TextureType type = gpu::Device::TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int depth = 1;
        gpu::Device::PixelFormat format = gpu::Device::PixelFormat::Rgba8;
        std::span<const std::span<const std::byte>> levels{};
    };

    // A range of the mip levels and layers of a texture that a view shows as a texture of the type, from its first mip level and layer: a flat view of a flat texture, an array view of an array or of faces of a cube or a cube array, a cube view of a cube, a cube array view of whole cubes and a volume view of every slice of a volume.
    struct TextureView {
        gpu::Device::TextureType type = gpu::Device::TextureType::Flat;
        int mipLevel = 0;
        int mipLevels = 1;
        int layer = 0;
        int layers = 1;
    };

    explicit Device(std::unique_ptr<gpu::Device> device);
    ~Device();

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    [[nodiscard]] Texture createTexture(const Image& image, Texture::Options options = {});

    // Creates a texture of its type from its levels in their format, which keeps mipmaps when it brings more than one level and `mipmaps` is not `false`. A texture that brings its first level alone with `mipmaps` set to `true` keeps the smaller levels that the GPU generates from it, in a format whose mipmaps the device generates. Throws `std::invalid_argument` for a format that is no format of textures, a type or a size that the device lacks, sides that hold no whole blocks, levels of another count or size, or a format that the device does not sample, filter or generate mipmaps of, with a sentence that names what is wrong and the graphics API.
    [[nodiscard]] Texture createTexture(const TextureData& data, Texture::Options options = {});

    // Creates a render target of the size with the attachments that the render target options name, whose textures sample as the texture options say. Throws `std::invalid_argument` for a size the device lacks, mipmaps, no color or more than four, a color format that the device does not draw into or filter, a format that is no depth format for the depth, a sample count that a format does not take, or a kept depth that the device does not sample or resolve, with a sentence that names what is wrong and the graphics API.
    [[nodiscard]] RenderTarget createRenderTarget(int width, int height, Texture::Options options = {}, const RenderTarget::Options& attachments = {});

    // Creates a buffer of the size, which starts with zeros, or one that starts with the data and takes its size, for what the options name. Throws `std::invalid_argument` for a buffer without a size or a usage, for usages that the device lacks or that no buffer combines, such as a uniform buffer that is also a vertex buffer, for an immutable buffer without data or that copies reach, and for a stream buffer that copies reach, with a sentence that names the buffer.
    [[nodiscard]] Buffer createBuffer(std::size_t size, const Buffer::Options& options);
    [[nodiscard]] Buffer createBuffer(std::span<const std::byte> data, const Buffer::Options& options);

    // Writes bytes into a buffer at an offset that is a multiple of 4, before the passes of the frame that read them. A dynamic buffer keeps them and takes writes of whole words, and a stream buffer holds in a frame only what that frame wrote, each part once. Throws `std::invalid_argument` for an immutable buffer, a range outside the buffer or a range that is not whole words.
    void writeBuffer(const Buffer& buffer, std::size_t offset, std::span<const std::byte> data);

    // Copies bytes from a buffer that is a copy source into another one that is a copy destination, at offsets and of a size that are multiples of 4, before the passes of the frame that read them. Throws `std::invalid_argument` for buffers without those usages, a buffer and itself, or ranges outside the buffers or not of whole words.
    void copyBuffer(const Buffer& source, std::size_t sourceOffset, const Buffer& destination, std::size_t destinationOffset, std::size_t size);

    // Reads a range of a buffer that is a copy source as the frame that asks for it leaves the buffer, at an offset and of a size that are multiples of 4. The copy starts once that frame is submitted, and the callback receives the bytes on the frame thread in a later frame. Throws `std::invalid_argument` for a buffer that is no copy source or a range outside it or not of whole words.
    void readBuffer(const Buffer& buffer, std::size_t offset, std::size_t size, BufferReadCallback done);

    // Creates a texture of one color, checking the size against the device before any pixel exists.
    [[nodiscard]] Texture createTexture(int width, int height, math::Color fill, Texture::Options options = {});

    // Returns the shared 1 by 1 white texture that untextured draws use.
    [[nodiscard]] const Texture& getWhiteTexture() const noexcept;

    // Returns the white texture of one pixel of the kind, with the six faces of a cube and of one cube of a cube array, which the texture slots of that kind of a material sample when the material leaves them unset. It is made the first time it is asked for.
    [[nodiscard]] const Texture& getWhiteTexture(gpu::Device::TextureType type);

    // Replaces the pixels and size of an existing texture. Every handle to it sees the new contents from the next draw, and a dynamic texture stays dynamic.
    void replaceTexture(const Texture& texture, const Image& image);

    // Creates a texture whose pixels change in place with `updateTexture`, such as an image an app paints each frame. Its pixels reach the GPU with the next upload.
    [[nodiscard]] Texture createDynamicTexture(const Image& image, Texture::Options options = {});
    [[nodiscard]] Texture createDynamicTexture(int width, int height, math::Color fill, Texture::Options options = {});

    // Creates a texture of the type and the size of the data, which brings no levels, with every pixel of every layer of one color, checking the shape against the device before any pixel exists. A texture keeps mipmaps as a texture of an image does, and a dynamic one when `mipmaps` is `true`, which the GPU generates.
    [[nodiscard]] Texture createTexture(const TextureData& shape, math::Color fill, Texture::Options options = {});
    [[nodiscard]] Texture createDynamicTexture(const TextureData& shape, math::Color fill, Texture::Options options = {});

    // Creates a dynamic texture of any type from the pixels of its first level in an uncompressed format, whose layers change in place with `updateTexture`. With `mipmaps` set to `true` the GPU generates its smaller levels after each upload. Throws `std::invalid_argument` as `createTexture` does, and for data of more than one level.
    [[nodiscard]] Texture createDynamicTexture(const TextureData& data, Texture::Options options = {});

    // Creates a view of a range of the mip levels and layers of a texture, which samples as a texture of the type of the view, with the sampling of its texture, and keeps its texture alive. Throws `std::invalid_argument` on a device without views, for a type that the texture has no view of, a range that the texture does not hold, or a texture that is a view or a render target of several samples.
    [[nodiscard]] Texture createView(const Texture& texture, const TextureView& view);

    // Creates a dynamic texture of one channel of half floats, such as the signed distance field of a glyph atlas, which every GPU filters without the steps of a byte. Its `updateTexture` takes the bytes of the halves.
    [[nodiscard]] Texture createDynamicDistanceTexture(int width, int height, std::span<const std::uint16_t> distances, Texture::Options options = {});

    // Replaces every pixel of a dynamic texture, keeping its size.
    void updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels);

    // Takes every pixel of a dynamic texture, of which only the region changed since its last update, such as the place of a glyph that an atlas added. The device keeps the pixels of the region, and a texture that changes several times before the next upload sends once the smallest region that holds every change, from the pixels of its last update, so the GPU receives only the pixels that changed.
    void updateTexture(const Texture& texture, std::span<const std::uint8_t> pixels, Region changed);

    // Takes every pixel of one layer of a dynamic texture, a layer of an array, a face of a cube or a slice of a volume, of which only the region changed, the way the other forms take the pixels of a flat texture.
    void updateTexture(const Texture& texture, int layer, std::span<const std::uint8_t> pixels, Region changed);

    // Sends the regions of the dynamic textures that changed since the last upload and returns how many bytes they took, together with the images that the frame fills itself, such as the shadow atlas of the lights, since the GPU device takes the images of a frame in one call, and then generates the mip levels of the textures whose GPU makes them. The renderer calls it once per frame, before its passes.
    std::size_t uploadTextures(std::span<const gpu::Device::ImageWrite> frameImages = {});

    // The name of the graphics API the device draws through, from `gpu::Device::kApiNames`: `metal`, `direct3D12`, `direct3D11`, `vulkan`, `openGl`, `openGlEs`, `webGl2`, `webGpu`, `port` for the device of a port, or `none` where nothing draws, such as the headless host of tests.
    [[nodiscard]] std::string_view getApiName() const noexcept;

    // The seconds the GPU took for the last frame that it finished, which lags the frame that runs by a frame or two, or nothing where the graphics API does not measure it. Metal measures it, and the other APIs do not.
    [[nodiscard]] std::optional<double> getGpuFrameSeconds() const noexcept;

    // The seconds from the moment the platform read the input of the last frame that the window showed until the display showed it, or nothing where the graphics API does not report when its frames reach the display. Metal, Direct3D 12, Direct3D 11 and Vulkan with `VK_GOOGLE_display_timing` report it.
    [[nodiscard]] std::optional<double> getInputAge() const noexcept;

    // The name of the GPU the device runs on, as its graphics API reports it: the name of the Metal device, the description of the DXGI adapter behind Direct3D 11, the `GL_RENDERER` string of OpenGL and WebGL 2, and the adapter info of WebGPU. It is empty where the API reports none, such as on the headless host of tests.
    [[nodiscard]] std::string getAdapterName() const;
    [[nodiscard]] int getMaxTextureSize() const noexcept;

    // Fills `pools` with how many images, buffers, samplers, programs and pipelines exist, out of the room the GPU device holds for them, in the storage it holds.
    void readPools(std::vector<Pool>& pools) const;

    // The seconds of frames that the memory the GPU device holds for later must stop growing before the device gives it back.
    static constexpr double kTrimDelay = 2.0;

    // Destroys GPU resources whose last handle was released, at a point where no pending draw can use them.
    void collectGarbage();

    // Follows the memory that the GPU device holds for later through the seconds of a frame, and trims the device once that memory stopped growing for `kTrimDelay`, so what a burst of loads and releases left behind goes back to the system while a frame that draws what the frame before drew trims nothing.
    void followSpareMemory(double seconds);

    // Gives back at once the memory that the GPU device holds for later, after the resources whose last handle was released, as a low memory warning of the system or the app asks.
    void trim();

    // The bytes of memory that the GPU device holds for later beyond what its objects take.
    [[nodiscard]] std::size_t getSpareBytes() const noexcept;

    // Reads the pixels of a render target as the frame that asks for them renders it, with straight colors and the top row first. The copy starts once that frame is submitted and the GPU finishes it without the frame thread waiting, where the graphics API allows, and the callback receives the image on the frame thread in a later frame, after the task pool turned the copy into straight colors.
    void readTarget(const RenderTarget& target, ReadCallback done);

    // Starts the copies the frame asked for and hands over the finished ones. The engine calls it once per frame, after the frame was submitted.
    void processReads(core::JobSystem& jobs);

    // Drops every read that has not reached its callback, which never runs, such as when the app stops.
    void cancelReads() noexcept;

    // Returns how many reads have not reached their callback yet.
    [[nodiscard]] std::size_t getPendingReadCount() const noexcept;

    // The tool of the graphics API that captures the frames of the GPU device, such as the frame capture of Metal or RenderDoc, or none with what is missing and how to get it.
    [[nodiscard]] gpu::Device::CaptureSupport getCaptureSupport() const;

    // Records every command of the GPU device into a capture of its capture tool from now until the number of frames was submitted, at the path without its extension, which the tool adds, and hands the path of the file to the callback on the frame thread once the last of them was submitted. Throws `std::invalid_argument` for fewer than one frame, `std::logic_error` while another capture records, and `std::runtime_error` when the device has no capture tool, with what is missing, or the tool refuses to start.
    void captureFrames(int frames, const std::filesystem::path& path, CaptureCallback done);
    [[nodiscard]] bool isCapturing() const noexcept;

    // Counts the frame of a capture that records and ends the capture after its last frame. The engine calls it once per frame, after the frame was submitted.
    void processCapture();

    // Ends the capture that records and drops its callback, which never runs, such as when the app stops.
    void cancelCapture() noexcept;

    [[nodiscard]] gpu::Device& getGpu() noexcept;

    [[nodiscard]] DeviceState& getState() noexcept {
        return *state;
    }

  private:
    static void validateSize(int width, int height, int limit);
    void validateBuffer(std::size_t size, bool data, const Buffer::Options& options) const;
    static void validateRange(const Buffer& buffer, std::size_t offset, std::size_t size, std::string_view action, bool wholeWords);
    void processBufferReads();
    static void validateDistances(int width, int height, std::span<const std::uint16_t> distances, int limit);
    void validateData(const TextureData& data, Texture::Options options) const;
    void validateShape(const TextureData& data) const;
    [[nodiscard]] static std::vector<std::uint8_t> fillLayers(const TextureData& shape, math::Color fill);
    void validateView(const Texture& texture, const TextureView& view) const;
    void validateAttachments(const RenderTarget::Options& attachments) const;
    void validateSamples(gpu::Device::PixelFormat format, int samples) const;
    [[nodiscard]] static std::string_view getDepthResolveName(gpu::Device::DepthResolve mode) noexcept;
    [[nodiscard]] static std::string_view getFormatName(gpu::Device::PixelFormat format) noexcept;

    std::unique_ptr<DeviceState> state;
};

} // namespace haylen::graphics
