#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/DeviceFactory.hpp"
#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/Conversions.hpp"
#include "gpu/opengl/DebugOutput.hpp"
#include "gpu/opengl/FramebufferCache.hpp"
#include "gpu/opengl/GpuTimer.hpp"
#include "gpu/opengl/ObjectTable.hpp"
#include "gpu/opengl/ShaderProgram.hpp"
#include "gpu/opengl/UploadRing.hpp"
#include "gpu/opengl/VertexArray.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// The device of OpenGL 4.3 core on Windows and Linux, OpenGL ES 3.0 on Android and WebGL2 on the web, one class that knows its flavour. It draws through the context the platform created with the window, which stays current on the frame thread, and keeps every object as long as that context lives. The first row of its render targets is their bottom row. OpenGL keeps an object that a queued command still uses until the GPU finished the command, so `destroy` deletes objects at once.
class Device final : public gpu::Device {
  public:
    static constexpr std::size_t kFramesInFlight = 3;

    // Makes the device of the flavour on the context of the surface, which must be a `Surface::Kind::Gl` surface, since OpenGL draws nowhere without the context of a window or of an offscreen drawable that the platform or a test made. Throws `std::runtime_error` when the context cannot run the flavour.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(Api flavour, const std::optional<Surface>& surface, const DeviceFactory::Options& options);

    Device(Api flavour, const Surface& surface, bool validation);
    ~Device() override;

    [[nodiscard]] Language getLanguage() const noexcept override;
    [[nodiscard]] Api getApi() const noexcept override;
    [[nodiscard]] const Capabilities& getCapabilities() const noexcept override;
    [[nodiscard]] std::string getAdapterName() const override;
    [[nodiscard]] std::optional<double> getGpuFrameSeconds() const noexcept override;
    [[nodiscard]] std::vector<Pool> getPools() const override;
    [[nodiscard]] bool isLost() const noexcept override;

