#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "2d/graphics/BlendDraw.hpp"
#include "2d/graphics/Canvas.hpp"
#include "2d/graphics/Capture.hpp"
#include "2d/graphics/Command.hpp"
#include "2d/graphics/DrawItem.hpp"
#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/GpuVertex.hpp"
#include "2d/graphics/GroupScope.hpp"
#include "2d/graphics/LightDraw.hpp"
#include "2d/graphics/LightInstance.hpp"
#include "2d/graphics/LitTargets.hpp"
#include "2d/graphics/MetaballDraw.hpp"
#include "2d/graphics/PolygonMesh.hpp"
#include "2d/graphics/Program.hpp"
#include "2d/graphics/Shade.hpp"
#include "2d/graphics/StaticBatchResource.hpp"
#include "2d/graphics/StrokeBuilder.hpp"
#include "2d/graphics/TextPainter.hpp"
#include "2d/graphics/VectorAtlas.hpp"
#include "2d/lighting/ShadowMap.hpp"
#include "graphics/PassTarget.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Shader.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {
struct TextureResource;
} // namespace haylen::graphics

namespace haylen::graphics2d {

struct MaterialResource;

// Everything a renderer records during a frame, and the GPU objects it keeps across frames.
struct RendererState {
    // Batches this large convert their instances on the worker pool while the frame thread takes its own share.
    static constexpr std::size_t kParallelThreshold = 8192;
    static constexpr std::size_t kParallelGrain = 4096;
    static constexpr int kLightTextureSize = 128;
    static constexpr int kMetaballTextureSize = 64;
    static constexpr std::size_t kProgramCount = 13;

    // The bind slots of the programs of the renderer, which their sources declare with `layout(binding = ...)` and `getProgram` checks against the reflection of their packages. Every program reads its view projection from the vertex block slot and the lighting of lit passes from the lit block slot, which the shader library reserves.
    static constexpr int kVertexBlock = 0;
    static constexpr std::uint32_t kVertexBlockSize = 80;
    static constexpr std::uint32_t kLitBlockSize = 32;
    static constexpr int kFragmentBlock = 1;
    static constexpr int kLitBlock = 7;
    static constexpr int kSpriteTexture = 0;
    static constexpr int kSpriteSampler = 0;
    static constexpr int kNormalTexture = 1;
    static constexpr int kPartMask = 1;
    static constexpr int kBatchTextures = 25;
    static constexpr int kLightShape = 0;
    static constexpr int kLightSurface = 1;
    static constexpr int kLightInfo = 2;
    static constexpr int kLightShadow = 3;
    static constexpr int kLightShapeSampler = 0;
    static constexpr int kLightSurfaceSampler = 1;
    static constexpr int kLightShadowSampler = 2;
    static constexpr int kCompositeScene = 0;
    static constexpr int kCompositeLight = 1;
    static constexpr int kCompositeEmission = 2;
    static constexpr int kCompositeDistortion = 3;
    static constexpr int kCompositeBloom = 4;
    static constexpr int kCompositeLut = 5;
    static constexpr int kFilterSource = 0;
    static constexpr int kBackdropImage = 0;
    static constexpr int kBackdropMask = 1;
    static constexpr int kBackdropCopy = 2;
    static constexpr int kBlendFrom = 0;
    static constexpr int kBlendTo = 1;
    static constexpr int kMetaballField = 0;

    // The submissions numbered so far on the frame thread, where one engine runs at a time.
    static std::uint64_t submissions;

    graphics::Device& device;
    core::JobSystem& jobs;

    graphics::Texture white;
    graphics::Texture light;
    graphics::Texture metaball;
    gpu::Device::Buffer quad;

    // Two instances that cover the unit square, the second with its rows flipped for render targets that start at the bottom, which post-processing materials draw with.
    gpu::Device::Buffer screen;

    // The packages of the programs of the renderer, read from the engine when it starts, and their pipelines.
    std::vector<graphics::Shader> packages;
    std::unordered_map<std::uint32_t, gpu::Device::Pipeline> pipelines;
    gpu::Device::PixelFormat lightFormat = gpu::Device::PixelFormat::Rgba8;

