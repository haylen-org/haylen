#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/gpu/Surface.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::gpu {

// The graphics device of the RHI of Haylen: the buffers, images, samplers, programs and pipelines that the renderers of the engine create, and the passes and draws they record, with no type of the graphics API behind it. Each graphics API has its device, which draws into the surface of the window the platform opened, and the renderers of every dimension share it. Every method runs on the frame thread. Methods that create an object throw `std::runtime_error` with a clear sentence when the device has no room for it or rejects it.
class Device {
  public:
    // The shader language that the device runs, which picks the code of a program in a shader package. The device of the headless host runs none.
    enum class Language : std::uint8_t {
        None,
        MetalMacos,
        MetalIos,
        MetalIosSimulator,
        Hlsl5,
        Glsl430,
        Glsl300es,
        Wgsl,
        Spirv,
    };

    // The graphics API that the device draws through, which every device names the same way. The device of the headless host draws through none.
    enum class Api : std::uint8_t {
        None,
        Metal,
        Direct3D11,
        Direct3D12,
        Vulkan,
        OpenGl,
        OpenGlEs,
        WebGl2,
        WebGpu,
    };

    static constexpr std::array<std::pair<std::string_view, Api>, 9> kApiNames{{
        {"none", Api::None},
        {"metal", Api::Metal},
        {"direct3D11", Api::Direct3D11},
        {"direct3D12", Api::Direct3D12},
        {"vulkan", Api::Vulkan},
        {"openGl", Api::OpenGl},
        {"openGlEs", Api::OpenGlEs},
        {"webGl2", Api::WebGl2},
        {"webGpu", Api::WebGpu},
    }};

    // The formats of images. The depth formats belong to depth and stencil attachments, and a device that lacks `Depth24Stencil8` gives `Depth32FloatStencil8` its place.
    enum class PixelFormat : std::uint8_t {
        R8,
        Rg8,
        Rgba8,
        Rgba8Srgb,
        Bgra8,
        Rgb10A2,
        R16Float,
        Rg16Float,
        Rgba16Float,
        R32Float,
        Rg32Float,
        Rgba32Float,
        Depth16,
        Depth32Float,
        Depth24Stencil8,
        Depth32FloatStencil8,
    };

    // The formats of vertex attributes. The normalized and float formats feed float inputs of the shader, and `UByte4`, `Short2` and `UInt` feed integer inputs.
    enum class VertexFormat : std::uint8_t {
        Float,
        Float2,
        Float3,
        Float4,
        Half2,
        Half4,
        UByte4,
        UByte4Normalized,
        Short2,
        Short2Normalized,
        UShort4Normalized,
        UInt,
    };

    enum class Primitive : std::uint8_t {
        Triangles,
        TriangleStrip,
        Lines,
        LineStrip,
        Points,
    };

    enum class IndexType : std::uint8_t {
        None,
        UInt16,
        UInt32,
    };

    enum class BlendFactor : std::uint8_t {
        Zero,
        One,
        SourceColor,
        OneMinusSourceColor,
        SourceAlpha,
        OneMinusSourceAlpha,
        DestinationColor,
        OneMinusDestinationColor,
        DestinationAlpha,
        OneMinusDestinationAlpha,
        SourceAlphaSaturated,
        BlendColor,
        OneMinusBlendColor,
    };

    enum class BlendOperation : std::uint8_t {
        Add,
        Subtract,
        ReverseSubtract,
        Minimum,
        Maximum,
    };

    // The channels a pipeline writes into a color target, as bits.
    enum class ColorMask : std::uint8_t {
        None = 0,
        Red = 1,
        Green = 2,
        Blue = 4,
        Alpha = 8,
        Rgb = 7,
        All = 15,
    };

    enum class CompareFunction : std::uint8_t {
        Never,
        Less,
        Equal,
        LessEqual,
        Greater,
        NotEqual,
        GreaterEqual,
        Always,
    };

