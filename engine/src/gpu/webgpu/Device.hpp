#pragma once

#include <webgpu/webgpu.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gpu/webgpu/BindGroupCache.hpp"
#include "gpu/webgpu/Connection.hpp"
#include "gpu/webgpu/FrameTimer.hpp"
#include "gpu/webgpu/HandlePool.hpp"
#include "gpu/webgpu/Swapchain.hpp"
#include "gpu/webgpu/UniformRing.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/gpu/Surface.hpp"

namespace haylen::gpu::webgpu {

// The device of WebGPU, which draws through the WebGPU C API of the browser into the canvas of the page. The uniform blocks of a program live in bind group 0 with dynamic offsets into the uniform ring, and its textures and samplers in bind group 1, whose groups the device keeps by what they hold. Queue writes carry buffers and images to the GPU in the order of the frames, and WebGPU frees a destroyed object only once the GPU finished the frames that use it, so the device hands what is destroyed to WebGPU once the frame that may still record it is submitted.
class Device final : public gpu::Device {
  public:
    // Creates the device on the connection that `Connection::request` made ready, drawing into the canvas of the surface, or offscreen when there is none. Throws `std::runtime_error` with the reason when WebGPU cannot start, so the platform tries the next graphics API.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(const std::optional<Surface>& surface);

    Device(std::unique_ptr<Connection> owned, const std::optional<Surface>& surface);
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
    void applyBindings(const Bindings& value) override;
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
        WGPUBuffer buffer = nullptr;
        std::size_t size = 0;
    };

    // The texture of an image, the view that draws sample it through and the views that passes draw into it through, by mip level and layer.
    struct ImageRecord {
        WGPUTexture texture = nullptr;
        WGPUTextureView sampled = nullptr;
        std::vector<std::pair<std::uint32_t, WGPUTextureView>> attachments;
        TextureType type = TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int sampleCount = 1;
        PixelFormat format = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
    };

    struct SamplerRecord {
        WGPUSampler sampler = nullptr;
    };

    // The uniform blocks that the programs of one layout of bind group 0 hold, by binding number and size, with the bind group of every chunk of the uniform ring.
    struct BlockLayout {
        WGPUBindGroupLayout layout = nullptr;
        std::vector<std::pair<std::uint32_t, std::uint32_t>> blocks;
        std::vector<WGPUBindGroup> groups;
    };

    // A shader module made from one source text, which the programs that run it share.
    struct ShaderModule {
        WGPUShaderModule module = nullptr;
        int programs = 0;
    };

    // A program keeps its uniform blocks in the order of their binding numbers, which is the order of their dynamic offsets.
    struct ProgramRecord {
        const std::string* vertexSource = nullptr;
        const std::string* fragmentSource = nullptr;
        WGPUShaderModule vertex = nullptr;
        WGPUShaderModule fragment = nullptr;
        std::string vertexEntry;
        std::string fragmentEntry;
        BlockLayout* blocks = nullptr;
        WGPUBindGroupLayout images = nullptr;
        WGPUPipelineLayout layout = nullptr;
        std::array<int, kMaxBlocks> blockSlots{};
        std::uint32_t blockCount = 0;
        std::vector<TextureBinding> textures;
        std::vector<SamplerBinding> samplers;
    };

    // What a pipeline of the swapchain is made from, kept so that a surface of another format or sample count makes it again.
    struct SwapchainPipeline {
        Program program;
        std::vector<VertexBufferLayout> buffers;
        std::vector<VertexAttribute> attributes;
        Primitive primitive = Primitive::Triangles;
        IndexType indexType = IndexType::None;
        std::vector<ColorTarget> colors;
        DepthStencil depthStencil;
        CullMode cullMode = CullMode::None;
        FrontFace frontFace = FrontFace::CounterClockwise;
        bool alphaToCoverage = false;
        std::string label;

        [[nodiscard]] static SwapchainPipeline from(const PipelineDesc& desc);
        [[nodiscard]] PipelineDesc describe() const noexcept;
    };

    struct PipelineRecord {
        WGPURenderPipeline pipeline = nullptr;
        std::uint32_t program = 0;
        WGPUIndexFormat indexFormat = WGPUIndexFormat_Undefined;
        std::optional<SwapchainPipeline> swapchain;
    };

    // The bytes of the uniform block at a bind slot in the uniform ring, which a slot the frame did not fill leaves empty.
    struct UniformBlock {
        std::uint32_t chunk = 0;
        std::uint32_t offset = 0;
        std::uint32_t size = 0;
    };