    // One row of the shadow atlas per shadowed light of the frame, written every frame that draws lights.
    gpu::Device::Image shadowImage;
    int shadowRows = 0;
    std::vector<float> shadowTexels;

    std::vector<GpuInstance> instances;
    std::vector<GpuInstance> upload;
    std::vector<GpuInstance> splats;
    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> uploadIndices;
    std::vector<BlendDraw> blends;
    std::vector<DrawItem> items;
    std::vector<Command> commands;
    std::vector<Canvas> canvases;
    std::vector<Capture> captures;
    std::vector<std::size_t> openCaptures;
    std::vector<std::size_t> closedCaptures;
    std::vector<math::Rect> clips;
    std::vector<std::uint32_t> clipStack;
    std::vector<int> layerOffsets;
    std::vector<Shade> shades;
    std::vector<std::uint8_t> uniformBytes;
    std::vector<graphics::TextureResource*> shadeTextures;
    std::vector<LightDraw> lights;
    std::vector<LightInstance> lightInstances;
    std::vector<lighting2d::ShadowMap::Segment> segments;
    std::vector<MetaballDraw> metaballs;
    std::vector<LitTargets> litTargets;

    // The open groups and masks from the outermost, the clips and layer offsets of the canvases they draw over, and the images they draw into, which frames reuse by their order.
    std::vector<GroupScope> groups;
    std::vector<std::uint32_t> savedClips;
    std::vector<int> savedLayerOffsets;
    std::vector<graphics::RenderTarget> groupTargets;
    std::size_t groupTargetsUsed = 0;
    std::vector<graphics::RenderTarget> fields;
    std::vector<std::shared_ptr<graphics::TextureResource>> retained;
    std::vector<std::shared_ptr<StaticBatchResource>> retainedBatches;
    std::vector<std::shared_ptr<MaterialResource>> retainedMaterials;

    // The number of the submission the frame goes into, unique across every renderer of the process, which a resource the frame already keeps alive carries so it is kept once.
    std::uint64_t epoch = ++submissions;

    // Reused by the draws that build their instances, vertices or nine-slice patches before recording them, so drawing allocates nothing once they have grown.
    std::vector<GpuInstance> scratchInstances;
    std::vector<GpuVertex> scratchVertices;
    std::vector<std::uint32_t> scratchIndices;
    std::vector<math::Vec2> polygonPoints;
    std::vector<std::uint32_t> polygonCounts;
    std::vector<NineSlice::Patch> patches;
    std::vector<math::Vec2> curvePoints;

    // Reused by the submission of every frame: the draw items of a canvas in their sorted order, the instance ranges of the commands in draw order, the shadowed lights with their canvases, and the canvases of a destination by their order.
    std::vector<std::uint32_t> itemOrder;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> placements;
    std::vector<std::pair<LightDraw*, const Canvas*>> shadowCasts;
    std::vector<const Canvas*> orderedCanvases;

    gpu::Device::Buffer instanceBuffer;
    std::size_t instanceCapacity = 0;
    gpu::Device::Buffer vertexBuffer;
    std::size_t vertexCapacity = 0;
    gpu::Device::Buffer indexBuffer;
    std::size_t indexCapacity = 0;
    gpu::Device::Buffer lightBuffer;
    std::size_t lightCapacity = 0;

    // The streaming buffers above count as one `RendererBuffers` object with their bytes in the debug statistics.
    static debug::ObjectCounter& bufferCounter;
    std::size_t bufferBytes = 0;
    debug::TrackedObject buffersTracked{bufferCounter};

    VectorAtlas vectors;

    StrokeBuilder strokes;
    PolygonMesh polygons;

    // Turns the layout of every text draw into instances, reusing its batches from draw to draw.
    TextPainter painter;

    // The canvas overlays by id, the copy of them that runs as a canvas closes, and whether they are drawing, which keeps them from running inside each other.
    std::vector<std::pair<std::uint64_t, Renderer::CanvasOverlay>> overlays;
    std::vector<std::pair<std::uint64_t, Renderer::CanvasOverlay>> overlaySnapshot;
    std::uint64_t nextOverlay = 1;
    bool drawingOverlays = false;

