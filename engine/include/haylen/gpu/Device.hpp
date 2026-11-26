#pragma once

#include <algorithm>
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
#include <utility>
#include <vector>

#include "haylen/gpu/Surface.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::gpu {

// The graphics device of the RHI of Haylen: the buffers, images, samplers, programs and pipelines that the renderers of the engine create, and the passes and draws they record, with no type of the graphics API behind it. Each graphics API has its device, which draws into the surface of the window the platform opened, and the renderers of every dimension share it. Every method runs on the frame thread. Methods that create an object throw `std::runtime_error` with a clear sentence when the device has no room for it or rejects it.
class Device {
  public:
    // The shader language that the device runs, which picks the code of a program in a shader package. The device of the headless host runs none, and `Port` is the language of the device of a port, whose shader tool writes its code into the packages.
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
        Port,
    };

    // The graphics API that the device draws through, which every device names the same way. The device of the headless host draws through none, and `Port` is the API of the device that a port of the engine to another platform adds, since a build holds at most one port.
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
        Port,
    };

    static constexpr std::array<std::pair<std::string_view, Api>, 10> kApiNames{{
        {"none", Api::None},
        {"metal", Api::Metal},
        {"direct3D11", Api::Direct3D11},
        {"direct3D12", Api::Direct3D12},
        {"vulkan", Api::Vulkan},
        {"openGl", Api::OpenGl},
        {"openGlEs", Api::OpenGlEs},
        {"webGl2", Api::WebGl2},
        {"webGpu", Api::WebGpu},
        {"port", Api::Port},
    }};

    // The tool of a graphics API that records the commands of frames into a capture file, which its debugger opens: the frame capture of Metal, which writes a `.gputrace` document, RenderDoc, which writes an `.rdc` file, and PIX, which writes a `.wpix` file.
    enum class CaptureTool : std::uint8_t {
        None,
        MetalFrameCapture,
        RenderDoc,
        Pix,
    };

    static constexpr std::array<std::pair<std::string_view, CaptureTool>, 4> kCaptureToolNames{{
        {"none", CaptureTool::None},
        {"metalFrameCapture", CaptureTool::MetalFrameCapture},
        {"renderDoc", CaptureTool::RenderDoc},
        {"pix", CaptureTool::Pix},
    }};

    // The formats of images. The depth formats belong to depth and stencil attachments, and a device that lacks `Depth24Stencil8` gives `Depth32FloatStencil8` its place. The block compressed formats of the BC, ETC2, EAC and ASTC families hold blocks of pixels that a device samples as they are, which images take only as textures and never as attachments, and the `Hdr` formats of ASTC hold the blocks of its HDR profile.
    enum class PixelFormat : std::uint8_t {
        R8,
        Rg8,
        Rgba8,
        Rgba8Srgb,
        Bgra8,
        Rgb10A2,
        Rg11B10Float,
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
        Bc1,
        Bc1Srgb,
        Bc2,
        Bc2Srgb,
        Bc3,
        Bc3Srgb,
        Bc4,
        Bc4Snorm,
        Bc5,
        Bc5Snorm,
        Bc6hUfloat,
        Bc6hFloat,
        Bc7,
        Bc7Srgb,
        Etc2Rgb8,
        Etc2Rgb8Srgb,
        Etc2Rgb8A1,
        Etc2Rgb8A1Srgb,
        Etc2Rgba8,
        Etc2Rgba8Srgb,
        EacR11,
        EacR11Snorm,
        EacRg11,
        EacRg11Snorm,
        Astc4x4,
        Astc4x4Srgb,
        Astc4x4Hdr,
        Astc5x4,
        Astc5x4Srgb,
        Astc5x4Hdr,
        Astc5x5,
        Astc5x5Srgb,
        Astc5x5Hdr,
        Astc6x5,
        Astc6x5Srgb,
        Astc6x5Hdr,
        Astc6x6,
        Astc6x6Srgb,
        Astc6x6Hdr,
        Astc8x5,
        Astc8x5Srgb,
        Astc8x5Hdr,
        Astc8x6,
        Astc8x6Srgb,
        Astc8x6Hdr,
        Astc8x8,
        Astc8x8Srgb,
        Astc8x8Hdr,
        Astc10x5,
        Astc10x5Srgb,
        Astc10x5Hdr,
        Astc10x6,
        Astc10x6Srgb,
        Astc10x6Hdr,
        Astc10x8,
        Astc10x8Srgb,
        Astc10x8Hdr,
        Astc10x10,
        Astc10x10Srgb,
        Astc10x10Hdr,
        Astc12x10,
        Astc12x10Srgb,
        Astc12x10Hdr,
        Astc12x12,
        Astc12x12Srgb,
        Astc12x12Hdr,
    };

    static constexpr std::size_t kPixelFormatCount = 83;

    static constexpr std::array<std::pair<std::string_view, PixelFormat>, kPixelFormatCount> kPixelFormatNames{{
        {"r8", PixelFormat::R8}, {"rg8", PixelFormat::Rg8}, {"rgba8", PixelFormat::Rgba8}, {"rgba8Srgb", PixelFormat::Rgba8Srgb}, {"bgra8", PixelFormat::Bgra8}, {"rgb10A2", PixelFormat::Rgb10A2}, {"rg11B10Float", PixelFormat::Rg11B10Float}, {"r16Float", PixelFormat::R16Float}, {"rg16Float", PixelFormat::Rg16Float}, {"rgba16Float", PixelFormat::Rgba16Float}, {"r32Float", PixelFormat::R32Float}, {"rg32Float", PixelFormat::Rg32Float}, {"rgba32Float", PixelFormat::Rgba32Float}, {"depth16", PixelFormat::Depth16}, {"depth32Float", PixelFormat::Depth32Float}, {"depth24Stencil8", PixelFormat::Depth24Stencil8}, {"depth32FloatStencil8", PixelFormat::Depth32FloatStencil8}, {"bc1", PixelFormat::Bc1}, {"bc1Srgb", PixelFormat::Bc1Srgb}, {"bc2", PixelFormat::Bc2}, {"bc2Srgb", PixelFormat::Bc2Srgb}, {"bc3", PixelFormat::Bc3}, {"bc3Srgb", PixelFormat::Bc3Srgb}, {"bc4", PixelFormat::Bc4}, {"bc4Snorm", PixelFormat::Bc4Snorm}, {"bc5", PixelFormat::Bc5}, {"bc5Snorm", PixelFormat::Bc5Snorm}, {"bc6hUfloat", PixelFormat::Bc6hUfloat}, {"bc6hFloat", PixelFormat::Bc6hFloat}, {"bc7", PixelFormat::Bc7}, {"bc7Srgb", PixelFormat::Bc7Srgb}, {"etc2Rgb8", PixelFormat::Etc2Rgb8}, {"etc2Rgb8Srgb", PixelFormat::Etc2Rgb8Srgb}, {"etc2Rgb8A1", PixelFormat::Etc2Rgb8A1}, {"etc2Rgb8A1Srgb", PixelFormat::Etc2Rgb8A1Srgb}, {"etc2Rgba8", PixelFormat::Etc2Rgba8}, {"etc2Rgba8Srgb", PixelFormat::Etc2Rgba8Srgb}, {"eacR11", PixelFormat::EacR11}, {"eacR11Snorm", PixelFormat::EacR11Snorm}, {"eacRg11", PixelFormat::EacRg11}, {"eacRg11Snorm", PixelFormat::EacRg11Snorm}, {"astc4x4", PixelFormat::Astc4x4}, {"astc4x4Srgb", PixelFormat::Astc4x4Srgb}, {"astc4x4Hdr", PixelFormat::Astc4x4Hdr}, {"astc5x4", PixelFormat::Astc5x4}, {"astc5x4Srgb", PixelFormat::Astc5x4Srgb}, {"astc5x4Hdr", PixelFormat::Astc5x4Hdr}, {"astc5x5", PixelFormat::Astc5x5}, {"astc5x5Srgb", PixelFormat::Astc5x5Srgb}, {"astc5x5Hdr", PixelFormat::Astc5x5Hdr}, {"astc6x5", PixelFormat::Astc6x5}, {"astc6x5Srgb", PixelFormat::Astc6x5Srgb}, {"astc6x5Hdr", PixelFormat::Astc6x5Hdr}, {"astc6x6", PixelFormat::Astc6x6}, {"astc6x6Srgb", PixelFormat::Astc6x6Srgb}, {"astc6x6Hdr", PixelFormat::Astc6x6Hdr}, {"astc8x5", PixelFormat::Astc8x5}, {"astc8x5Srgb", PixelFormat::Astc8x5Srgb}, {"astc8x5Hdr", PixelFormat::Astc8x5Hdr}, {"astc8x6", PixelFormat::Astc8x6}, {"astc8x6Srgb", PixelFormat::Astc8x6Srgb}, {"astc8x6Hdr", PixelFormat::Astc8x6Hdr}, {"astc8x8", PixelFormat::Astc8x8}, {"astc8x8Srgb", PixelFormat::Astc8x8Srgb}, {"astc8x8Hdr", PixelFormat::Astc8x8Hdr}, {"astc10x5", PixelFormat::Astc10x5}, {"astc10x5Srgb", PixelFormat::Astc10x5Srgb}, {"astc10x5Hdr", PixelFormat::Astc10x5Hdr}, {"astc10x6", PixelFormat::Astc10x6}, {"astc10x6Srgb", PixelFormat::Astc10x6Srgb}, {"astc10x6Hdr", PixelFormat::Astc10x6Hdr}, {"astc10x8", PixelFormat::Astc10x8}, {"astc10x8Srgb", PixelFormat::Astc10x8Srgb}, {"astc10x8Hdr", PixelFormat::Astc10x8Hdr}, {"astc10x10", PixelFormat::Astc10x10}, {"astc10x10Srgb", PixelFormat::Astc10x10Srgb}, {"astc10x10Hdr", PixelFormat::Astc10x10Hdr}, {"astc12x10", PixelFormat::Astc12x10}, {"astc12x10Srgb", PixelFormat::Astc12x10Srgb}, {"astc12x10Hdr", PixelFormat::Astc12x10Hdr}, {"astc12x12", PixelFormat::Astc12x12}, {"astc12x12Srgb", PixelFormat::Astc12x12Srgb}, {"astc12x12Hdr", PixelFormat::Astc12x12Hdr},
    }};

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

    // How a sampler reads coordinates outside the image, where `ClampToBorder` reads the border color of the sampler and needs `Capabilities::borderColors`.
    enum class Wrap : std::uint8_t {
        ClampToEdge,
        Repeat,
        MirroredRepeat,
        ClampToBorder,
    };

    // The border colors that every graphics API with border colors has.
    enum class BorderColor : std::uint8_t {
        TransparentBlack,
        OpaqueBlack,
        OpaqueWhite,
    };

    enum class Stage : std::uint8_t {
        Vertex,
        Fragment,
    };

    // The types of images: a flat image, an array of flat layers, a volume of slices, a cube of six square faces, and an array of cubes, whose layers hold six faces for each cube and which needs `Capabilities::cubeArrays`.
    enum class TextureType : std::uint8_t {
        Flat,
        Array,
        Volume,
        Cube,
        CubeArray,
    };

    static constexpr std::array<std::pair<std::string_view, TextureType>, 5> kTextureTypeNames{{
        {"flat", TextureType::Flat},
        {"array", TextureType::Array},
        {"volume", TextureType::Volume},
        {"cube", TextureType::Cube},
        {"cubeArray", TextureType::CubeArray},
    }};

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

    // How the contents of a buffer change. An immutable buffer keeps the data it was made with. A dynamic buffer keeps its contents and takes new bytes for ranges of it through `writeBuffer` and `copyBuffer` between passes, which the passes after them read and the passes before them never see. A stream buffer takes new contents every frame through `writeBuffer` and holds in a frame only what that frame wrote into it, such as the vertices of the draws of the frame.
    enum class BufferUpdate : std::uint8_t {
        Immutable,
        Dynamic,
        Stream,
    };

    static constexpr std::array<std::pair<std::string_view, BufferUpdate>, 3> kBufferUpdateNames{{
        {"immutable", BufferUpdate::Immutable},
        {"dynamic", BufferUpdate::Dynamic},
        {"stream", BufferUpdate::Stream},
    }};

    // What a buffer serves as: the vertices and the indices of draws, the bytes of uniform blocks, storage that shaders read and write, the arguments of indirect draws, and the source and the destination of copies, where reading it back is a copy too. A uniform buffer is no vertex, index, storage or indirect buffer too, since Direct3D 11 binds a constant buffer as nothing else, and an index buffer is no vertex buffer, since WebGL2 binds a buffer of indices only to copies. Storage needs `Capabilities::storageBuffers` and indirect arguments `Capabilities::indirectBuffers`.
    struct BufferUsage {
        bool vertex = false;
        bool index = false;
        bool uniform = false;
        bool storage = false;
        bool indirect = false;
        bool copySource = false;
        bool copyDestination = false;

        friend bool operator==(const BufferUsage&, const BufferUsage&) = default;
    };

    // A texture keeps the pixels it was made with, a dynamic image of any type takes new pixels for regions of a mip level and a layer of it through `writeImages`, a render target is a color attachment of offscreen passes that draws sample later, and a depth stencil image is the depth and stencil attachment of offscreen passes. Dynamic images and render targets of a format whose `FormatSupport::mipmaps` holds fill their smaller mip levels from their first one through `generateMipmaps`.
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
        std::uint64_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Buffer, Buffer) = default;
    };

    struct Image {
        std::uint64_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Image, Image) = default;
    };

    struct Sampler {
        std::uint64_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Sampler, Sampler) = default;
    };

    struct Program {
        std::uint64_t id = 0;
        [[nodiscard]] bool isValid() const noexcept {
            return id != 0;
        }
        friend bool operator==(Program, Program) = default;
    };

    struct Pipeline {
        std::uint64_t id = 0;
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

    // The offsets of the uniform buffers that blocks read are multiples of this many bytes, the strictest alignment of the graphics APIs.
    static constexpr std::size_t kUniformBufferAlignment = 256;

    // What a device does with one pixel format. A texture of the format is made and sampled with `sample`, filtered linearly through texture slots of the type `SampleType::Float` with `filter`, while the others are sampled only through slots of `SampleType::UnfilterableFloat` with samplers of `SamplerType::NonFiltering`, drawn into as a render target or a depth stencil image with `render`, blended into with `blend`, written by shaders as a storage image with `storage`, and its dynamic images and render targets fill their smaller mip levels on the GPU through `generateMipmaps` with `mipmaps`. The sample counts that its attachments take are bits whose values are the counts, so 5 holds one sample and four, and a multisampled color format also resolves into an image of one sample, while a format that the device does not draw into takes none.
    struct FormatSupport {
        bool sample = false;
        bool filter = false;
        bool render = false;
        bool blend = false;
        bool storage = false;
        bool mipmaps = false;
        std::uint8_t sampleCounts = 0;
    };

    // What the device and its graphics API can do, which the renderers read instead of the API. A capability that a device lacks is never used by the engine.
    struct Capabilities {
        // The largest side of a flat image and of the layers of an array, the most layers of an array and of a cube array, the largest side and the most slices of a volume, and the largest side of the faces of a cube and of a cube array, in pixels.
        int maxImageSize = 0;
        int maxArrayLayers = 0;
        int maxVolumeSize = 0;
        int maxCubeSize = 0;

        // Whether images take the type `TextureType::CubeArray` and programs sample them, which OpenGL ES before 3.2 and WebGL2 lack.
        bool cubeArrays = false;

        // Whether `makeView` makes views of ranges of the mip levels and layers of images, which OpenGL ES without a texture view extension and WebGL2 lack.
        bool imageViews = false;

        // Whether the first row of a render target is its top row, as on Metal, Direct3D, Vulkan and WebGPU, rather than its bottom row, as on OpenGL.
        bool originTopLeft = true;

        // The range of depth in clip space that the vertex stages write and the device maps onto the depth of its attachments: `0` to `1` on Metal, Direct3D, Vulkan and WebGPU, and on OpenGL, OpenGL ES and WebGL2 where the driver takes the range through clip control, and `-1` to `1` on those without it, whose projections take the range.
        math::Mat4::DepthRange depthRange = math::Mat4::DepthRange::ZeroToOne;

        // Whether each color target of a pipeline takes a blend and a write mask of its own, rather than all of them those of the first target.
        bool independentBlend = false;

        // Whether the device measures the time the GPU takes for each frame.
        bool gpuTiming = false;

        // The most samples of anisotropic filtering that a sampler takes, 1 on a device without it.
        int maxAnisotropy = 1;

        // The largest bias of the level of detail that a sampler adds, in either direction, 0 on a device whose samplers take no bias.
        float maxLodBias = 0.0F;

        // Whether samplers read a border color outside the image, through `Wrap::ClampToBorder`.
        bool borderColors = false;

        // Whether buffers serve as storage that shaders read and write, and as the arguments of indirect draws, which OpenGL ES 3.0 and WebGL2 lack.
        bool storageBuffers = false;
        bool indirectBuffers = false;

        // The most bytes of one uniform block of a program, which a uniform buffer feeds or `applyUniforms` takes.
        std::size_t maxUniformBlockSize = 0;

        // What the device does with each pixel format, by `PixelFormat`.
        std::array<FormatSupport, kPixelFormatCount> formats{};

        [[nodiscard]] constexpr const FormatSupport& getFormat(PixelFormat format) const noexcept {
            return formats[static_cast<std::size_t>(format)];
        }
        [[nodiscard]] constexpr FormatSupport& getFormat(PixelFormat format) noexcept {
            return formats[static_cast<std::size_t>(format)];
        }
    };

    // A buffer of the size, or of the size of its data, with at least one usage. An immutable buffer comes with its data and is no copy destination, and the others may start with it. A stream buffer is never the source or the destination of a copy, since each frame holds its own contents.
    struct BufferDesc {
        std::size_t size = 0;
        std::span<const std::byte> data{};
        BufferUsage usage{};
        BufferUpdate update = BufferUpdate::Immutable;
        std::string_view label{};
    };

    // The pixels of a texture come with the image, one span per mip level from the largest, each holding every layer of its level, and the other kinds start without pixels. The layers count the slices of a volume, the layers of an array, the six faces of a cube and six faces for each cube of a cube array, in the order +X, -X, +Y, -Y, +Z and -Z.
    struct ImageDesc {
        TextureType type = TextureType::Flat;
        int width = 0;
        int height = 0;
        int layers = 1;
        int mipLevels = 1;
        int sampleCount = 1;
        PixelFormat format = PixelFormat::Rgba8;
        ImageKind kind = ImageKind::Texture;
        std::span<const std::span<const std::byte>> levels{};
        std::string_view label{};
    };

    // A view of a range of the mip levels and layers of an image that is no view, which draws sample as a texture of the type of the view, while attachments name the mip level and the layer of the image itself. A view takes the type of its image, or `TextureType::Array` for faces of a cube or a cube array. The layers count faces as the image does, so a cube view takes six of them and a cube array view whole cubes from a layer that is a multiple of six. A volume view takes every slice, so `baseLayer` is 0 and `layers` the slices of the volume. A multisampled image has no views. Views need `Capabilities::imageViews`.
    struct ViewDesc {
        Image image{};
        TextureType type = TextureType::Flat;
        int baseMipLevel = 0;
        int mipLevels = 1;
        int baseLayer = 0;
        int layers = 1;
        std::string_view label{};
    };

    // The filters, the wrap of each axis with the border color that `Wrap::ClampToBorder` reads, the levels of detail that lookups stay within with the bias they add, the most samples of anisotropic filtering and an optional compare function, within the capabilities of the device and with linear filters for anisotropy.
    struct SamplerDesc {
        Filter minification = Filter::Linear;
        Filter magnification = Filter::Linear;
        Filter mipmap = Filter::Nearest;
        Wrap wrapU = Wrap::ClampToEdge;
        Wrap wrapV = Wrap::ClampToEdge;
        Wrap wrapW = Wrap::ClampToEdge;
        float minLod = 0.0F;
        float maxLod = 1000.0F;
        float lodBias = 0.0F;
        int maxAnisotropy = 1;
        std::optional<CompareFunction> compare{};
        BorderColor borderColor = BorderColor::TransparentBlack;
        std::string_view label{};
    };

    // One stage of a program in the language of the device, with the source it runs and its entry point, and the profile that HLSL compiles it for. SPIR-V comes as the bytes of the module, and the language of a port as the bytes its shader tool wrote.
    struct StageCode {
        std::string_view source{};
        std::span<const std::byte> bytecode{};
        std::string_view entry{};
        std::string_view profile{};
    };

    // A uniform block of a program at its bind slot, with the binding number that the language of the device gives it and its descriptor set or bind group where the language has them. The OpenGL languages find a block by its name, `glslName`, and bind it to the binding point of its slot.
    struct BlockBinding {
        int slot = 0;
        Stage stage = Stage::Vertex;
        std::uint32_t size = 0;
        int binding = 0;
        int set = 0;
        std::string_view glslName{};
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
        std::string_view glslName{};
    };

    // A vertex input of a program, which HLSL reads through the semantic `TEXCOORD` with the location as its index and OpenGL by its name.
    struct Attribute {
        int location = 0;
        std::string_view name{};
    };

    struct ProgramDesc {
        StageCode vertex{};
        StageCode fragment{};
        std::span<const Attribute> attributes{};
        std::span<const BlockBinding> blocks{};
        std::span<const TextureBinding> textures{};
        std::span<const SamplerBinding> samplers{};
        std::span<const TexturePair> pairs{};
        std::string_view label{};
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
        Blend blend{};
    };

    struct StencilFace {
        CompareFunction compare = CompareFunction::Always;
        StencilOperation fail = StencilOperation::Keep;
        StencilOperation depthFail = StencilOperation::Keep;
        StencilOperation pass = StencilOperation::Keep;
    };

    // The depth and stencil tests of a pipeline, which draw into the depth stencil attachment of the format, or into none when it has no format.
    struct DepthStencil {
        std::optional<PixelFormat> format{};
        CompareFunction depthCompare = CompareFunction::Always;
        bool depthWrite = false;
        bool stencilEnabled = false;
        StencilFace stencilFront{};
        StencilFace stencilBack{};
        std::uint8_t stencilReadMask = 0xFF;
        std::uint8_t stencilWriteMask = 0xFF;
        float depthBias = 0.0F;
        float depthBiasSlope = 0.0F;
    };

    // A pipeline that draws into the swapchain takes the color format, the depth format and the sample count of the swapchain, whatever its targets name, and the others draw into the attachments their formats and sample count describe. A pipeline without color targets draws depth alone, such as into a shadow map, and names the format of its depth stencil attachment. A device without `independentBlend` applies the blend and the write mask of the first color target to all of them.
    struct PipelineDesc {
        Program program{};
        std::span<const VertexBufferLayout> buffers{};
        std::span<const VertexAttribute> attributes{};
        Primitive primitive = Primitive::Triangles;
        IndexType indexType = IndexType::None;
        std::span<const ColorTarget> colors{};
        DepthStencil depthStencil{};
        CullMode cullMode = CullMode::None;
        FrontFace frontFace = FrontFace::CounterClockwise;
        int sampleCount = 1;
        bool alphaToCoverage = false;
        bool swapchain = false;
        std::string_view label{};
    };

    // A color attachment of an offscreen pass, at a mip level and a layer of its image, with the image of one sample that resolves a multisampled attachment into a mip level and a layer of its own when the pass ends. The layer of a volume is its slice at that mip level.
    struct ColorAttachment {
        Image image{};
        int mipLevel = 0;
        int layer = 0;
        Load load = Load::Clear;
        Store store = Store::Store;
        math::Color clear = math::Color::transparent();
        Image resolve{};
        int resolveMipLevel = 0;
        int resolveLayer = 0;
    };

    // The depth and stencil attachment of an offscreen pass, at a mip level and a layer of its image, such as one cascade of an array of shadow maps or one face of a cube of shadows.
    struct DepthStencilAttachment {
        Image image{};
        int mipLevel = 0;
        int layer = 0;
        Load depthLoad = Load::Clear;
        Store depthStore = Store::Discard;
        float clearDepth = 1.0F;
        Load stencilLoad = Load::Clear;
        Store stencilStore = Store::Discard;
        std::uint8_t clearStencil = 0;
    };

    // A pass draws into the swapchain of a surface when it names no attachment, the window for the surface 0 or a surface that `addSurface` made, clearing its color, depth and stencil as its loads say. The swapchain passes of a frame into one surface follow one another, since a device may present a surface once a pass into another surface leaves it. A swapchain pass that keeps the contents keeps the depth, the stencil and the multisampled color only when it directly follows the swapchain pass of the same surface whose contents it keeps, with no debug group pushed or popped and no write into a dynamic buffer or copy of buffers between them. After an offscreen pass, a debug group, such a write or a copy between them, a window of one sample keeps its color alone, and a multisampled window keeps no color, so a renderer of a multisampled window draws the offscreen passes of a frame before its swapchain passes.
    struct PassDesc {
        std::span<const ColorAttachment> colors{};
        std::optional<DepthStencilAttachment> depthStencil{};
        Load load = Load::Clear;
        math::Color clear = math::Color::transparent();
        float clearDepth = 1.0F;
        std::uint8_t clearStencil = 0;
        std::string_view label{};
        int surface = 0;
    };

    // A uniform buffer that feeds the uniform block of a slot from an offset that is a multiple of `kUniformBufferAlignment`, with the bytes of the block from there.
    struct BufferRange {
        Buffer buffer{};
        std::size_t offset = 0;

        friend bool operator==(const BufferRange&, const BufferRange&) = default;
    };

    // The buffers, images and samplers that the draws after `applyBindings` read. Each uniform block of the program reads the uniform buffer at its slot when the bindings name one there, and otherwise the bytes that `applyUniforms` gives its slot.
    struct Bindings {
        std::array<Buffer, kMaxVertexBuffers> vertexBuffers{};
        std::array<int, kMaxVertexBuffers> vertexOffsets{};
        Buffer indexBuffer{};
        int indexOffset = 0;
        std::array<BufferRange, kMaxBlocks> uniformBuffers{};
        std::array<Image, kMaxTextures> textures{};
        std::array<Sampler, kMaxSamplers> samplers{};

        friend bool operator==(const Bindings&, const Bindings&) = default;
    };

    // A rectangle of the pixels of an image, from the first row and the first column of the pixels it was made with.
    struct Region {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        // The smallest region that holds both regions.
        [[nodiscard]] constexpr Region merged(Region other) const noexcept {
            const int left = std::min(x, other.x);
            const int top = std::min(y, other.y);
            const int right = std::max(x + width, other.x + other.width);
            const int bottom = std::max(y + height, other.y + other.height);
            return {left, top, right - left, bottom - top};
        }

        friend bool operator==(Region, Region) = default;
    };

    // New pixels for a region of a mip level and a layer of a dynamic image, which replace the pixels of that region and keep the others. The layer of a volume is its slice at that mip level. The pixels hold the rows of the region one after another, `region.width` pixels each with no gap between rows.
    struct ImageWrite {
        Image image{};
        int mipLevel = 0;
        int layer = 0;
        Region region{};
        std::span<const std::byte> pixels{};
    };

    // How many objects of one kind exist, out of the room the device holds for them now.
    struct Pool {
        std::string_view name{};
        int used = 0;
        int size = 0;
    };

    // The tool that captures the frames of the device, which the process loads before the device exists, or `CaptureTool::None` with the sentence that says what is missing and how to get it.
    struct CaptureSupport {
        CaptureTool tool = CaptureTool::None;
        std::string missing{};
    };

    // A copy to memory the CPU reads, which `poll` hands over once the GPU finished it, and never again. The pixels of a render target come as RGBA8 whatever the format of the image, four bytes a pixel from the first row the device stores, and a multisampled image is read through its resolve image. The bytes of a buffer come as they are.
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

    // The seconds from the moment the platform read the input of the last frame that the window showed, when the wait for the display before that frame returned, until the display showed it, where the graphics API reports when its frames reach the display.
    [[nodiscard]] virtual std::optional<double> getInputAge() const noexcept = 0;

    // Fills `pools` with the pools of the device in the storage it holds, so a reader that keeps the vector allocates nothing once it grew.
    virtual void readPools(std::vector<Pool>& pools) const = 0;

    // The bytes of memory that the device holds for later beyond what its objects take, such as memory blocks that hold no object and the chunks of its upload rings that its last frames left empty, which `trim` gives back. A reader may ask in every frame, since the answer allocates nothing.
    [[nodiscard]] virtual std::size_t getSpareBytes() const noexcept = 0;

    // Whether the device stopped working, such as after a reset of the GPU or a lost context, after which the platform creates a new device and the app starts again.
    [[nodiscard]] virtual bool isLost() const noexcept = 0;

    [[nodiscard]] virtual Buffer makeBuffer(const BufferDesc& desc) = 0;
    [[nodiscard]] virtual Image makeImage(const ImageDesc& desc) = 0;
    [[nodiscard]] virtual Sampler makeSampler(const SamplerDesc& desc) = 0;
    [[nodiscard]] virtual Program makeProgram(const ProgramDesc& desc) = 0;
    [[nodiscard]] virtual Pipeline makePipeline(const PipelineDesc& desc) = 0;

    // Makes a view of a range of the mip levels and layers of an image, which bindings take in the place of an image. Destroying the image destroys its views with it, and destroying a view leaves its image as it was.
    [[nodiscard]] virtual Image makeView(const ViewDesc& desc) = 0;

    // Starts compiling the stages of a program on the I/O pool where the API compiles them when the program is made, so making it later waits less. It may run on any thread, and copies what it needs from the description.
    virtual void prepareProgram(const ProgramDesc& desc, core::JobSystem& jobs) = 0;

    // Destroys an object once the frames in flight that use it finished on the GPU.
    virtual void destroy(Buffer buffer) noexcept = 0;
    virtual void destroy(Image image) noexcept = 0;
    virtual void destroy(Sampler sampler) noexcept = 0;
    virtual void destroy(Program program) noexcept = 0;
    virtual void destroy(Pipeline pipeline) noexcept = 0;

    // Writes data into a buffer that is not immutable at an offset that is a multiple of 4, outside a pass. A stream buffer takes the write before the passes of the frame that read it, and a frame may write several parts of one stream buffer, each at most once. A dynamic buffer takes writes of whole words at any point between passes, which the passes and copies after them read.
    virtual void writeBuffer(Buffer buffer, std::size_t offset, std::span<const std::byte> data) = 0;

    // Copies bytes from a buffer that is a copy source into another one that is a copy destination outside a pass, at offsets and of a size that are multiples of 4, which the passes and copies after it read.
    virtual void copyBuffer(Buffer source, std::size_t sourceOffset, Buffer destination, std::size_t destinationOffset, std::size_t size) = 0;

    // Replaces the pixels of regions of mip levels and layers of dynamic images, every image of a frame in one call before its first pass and its first `generateMipmaps`, so a device stages them all before it copies any of them. Each mip level and layer of an image takes at most one write in the call, whose region lies inside it, so a device copies only the pixels that changed and no copy of the call waits for another.
    virtual void writeImages(std::span<const ImageWrite> writes) = 0;

    // Fills every smaller mip level of every layer of a dynamic image or a render target from its first level, each level a box filter of the level above it, outside a pass, which the passes after it read. The format of the image takes `FormatSupport::mipmaps`, and the image has more than one mip level and one sample.
    virtual void generateMipmaps(Image image) = 0;

    virtual void beginPass(const PassDesc& desc) = 0;

    // The quarter turns of the images of the pass that is open, as `SurfaceTurn` describes, by which its draws turn the positions they write in clip space, while the device turns their viewports and scissors itself. Only a swapchain pass whose window system turns the images back as it shows them, such as a phone of Android held across its panel, has turns.
    [[nodiscard]] virtual int getPassTurns() const noexcept = 0;
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

    // Submits the passes of the frame and presents the swapchain of every surface that a pass drew into.
    virtual void commit() = 0;

    // Waits until every surface that the frames since the last wait presented can take another frame, which the platform calls between frames, outside a pass and after the readbacks that it started for the frame it committed, before it reads the input of the next one, so that input reaches the screen as soon as the displays allow. A frame that presented nothing leaves nothing to wait for.
    virtual void waitForDisplay() = 0;

    // Where the graphics API paces the window itself, such as through a display link of the layer of the window, calls `due` on the frame thread whenever the window can take another frame, at the frames per second or at every refresh for 0, and returns `true`, so the platform plays a frame at each call. Returns `false` where the platform paces the frames itself. An empty `due` stops the pacing, as for a window that shows nothing, and the platform paces the frames itself until it asks again.
    [[nodiscard]] virtual bool paceWindow(int framesPerSecond, std::function<void()> due) = 0;

    // Gives back to the system the memory that the device holds for later, outside a pass. What the GPU finished with goes at once and the rest as the frames in flight finish, so a trim never waits for the GPU. While no frame is in flight or open, the objects destroyed since the last frame go at once too. A device that holds nothing for later says why in its code.
    virtual void trim() = 0;

    // Makes the swapchain follow a new size of the drawable and of its window in pixels, from the next frame.
    virtual void resizeSurface(const Surface::Size& size) = 0;

    // Lets go of the surface while the platform has none, such as an Android app in the background, and takes the new one when it comes back, keeping every object the device made. A frame without a surface records nothing into the swapchain.
    virtual void detachSurface() = 0;
    virtual void attachSurface(const Surface& surface) = 0;

    // Makes a swapchain on a surface beyond the window, such as a window of the app on another display, with the color format, the samples and the depth and stencil of the swapchain of the window, so the swapchain pipelines draw into it too. Returns its number from 1, which swapchain passes name, and which no other surface takes while it lives. Throws `std::runtime_error` when the device cannot draw into the surface.
    [[nodiscard]] virtual int addSurface(const Surface& surface) = 0;

    // Makes the swapchain of a surface that `addSurface` made follow a new size of its drawable and of its window in pixels, from the next frame.
    virtual void resizeSurface(int surface, const Surface::Size& size) = 0;

    // Lets go of a surface that `addSurface` made, whose objects the frames in flight still draw into wait for them.
    virtual void removeSurface(int surface) = 0;

    // Starts copying the pixels of the first mip level and layer of a render target, which must come after the frame that renders it was committed.
    [[nodiscard]] virtual std::unique_ptr<Readback> readImage(Image image, int width, int height) = 0;

    // Starts copying a range of a buffer that is a copy source, at an offset and of a size that are multiples of 4, outside a pass. The copy reads what the committed frames left in the buffer, so it comes after the frame that wrote it was committed.
    [[nodiscard]] virtual std::unique_ptr<Readback> readBuffer(Buffer buffer, std::size_t offset, std::size_t size) = 0;

    [[nodiscard]] virtual CaptureSupport getCaptureSupport() const = 0;

    // Starts recording every command of the device into a capture of its capture tool, between frames, at a path without its extension, which the tool adds. Throws `std::logic_error` without a capture tool or while a capture records, and `std::runtime_error` when the tool refuses to start.
    virtual void beginCapture(const std::filesystem::path& path) = 0;

    // Ends the capture after the commit of the last frame it records, and returns the path of the file that the tool wrote. Throws `std::logic_error` when no capture records, and `std::runtime_error` when the tool wrote no file.
    [[nodiscard]] virtual std::filesystem::path endCapture() = 0;
};

} // namespace haylen::gpu
