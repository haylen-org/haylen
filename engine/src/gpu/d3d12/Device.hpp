#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <wrl/wrappers/corewrappers.h>

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
#include "gpu/ImageRules.hpp"
#include "gpu/d3d12/BufferArena.hpp"
#include "gpu/d3d12/DescriptorHeap.hpp"
#include "gpu/d3d12/DescriptorRing.hpp"
#include "gpu/d3d12/FrameTimer.hpp"
#include "gpu/d3d12/MipmapGenerator.hpp"
#include "gpu/d3d12/PixCapturer.hpp"
#include "gpu/d3d12/RootSignatures.hpp"
#include "gpu/d3d12/UploadAllocator.hpp"
#include "gpu/direct3d/DebugMessages.hpp"
#include "gpu/direct3d/Swapchain.hpp"
#include "gpu/renderdoc/Capturer.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {
class ShaderCompiler;
}

namespace haylen::gpu::d3d12 {

// The device of Direct3D 12, on GPUs of feature level 11_0 and later. Each frame records a transfer list, which takes the data of new buffers and images, the writes of stream buffers and images and the copies out of render targets and buffers, and the list of its passes, which also takes the writes into dynamic buffers and the copies between buffers at their places between the passes, and the two lists run in that order. The GPU runs at most `kFramesInFlight` frames behind the CPU, and a fence tells when the memory, descriptors and objects of a frame can be used again or released. Uniforms are root constant buffer views into upload memory that stays mapped or into the uniform buffers of the bindings, textures and samplers are descriptor tables copied into rings of shader visible heaps, views of images are shader resource views of ranges of their subresources, and passes are render passes. Images rest in the state that shaders read, and passes, copies and the generation of mip levels move only the subresources they use out of it and back, so a pass samples the mip levels and layers of an image that it does not draw into. Validation turns on the debug layer and the breadcrumbs that say what the GPU ran when it was removed.
class Device final : public gpu::Device {
  public:
    static constexpr std::size_t kFramesInFlight = 2;

    // Starts the device on the first GPU that runs it, drawing into the surface or offscreen. Throws `std::runtime_error` saying why when no GPU of the machine runs it.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(const std::optional<Surface>& target, const DeviceOptions& options);

    Device(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> card, const std::optional<Surface>& target, bool validation);
    ~Device() override;

    [[nodiscard]] Language getLanguage() const noexcept override;
    [[nodiscard]] Api getApi() const noexcept override;
    [[nodiscard]] const Capabilities& getCapabilities() const noexcept override;
    [[nodiscard]] std::string getAdapterName() const override;
    [[nodiscard]] std::optional<double> getGpuFrameSeconds() const noexcept override;
    [[nodiscard]] std::optional<double> getInputAge() const noexcept override;
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

    // DXGI paces the swapchains through their frame latency waitable objects, which the wait for the display waits on, so the platform plays the frames itself.
    [[nodiscard]] bool paceWindow(int framesPerSecond, std::function<void()> due) override;

    // Releases the objects that the GPU finished with, the upload pages of the pool beyond one for every frame in flight and the blocks of buffers that hold no buffer.
    void trim() override;
    void resizeSurface(const Surface::Size& size) override;
    void detachSurface() override;
    void attachSurface(const Surface& target) override;
    [[nodiscard]] int addSurface(const Surface& added) override;
    void resizeSurface(int number, const Surface::Size& size) override;
    void removeSurface(int number) override;

    [[nodiscard]] std::unique_ptr<Readback> readImage(Image image, int width, int height) override;
    [[nodiscard]] std::unique_ptr<Readback> readBuffer(Buffer buffer, std::size_t offset, std::size_t size) override;

    [[nodiscard]] CaptureSupport getCaptureSupport() const override;
    void beginCapture(const std::filesystem::path& path) override;
    [[nodiscard]] std::filesystem::path endCapture() override;

  private:
    static constexpr std::string_view kMissingCaptureTool = "Neither PIX nor RenderDoc is loaded into the app. Launch the app from PIX for a GPU capture, or from RenderDoc through its \"Launch Application\" panel or \"renderdoccmd capture\", which load them before the graphics API starts, and capture again.";