    math::Rect pixelRect{};
    math::Rect visibleRect{};
    math::Color clearColor = math::Color::black();
    std::uint64_t frame = 0;
    std::uint32_t sequence = 0;
    int layerOffset = 0;
    bool canvasOpen = false;
    Renderer::Stats stats{};

    RendererState(graphics::Device& graphicsDevice, core::JobSystem& jobSystem) : device(graphicsDevice), jobs(jobSystem), white(graphicsDevice.getWhiteTexture()), vectors(graphicsDevice, jobSystem), painter(white) {}

    [[nodiscard]] gpu::Device& getGpu() noexcept {
        return device.getGpu();
    }

    // Returns the open canvas, and throws when no canvas is active.
    [[nodiscard]] Canvas& getCanvas();

    // Returns the rectangle in pixels of its destination, the screen, a render target or a capture, that a canvas covers.
    [[nodiscard]] math::Rect getPassRect(const Canvas& canvas) const;

    // Returns how many pixels of its destination one unit of the open canvas covers, along the axis the view stretches the most.
    [[nodiscard]] float getPixelsPerUnit();

    // Returns how many units of the open canvas one pixel of its destination spans, along the axis the view stretches the least.
    [[nodiscard]] float getPixelSize();

    // Returns the pixels of the destination of the open canvas in its units, or nothing when its view turns or skews them.
    [[nodiscard]] std::optional<TextPainter::PixelGrid> getPixelGrid();

    // Tells whether a draw with the order shows in the open canvas, whose visibility mask may leave it out.
    [[nodiscard]] bool accepts(const DrawOrder& order);

    // Keeps the texture or the baked batch alive until the frame is submitted.
    void retain(const graphics::Texture& texture);
    void retain(const std::shared_ptr<StaticBatchResource>& batch);

    // Returns the next image of the groups of the frame, with the size of the pass of a canvas.
    [[nodiscard]] std::size_t acquireGroupTarget(math::Vec2 size);

    void openCanvas(Canvas next);
    void closeCanvas();
    void closeCapture();
    void resetFrame() noexcept;

    // Throws when a draw would sample the image its canvas draws into, which no backend can read and write in one pass.
    void requireReadable(const graphics::TextureResource* texture);

    // The standing y is the point a y-sorted canvas sorts the draw by, and a draw of the batch program names every texture of its batch, the first one being the texture of the draw.
    DrawItem& addItem(Program program, const DrawOrder& order, graphics::TextureResource* texture, float standingY, std::span<const graphics::Texture> textures = {});

    // Scales the coverage of the instances or vertices a distortion draw recorded by its distortion, so their alpha adds up in the distortion map.
    static void scaleCoverage(std::span<GpuInstance> data, Program program, float distortion) noexcept;
    static void scaleCoverage(std::span<GpuVertex> data, float distortion) noexcept;
    void addInstances(Program program, const DrawOrder& order, const graphics::Texture& texture, std::span<const GpuInstance> data, float standingY);

    // Records a shape whose quad reaches a pixel of the destination past its edge, where the edge fades out, and half its softness further. A shape stands on the lowest point of its turned rectangle.
    void addShape(const Shape& shape, const DrawOrder& order);

    // Meshes stand on their lowest vertex.
    void addMesh(const graphics::Texture& texture, std::span<const GpuVertex> meshVertices, std::span<const std::uint32_t> meshIndices, const DrawOrder& order);

    // Adds a shade that copies the values a material has now, for the post-processing passes of the frame.
    std::uint32_t addMaterialShade(const Material& material);

    // Tells whether a blend mode expects colors premultiplied by their alpha from the shader, which the programs then write through `haylen_output`.
    [[nodiscard]] static bool expectsPremultiplied(graphics::BlendMode::Type mode) noexcept;

    // Returns the pipeline blend of a blend mode, which mixes what the shader wrote with the target, so every mode but `premultiplied` blends the straight colors of a draw.
    [[nodiscard]] static gpu::Device::Blend blendState(graphics::BlendMode::Type mode) noexcept;