    enum class StencilOperation : std::uint8_t {
        Keep,
        Zero,
        Replace,
        IncrementClamp,
        DecrementClamp,
        Invert,
        IncrementWrap,
        DecrementWrap,
    };

    enum class CullMode : std::uint8_t {
        None,
        Front,
        Back,
    };

    enum class FrontFace : std::uint8_t {
        CounterClockwise,
        Clockwise,
    };

    enum class Filter : std::uint8_t {
        Nearest,
        Linear,
    };

    enum class Wrap : std::uint8_t {
        ClampToEdge,
        Repeat,
        MirroredRepeat,
    };

    enum class Stage : std::uint8_t {
        Vertex,
        Fragment,
    };

    enum class TextureType : std::uint8_t {
        Flat,
        Array,
        Volume,
        Cube,
    };

    enum class SampleType : std::uint8_t {
        Float,
        UnfilterableFloat,
        Depth,
        SignedInteger,
        UnsignedInteger,
    };

    enum class SamplerType : std::uint8_t {
        Filtering,
        NonFiltering,
        Comparison,
    };

    // A dynamic buffer takes new contents every frame through `writeBuffer` and holds in a frame only what that frame wrote into it, and an immutable one keeps the data it was made with.
    enum class Usage : std::uint8_t {
        Immutable,
        Dynamic,
    };

    // A texture keeps the pixels it was made with, a dynamic image takes new pixels through `writeImages`, a render target is a color attachment of offscreen passes that draws sample later, and a depth stencil image is the depth and stencil attachment of offscreen passes.
    enum class ImageKind : std::uint8_t {
        Texture,
        Dynamic,
        RenderTarget,
        DepthStencil,
    };

    enum class Load : std::uint8_t {
        Clear,
        Keep,
        DontCare,
    };

    // Whether a pass keeps what it drew into an attachment once it ends, which tile GPUs skip writing back to memory when the pass discards it.
    enum class Store : std::uint8_t {
        Store,
        Discard,
    };