    static constexpr UINT kTextureHeapSize = 16 * 1024;
    static constexpr UINT kUniformAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    static constexpr D3D12_RESOURCE_STATES kShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    static constexpr std::size_t kTables = 4;
    // A pass moves each color attachment and the image it resolves into, and both planes of a depth stencil image.
    static constexpr std::size_t kMaxPassBarriers = (kMaxColorTargets * 2) + 2;

    // Buffers rest in the common state, from which each list promotes them to what it does with them, and to which they return when the list ends. A buffer holds a range of a block of the arena at the offset of the range, or a resource of its own when it is large, takes part in copies, is storage or is dynamic. A buffer that the list of the passes writes, a dynamic buffer or the destination of copies, goes from the state that list gave it to the copy and on to the read states of its usages around each write, so the list keeps the state of the buffer for the frame that `stateFrame` numbers.
    struct BufferRecord {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource{};
        BufferArena::Range range{};
        D3D12_GPU_VIRTUAL_ADDRESS address = 0;
        BufferRules::Description description{};
        D3D12_RESOURCE_STATES readState = D3D12_RESOURCE_STATE_COMMON;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
        std::uint64_t stateFrame = 0;
    };

    // A buffer that the bindings name, which the draws after them promote to the state.
    struct Promotion {
        std::uint64_t buffer = 0;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COMMON;
    };

    struct Attachment {
        int mipLevel = 0;
        int layer = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE handle{};
    };

    // An image or a view of one, which shares the resource of its image and samples its range through a view of its own. An image keeps the ids of its views, which go with it, and the views of single mip levels that the generation of its mip levels samples, made the first time it generates them.
    struct ImageRecord {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource{};
        D3D12_CPU_DESCRIPTOR_HANDLE view{};
        std::vector<Attachment> attachments{};
        ImageRules::Description description{};
        std::vector<std::uint64_t> views{};
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> levelViews{};
        // The state that every subresource of the image rests in between the passes, copies and generations of mip levels that use it.
        D3D12_RESOURCE_STATES home = kShaderResource;
    };

    struct ProgramRecord {
        RootSignatures::Layout layout{};
        Microsoft::WRL::ComPtr<ID3DBlob> vertexCode{};
        Microsoft::WRL::ComPtr<ID3DBlob> fragmentCode{};
    };

    // A pipeline that draws into the swapchain keeps its description, so it is made again when the window takes another sample count.
    struct PipelineRecord {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> state{};
        RootSignatures::Layout layout{};
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        std::array<UINT, kMaxVertexBuffers> strides{};
        UINT bufferCount = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        bool swapchain = false;
        Microsoft::WRL::ComPtr<ID3DBlob> vertexCode{};
        Microsoft::WRL::ComPtr<ID3DBlob> fragmentCode{};
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements{};
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    };

    struct Frame {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commands;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> transfers;
        std::uint64_t fence = 0;
    };

    // An object, a descriptor of a heap with the views of single mip levels of an image in the same heap, the views of the attachments of an image or a range of the buffer arena, that the GPU may use until the fence passes the value. A destroyed object leaves exactly one entry, whose place the device holds from the moment the object was made.
    struct Garbage {
        std::uint64_t fence = 0;
        Microsoft::WRL::ComPtr<IUnknown> object{};
        DescriptorHeap* heap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE handle{};
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> levelViews{};
        DescriptorHeap* attachmentHeap = nullptr;
        std::vector<Attachment> attachments{};
        BufferArena::Range range{};
    };

    // The state that the list of the frame holds, so a call that would set what it holds already is skipped.
    struct ListState {
        std::uint64_t pipeline = 0;
        ID3D12PipelineState* state = nullptr;
        ID3D12RootSignature* signature = nullptr;
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
        bool indexed = false;
        std::array<D3D12_VERTEX_BUFFER_VIEW, kMaxVertexBuffers> vertexBuffers{};
        UINT vertexBufferCount = 0;
        D3D12_INDEX_BUFFER_VIEW indexBuffer{};
        std::array<std::array<D3D12_CPU_DESCRIPTOR_HANDLE, DescriptorRing::kMaxTable>, kTables> tables{};
        std::array<UINT, kTables> tableCounts{};
        std::array<bool, kTables> tablesSet{};
        // The buffers of the last bindings that the list of the passes writes, which the next draw promotes, and whether a draw since them did.
        std::array<Promotion, kMaxVertexBuffers + 1 + kMaxBlocks> promotions{};
        UINT promotionCount = 0;
        bool promoted = false;
    };

