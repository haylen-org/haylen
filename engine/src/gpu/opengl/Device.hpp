#pragma once

#include <array>
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

#include "gpu/BufferRules.hpp"
#include "gpu/DeviceFactory.hpp"
#include "gpu/HandleTable.hpp"
#include "gpu/ImageInfo.hpp"
#include "gpu/ImageRules.hpp"
#include "gpu/opengl/Context.hpp"
#include "gpu/opengl/Conversions.hpp"
#include "gpu/opengl/DebugOutput.hpp"
#include "gpu/opengl/DrawnClear.hpp"
#include "gpu/opengl/DrawnResolve.hpp"
#include "gpu/opengl/FramebufferCache.hpp"
#include "gpu/opengl/GpuTimer.hpp"
#include "gpu/opengl/ScaledDrawable.hpp"
#include "gpu/opengl/ShaderProgram.hpp"
#include "gpu/opengl/UniformRing.hpp"
#include "gpu/opengl/UploadRing.hpp"
#include "gpu/opengl/VertexArray.hpp"
#if !defined(__EMSCRIPTEN__)
#include "gpu/renderdoc/Capturer.hpp"
#endif
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::opengl {

// The device of OpenGL 4.3 core on Windows and Linux, OpenGL ES 3.0 on Android and WebGL2 on the web, one class that knows its flavour. It draws through the context the platform created with the window, which stays current on the frame thread, and keeps every object as long as that context lives. The surfaces beyond the window are other drawables of that same context, which the device makes current for the swapchain passes that draw into them and presents as a pass leaves them, so every object serves every surface. The first row of its render targets is their bottom row. Each uniform block reads the range of a buffer bound to the binding point of its slot: a uniform buffer of the bindings, or the uniform ring of the frame, which holds the bytes that `applyUniforms` gives. OpenGL orders the writes into a buffer and the copies between buffers with the draws around them, so a dynamic or a stream buffer is one buffer whose writes and copies take their place among the draws of the frame, and a readback of a buffer copies its range into a buffer that the CPU reads behind a fence. Every image that is no multisampled renderbuffer is a texture of immutable storage, whose mip levels and layers take the writes of dynamic images through the pixel unpack buffer, whose levels `glGenerateMipmap` fills in their place among the draws, and whose views are textures of `glTextureView` that share its storage. OpenGL keeps an object that a queued command still uses until the GPU finished the command, so `destroy` deletes objects at once.
class Device final : public gpu::Device {
  public:
    static constexpr std::size_t kFramesInFlight = 3;

    // Makes the device of the flavour on the context of the surface, which must be a `Surface::Kind::Gl` surface, since OpenGL draws nowhere without the context of a window or of an offscreen drawable that the platform or a test made. Throws `std::runtime_error` when the context cannot run the flavour.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(Api flavour, const std::optional<Surface>& surface, const DeviceOptions& options);

    Device(Api flavour, const Surface& surface, bool validation);
    ~Device() override;

    [[nodiscard]] Language getLanguage() const noexcept override;
    [[nodiscard]] Api getApi() const noexcept override;
    [[nodiscard]] const Capabilities& getCapabilities() const noexcept override;
    [[nodiscard]] std::string getAdapterName() const override;
    [[nodiscard]] std::optional<double> getGpuFrameSeconds() const noexcept override;

    // OpenGL, OpenGL ES and WebGL2 tell nothing of when a display shows a frame, so the device knows no age of the input.
    [[nodiscard]] std::optional<double> getInputAge() const noexcept override {
        return std::nullopt;
    }
    void readPools(std::vector<Pool>& pools) const override;
    [[nodiscard]] std::size_t getSpareBytes() const noexcept override;
    [[nodiscard]] bool isLost() const noexcept override;

    [[nodiscard]] Buffer makeBuffer(const BufferDesc& desc) override;
    [[nodiscard]] Image makeImage(const ImageDesc& desc) override;
    [[nodiscard]] Sampler makeSampler(const SamplerDesc& desc) override;
    [[nodiscard]] Program makeProgram(const ProgramDesc& desc) override;
    [[nodiscard]] Pipeline makePipeline(const PipelineDesc& desc) override;
    [[nodiscard]] Image makeView(const ViewDesc& desc) override;
    void prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) override;

    void destroy(Buffer buffer) noexcept override;
    void destroy(Image image) noexcept override;
    void destroy(Sampler sampler) noexcept override;
    void destroy(Program program) noexcept override;
    void destroy(Pipeline pipeline) noexcept override;

    void writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) override;
    void copyBuffer(Buffer source, std::size_t sourceOffset, Buffer destination, std::size_t destinationOffset, std::size_t size) override;
    void writeImages(std::span<const ImageWrite> writes) override;
    void generateMipmaps(Image image) override;

    void beginPass(const PassDesc& desc) override;

    [[nodiscard]] int getPassTurns() const noexcept override {
        return 0;
    }

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
    void waitForDisplay() override;

    // OpenGL has no clock of the display that calls the app, so the platform plays the frames itself.
    [[nodiscard]] bool paceWindow(int framesPerSecond, std::function<void()> due) override;

    // Makes the upload ring and the uniform ring as small as their last frames needed, which is all that the device keeps for later: OpenGL frees the memory of each object that the device deletes, and WebGL2 hands the writes into buffers and images to the browser without an upload ring.
    void trim() override;

    void resizeSurface(const Surface::Size& size) override;
    void detachSurface() override;
    void attachSurface(const Surface& surface) override;
    [[nodiscard]] int addSurface(const Surface& added) override;
    void resizeSurface(int number, const Surface::Size& size) override;
    void removeSurface(int number) override;

    [[nodiscard]] std::unique_ptr<Readback> readImage(Image image, int width, int height) override;
    [[nodiscard]] std::unique_ptr<Readback> readBuffer(Buffer buffer, std::size_t offset, std::size_t size) override;

    [[nodiscard]] CaptureSupport getCaptureSupport() const override;
    void beginCapture(const std::filesystem::path& path) override;
    [[nodiscard]] std::filesystem::path endCapture() override;

  private:
    static constexpr Gl::Uint64 kWaitStep = 100'000'000;
    static constexpr Gl::Uint64 kFrameTimeout = 10'000'000'000;
    static constexpr std::string_view kMissingWebCaptureTool = "Browsers give a page no capture tool for WebGL 2, so the engine cannot capture its frames. Capture them on a desktop, where RenderDoc captures OpenGL, Vulkan and Direct3D and Metal captures its own.";

    struct BufferRecord {
        Gl::Uint name = 0;
        BufferRules::Description description{};
    };

    // A multisampled image is a renderbuffer, whose target is `Gl::kRenderbuffer`, and the others are textures, with the format they are stored in. An image keeps the ids of its views, which go with it.
    struct ImageRecord {
        Gl::Uint name = 0;
        Gl::Enum target = Gl::kTexture2d;
        Conversions::Pixel pixel;
        ImageRules::Description description;
        std::vector<std::uint64_t> views{};
    };

    struct PipelineRecord {
        std::shared_ptr<ShaderProgram> shader;
        std::unique_ptr<VertexArray> vertexArray{};
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
        int surface = 0;
    };

    // A drawable beyond the window that the context of the device draws into, with the size of its pixels and of its window, the framebuffer that it draws into while its window shows it larger, whether the frame drew into it and whether the frame presented it.
    struct Drawable {
        GlContext* context = nullptr;
        Surface::Size size;
        std::optional<Surface::Size> nextSize{};
        std::unique_ptr<ScaledDrawable> scaled{};
        bool drawn = false;
        bool presented = false;
    };

    [[nodiscard]] static std::shared_ptr<Context> makeContext(Api flavour, const Surface& surface);
    [[nodiscard]] static Language toLanguage(Api flavour) noexcept;
    [[nodiscard]] static std::uintptr_t toAddress(const void* pointer) noexcept;
    [[nodiscard]] static std::size_t readUniformAlignment(const Gl& functions);
    static void copyPixels(std::byte* destination, std::span<const std::byte> source, bool swapsRedBlue) noexcept;
    [[nodiscard]] StateCache::Rectangle toWindow(const math::Rect& pixels) const noexcept;

    void applyDriverIssues();
    void readCapabilities();
    [[nodiscard]] bool hasComputeVersion() const noexcept;
    [[nodiscard]] bool isRenderable(const Conversions::Pixel& pixel) const noexcept;
    [[nodiscard]] bool isSampled(PixelFormat format) const noexcept;
    [[nodiscard]] bool isStorage(PixelFormat format) const noexcept;
    [[nodiscard]] std::uint8_t getSampleCounts(const Conversions::Pixel& pixel) const;
    [[nodiscard]] Gl::Uint makeRenderbuffer(const ImageDesc& desc, const Conversions::Pixel& pixel);
    [[nodiscard]] Gl::Uint makeTexture(const ImageDesc& desc, const Conversions::Pixel& pixel, Gl::Enum target);
    void uploadTexture(const ImageDesc& desc, const ImageRecord& record);
    [[nodiscard]] std::size_t checkImageWrites(std::span<const ImageWrite> writes);
    void uploadImagesFromMemory(std::span<const ImageWrite> writes);
    void uploadImagesThroughRing(std::span<const ImageWrite> writes, std::size_t total);
    void uploadLevel(const ImageRecord& record, int level, std::uintptr_t source);
    void uploadLayers(const ImageRecord& record, Gl::Enum target, int level, const ImageInfo::Extent& extent, std::uintptr_t source);
    void uploadRegion(const ImageRecord& record, const ImageWrite& write, std::uintptr_t source);
    void deleteImage(const ImageRecord& record) noexcept;
    [[nodiscard]] FramebufferCache::Attachment toAttachment(Image image, int mipLevel, int layer);
    [[nodiscard]] Gl::Uint getSingleFramebuffer(Image image, int mipLevel, int layer, std::string_view label);
    [[nodiscard]] StateCache::Raster toRaster(const PipelineDesc& desc) const;

    void beginFrame();
    void applySizes();
    void waitForFrame(std::size_t slot);
    void requirePass() const;
    void bindApplied();
    void bindUniformBuffers(const Bindings& bindings);
    [[nodiscard]] Drawable& requireDrawable(int number);
    void makeDrawableCurrent(int number, GlContext& drawable);
    void scaleDrawable(std::unique_ptr<ScaledDrawable>& scaled, const Surface::Size& size);
    void presentDrawable(int number, GlContext& drawable, ScaledDrawable* scaled);
    void requireUnpresented(int surface);
    void leaveDrawable(int next);
    void presentCurrentDrawable();
    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    void loadAttachments();
    void clearByDrawing();
    [[nodiscard]] bool isClearedByDrawing(const ColorAttachment& color);
    [[nodiscard]] bool isDepthStencilClearedByDrawing();
    [[nodiscard]] bool isLayerDrawnInto(Gl::Enum target, int layer) const noexcept;
    void resolveAttachments();
    void discardAttachments();
    void checkLost();

    std::shared_ptr<Context> context;
    Gl& gl;
    StateCache& state;
    Api api = Api::OpenGl;
    GlContext* surfaceContext = nullptr;
    Surface::Size surfaceSize;
    std::optional<Surface::Size> nextSurfaceSize;
    int surfaceSamples = 1;
    std::unique_ptr<ScaledDrawable> surfaceScaled;
    bool surfaceAttached = false;
    bool surfaceVsync = true;

    // The drawables of the surfaces beyond the window at their numbers less one, with an empty place for a surface that went away, and the surface whose drawable is current.
    std::vector<Drawable> drawables;
    int currentSurface = 0;
    std::string adapterName;
    Capabilities capabilities;
    bool noFloatFiltering = false;
    // Whether samplers filter anisotropically where the context can, which the driver issues turn off for a driver whose anisotropic filtering ignores the slopes of a lookup.
    bool anisotropic = true;
    // Whether each pass ends with a flush, which the driver issues ask of a driver that drops passes otherwise.
    bool flushEachPass = false;
    // Whether textures of 32-bit float depth take a stencil plane, which the driver issues ask of a driver whose lookups read 32-bit float depth in other places than its draws write it.
    bool floatDepthWithStencil = false;
    DebugOutput debug;
    GpuTimer timer;
    std::unique_ptr<UploadRing> ring;
    UniformRing uniformRing;
    FramebufferCache framebuffers;
    // Whether a pass clears its attachments at a later layer of an array, a cube array or a volume and resolves into such a layer by drawing, which the driver issues ask of a driver whose clears and blits reach only the first layer.
    bool laterLayersByDrawing = false;
    // Whether a pass clears its multisampled attachments by drawing, which the driver issues ask of a driver whose clears reach only one sample.
    bool samplesClearedByDrawing = false;
    // The clear by drawing, which the device makes only for a driver whose issues ask for it.
    std::unique_ptr<DrawnClear> drawnClear;
    // The resolve by drawing into a later layer, which the device makes only for a driver whose issues ask for it.
    std::unique_ptr<DrawnResolve> drawnResolve;
    Gl::Uint uploadVertexArray = 0;

    HandleTable<BufferRecord> buffers{"buffer"};
    HandleTable<ImageRecord> images{"image"};
    HandleTable<Gl::Uint> samplers{"sampler"};
    HandleTable<std::shared_ptr<ShaderProgram>> programs{"program"};
    HandleTable<PipelineRecord> pipelines{"pipeline"};

    std::array<Gl::Sync, kFramesInFlight> frameFences{};
    std::size_t frameIndex = 0;
    bool frameStarted = false;
    bool drewSwapchain = false;
    bool presentedSwapchain = false;

    // Whether a frame presented a drawable since the platform last waited for the display.
    bool presentedSinceWait = false;
    PassState pass;
#if !defined(__EMSCRIPTEN__)
    // The in-application API of RenderDoc when RenderDoc launched the app, which captures its frames.
    std::unique_ptr<renderdoc::Capturer> renderDoc = renderdoc::Capturer::find();
#endif
    PipelineRecord* applied = nullptr;
    int indexOffset = 0;

    // The uniform blocks of the applied pipeline that `applyUniforms` fed since the pipeline, and those that uniform buffers of the bindings feed, as bits of their slots.
    std::uint32_t appliedBlocks = 0;
    std::uint32_t bufferBlocks = 0;

    // The texture and the sampler of each unit that the bindings of the applied pipeline name.
    struct DrawUnit {
        Gl::Enum target = Gl::kTexture2d;
        Gl::Uint texture = 0;
        Gl::Uint sampler = 0;
    };

    std::array<DrawUnit, StateCache::kMaxUnits> drawUnits{};
    std::size_t appliedUnits = 0;

    // The pixels of the textures and of the dynamic images whose red and blue the device swaps before it uploads them.
    std::vector<std::byte> staging;
};

} // namespace haylen::gpu::opengl