    // The handles of the objects of a device, whose id 0 names nothing.
    struct Buffer {
        std::uint32_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Buffer, Buffer) = default;
    };

    struct Image {
        std::uint32_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Image, Image) = default;
    };

    struct Sampler {
        std::uint32_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Sampler, Sampler) = default;
    };

    struct Program {
        std::uint32_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Program, Program) = default;
    };

    struct Pipeline {
        std::uint32_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Pipeline, Pipeline) = default;
    };

    static constexpr std::size_t kMaxVertexBuffers = 4;
    static constexpr std::size_t kMaxColorTargets = 4;
    static constexpr std::size_t kMaxBlocks = 8;
    static constexpr std::size_t kMaxTextures = 32;
    static constexpr std::size_t kMaxSamplers = 12;
    static constexpr std::size_t kMaxAttributes = 16;

    // What the device and its graphics API can do, which the renderers read instead of the API. A capability that a device lacks is never used by the engine.
    struct Capabilities {
        int maxImageSize = 0;
        int maxArrayLayers = 0;
        int maxSampleCount = 1;

        // Whether the first row of a render target is its top row, as on Metal, Direct3D, Vulkan and WebGPU, rather than its bottom row, as on OpenGL.
        bool originTopLeft = true;

        // Whether each color target of a pipeline takes a blend and a write mask of its own, rather than all of them those of the first target.
        bool independentBlend = false;

        // Whether a render target of one sample can resolve a multisampled one at the end of a pass.
        bool multisampledRenderTargets = false;

        // Whether the device measures the time the GPU takes for each frame.
        bool gpuTiming = false;

        // The formats that images can be made in and sampled, by `PixelFormat`.
        std::array<bool, 16> textureFormats{};

        // The formats that render targets and depth stencil images can be made in and drawn into.
        std::array<bool, 16> renderFormats{};

        // The color formats that render targets can be drawn into with blending.
        std::array<bool, 16> blendableFormats{};
    };

    struct BufferDesc {
        std::size_t size = 0;
        std::span<const std::byte> data;
        Usage usage = Usage::Immutable;
        bool index = false;
        std::string_view label;
    };

    // The pixels of a texture come with the image, one span per mip level from the largest, each holding every layer of its level, and the other kinds start without pixels. The layers count the slices of a volume, the layers of an array and the six faces of a cube.
    struct ImageDesc {
        TextureType type = TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        int sampleCount = 1;
        PixelFormat format = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
        std::span<const std::span<const std::byte>> levels;
        std::string_view label;
    };

    struct SamplerDesc {
        Filter minification = Filter::Linear;
        Filter magnification = Filter::Linear;
        Filter mipmap = Filter::Nearest;
        Wrap wrapU = Wrap::ClampToEdge;
        Wrap wrapV = Wrap::ClampToEdge;
        Wrap wrapW = Wrap::ClampToEdge;
        float minLod = 0.0F;
        float maxLod = 1000.0F;
        int maxAnisotropy = 1;
        std::optional<CompareFunction> compare;
        std::string_view label;
    };

    // One stage of a program in the language of the device, with the source it runs and its entry point, and the profile that HLSL compiles it for. SPIR-V comes as the bytes of the module.
    struct StageCode {
        std::string_view source;
        std::span<const std::byte> bytecode;
        std::string_view entry;
        std::string_view profile;
    };

    // A uniform block of a program at its bind slot, with the binding number that the language of the device gives it and its descriptor set or bind group where the language has them. The OpenGL languages read a block as one array of vectors, which `glslName`, `glslType` and `glslCount` describe.
    struct BlockBinding {
        int slot = 0;
        Stage stage = Stage::Vertex;
        std::uint32_t size = 0;
        int binding = 0;
        int set = 0;
        std::string_view glslName;
        bool glslIntegers = false;
        int glslCount = 0;
    };

    struct TextureBinding {
        int slot = 0;
        Stage stage = Stage::Fragment;
        TextureType type = TextureType::Flat;
        SampleType sampleType = SampleType::Float;
        bool multisampled = false;
        int binding = 0;
        int set = 0;
    };

    struct SamplerBinding {
        int slot = 0;
        Stage stage = Stage::Fragment;
        SamplerType type = SamplerType::Filtering;
        int binding = 0;
        int set = 0;
    };

    // A texture and a sampler that the OpenGL languages combine into one sampler object under the name.
    struct TexturePair {
        Stage stage = Stage::Fragment;
        int texture = 0;
        int sampler = 0;
        std::string_view glslName;
    };

    // A vertex input of a program, which HLSL reads through the semantic `TEXCOORD` with the location as its index and OpenGL by its name.
    struct Attribute {
        int location = 0;
        std::string_view name;
    };

    struct ProgramDesc {
        StageCode vertex;
        StageCode fragment;
        std::span<const Attribute> attributes;
        std::span<const BlockBinding> blocks;
        std::span<const TextureBinding> textures;
        std::span<const SamplerBinding> samplers;
        std::span<const TexturePair> pairs;
        std::string_view label;
    };

    struct VertexBufferLayout {
        int stride = 0;
        bool perInstance = false;
    };

    struct VertexAttribute {
        int location = 0;
        int buffer = 0;
        int offset = 0;
        VertexFormat format = VertexFormat::Float;
    };

    struct Blend {
        bool enabled = false;
        BlendFactor sourceColor = BlendFactor::One;
        BlendFactor destinationColor = BlendFactor::Zero;
        BlendOperation colorOperation = BlendOperation::Add;
        BlendFactor sourceAlpha = BlendFactor::One;
        BlendFactor destinationAlpha = BlendFactor::Zero;
        BlendOperation alphaOperation = BlendOperation::Add;

        friend bool operator==(const Blend&, const Blend&) = default;
    };

    struct ColorTarget {
        PixelFormat format = PixelFormat::Rgba8;
        ColorMask writeMask = ColorMask::All;
        Blend blend;
    };

    struct StencilFace {
        CompareFunction compare = CompareFunction::Always;
        StencilOperation fail = StencilOperation::Keep;
        StencilOperation depthFail = StencilOperation::Keep;
        StencilOperation pass = StencilOperation::Keep;
    };

    // The depth and stencil tests of a pipeline, which draw into the depth stencil attachment of the format, or into none when it has no format.
    struct DepthStencil {
        std::optional<PixelFormat> format;
        CompareFunction depthCompare = CompareFunction::Always;
        bool depthWrite = false;
        bool stencilEnabled = false;
        StencilFace stencilFront;
        StencilFace stencilBack;
        std::uint8_t stencilReadMask = 0xFF;
        std::uint8_t stencilWriteMask = 0xFF;
        float depthBias = 0.0F;
        float depthBiasSlope = 0.0F;
    };

    // A pipeline that draws into the swapchain takes the color format, the depth format and the sample count of the swapchain, whatever its targets name, and the others draw into the attachments their formats and sample count describe. A device without `independentBlend` applies the blend and the write mask of the first color target to all of them.
    struct PipelineDesc {
        Program program;
        std::span<const VertexBufferLayout> buffers;
        std::span<const VertexAttribute> attributes;
        Primitive primitive = Primitive::Triangles;
        IndexType indexType = IndexType::None;
        std::span<const ColorTarget> colors;
        DepthStencil depthStencil;
        CullMode cullMode = CullMode::None;
        FrontFace frontFace = FrontFace::CounterClockwise;
        int sampleCount = 1;
        bool alphaToCoverage = false;
        bool swapchain = false;
        std::string_view label;
    };

    // A color attachment of an offscreen pass, at a mip level and a layer of its image, with the image of one sample that resolves a multisampled attachment into its first mip level and layer when the pass ends.
    struct ColorAttachment {
        Image image;
        int mipLevel = 0;
        int layer = 0;
        Load load = Load::Clear;
        Store store = Store::Store;
        math::Color clear = math::Color::transparent();
        Image resolve;
    };

    struct DepthStencilAttachment {
        Image image;
        Load depthLoad = Load::Clear;
        Store depthStore = Store::Discard;
        float clearDepth = 1.0F;
        Load stencilLoad = Load::Clear;
        Store stencilStore = Store::Discard;
        std::uint8_t clearStencil = 0;
    };

    // A pass draws into the swapchain of the frame when it names no attachment, clearing its color, depth and stencil as its loads say. A swapchain pass that keeps the contents keeps the depth, the stencil and multisampled color only when it directly follows the swapchain pass whose contents it keeps, and otherwise keeps only the color of one sample.
    struct PassDesc {
        std::span<const ColorAttachment> colors;
        std::optional<DepthStencilAttachment> depthStencil;
        Load load = Load::Clear;
        math::Color clear = math::Color::transparent();
        float clearDepth = 1.0F;
        std::uint8_t clearStencil = 0;
        std::string_view label;
    };

    struct Bindings {
        std::array<Buffer, kMaxVertexBuffers> vertexBuffers{};
        std::array<int, kMaxVertexBuffers> vertexOffsets{};
        Buffer indexBuffer;
        int indexOffset = 0;
        std::array<Image, kMaxTextures> textures{};
        std::array<Sampler, kMaxSamplers> samplers{};
    };

    // New pixels for a dynamic image, which replace the pixels of its first mip level, every layer at once.
    struct ImageWrite {
        Image image;
        std::span<const std::byte> pixels;
    };

    // How many objects of one kind exist, out of the room the device holds for them now.
    struct Pool {
        std::string_view name;
        int used = 0;
        int size = 0;
    };

    // A copy of the pixels of a render target to memory the CPU reads, which `poll` hands over once the GPU finished it, as RGBA8 whatever the format of the image, four bytes a pixel from the first row the device stores. A multisampled image is read through its resolve image.
    class Readback {
      public:
        virtual ~Readback() = default;
        [[nodiscard]] virtual std::optional<std::vector<std::uint8_t>> poll() = 0;
    };

    Device() = default;
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;
    virtual ~Device() = default;

    [[nodiscard]] virtual Language getLanguage() const noexcept = 0;
    [[nodiscard]] virtual Api getApi() const noexcept = 0;
    [[nodiscard]] virtual const Capabilities& getCapabilities() const noexcept = 0;

    // The name of the GPU as its API reports it, or an empty text where it reports none.
    [[nodiscard]] virtual std::string getAdapterName() const = 0;

    // The seconds the GPU took for the last frame it finished, where the API measures it.
    [[nodiscard]] virtual std::optional<double> getGpuFrameSeconds() const noexcept = 0;
    [[nodiscard]] virtual std::vector<Pool> getPools() const = 0;

    // Whether the device stopped working, such as after a reset of the GPU or a lost context, after which the platform creates a new device and the app starts again.
    [[nodiscard]] virtual bool isLost() const noexcept = 0;

    [[nodiscard]] virtual Buffer makeBuffer(const BufferDesc& desc) = 0;
    [[nodiscard]] virtual Image makeImage(const ImageDesc& desc) = 0;
    [[nodiscard]] virtual Sampler makeSampler(const SamplerDesc& desc) = 0;
    [[nodiscard]] virtual Program makeProgram(const ProgramDesc& desc) = 0;
    [[nodiscard]] virtual Pipeline makePipeline(const PipelineDesc& desc) = 0;

    // Starts compiling the stages of a program on the I/O pool where the API compiles them when the program is made, so making it later waits less. It may run on any thread, and copies what it needs from the description.
    virtual void prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) = 0;

    // Destroys an object once the frames in flight that use it finished on the GPU.
    virtual void destroy(Buffer buffer) noexcept = 0;
    virtual void destroy(Image image) noexcept = 0;
    virtual void destroy(Sampler sampler) noexcept = 0;
    virtual void destroy(Program program) noexcept = 0;
    virtual void destroy(Pipeline pipeline) noexcept = 0;

    // Writes data into a dynamic buffer at the offset, before the passes of the frame that read it. A frame may write several parts of one buffer, each at most once.
    virtual void writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) = 0;

    // Replaces the pixels of dynamic images, every image of a frame in one call before its first pass, so a device stages them all before it copies any of them.
    virtual void writeImages(std::span<const ImageWrite> writes) = 0;

    virtual void beginPass(const PassDesc& desc) = 0;
    virtual void applyViewport(const math::Rect& pixels) = 0;
    virtual void applyScissor(const math::Rect& pixels) = 0;
    virtual void applyPipeline(Pipeline pipeline) = 0;
    virtual void applyBindings(const Bindings& bindings) = 0;
    virtual void applyUniforms(int slot, std::span<const std::byte> data) = 0;
    virtual void applyStencilReference(std::uint8_t value) = 0;
    virtual void applyBlendColor(math::Color value) = 0;
    virtual void draw(int first, int count, int instances) = 0;
    virtual void endPass() = 0;

    // Names the commands until the matching pop in the captures and traces of GPU debuggers.
    virtual void pushDebugGroup(std::string_view name) = 0;
    virtual void popDebugGroup() = 0;

    // Submits the passes of the frame and presents the swapchain when a pass drew into it.
    virtual void commit() = 0;

    // Makes the swapchain follow a new size of the drawable in pixels, from the next frame.
    virtual void resizeSurface(int width, int height) = 0;

    // Lets go of the surface while the platform has none, such as an Android app in the background, and takes the new one when it comes back, keeping every object the device made. A frame without a surface records nothing into the swapchain.
    virtual void detachSurface() = 0;
    virtual void attachSurface(const Surface& surface) = 0;

    // Starts copying the pixels of the first mip level and layer of a render target, which must come after the frame that renders it was committed.
    [[nodiscard]] virtual std::unique_ptr<Readback> readImage(Image image, int width, int height) = 0;
};

} // namespace haylen::gpu
