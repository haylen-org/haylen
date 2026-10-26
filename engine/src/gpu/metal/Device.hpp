#pragma once

#include <memory>
#include <optional>

#include "gpu/DeviceFactory.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::metal {

// The device of Metal on macOS, iOS, iPadOS, Mac Catalyst, tvOS and visionOS. It records each frame into one command buffer that holds no references, keeps up to `kFramesInFlight` frames on the GPU behind a semaphore, and releases destroyed objects once the frames that may use them finished. Uniforms and the pixels of dynamic images pass through an upload ring of each frame in shared memory, dynamic buffers keep a region for each frame in flight, and the encoder of a pass receives only the state that changes. Swapchain passes draw into the drawables of the `CAMetalLayer` of the surface, with their multisampled color and their depth and stencil in tile memory on Apple GPUs. Its Metal objects live in the source file, so the device factory includes this header from C++.
class Device final : public gpu::Device {
  public:
    static constexpr int kFramesInFlight = 3;

    // Creates the device of the GPU that shows the layer of the surface, or of the default GPU to draw offscreen without one. Throws `std::runtime_error` when the machine has no Metal GPU or the surface holds no `CAMetalLayer`.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(const std::optional<Surface>& surface, const DeviceFactory::Options& options);

    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
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
    struct Native;

    explicit Device(std::unique_ptr<Native> state);

    void requirePass(const char* call) const;
    void openFrame();
    void endPendingPass();
    void submitTransfers();
    void reportErrors();
    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    void uploadLevels(Image image, std::span<const std::span<const std::byte>> levels);
    void closeDebugGroups(std::uint32_t owner);

    std::unique_ptr<Native> native;
};

} // namespace haylen::gpu::metal