    // Returns the pipeline of a program, blend mode and pass target, creating it on first use. The light program takes the blend of the light instead.
    [[nodiscard]] gpu::Device::Pipeline getPipeline(Program program, std::uint8_t blend, graphics::PassTarget target);

    // Returns the pipeline of a material for the program it replaces, which lives with the material's shader, and the program with its bind slots.
    [[nodiscard]] gpu::Device::Pipeline getMaterialPipeline(MaterialResource& material, Program program, std::uint8_t blend, graphics::PassTarget target);
    [[nodiscard]] const graphics::ShaderResource::Compiled& getMaterialProgram(MaterialResource& material, Program program, graphics::PassTarget target);

    // Grows a dynamic buffer to hold at least the needed number of elements.
    void ensureBuffer(gpu::Device::Buffer& buffer, std::size_t& capacity, std::size_t needed, std::size_t elementSize, bool isIndexBuffer);

    // Reads the packages of the programs of the renderer and starts compiling them in the background, so the first draws that make them do not wait for a cold compiler.
    void loadPrograms();

  private:
    // A program of the renderer: its package, its program in the package, and the name of its version for lit canvases where it draws into them.
    struct ProgramSource {
        std::size_t package = 0;
        std::string_view program;
        std::string_view lit;
    };

    // A vertex input of a program, which a pipeline reads from one of its buffers at an offset.
    struct Input {
        std::string_view name;
        int buffer = 0;
        int offset = 0;
        gpu::Device::VertexFormat format = gpu::Device::VertexFormat::Float;
    };

    // The vertex buffers, inputs and primitive of the programs that share a layout.
    struct Layout {
        std::array<gpu::Device::VertexBufferLayout, gpu::Device::kMaxVertexBuffers> buffers{};
        std::size_t bufferCount = 0;
        std::span<const Input> inputs;
        gpu::Device::Primitive primitive = gpu::Device::Primitive::TriangleStrip;
        gpu::Device::IndexType indexType = gpu::Device::IndexType::None;
    };

    // The colors a pipeline writes for a pass target, with their formats, masks and blends.
    struct Targets {
        std::array<gpu::Device::ColorTarget, gpu::Device::kMaxColorTargets> colors{};
        std::size_t count = 1;
        int sampleCount = 1;
    };

    static const std::array<std::span<const std::uint8_t> (*)(), 11> kPackages;
    static const std::array<ProgramSource, kProgramCount> kPrograms;
    static const std::array<Input, 9> kSpriteInputs;
    static const std::array<Input, 11> kBatchInputs;
    static const std::array<Input, 13> kRecolorInputs;
    static const std::array<Input, 15> kEffectInputs;
    static const std::array<Input, 9> kShapeInputs;
    static const std::array<Input, 10> kLightInputs;
    static const std::array<Input, 3> kMeshInputs;
    static const std::array<Input, 1> kQuadInputs;

    [[nodiscard]] const graphics::ShaderResource::Compiled& getProgram(Program program, bool lit);
    [[nodiscard]] static std::uint32_t pipelineKey(Program program, std::uint8_t blend, graphics::PassTarget target) noexcept;
    [[nodiscard]] static gpu::Device::Blend lightBlend(std::uint8_t blend) noexcept;
    [[nodiscard]] static const char* materialProgramName(Program program, graphics::PassTarget target);

    // Returns the index of the shade of a draw, reusing the last shade when the draw shades the same way.
    [[nodiscard]] std::uint32_t getShade(Program program, const DrawOrder& order, std::span<const graphics::Texture> textures);
    [[nodiscard]] std::uint32_t pushShade(Shade shade, const Material& material);
    [[nodiscard]] static Layout describeLayout(Program program);
    [[nodiscard]] Targets describeTargets(std::uint8_t blend, graphics::PassTarget target) const;

    // Makes a pipeline of a program of a package with the layout of a program of the renderer and the colors of a pass target.
    [[nodiscard]] gpu::Device::Pipeline makePipeline(const graphics::ShaderResource& package, std::string_view programName, gpu::Device::Program made, Program program, std::uint8_t blend, graphics::PassTarget target, std::string_view label);
};

} // namespace haylen::graphics2d
