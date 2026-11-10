#pragma once

#include <d3d11_1.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "gpu/DeviceFactory.hpp"
#include "gpu/d3d11/DynamicRing.hpp"
#include "gpu/d3d11/FrameTimer.hpp"
#include "gpu/direct3d/DebugMessages.hpp"
#include "gpu/direct3d/Slots.hpp"
#include "gpu/direct3d/WideText.hpp"
#include "gpu/renderdoc/Capturer.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu::direct3d {
class ShaderCompiler;
class Swapchain;
} // namespace haylen::gpu::direct3d

namespace haylen::gpu::d3d11 {

// The device of Direct3D 11, on GPUs of feature level 11_0 and later with the runtime of Direct3D 11.1, which binds parts of one constant buffer to each draw. Uniforms go through a ring of one dynamic buffer and dynamic buffers take their data straight from the CPU, both mapped without waiting, images take their pixels through the staging of the driver, and the context state it keeps skips every binding that would change nothing. The GPU runs at most `kFramesInFlight` frames behind the CPU, which event queries enforce. Validation turns on the debug layer and moves its messages into the log.
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
    void resizeSurface(const Surface::Size& size) override;
    void detachSurface() override;
    void attachSurface(const Surface& target) override;
    [[nodiscard]] int addSurface(const Surface& added) override;
    void resizeSurface(int number, const Surface::Size& size) override;
    void removeSurface(int number) override;

    [[nodiscard]] std::unique_ptr<Readback> readImage(Image image, int width, int height) override;

    [[nodiscard]] CaptureSupport getCaptureSupport() const override;
    void beginCapture(const std::filesystem::path& path) override;
    [[nodiscard]] std::filesystem::path endCapture() override;

  private:
    static constexpr UINT kUniformRingSize = 4U << 20U;
    static constexpr UINT kUniformAlignment = 256;
    static constexpr UINT kMaxBlockSize = D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16;
    static_assert(kUniformRingSize >= kMaxBlocks * kMaxBlockSize, "The ring of uniforms holds every block of a pipeline at once.");

    // A dynamic buffer remembers the frame that wrote it last, since the first write of a frame hands the GPU a fresh copy of it.
    struct BufferRecord {
        Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
        std::size_t size = 0;
        Usage usage = Usage::Immutable;
        std::uint64_t writtenFrame = 0;
    };

    // A view that a pass draws into, at a mip level and a layer of its image.
    struct Attachment {
        int mipLevel = 0;
        int layer = 0;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> color;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depth;
    };

    struct ImageRecord {
        Microsoft::WRL::ComPtr<ID3D11Resource> resource;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
        std::vector<Attachment> attachments;
        TextureType type = TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        int sampleCount = 1;
        PixelFormat format = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
    };

    // Where a uniform block, a texture or a sampler of a program binds: its stage and its register.
    struct Register {
        int slot = 0;
        std::size_t stage = 0;
        UINT number = 0;
        UINT size = 0;
    };

    struct ProgramRecord {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> fragment;
        Microsoft::WRL::ComPtr<ID3DBlob> vertexCode;
        std::array<std::optional<Register>, kMaxBlocks> blocks{};
        std::vector<Register> textures;
        std::vector<Register> samplers;
    };

    // A pipeline holds its program's shaders and registers itself, so it outlives a program destroyed before it.
    struct PipelineRecord {
        Microsoft::WRL::ComPtr<ID3D11VertexShader> vertex;
        Microsoft::WRL::ComPtr<ID3D11PixelShader> fragment;
        Microsoft::WRL::ComPtr<ID3D11InputLayout> layout;
        Microsoft::WRL::ComPtr<ID3D11BlendState> blend;
        Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizer;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencil;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        std::array<UINT, kMaxVertexBuffers> strides{};
        UINT bufferCount = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        std::array<std::optional<Register>, kMaxBlocks> blocks{};
        std::vector<Register> textures;
        std::vector<Register> samplers;
    };

    // The state that the context holds, so a call that would set what it holds already is skipped. Bound objects stay alive in the context, so no other object takes the address of one of them while it is bound.
    struct ContextState {
        std::uint32_t pipeline = 0;
        ID3D11InputLayout* layout = nullptr;
        D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        ID3D11VertexShader* vertex = nullptr;
        ID3D11PixelShader* fragment = nullptr;
        ID3D11BlendState* blend = nullptr;
        ID3D11RasterizerState* rasterizer = nullptr;
        ID3D11DepthStencilState* depthStencil = nullptr;
        std::array<ID3D11Buffer*, kMaxVertexBuffers> vertexBuffers{};
        std::array<UINT, kMaxVertexBuffers> vertexOffsets{};
        std::array<UINT, kMaxVertexBuffers> vertexStrides{};
        ID3D11Buffer* indexBuffer = nullptr;
        UINT indexOffset = 0;
        DXGI_FORMAT indexFormat = DXGI_FORMAT_UNKNOWN;
        bool indexed = false;
        std::array<std::array<ID3D11ShaderResourceView*, kMaxTextures>, 2> textures{};
        std::array<UINT, 2> textureEnds{};
        std::array<std::array<ID3D11SamplerState*, kMaxSamplers>, 2> samplers{};
    };

