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

#include <volk.h>

#include <vk_mem_alloc.h>

#include "gpu/DeviceFactory.hpp"
#include "gpu/vulkan/DescriptorCache.hpp"
#include "gpu/vulkan/HandlePool.hpp"
#include "gpu/vulkan/LayoutCache.hpp"
#include "gpu/vulkan/ReadbackQueue.hpp"
#include "gpu/vulkan/ReleaseQueue.hpp"
#include "gpu/vulkan/RenderPassCache.hpp"
#include "gpu/vulkan/Swapchain.hpp"
#include "gpu/vulkan/UploadRing.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::vulkan {

class Context;
class Instance;
class PreparedModules;

// The device of Vulkan 1.1 on Windows, Linux and Android. It records each frame into a command buffer of uploads and one of passes, with dynamic rendering where the GPU has it and render passes otherwise, and keeps a fixed number of frames in flight behind fences, releasing what `destroy` lets go of once those frames finished. Uniforms take dynamic offsets into a ring that stays mapped, dynamic buffers rotate through a copy for each frame in flight, and the descriptor sets of textures and samplers are made once for each choice of them. Its viewport flips the vertical axis, so clip space and the rows of render targets match Metal and Direct3D.
class Device final : public gpu::Device {
  public:
    static constexpr int kFramesInFlight = 2;

    struct Settings {
        // Turns on the validation layers when they are installed, whose messages become errors of the engine log.
        bool validation = false;

        // Draws with dynamic rendering where the GPU has it. Turned off, the device records render passes on every GPU, which tests use to run that path where dynamic rendering exists.
        bool dynamicRendering = true;
    };

    [[nodiscard]] static std::unique_ptr<gpu::Device> create(const std::optional<Surface>& surface, const DeviceFactory::Options& options);

    // Draws into the surface, or offscreen when there is none. Throws `std::runtime_error` when Vulkan cannot start on this machine.
    Device(const std::optional<Surface>& surface, const Settings& settings);
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

    // Whether the device draws its passes with dynamic rendering rather than render passes.
    [[nodiscard]] bool isRenderingDynamically() const noexcept {
        return dynamicRendering;
    }

  private:
    static constexpr VkDeviceSize kStagingChunkSize = VkDeviceSize{8} * 1024 * 1024;
    static constexpr VkDeviceSize kUniformChunkSize = VkDeviceSize{1} * 1024 * 1024;
    static constexpr VkDeviceSize kDynamicBufferAlignment = 256;
    static constexpr std::uint32_t kNoSet = 0xFFFFFFFFU;

    struct Frame {
        VkCommandPool pool = VK_NULL_HANDLE;
        VkCommandBuffer uploads = VK_NULL_HANDLE;
        VkCommandBuffer graphics = VK_NULL_HANDLE;
        VkFence fence = VK_NULL_HANDLE;
        VkSemaphore acquired = VK_NULL_HANDLE;
        VkQueryPool timestamps = VK_NULL_HANDLE;
        std::uint64_t serial = 0;
        bool submitted = false;
    };

    // A buffer, which a dynamic one holds once for each frame in flight at the stride, and whose current copy changes with the first write of a frame.
    struct BufferRecord {
        VkBuffer buffer = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        std::byte* mapped = nullptr;
        VkDeviceSize size = 0;
        VkDeviceSize stride = 0;
        std::uint32_t copies = 1;
        std::uint32_t current = 0;
        std::uint64_t writtenSerial = 0;
        Usage usage = Usage::Immutable;
    };

    struct AttachmentView {
        int mipLevel = 0;
        int layer = 0;
        VkImageView view = VK_NULL_HANDLE;
    };

    // An image, which rests in one layout between the passes and copies that move it out of it.
    struct ImageRecord {
        VkImage image = VK_NULL_HANDLE;
        VmaAllocation allocation = VK_NULL_HANDLE;
        VkImageView sampleView = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        PixelFormat pixelFormat = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
        TextureType type = TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkImageLayout restLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkImageAspectFlags aspects = VK_IMAGE_ASPECT_COLOR_BIT;
        std::vector<AttachmentView> attachmentViews;
    };