    // What the open pass does when it ends: the barriers that return its attachments to their states, and the storage that its render pass reads for the resolves.
    struct PassState {
        std::array<D3D12_RESOURCE_BARRIER, kMaxPassBarriers> endBarriers{};
        UINT endBarrierCount = 0;
        std::array<D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS, kMaxColorTargets> resolves{};
        bool labeled = false;
    };

    static void enableDebugLayers(bool validation);
    // The swapchain of a surface, the window or one that `addSurface` made, with the views of its buffers and attachments, which stay in their heaps while the surface lives, and the buffer that the frame draws into. The window keeps its place while it has no surface, and a surface makes its swapchain once it has pixels.
    struct SurfaceChain {
        std::optional<Surface> surface{};
        std::unique_ptr<direct3d::Swapchain> swapchain{};
        std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, direct3d::Swapchain::kBufferCount> backBuffers{};
        std::array<D3D12_CPU_DESCRIPTOR_HANDLE, direct3d::Swapchain::kBufferCount> backBufferViews{};
        Microsoft::WRL::ComPtr<ID3D12Resource> multisampleColor{};
        D3D12_CPU_DESCRIPTOR_HANDLE multisampleView{};
        Microsoft::WRL::ComPtr<ID3D12Resource> depth{};
        D3D12_CPU_DESCRIPTOR_HANDLE depthView{};
        UINT backBufferIndex = 0;
        bool presentPending = false;
        bool presented = false;
    };

    void createDevice(bool validation);
    void readCapabilities();
    void openFrame();
    void checkFrame(HRESULT result, std::string_view action);
    void submit(ID3D12GraphicsCommandList& list);
    [[nodiscard]] ID3D12GraphicsCommandList4& getTransferList();
    void waitFor(std::uint64_t value);
    void waitIdle();
    void release(std::uint64_t completedValue);
    void reserveGarbage();
    void retire(Microsoft::WRL::ComPtr<IUnknown> object);
    void discard(Garbage&& entry) noexcept;
    void loseDevice(HRESULT reason);
    void describeRemoval();
    void setHeaps();
    [[nodiscard]] int getSwapchainSampleCount() const noexcept;
    [[nodiscard]] std::unique_ptr<SurfaceChain> makeChain(const std::optional<Surface>& target);
    [[nodiscard]] SurfaceChain& requireChain(int number) const;
    [[nodiscard]] SurfaceChain& requireAddedChain(int number) const;
    [[nodiscard]] bool prepareSwapchain(SurfaceChain& chain);
    void makeSwapchainTargets(SurfaceChain& chain);
    static void releaseSwapchainTargets(SurfaceChain& chain);
    void releaseChainViews(const SurfaceChain& chain);
    void rebuildSwapchainPipelines();
    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    void setViewportAndScissor(int width, int height);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE getAttachment(ImageRecord& image, int mipLevel, int layer);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE makeTextureView(ID3D12Resource& resource, const ImageRules::Description& description);
    [[nodiscard]] static UINT getSubresource(const ImageRules::Description& description, int mipLevel, int layer, UINT plane = 0) noexcept;
    void stagePixels(const ImageRecord& image, std::span<const std::span<const std::byte>> levels);
    void stageRegion(const ImageRecord& image, const ImageWrite& write);
    void makeLevelViews(ImageRecord& image);
    void moveLevel(const ImageRecord& image, int mipLevel, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
    [[nodiscard]] D3D12_GPU_DESCRIPTOR_HANDLE writeTextureTable(D3D12_CPU_DESCRIPTOR_HANDLE view);
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12Resource> makeBufferResource(D3D12_HEAP_TYPE heap, UINT64 size, D3D12_RESOURCE_STATES state, D3D12_RESOURCE_FLAGS flags, std::string_view label);
    [[nodiscard]] UploadAllocator::Allocation stageBytes(std::span<const std::byte> data);
    [[nodiscard]] static bool isWrittenBetweenPasses(const BufferRules::Description& description) noexcept;
    [[nodiscard]] static D3D12_RESOURCE_STATES getReadState(const BufferUsage& usage) noexcept;
    [[nodiscard]] D3D12_RESOURCE_STATES getListState(const BufferRecord& record) const noexcept;
    void setListState(BufferRecord& record, D3D12_RESOURCE_STATES state) const noexcept;
    void notePromotion(const BufferRecord& record, std::uint64_t id, D3D12_RESOURCE_STATES state) noexcept;
    void promoteBound() noexcept;
    void copyIntoBuffer(BufferRecord& record, std::size_t offset, ID3D12Resource& source, UINT64 sourceOffset, std::size_t size);
    void bindUniformBuffers(const PipelineRecord& pipeline, const Bindings& bindings);
    [[nodiscard]] DescriptorRing* applyTables(const PipelineRecord& pipeline, const Bindings& bindings);
    [[nodiscard]] bool applyTable(std::size_t table, int parameter, std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> handles);
    void drainMessages();
    static D3D12_RESOURCE_BARRIER makeTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) noexcept;
    static void setName(ID3D12Object& object, std::string_view label);

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    // The capturers of PIX and RenderDoc when either one launched the app, which capture its frames, PIX first.
    std::unique_ptr<PixCapturer> pix = PixCapturer::find();
    std::unique_ptr<renderdoc::Capturer> renderDoc = renderdoc::Capturer::find();
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    Microsoft::WRL::ComPtr<ID3D12Fence> idleFence;
    Microsoft::WRL::Wrappers::Event fenceEvent;
    std::uint64_t fenceValue = 0;
    std::uint64_t idleValue = 0;
    Microsoft::WRL::ComPtr<ID3D12InfoQueue> infoQueue;
    std::unique_ptr<direct3d::DebugMessages> messages;
    std::shared_ptr<direct3d::ShaderCompiler> compiler;
    std::string adapterName;
    Capabilities capabilities;
    bool dred = false;
    bool lost = false;