    [[nodiscard]] Buffer makeBuffer(const BufferDesc& desc) override;
    [[nodiscard]] Image makeImage(const ImageDesc& desc) override;
    [[nodiscard]] Sampler makeSampler(const SamplerDesc& desc) override;
    [[nodiscard]] Program makeProgram(const ProgramDesc& desc) override;
    [[nodiscard]] Pipeline makePipeline(const PipelineDesc& desc) override;
    void prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) override;

    void destroy(Buffer buffer) noexcept override;
    void destroy(Image image) noexcept override;
    void destroy(Sampler sampler) noexcept override;
    void destroy(Program program) noexcept override;
    void destroy(Pipeline pipeline) noexcept override;

    void writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) override;
    void writeImages(std::span<const ImageWrite> writes) override;

    void beginPass(const PassDesc& desc) override;
    void applyViewport(const math::Rect& pixels) override;
    void applyScissor(const math::Rect& pixels) override;
    void applyPipeline(Pipeline pipeline) override;
    void applyBindings(const Bindings& bindings) override;
    void applyUniforms(int slot, std::span<const std::byte> data) override;
    void applyStencilReference(std::uint8_t value) override;
    void applyBlendColor(math::Color value) override;
    void draw(int first, int count, int instances) override;
    void endPass() override;

    void pushDebugGroup(std::string_view name) override;
    void popDebugGroup() override;

    void commit() override;

    void resizeSurface(int width, int height) override;
    void detachSurface() override;
    void attachSurface(const Surface& surface) override;

    [[nodiscard]] std::unique_ptr<Readback> readImage(Image image, int width, int height) override;

  private:
    struct BufferRecord {
        Gl::Uint name = 0;
        std::size_t size = 0;
        Usage usage = Usage::Immutable;
        bool index = false;
    };

    // A multisampled image is a renderbuffer, whose target is `Gl::kRenderbuffer`, and the others are textures.
    struct ImageRecord {
        Gl::Uint name = 0;
        Gl::Enum target = Gl::kTexture2d;
        TextureType type = TextureType::Flat;
        PixelFormat format = PixelFormat::Rgba8;
        Conversions::Pixel pixel;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        int sampleCount = 1;
        ImageKind kind = ImageKind::Texture;
    };

    struct PipelineRecord {
        std::shared_ptr<ShaderProgram> shader;
        std::unique_ptr<VertexArray> vertexArray;
        StateCache::Raster raster;
        Gl::Enum primitive = Gl::kTriangles;
        bool indexed = false;
        Gl::Enum indexType = Gl::kUnsignedShort;
        std::size_t indexSize = 2;
        std::size_t bufferCount = 0;
    };

    // The pass that is open: the size of what it draws into, its framebuffer and what its attachments do when it ends. A pass into the swapchain of a frame without a surface records nothing.
    struct PassState {
        bool open = false;
        bool skipped = false;
        bool swapchain = false;
        int width = 0;
        int height = 0;
        Gl::Uint framebuffer = 0;
        std::array<ColorAttachment, kMaxColorTargets> colors{};
        std::size_t colorCount = 0;
        std::optional<DepthStencilAttachment> depthStencil;
        bool stencil = false;
        bool grouped = false;
    };

    [[nodiscard]] static std::shared_ptr<Context> makeContext(Api flavour, const Surface& surface);
    [[nodiscard]] static Language toLanguage(Api flavour) noexcept;
    [[nodiscard]] static std::uintptr_t toAddress(const void* pointer) noexcept;
    static void copyPixels(std::byte* destination, std::span<const std::byte> source, bool swapsRedBlue) noexcept;
    [[nodiscard]] static int getLevelSize(int size, int level) noexcept;
    [[nodiscard]] StateCache::Rectangle toWindow(const math::Rect& pixels) const noexcept;

    void applyDriverIssues();
    void readCapabilities();
    [[nodiscard]] bool isRenderable(const Conversions::Pixel& pixel) const noexcept;
    void checkSampleCount(const ImageDesc& desc, const Conversions::Pixel& pixel) const;
    void checkImage(const ImageDesc& desc, const Conversions::Pixel& pixel) const;
    void checkLevels(const ImageDesc& desc, const Conversions::Pixel& pixel) const;
    [[nodiscard]] Gl::Uint makeRenderbuffer(const ImageDesc& desc, const Conversions::Pixel& pixel);
    [[nodiscard]] Gl::Uint makeTexture(const ImageDesc& desc, const Conversions::Pixel& pixel, Gl::Enum target);
    void uploadTexture(const ImageDesc& desc, const ImageRecord& record);
    [[nodiscard]] std::size_t checkImageWrites(std::span<const ImageWrite> writes);
    void uploadImagesFromMemory(std::span<const ImageWrite> writes);
    void uploadImagesThroughRing(std::span<const ImageWrite> writes, std::size_t total);
    void uploadLevel(const ImageRecord& record, int level, std::uintptr_t source);
    [[nodiscard]] std::size_t getLevelBytes(const ImageRecord& record, int level) const noexcept;
    [[nodiscard]] FramebufferCache::Attachment toAttachment(Image image, int mipLevel, int layer);
    [[nodiscard]] Gl::Uint getSingleFramebuffer(Image image, std::string_view label);
    [[nodiscard]] StateCache::Raster toRaster(const PipelineDesc& desc) const;

    void beginFrame();
    void waitForFrame(std::size_t slot);
    void requirePass(std::string_view call) const;
    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    void loadAttachments();
    void resolveAttachments();
    void discardAttachments();
    void checkLost();

    std::shared_ptr<Context> context;
    Gl& gl;
    StateCache& state;
    Api api = Api::OpenGl;
    GlContext* surfaceContext = nullptr;
    int surfaceWidth = 0;
    int surfaceHeight = 0;
    bool surfaceAttached = false;
    std::string adapterName;
    Capabilities capabilities;
    bool noFloatFiltering = false;
    // Whether each pass ends with a flush, which the driver issues ask of a driver that drops passes otherwise.
    bool flushEachPass = false;
    DebugOutput debug;
    GpuTimer timer;
    std::unique_ptr<UploadRing> ring;
    FramebufferCache framebuffers;
    Gl::Uint uploadVertexArray = 0;

    ObjectTable<BufferRecord> buffers{"buffers"};
    ObjectTable<ImageRecord> images{"images"};
    ObjectTable<Gl::Uint> samplers{"samplers"};
    ObjectTable<std::shared_ptr<ShaderProgram>> programs{"programs"};
    ObjectTable<PipelineRecord> pipelines{"pipelines"};

    std::array<Gl::Sync, kFramesInFlight> frameFences{};
    std::size_t frameIndex = 0;
    bool frameStarted = false;
    bool drewSwapchain = false;
    bool lost = false;
    PassState pass;
    PipelineRecord* applied = nullptr;
    int indexOffset = 0;
    float maxAnisotropy = 1.0F;
    std::vector<std::byte> staging;
};

} // namespace haylen::gpu::opengl