    struct SamplerRecord {
        VkSampler sampler = VK_NULL_HANDLE;
    };

    struct SlotBinding {
        int slot = 0;
        std::uint32_t binding = 0;
    };

    // A program, whose record stays while pipelines that draw into the swapchain may make themselves again with its modules after its handle was destroyed.
    struct ProgramRecord {
        VkShaderModule vertex = VK_NULL_HANDLE;
        VkShaderModule fragment = VK_NULL_HANDLE;
        std::string vertexEntry;
        std::string fragmentEntry;
        std::string label;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        VkDescriptorSetLayout uniformLayout = VK_NULL_HANDLE;
        std::uint32_t uniformSet = kNoSet;
        VkDescriptorSetLayout resourceLayout = VK_NULL_HANDLE;
        std::uint32_t resourceSet = kNoSet;
        std::vector<DescriptorCache::UniformBlock> uniformBlocks;
        std::array<int, kMaxBlocks> blockIndices{};
        std::vector<SlotBinding> textures;
        std::vector<SlotBinding> samplers;
        int pipelines = 0;
        bool destroyed = false;
    };

    // The description of a pipeline that draws into the swapchain, kept so the pipeline makes itself again when the swapchain changes its format or sample count.
    struct StoredPipeline {
        std::vector<VertexBufferLayout> buffers;
        std::vector<VertexAttribute> attributes;
        std::vector<ColorTarget> colors;
        std::string label;
        PipelineDesc desc;
    };

    struct PipelineRecord {
        VkPipeline pipeline = VK_NULL_HANDLE;
        std::uint32_t program = 0;
        VkPipelineLayout layout = VK_NULL_HANDLE;
        IndexType indexType = IndexType::None;
        std::uint32_t bufferCount = 0;
        std::uint32_t generation = 0;
        std::unique_ptr<StoredPipeline> stored;
    };

    // The color format, depth format and sample count of the swapchain that pipelines drawing into it take, whose generation changes with any of them.
    struct SwapchainTarget {
        VkFormat color = VK_FORMAT_B8G8R8A8_UNORM;
        VkFormat depth = VK_FORMAT_D24_UNORM_S8_UINT;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        std::uint32_t generation = 0;
    };

    struct AttachmentTarget {
        VkImageView view = VK_NULL_HANDLE;
        VkFormat format = VK_FORMAT_UNDEFINED;
        VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
        VkAttachmentLoadOp load = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp store = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkAttachmentLoadOp stencilLoad = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        VkAttachmentStoreOp stencilStore = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        VkClearValue clear{};
        VkImageView resolveView = VK_NULL_HANDLE;
        VkFormat resolveFormat = VK_FORMAT_UNDEFINED;
    };

    // The attachments of a pass as both ways of recording passes take them.
    struct PassTarget {
        std::array<AttachmentTarget, kMaxColorTargets> colors{};
        std::uint32_t colorCount = 0;
        AttachmentTarget depth;
        bool hasDepth = false;
        VkExtent2D extent{};
    };

    // A part of an image that a pass moves out of its resting layout, and back once the pass ends.
    struct Transition {
        VkImage image = VK_NULL_HANDLE;
        VkImageSubresourceRange range{};
        VkImageLayout attachmentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkImageLayout fromLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout restLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    };

    struct PassState {
        bool open = false;
        bool skipped = false;
        bool swapchain = false;
        bool labeled = false;
        VkExtent2D extent{};
        std::array<Transition, RenderPassCache::kMaxViews> transitions{};
        std::size_t transitionCount = 0;
    };

    // What the draws of a pass bound so far, so a draw sends only what changed.
    struct DrawState {
        PipelineRecord* pipeline = nullptr;
        ProgramRecord* program = nullptr;
        VkPipeline boundPipeline = VK_NULL_HANDLE;
        Bindings bindings;
        bool vertexDirty = true;
        bool indexDirty = true;
        bool resourcesDirty = true;
        bool uniformsDirty = true;
        VkBuffer uniformBuffer = VK_NULL_HANDLE;
        VkDescriptorSetLayout uniformSetLayout = VK_NULL_HANDLE;
        VkDescriptorSet uniformSet = VK_NULL_HANDLE;
        std::uint32_t uniformMask = 0;
        std::array<std::uint32_t, kMaxBlocks> uniformOffsets{};
        std::array<const std::byte*, kMaxBlocks> uniformData{};
    };