    // What the open pass does when it ends: the multisampled attachments it resolves and the attachments it discards.
    struct PassState {
        struct Resolve {
            ID3D11Resource* source = nullptr;
            UINT sourceSubresource = 0;
            ID3D11Resource* destination = nullptr;
            DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        };

        std::array<ID3D11RenderTargetView*, kMaxColorTargets> colors{};
        std::array<bool, kMaxColorTargets> discardColors{};
        std::array<Resolve, kMaxColorTargets> resolves{};
        UINT colorCount = 0;
        ID3D11DepthStencilView* depthStencil = nullptr;
        bool discardDepthStencil = false;
        bool labeled = false;
    };

    // The swapchain of a surface, the window or one that `addSurface` made, with the views that its passes draw into and whether the frame drew into it. The window keeps its place while it has no surface, and a surface makes its swapchain once it has pixels.
    struct SurfaceChain {
        std::optional<Surface> surface;
        std::unique_ptr<direct3d::Swapchain> swapchain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> backBufferView;
        Microsoft::WRL::ComPtr<ID3D11Texture2D> multisampleColor;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> multisampleView;
        Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthView;
        bool presentPending = false;
    };

    void createDevice(bool validation);
    void readCapabilities();
    void openFrame();
    void loseDevice(HRESULT reason) noexcept;
    [[nodiscard]] SurfaceChain& requireChain(int number) const;
    [[nodiscard]] SurfaceChain& requireAddedChain(int number) const;
    [[nodiscard]] bool prepareSwapchain(SurfaceChain& chain);
    void makeSwapchainTargets(SurfaceChain& chain);
    void releaseSwapchainTargets(SurfaceChain& chain);
    void releaseChain(SurfaceChain& chain);
    void unbindTextures();
    void setViewportAndScissor(int width, int height);
    void beginSwapchainPass(const PassDesc& desc);
    void beginOffscreenPass(const PassDesc& desc);
    [[nodiscard]] Attachment& getAttachment(ImageRecord& image, int mipLevel, int layer);
    [[nodiscard]] Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> makeTextureView(ID3D11Resource& resource, const ImageDesc& desc);
    [[nodiscard]] static std::vector<D3D11_SUBRESOURCE_DATA> describeLevels(const ImageDesc& desc);
    [[nodiscard]] static std::vector<Register> readRegisters(std::span<const TextureBinding> textures);
    [[nodiscard]] static std::vector<Register> readRegisters(std::span<const SamplerBinding> samplersOfProgram);
    void bindBlock(const Register& block, UINT offset);
    void rewriteBlocks(const PipelineRecord& pipeline, std::size_t written);
    void applyTextures(const PipelineRecord& pipeline, const Bindings& bindings);
    void applySamplers(const PipelineRecord& pipeline, const Bindings& bindings);
    void drainMessages();
    static void setName(ID3D11DeviceChild& object, std::string_view label) noexcept;

    Microsoft::WRL::ComPtr<IDXGIFactory6> factory;
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    Microsoft::WRL::ComPtr<ID3D11Device1> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext1> context;
    // The in-application API of RenderDoc when RenderDoc launched the app, which captures its frames.
    std::unique_ptr<renderdoc::Capturer> renderDoc = renderdoc::Capturer::find();
    Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> annotation;
    Microsoft::WRL::ComPtr<ID3D11InfoQueue> infoQueue;
    std::unique_ptr<direct3d::DebugMessages> messages;
    std::shared_ptr<direct3d::ShaderCompiler> compiler;
    std::string adapterName;
    Capabilities capabilities;
    bool lost = false;

    direct3d::Slots<BufferRecord> buffers{"buffer"};
    direct3d::Slots<ImageRecord> images{"image"};
    direct3d::Slots<Microsoft::WRL::ComPtr<ID3D11SamplerState>> samplers{"sampler"};
    direct3d::Slots<ProgramRecord> programs{"program"};
    direct3d::Slots<PipelineRecord> pipelines{"pipeline"};

    std::unique_ptr<DynamicRing> uniforms;
    // The bytes of the uniform blocks applied since the pipeline, which the blocks take again when the ring starts over.
    std::array<std::vector<std::byte>, kMaxBlocks> blockBytes;
    std::array<bool, kMaxBlocks> blocksApplied{};
    std::unique_ptr<FrameTimer> timer;
    std::array<Microsoft::WRL::ComPtr<ID3D11Query>, kFramesInFlight> frameFinished;
    std::array<bool, kFramesInFlight> frameSubmitted{};
    std::size_t frameIndex = 0;
    std::uint64_t frameNumber = 1;
    bool frameOpen = false;

    // The swapchains of the surfaces at their numbers, the window at 0, with an empty place for a surface that went away.
    std::vector<std::unique_ptr<SurfaceChain>> chains;

    ContextState bound;
    PassState pass;
    bool passOpen = false;
    // A pass into the swapchain of a window without a drawable, such as a minimized one, records nothing.
    bool skipping = false;
    std::array<float, 4> blendColor{};
    UINT stencilReference = 0;
    direct3d::WideText markerText;
};

} // namespace haylen::gpu::d3d11
