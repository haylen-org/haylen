#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/gpu/Device.hpp"
#include "sokol_gfx.h"

namespace haylen::sokol {

// The GPU device of the Sokol backend, which draws through `sokol_gfx` on the graphics API the build chose. Sokol keeps one global context, so one device exists at a time. The host hands it the environment of its window and the swapchain of every frame.
class GpuDevice final : public gpu::Device {
  public:
    // Sokol preallocates its resource pools, so these sizes bound how many images, buffers, programs and pipelines exist at once.
    static constexpr int kImagePoolSize = 4096;
    static constexpr int kViewPoolSize = 3 * kImagePoolSize;
    static constexpr int kBufferPoolSize = 4096;
    static constexpr int kSamplerPoolSize = 256;
    static constexpr int kShaderPoolSize = 512;
    static constexpr int kPipelinePoolSize = 2048;

    struct Setup {
        sg_environment environment{};
        sg_logger logger{};
        bool validation = true;
        std::function<sg_swapchain()> swapchain;
    };

    explicit GpuDevice(Setup setup);
    ~GpuDevice() override;

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
    void attachSurface(const gpu::Surface& surface) override;

    [[nodiscard]] std::unique_ptr<Readback> readImage(Image image, int width, int height) override;

  private:
    // A view that renders into one mip level and layer of an image, as a color, a resolve or a depth stencil attachment.
    enum class AttachmentKind : std::uint8_t {
        Color,
        Resolve,
        DepthStencil,
    };

    struct AttachmentView {
        AttachmentKind kind = AttachmentKind::Color;
        int mipLevel = 0;
        int layer = 0;
        sg_view view{};
    };

    // The view Sokol samples an image through and the views it renders into it with, which passes make when they first need them, next to the description of the image.
    struct ImageRecord {
        sg_view texture{};
        std::vector<AttachmentView> attachments;
        ImageDesc desc;
    };

    [[nodiscard]] static double millisecondsSince(std::chrono::steady_clock::time_point start) noexcept;
    [[nodiscard]] static Capabilities queryCapabilities() noexcept;
    [[nodiscard]] static sg_pixel_format toPixelFormat(PixelFormat format) noexcept;
    [[nodiscard]] static sg_vertex_format toVertexFormat(VertexFormat format) noexcept;
    [[nodiscard]] static sg_blend_factor toBlendFactor(BlendFactor factor) noexcept;
    [[nodiscard]] static sg_blend_op toBlendOperation(BlendOperation operation) noexcept;
    [[nodiscard]] static sg_color_mask toColorMask(ColorMask mask) noexcept;
    [[nodiscard]] static sg_filter toFilter(Filter filter) noexcept;
    [[nodiscard]] static sg_wrap toWrap(Wrap wrap) noexcept;
    [[nodiscard]] static sg_shader_stage toStage(Stage stage) noexcept;
    [[nodiscard]] static sg_image_type toImageType(TextureType type) noexcept;
    [[nodiscard]] static sg_image_sample_type toSampleType(SampleType type) noexcept;
    [[nodiscard]] static sg_sampler_type toSamplerType(SamplerType type) noexcept;
    [[nodiscard]] static sg_primitive_type toPrimitive(Primitive primitive) noexcept;
    [[nodiscard]] static sg_index_type toIndexType(IndexType type) noexcept;
    [[nodiscard]] static sg_compare_func toCompare(CompareFunction function) noexcept;
    [[nodiscard]] static sg_stencil_op toStencilOperation(StencilOperation operation) noexcept;
    [[nodiscard]] static sg_stencil_face_state toStencilFace(const StencilFace& face) noexcept;
    [[nodiscard]] static sg_cull_mode toCullMode(CullMode mode) noexcept;
    [[nodiscard]] static sg_load_action toLoadAction(Load load) noexcept;
    [[nodiscard]] static sg_store_action toStoreAction(Store store) noexcept;
    [[nodiscard]] static sg_color toColor(math::Color color) noexcept;

    // Makes the buffer that carries the pixels of dynamic images to them hold at least the bytes, replacing the smaller one.
    void reserveStaging(std::size_t bytes);
    [[nodiscard]] const ImageRecord& getRecord(Image image) const;
    [[nodiscard]] sg_view getAttachment(Image image, AttachmentKind kind, int mipLevel, int layer);
    void describeOffscreenPass(const PassDesc& desc, sg_pass& pass);

    std::function<sg_swapchain()> swapchain;
    Capabilities capabilities;
    std::string debugGroup;
    std::unordered_map<std::uint32_t, ImageRecord> images;
    sg_buffer staging{};
    std::size_t stagingSize = 0;
    std::vector<sg_buffer> retired;
};

} // namespace haylen::sokol