    void createFrames();
    void fillCapabilities();
    void shutdown() noexcept;

    [[nodiscard]] bool ensureFrame();
    void completeFrame(Frame& frame);
    void pollFrames();
    void markLost(VkResult result);
    void waitIdle();

    void reconfigureSwapchain();
    void updateSwapchainTarget();
    [[nodiscard]] bool acquireSwapchainImage();

    [[nodiscard]] BufferRecord& requireBuffer(Buffer buffer) const;
    [[nodiscard]] ImageRecord& requireImage(Image image) const;
    [[nodiscard]] VkFormat resolveFormat(PixelFormat format) const noexcept;
    [[nodiscard]] VkImageView getAttachmentView(ImageRecord& record, int mipLevel, int layer);
    void uploadLevels(const ImageRecord& record, std::span<const std::span<const std::byte>> levels);
    void clearImage(const ImageRecord& record);
    void uploadBuffer(const BufferRecord& record, std::span<const std::byte> data);
    void releaseImage(ImageRecord& record, std::uint64_t serial);
    void releaseProgram(ProgramRecord& record, std::uint64_t serial);
    void forgetProgramUse(std::uint32_t program, std::uint64_t serial) noexcept;

    [[nodiscard]] VkPipeline buildPipeline(const PipelineDesc& desc, const ProgramRecord& program);
    [[nodiscard]] static std::unique_ptr<StoredPipeline> storePipeline(const PipelineDesc& desc);
    void rebuildPipeline(PipelineRecord& record);

    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    void addTransition(const ImageRecord& record, int mipLevel, int layer, bool keeps);
    void beginRendering(const PassTarget& target);
    void endRendering();
    void endSwapchainRendering();
    void resetDrawState() noexcept;
    void bindVertexBuffers();
    void bindIndexBuffer();
    void bindResources();
    void bindUniforms();
    [[nodiscard]] UploadRing::Allocation allocateUniform(std::size_t index, VkDeviceSize size);

    std::unique_ptr<Instance> instance;
    std::unique_ptr<Swapchain> swapchain;
    std::unique_ptr<Context> context;
    ReleaseQueue releases;
    LayoutCache layouts;
    DescriptorCache descriptors;
    RenderPassCache renderPasses;
    UploadRing staging;
    UploadRing uniforms;
    ReadbackQueue readbacks;
    std::shared_ptr<PreparedModules> prepared;
    const VolkDeviceTable& vk;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;
    PFN_vkCmdBeginRendering beginRenderingCommand = nullptr;
    PFN_vkCmdEndRendering endRenderingCommand = nullptr;
    bool dynamicRendering = false;
    bool labels = false;
    Capabilities capabilities;

    HandlePool<BufferRecord> buffers;
    HandlePool<ImageRecord> images;
    HandlePool<SamplerRecord> samplers;
    HandlePool<ProgramRecord> programs;
    HandlePool<PipelineRecord> pipelines;

    std::array<Frame, kFramesInFlight> frames{};
    std::size_t frameIndex = 0;
    bool frameOpen = false;
    bool bufferCopies = false;
    std::uint64_t nextSerial = 1;
    std::uint64_t completedSerial = 0;
    std::optional<double> gpuFrameSeconds;
    bool lost = false;

    Swapchain::Settings surfaceSettings;
    SwapchainTarget swapchainTarget;
    bool swapchainDirty = false;
    bool acquired = false;
    std::uint32_t imageIndex = 0;
    bool swapchainDrawn = false;
    bool swapchainRendering = false;

    PassState pass;
    DrawState bound;
    std::string labelText;
    std::vector<VkImageMemoryBarrier> imageBarriers;
    std::vector<VkBufferImageCopy> copyRegions;
    std::vector<VkBuffer> stagingSources;
    std::vector<VkWriteDescriptorSet> descriptorWrites;
    std::vector<VkDescriptorImageInfo> descriptorImages;
};

} // namespace haylen::gpu::vulkan
