#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/DeviceFactory.hpp"
#include "gpu/d3d12/DescriptorHeap.hpp"
#include "gpu/d3d12/DescriptorRing.hpp"
#include "gpu/d3d12/FrameTimer.hpp"
#include "gpu/d3d12/RootSignatures.hpp"
#include "gpu/d3d12/UploadAllocator.hpp"
#include "gpu/direct3d/DebugMessages.hpp"
#include "gpu/direct3d/Slots.hpp"
#include "gpu/direct3d/Swapchain.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {
class ShaderCompiler;
}

namespace haylen::gpu::d3d12 {

// The device of Direct3D 12, on GPUs of feature level 11_0 and later. Each frame records a transfer list, which takes the copies into buffers and images and the copies out of render targets, and the list of its passes, which run in that order. The GPU runs at most `kFramesInFlight` frames behind the CPU, and a fence tells when the memory, descriptors and objects of a frame can be used again or released. Uniforms are root constant buffer views into upload memory that stays mapped, textures and samplers are descriptor tables copied into rings of shader visible heaps, and passes are render passes. Images rest in the state that shaders read, and passes move only their attachments out of it and back. Validation turns on the debug layer and the breadcrumbs that say what the GPU ran when it was removed.
class Device final : public gpu::Device {
  public:
    static constexpr std::size_t kFramesInFlight = 2;

    // Starts the device on the first GPU that runs it, drawing into the surface or offscreen. Throws `std::runtime_error` saying why when no GPU of the machine runs it.
    [[nodiscard]] static std::unique_ptr<gpu::Device> create(const std::optional<Surface>& target, const DeviceFactory::Options& options);

    Device(Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory, Microsoft::WRL::ComPtr<IDXGIAdapter1> card, const std::optional<Surface>& target, bool validation);
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
    void attachSurface(const Surface& target) override;

    [[nodiscard]] std::unique_ptr<Readback> readImage(Image image, int width, int height) override;

  private:
    static constexpr UINT kTextureHeapSize = 16 * 1024;
    static constexpr UINT kUniformAlignment = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;
    static constexpr D3D12_RESOURCE_STATES kShaderResource = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    static constexpr std::size_t kTables = 4;

    // Buffers rest in the common state, from which each list promotes them to what it does with them, and to which they return when the list ends.
    struct BufferRecord {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_GPU_VIRTUAL_ADDRESS address = 0;
        std::size_t size = 0;
        Usage usage = Usage::Immutable;
    };

    struct Attachment {
        int mipLevel = 0;
        int layer = 0;
        D3D12_CPU_DESCRIPTOR_HANDLE handle{};
    };

    struct ImageRecord {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE view{};
        std::vector<Attachment> attachments;
        TextureType type = TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        int sampleCount = 1;
        PixelFormat format = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
        // The state the image rests in between the passes and copies that use it.
        D3D12_RESOURCE_STATES home = kShaderResource;
    };

    struct ProgramRecord {
        RootSignatures::Layout layout;
        Microsoft::WRL::ComPtr<ID3DBlob> vertexCode;
        Microsoft::WRL::ComPtr<ID3DBlob> fragmentCode;
    };

    // A pipeline that draws into the swapchain keeps its description, so it is made again when the window takes another sample count.
    struct PipelineRecord {
        Microsoft::WRL::ComPtr<ID3D12PipelineState> state;
        RootSignatures::Layout layout;
        D3D12_PRIMITIVE_TOPOLOGY topology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        std::array<UINT, kMaxVertexBuffers> strides{};
        UINT bufferCount = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        bool swapchain = false;
        Microsoft::WRL::ComPtr<ID3DBlob> vertexCode;
        Microsoft::WRL::ComPtr<ID3DBlob> fragmentCode;
        std::vector<D3D12_INPUT_ELEMENT_DESC> elements;
        D3D12_GRAPHICS_PIPELINE_STATE_DESC description{};
    };

    struct Frame {
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commands;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> transfers;
        std::uint64_t fence = 0;
    };

    // An object, or a descriptor of a heap, that the GPU may use until the fence passes the value.
    struct Garbage {
        std::uint64_t fence = 0;
        Microsoft::WRL::ComPtr<IUnknown> object;
        DescriptorHeap* heap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE handle{};
    };

