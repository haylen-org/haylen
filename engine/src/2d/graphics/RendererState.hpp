#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "2d/graphics/Canvas.hpp"
#include "2d/graphics/Capture.hpp"
#include "2d/graphics/Command.hpp"
#include "2d/graphics/DrawItem.hpp"
#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/GpuVertex.hpp"
#include "2d/graphics/LightDraw.hpp"
#include "2d/graphics/LitTargets.hpp"
#include "2d/graphics/MetaballDraw.hpp"
#include "2d/graphics/Program.hpp"
#include "2d/graphics/Shade.hpp"
#include "2d/graphics/StaticBatchResource.hpp"
#include "2d/graphics/TextPainter.hpp"
#include "2d/lighting/ShadowMap.hpp"
#include "graphics/PassTarget.hpp"
#include "graphics/ShaderResource.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "sokol_gfx.h"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {
class Device;
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
    static constexpr std::size_t kProgramCount = 7;

    graphics::Device& device;
    core::JobSystem& jobs;

    graphics::Texture white;
    graphics::Texture light;
    graphics::Texture metaball;
    sg_buffer quad{};

    // Two instances that cover the unit square, the second with its rows flipped for render targets that start at the bottom, which post-processing materials draw with.
    sg_buffer screen{};

    // The programs of the renderer, made the first time a draw needs them.
    std::array<sg_shader, kProgramCount> shaders{};
    std::array<sg_shader, kProgramCount> litShaders{};
    std::unordered_map<std::uint32_t, sg_pipeline> pipelines;
    sg_pixel_format lightFormat = SG_PIXELFORMAT_RGBA8;

    // One row of the shadow atlas per shadowed light of the frame, written every frame that draws lights.
    sg_image shadowImage{};
    sg_view shadowView{};
    int shadowRows = 0;
    std::vector<float> shadowTexels;

    std::vector<GpuInstance> instances;
    std::vector<GpuInstance> upload;
    std::vector<GpuInstance> splats;
    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<std::uint32_t> uploadIndices;
    std::vector<ImageBlend> blends;
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
    std::vector<lighting2d::ShadowMap::Segment> segments;
    std::vector<MetaballDraw> metaballs;
    std::vector<LitTargets> litTargets;
    std::vector<graphics::RenderTarget> fields;
    std::vector<std::shared_ptr<graphics::TextureResource>> retained;
    std::vector<std::shared_ptr<StaticBatchResource>> retainedBatches;
    std::vector<std::shared_ptr<MaterialResource>> retainedMaterials;
    std::unordered_set<const void*> retainedSet;

    // Reused by the draws that build their instances or nine-slice patches before recording them, so drawing allocates nothing once they have grown.
    std::vector<GpuInstance> scratchInstances;
    std::vector<NineSlice::Patch> patches;

    sg_buffer instanceBuffer{};
    std::size_t instanceCapacity = 0;
    sg_buffer vertexBuffer{};
    std::size_t vertexCapacity = 0;
    sg_buffer indexBuffer{};
    std::size_t indexCapacity = 0;

    math::Rect pixelRect{};
    math::Rect visibleRect{};
    math::Color clearColor = math::Color::black();
    std::uint32_t sequence = 0;
    int layerOffset = 0;
    bool canvasOpen = false;
    Renderer::Stats stats{};

    RendererState(graphics::Device& graphicsDevice, core::JobSystem& jobSystem) : device(graphicsDevice), jobs(jobSystem) {}

    // Returns the open canvas, and throws when no canvas is active.
    [[nodiscard]] Canvas& getCanvas();

    // Returns the rectangle in pixels of its destination, the screen, a render target or a capture, that a canvas covers.
    [[nodiscard]] math::Rect getPassRect(const Canvas& canvas) const;

    // Returns the pixels of the destination of the open canvas in its units, or nothing when its view turns or skews them.
    [[nodiscard]] std::optional<TextPainter::PixelGrid> getPixelGrid();

    // Tells whether a draw with the order shows in the open canvas, whose visibility mask may leave it out.
    [[nodiscard]] bool accepts(const DrawOrder& order);

    // Keeps the texture alive until the frame is submitted.
    void retain(const graphics::Texture& texture);

    void openCanvas(Canvas next);
    void closeCanvas();
    void closeCapture();
    void resetFrame() noexcept;

    // The standing y is the point a y-sorted canvas sorts the draw by.
    // Throws when a draw would sample the image its canvas draws into, which no backend can read and write in one pass.
    void requireReadable(const graphics::TextureResource* texture);

    DrawItem& addItem(Program program, const DrawOrder& order, graphics::TextureResource* texture, float standingY);
    void addInstances(Program program, const DrawOrder& order, const graphics::Texture& texture, std::span<const GpuInstance> data, float standingY);

    // Meshes stand on their lowest vertex.
    void addMesh(const graphics::Texture& texture, std::span<const GpuVertex> meshVertices, std::span<const std::uint32_t> meshIndices, const DrawOrder& order);

    // Adds a shade that copies the values a material has now, for the post-processing passes of the frame.
    std::uint32_t addMaterialShade(const Material& material);

    // Tells whether a blend mode expects colors premultiplied by their alpha from the shader, which the programs then write through `haylen_output`.
    [[nodiscard]] static bool expectsPremultiplied(graphics::BlendMode::Type mode) noexcept;

    // Returns the pipeline blend of a blend mode, which mixes what the shader wrote with the target, so every mode but `premultiplied` blends the straight colors of a draw.
    [[nodiscard]] static sg_blend_state blendState(graphics::BlendMode::Type mode) noexcept;

    // Returns the pipeline of a program, blend mode and pass target, creating it on first use. The light program takes the blend of the light instead.
    [[nodiscard]] sg_pipeline getPipeline(Program program, std::uint8_t blend, graphics::PassTarget target);

    // Returns the pipeline of a material for the program it replaces, which lives with the material's shader, and the program with its bind slots.
    [[nodiscard]] sg_pipeline getMaterialPipeline(MaterialResource& material, Program program, std::uint8_t blend, graphics::PassTarget target);
    [[nodiscard]] const graphics::ShaderResource::Program& getMaterialProgram(MaterialResource& material, Program program, graphics::PassTarget target);

    // Grows a streaming buffer to hold at least the needed number of elements.
    void ensureBuffer(sg_buffer& buffer, std::size_t& capacity, std::size_t needed, std::size_t elementSize, bool isIndexBuffer);

    // Starts compiling the sources of every program of the renderer in the background, so the first draws that make them do not wait for a cold compiler.
    void precompilePrograms();

  private:
    using Description = const sg_shader_desc* (*)(sg_backend);

    // The generated description of each program, and of its version for lit canvases where the program draws into them.
    static const std::array<Description, kProgramCount> kPrograms;
    static const std::array<Description, kProgramCount> kLitPrograms;

    [[nodiscard]] sg_shader getShader(Program program, bool lit);
    [[nodiscard]] static std::uint32_t pipelineKey(Program program, std::uint8_t blend, graphics::PassTarget target) noexcept;
    [[nodiscard]] static sg_blend_state lightBlend(std::uint8_t blend) noexcept;
    [[nodiscard]] static const char* materialProgramName(Program program, graphics::PassTarget target);

    // Returns the index of the shade of a draw, reusing the last shade when the draw shades the same way.
    [[nodiscard]] std::uint32_t getShade(const DrawOrder& order);
    [[nodiscard]] std::uint32_t pushShade(Shade shade, const Material& material);
    void describeLayout(sg_pipeline_desc& desc, Program program) const;
    void describeTargets(sg_pipeline_desc& desc, std::uint8_t blend, graphics::PassTarget target) const;
};

} // namespace haylen::graphics2d
