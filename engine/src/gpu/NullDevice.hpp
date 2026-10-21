#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "haylen/gpu/Device.hpp"

namespace haylen::gpu {

// The device of the headless host, which draws nothing and checks every call against the contract of `Device`: objects exist when they are used, passes open and close in turn, draws happen inside a pass with a pipeline, bindings fill the slots the program declares, and uniforms match the size of their block. A broken call throws `std::logic_error`, so the tests of the engine fail on it. It keeps what the last committed frame did for tests to read.
class NullDevice final : public Device {
  public:
    static constexpr int kMaxImageSize = 16 * 1024;

    // How many objects of each kind exist at once, past which the device has no room, the sizes of the pools of the device of the Sokol backend.
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

    struct FrameRecord {
        std::vector<PassRecord> passes;
        int imageWrites = 0;
        std::size_t imageWriteBytes = 0;
        int bufferWrites = 0;
    };

    // A device that runs a shader language asks every program for code in it, which tests use to see what a package lacks.
    explicit NullDevice(Language runs = Language::None) noexcept : language(runs) {}

    [[nodiscard]] Language getLanguage() const noexcept override;
    [[nodiscard]] Api getApi() const noexcept override;
    [[nodiscard]] std::string getAdapterName() const override;
    [[nodiscard]] int getMaxImageSize() const noexcept override;
    [[nodiscard]] bool isOriginTopLeft() const noexcept override;
    [[nodiscard]] bool canBlendInto(PixelFormat format) const noexcept override;
    [[nodiscard]] std::optional<double> getGpuFrameSeconds() const noexcept override;
    [[nodiscard]] std::vector<Pool> getPools() const override;

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
    void draw(int first, int count, int instances) override;
    void endPass() override;
    void commit() override;

    [[nodiscard]] std::unique_ptr<Readback> readImage(Image image, int width, int height) override;

    // What the frame committed last did, and how many programs were prepared ahead of being made.
    [[nodiscard]] const FrameRecord& getLastFrame() const noexcept {
        return lastFrame;
    }

    [[nodiscard]] int getPreparedPrograms() const noexcept {
        return preparedPrograms;
    }

  private:
    struct BufferState {
        std::size_t size = 0;
        Usage usage = Usage::Immutable;
        bool index = false;
    };

    struct ImageState {
        int width = 0;
        int height = 0;
        PixelFormat format = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
    };

    struct ProgramState {
        std::vector<BlockBinding> blocks;
        std::vector<TextureBinding> textures;
        std::vector<SamplerBinding> samplers;
    };

    struct PipelineState {
        Program program;
        std::vector<VertexBufferLayout> buffers;
        IndexType indexType = IndexType::None;
        ColorMask firstMask = ColorMask::All;
        std::size_t colors = 0;
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

    [[nodiscard]] static std::size_t getBytesPerPixel(PixelFormat format);
    static void requireRoom(std::size_t used, int size, std::string_view message);
    [[nodiscard]] const BufferState& getBuffer(Buffer buffer) const;
    [[nodiscard]] const ImageState& getImage(Image image) const;
    void requirePass(std::string_view call) const;

    std::unordered_map<std::uint32_t, BufferState> buffers;
    std::unordered_map<std::uint32_t, ImageState> images;
    std::unordered_map<std::uint32_t, SamplerDesc> samplers;
    std::unordered_map<std::uint32_t, ProgramState> programs;
    std::unordered_map<std::uint32_t, PipelineState> pipelines;
    Language language = Language::None;
    int preparedPrograms = 0;
    std::uint32_t nextId = 1;
    bool passOpen = false;
    Pipeline applied;
    // The frame that records holds its passes up to the count it recorded, and reuses the ones past it.
    FrameRecord currentFrame;
    std::size_t recordedPasses = 0;
    bool imagesWritten = false;
    FrameRecord lastFrame;
};

} // namespace haylen::gpu