    // The state that the list of the frame holds, so a call that would set what it holds already is skipped.
    struct ListState {
        std::uint32_t pipeline = 0;
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
    };

    // What the open pass does when it ends: the barriers that return its attachments to their states, and the storage that its render pass reads for the resolves.
    struct PassState {
        std::array<D3D12_RESOURCE_BARRIER, kMaxColorTargets * 2 + 1> endBarriers{};
        UINT endBarrierCount = 0;
        std::array<D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_SUBRESOURCE_PARAMETERS, kMaxColorTargets> resolves{};
        bool labeled = false;
    };

    static void enableDebugLayers(bool validation);
    void createDevice(bool validation);
    void readCapabilities();
    void openFrame();
    [[nodiscard]] ID3D12GraphicsCommandList4& getTransferList();
    void waitFor(std::uint64_t value);
    void waitIdle();
    void release(std::uint64_t completedValue);
    void retire(Microsoft::WRL::ComPtr<IUnknown> object, DescriptorHeap* heap = nullptr, D3D12_CPU_DESCRIPTOR_HANDLE handle = {});
    void loseDevice(HRESULT reason) noexcept;
    void describeRemoval() noexcept;
    void setHeaps();
    [[nodiscard]] int getSwapchainSampleCount() const noexcept;
    [[nodiscard]] bool prepareSwapchain();
    void makeSwapchainTargets();
    void releaseSwapchainTargets();
    void rebuildSwapchainPipelines();
    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    void setViewportAndScissor(int width, int height);
    [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE getAttachment(ImageRecord& image, int mipLevel, int layer);
    void makeTextureView(ImageRecord& image);
    void stagePixels(const ImageRecord& image, std::span<const std::span<const std::byte>> levels);
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D12Resource> makeBufferResource(D3D12_HEAP_TYPE heap, UINT64 size, D3D12_RESOURCE_STATES state, std::string_view label);
    [[nodiscard]] bool applyTables(const PipelineRecord& pipeline, const Bindings& bindings);
    [[nodiscard]] bool applyTable(std::size_t table, int parameter, std::span<const D3D12_CPU_DESCRIPTOR_HANDLE> handles);
    void drainMessages();
    static D3D12_RESOURCE_BARRIER makeTransition(ID3D12Resource* resource, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after, UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES) noexcept;
    static void setName(ID3D12Object& object, std::string_view label);

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<ID3D12Device> device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
    Microsoft::WRL::ComPtr<ID3D12Fence> fence;
    Microsoft::WRL::ComPtr<ID3D12Fence> idleFence;
    HANDLE fenceEvent = nullptr;
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
    std::unique_ptr<RootSignatures> rootSignatures;
    std::unique_ptr<FrameTimer> timer;

    direct3d::Slots<BufferRecord> buffers{"buffer"};
    direct3d::Slots<ImageRecord> images{"image"};
    direct3d::Slots<D3D12_CPU_DESCRIPTOR_HANDLE> samplers{"sampler"};
    direct3d::Slots<ProgramRecord> programs{"program"};
    direct3d::Slots<PipelineRecord> pipelines{"pipeline"};
    std::deque<Garbage> garbage;

    std::array<Frame, kFramesInFlight> frames;
    std::size_t frameIndex = 0;
    bool frameOpen = false;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commands;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> transfers;
    bool transfersOpen = false;

    std::optional<Surface> surface;
    std::unique_ptr<direct3d::Swapchain> swapchain;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, direct3d::Swapchain::kBufferCount> backBuffers;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, direct3d::Swapchain::kBufferCount> backBufferViews{};
    Microsoft::WRL::ComPtr<ID3D12Resource> multisampleColor;
    D3D12_CPU_DESCRIPTOR_HANDLE multisampleView{};
    Microsoft::WRL::ComPtr<ID3D12Resource> swapchainDepth;
    D3D12_CPU_DESCRIPTOR_HANDLE swapchainDepthView{};
    UINT backBufferIndex = 0;
    bool presentPending = false;
    int pipelineSampleCount = 1;

    ListState bound;
    PassState pass;
    bool passOpen = false;
    // A pass into the swapchain of a window without a drawable, such as a minimized one, records nothing.
    bool skipping = false;
    std::array<float, 4> blendColor{};
    UINT stencilReference = 0;
    std::string markerText;
};

} // namespace haylen::gpu::d3d12