    // An object that WebGPU destroys or forgets once the frame that may still record it is submitted.
    struct Retired {
        enum class Kind : std::uint8_t {
            Buffer,
            Texture,
            TextureView,
            Sampler,
            Pipeline,
            ShaderModule,
        };

        Kind kind = Kind::Buffer;
        void* handle = nullptr;
    };

    // What the pass encoder holds, so that state it already holds is never sent again.
    struct BoundState {
        WGPURenderPipeline pipeline = nullptr;
        std::array<WGPUBuffer, kMaxVertexBuffers> vertexBuffers{};
        std::array<int, kMaxVertexBuffers> vertexOffsets{};
        WGPUBuffer indexBuffer = nullptr;
        int indexOffset = 0;
        WGPUIndexFormat indexFormat = WGPUIndexFormat_Undefined;
        WGPUBindGroup blockGroup = nullptr;
        std::array<std::uint32_t, kMaxBlocks> blockOffsets{};
        WGPUBindGroup imageGroup = nullptr;
    };

    [[nodiscard]] static Capabilities describeCapabilities(const Connection& connection);
    [[nodiscard]] static std::uint64_t alignTo(std::uint64_t value, std::uint64_t alignment) noexcept;

    void validateImage(const ImageDesc& desc) const;
    void writeLevel(WGPUTexture texture, int level, int width, int height, int depth, PixelFormat format, std::span<const std::byte> pixels);
    [[nodiscard]] WGPUTextureView getAttachmentView(ImageRecord& image, int mipLevel, int layer);
    [[nodiscard]] WGPUShaderModule acquireModule(std::string_view source, std::string_view label, const std::string*& key);
    void releaseModule(const std::string* key) noexcept;
    [[nodiscard]] BlockLayout& getBlockLayout(std::span<const BlockBinding> blocks);
    [[nodiscard]] WGPUBindGroupLayout getImageLayout(std::span<const TextureBinding> textures, std::span<const SamplerBinding> samplers);
    [[nodiscard]] WGPUPipelineLayout getPipelineLayout(WGPUBindGroupLayout blocks, WGPUBindGroupLayout images);
    [[nodiscard]] WGPUBindGroup getBlockGroup(BlockLayout& layout, std::uint32_t chunk);
    [[nodiscard]] WGPUBindGroup makeImageGroup(const ProgramRecord& program, const BindGroupCache::Key& key);
    [[nodiscard]] WGPURenderPipeline createPipeline(const PipelineDesc& desc);
    void rebuildSwapchainPipelines();

    void openEncoder();
    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    void carryUniforms() noexcept;
    void bindBlocks(const ProgramRecord& program);
    void bindImages(const ProgramRecord& program);
    void bindIndexBuffer(WGPUIndexFormat format);
    void retire(Retired::Kind kind, void* handle) noexcept;
    void releaseRetired() noexcept;
    void releaseImage(ImageRecord& image) noexcept;

    std::unique_ptr<Connection> connection;
    WGPUDevice device;
    WGPUQueue queue;
    Capabilities capabilities;
    std::unique_ptr<Swapchain> swapchain;
    PixelFormat swapchainFormat = PixelFormat::Bgra8;
    int swapchainSamples = 1;
    UniformRing ring;
    std::unique_ptr<FrameTimer> timer;
    BindGroupCache imageGroups;

    HandlePool<BufferRecord> buffers{"buffer"};
    HandlePool<ImageRecord> images{"image"};
    HandlePool<SamplerRecord> samplers{"sampler"};
    HandlePool<ProgramRecord> programs{"program"};
    HandlePool<PipelineRecord> pipelines{"pipeline"};
    std::unordered_map<std::string, ShaderModule> modules;
    std::map<std::vector<std::uint64_t>, std::unique_ptr<BlockLayout>> blockLayouts;
    std::map<std::vector<std::uint64_t>, WGPUBindGroupLayout> imageLayouts;
    std::map<std::pair<WGPUBindGroupLayout, WGPUBindGroupLayout>, WGPUPipelineLayout> pipelineLayouts;
    std::vector<Retired> retired;
    std::vector<std::byte> padded;

    WGPUCommandEncoder encoder = nullptr;
    WGPURenderPassEncoder pass = nullptr;
    bool skipping = false;
    int passWidth = 0;
    int passHeight = 0;
    int encoderGroups = 0;
    int passGroups = 0;
    std::uint32_t currentPipeline = 0;
    Bindings bindings;
    std::array<UniformBlock, kMaxBlocks> uniforms{};
    bool blocksDirty = true;
    bool imagesDirty = true;
    BoundState bound;
    std::uint64_t frame = 0;
};

} // namespace haylen::gpu::webgpu