    std::unique_ptr<DescriptorHeap> textureViews;
    std::unique_ptr<DescriptorHeap> samplerViews;
    std::unique_ptr<DescriptorHeap> targetViews;
    std::unique_ptr<DescriptorHeap> depthViews;
    std::unique_ptr<DescriptorRing> textureTables;
    std::unique_ptr<DescriptorRing> samplerTables;
    std::unique_ptr<UploadAllocator> uploads;
    std::unique_ptr<BufferArena> bufferArena;
    std::unique_ptr<RootSignatures> rootSignatures;
    std::unique_ptr<MipmapGenerator> mipmaps;
    std::unique_ptr<FrameTimer> timer;

    HandleTable<BufferRecord> buffers{"buffer"};
    HandleTable<ImageRecord> images{"image"};
    HandleTable<D3D12_CPU_DESCRIPTOR_HANDLE> samplers{"sampler"};
    HandleTable<ProgramRecord> programs{"program"};
    HandleTable<PipelineRecord> pipelines{"pipeline"};
    // The entries in the order of their fences, with room for one more entry for every object that lives.
    std::vector<Garbage> garbage;

    std::array<Frame, kFramesInFlight> frames;
    std::size_t frameIndex = 0;
    // The frame that records, counted from 1, whose list of passes holds the states that the buffers written between passes keep.
    std::uint64_t frameNumber = 1;
    bool frameOpen = false;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commands;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> transfers;
    bool transfersOpen = false;

    // The swapchains of the surfaces at their numbers, the window at 0, with an empty place for a surface that went away, and the samples of the window, which the pipelines of the swapchain and every surface draw with.
    std::vector<std::unique_ptr<SurfaceChain>> chains;
    int pipelineSampleCount = 1;

    ListState bound;
    // The uniform blocks that `applyUniforms` fed since the pipeline, and the blocks that the uniform buffers of the last bindings feed.
    std::array<bool, kMaxBlocks> blocksApplied{};
    std::array<bool, kMaxBlocks> blocksFromBuffers{};
    PassState pass;
    bool passOpen = false;
    // A pass into the swapchain of a window without a drawable, such as a minimized one, records nothing.
    bool skipping = false;
    std::array<float, 4> blendColor{};
    UINT stencilReference = 0;
    std::string markerText;
    // The footprints of the pixels that a copy stages and the barriers of the layers of a mip level that a generation draws into, which keep their storage, so writing the same images and generating their mip levels every frame allocates nothing.
    std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints;
    std::vector<D3D12_RESOURCE_BARRIER> levelBarriers;
};

} // namespace haylen::gpu::d3d12
