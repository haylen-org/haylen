#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gpu/ImageInfo.hpp"
#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The device of the headless host, which draws nothing and checks every call against the contract of `Device`: objects exist when they are used, images fit their type, mip levels, layers, sample count and format, passes open and close in turn with attachments that match each other and the pipelines drawn with them, multisampled attachments resolve into images of one sample, draws happen inside a pass with a pipeline, its bindings and every uniform block of its program, debug groups balance, and the surface comes and goes in turn. A broken call throws `std::logic_error`, so the tests of the engine fail on it. It keeps what the last committed frame did for tests to read.
class NullDevice final : public Device {
  public:
    static constexpr int kMaxImageSize = 16 * 1024;
    static constexpr int kMaxArrayLayers = 2048;
    static constexpr int kMaxSampleCount = 8;
    static constexpr int kMaxAnisotropy = 16;

    // How many objects of each kind exist at once, past which the device has no room.
    static constexpr int kImagePoolSize = 4096;
    static constexpr int kBufferPoolSize = 4096;
    static constexpr int kSamplerPoolSize = 256;
    static constexpr int kProgramPoolSize = 512;
    static constexpr int kPipelinePoolSize = 2048;

    // A pass of a frame: whether it drew into the swapchain, its clear color, and the write masks of the first color of the pipelines it applied.
    struct PassRecord {
        bool swapchain = false;
        Load load = Load::Clear;
        math::Color clear = math::Color::transparent();
        std::vector<ColorMask> masks;
        int draws = 0;
    };

    // The bindings a program was made with and the bytes of its SPIR-V modules.
    struct ProgramRecord {
        std::vector<BlockBinding> blocks;
        std::vector<TextureBinding> textures;
        std::vector<SamplerBinding> samplers;
        std::size_t vertexBytes = 0;
        std::size_t fragmentBytes = 0;
    };

    struct FrameRecord {
        std::vector<PassRecord> passes;
        int imageWrites = 0;
        std::size_t imageWriteBytes = 0;
        int bufferWrites = 0;
    };

    // A device that runs a shader language asks every program for code in it, which tests use to see what a package lacks, and the capabilities say what the engine may use.
    explicit NullDevice(Language runs = Language::None, const Capabilities& capabilities = getDefaultCapabilities()) noexcept : language(runs), features(capabilities) {}

    // The capabilities of a device that renders every color format, blends into all but the 32-bit floats, and resolves multisampled render targets, but blends every color of a pipeline alike, as OpenGL ES does.
    [[nodiscard]] static Capabilities getDefaultCapabilities() noexcept;

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

    // What the frame committed last did, and how many programs were prepared ahead of being made.
    [[nodiscard]] const FrameRecord& getLastFrame() const noexcept {
        return lastFrame;
    }

    [[nodiscard]] int getPreparedPrograms() const noexcept {
        return preparedPrograms;
    }

    [[nodiscard]] const ProgramRecord& getProgramRecord(Program program) const {
        return programs.at(program.id);
    }

  private:
    // A part of a dynamic buffer that the frame of the stamp wrote.
    struct WrittenRange {
        std::size_t offset = 0;
        std::size_t size = 0;
    };

    struct BufferState {
        std::size_t size = 0;
        Usage usage = Usage::Immutable;
        bool index = false;
        std::uint64_t boundFrame = 0;
        std::uint64_t writtenFrame = 0;
        std::vector<WrittenRange> written;
    };

    // The attachments that a pipeline draws into, which the pass it is applied in must have.
    struct Targets {
        bool swapchain = false;
        std::array<PixelFormat, kMaxColorTargets> colors{};
        std::size_t colorCount = 0;
        std::optional<PixelFormat> depthStencil;
        int sampleCount = 1;

        friend bool operator==(const Targets&, const Targets&) = default;
    };

    struct PipelineState {
        Program program;
        std::vector<VertexBufferLayout> buffers;
        IndexType indexType = IndexType::None;
        ColorMask firstMask = ColorMask::All;
        Targets targets;
    };

    struct OpenPass {
        Targets targets;
        std::array<std::uint32_t, kMaxColorTargets + 1> images{};
        std::size_t debugGroups = 0;
    };

    // A copy that finishes the second time it is polled, as if the GPU took a frame, with transparent pixels.
    class NullReadback final : public Readback {
      public:
        explicit NullReadback(std::size_t bytes) : size(bytes) {}
        [[nodiscard]] std::optional<std::vector<std::uint8_t>> poll() override;

      private:
        std::size_t size = 0;
        bool started = false;
    };

    [[nodiscard]] static bool isPowerOfTwo(int value) noexcept;
    static void requireRoom(std::size_t used, int size, std::string_view message);
    void checkImageShape(const ImageDesc& desc) const;
    static void checkImageKind(const ImageDesc& desc);
    static void checkTextureLevels(const ImageDesc& desc);
    void checkColorTargets(const PipelineDesc& desc) const;
    static void checkDepthStencilState(const PipelineDesc& desc);
    void checkVertexLayout(const PipelineDesc& desc) const;
    [[nodiscard]] Targets describeOffscreenPass(const PassDesc& desc, OpenPass& pass) const;
    void checkColorAttachment(const ColorAttachment& color, ImageInfo::Extent& extent, int& sampleCount) const;
    void checkTexture(const TextureBinding& binding, Image image) const;
    void checkSampler(const SamplerBinding& binding, Sampler sampler) const;
    void checkIndexRange(int first, int count) const;
    [[nodiscard]] BufferState& getBuffer(Buffer buffer);
    [[nodiscard]] const BufferState& getBuffer(Buffer buffer) const;
    [[nodiscard]] const ImageDesc& getImage(Image image) const;
    [[nodiscard]] const PipelineState& getAppliedPipeline(std::string_view call) const;
    void requirePass(std::string_view call) const;
    void requireNoPass(std::string_view call) const;
    void recordPass(bool swapchain, Load load, math::Color clear);

    std::unordered_map<std::uint32_t, BufferState> buffers;
    std::unordered_map<std::uint32_t, ImageDesc> images;
    std::unordered_map<std::uint32_t, SamplerDesc> samplers;
    std::unordered_map<std::uint32_t, ProgramRecord> programs;
    std::unordered_map<std::uint32_t, PipelineState> pipelines;
    Language language = Language::None;
    Capabilities features;
    int preparedPrograms = 0;
    std::uint32_t nextId = 1;

    // The frame that records, counted from 1, and the pass, pipeline, bindings and uniform blocks it applied last.
    std::uint64_t frame = 1;
    std::optional<OpenPass> pass;
    Pipeline applied;
    bool bindingsApplied = false;
    Bindings bound;
    std::uint32_t appliedBlocks = 0;
    std::vector<bool> debugGroups;

    // The surface the swapchain passes draw into, which a headless window has from the start.
    bool surfaceAttached = true;

    // The frame that records holds its passes up to the count it recorded, and reuses the ones past it.
    FrameRecord currentFrame;
    std::size_t recordedPasses = 0;
    bool imagesWritten = false;
    FrameRecord lastFrame;
};

} // namespace haylen::gpu
