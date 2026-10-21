#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/2d/graphics/Mesh.hpp"
#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/PostProcess.hpp"
#include "haylen/2d/graphics/Shape.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/2d/graphics/SpriteData.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/2d/graphics/Stroke.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {
class Device;
class Viewport;
} // namespace haylen::graphics

namespace haylen::lighting2d {
struct Light;
struct Occluder;
} // namespace haylen::lighting2d

namespace haylen::math {
class Spline;
}

namespace haylen::text {
class Font;
class FontFamily;
class RichText;
struct Layout;
} // namespace haylen::text

namespace haylen::graphics2d {

struct GroupScope;
struct RendererState;

// Records 2D draw commands into canvases during a frame and submits them to the GPU at the end of the frame.
class Renderer final {
  public:
    // The kinds of canvas: a world canvas through a camera, a screen canvas in design units and a render target canvas.
    enum class CanvasKind : std::uint8_t {
        World,
        Screen,
        Target,
    };

    // Draws into a canvas that is about to close, over what it holds and in its coordinates.
    using CanvasOverlay = std::function<void(Renderer& renderer)>;

    // Inside each layer, draws keep their order, sort by depth, or sort by the y they stand on so lower ones draw later.
    enum class SortMode : std::uint8_t {
        Layer,
        Depth,
        Y,
    };

    struct CanvasOptions {
        SortMode sort = SortMode::Layer;

        // Canvases reach their destination by order, lowest first, and canvases with the same order in the order they began.
        int order = 0;
        std::uint32_t visibilityMask = 0xFFFFFFFFU;
        std::optional<math::Color> ambientLight;
        std::optional<PostProcess> postProcess;
        std::optional<math::Color> clear;
    };

    // How `drawMetaballs` shows the surface where its soft circles meet: the fill color, and an outline in the band of the field just above the threshold. The field adds up to 1 at the center of a lone circle, whose surface reaches the radius at the default threshold.
    // Whether the content of a mask draws inside its shape or outside it.
    enum class MaskMode : std::uint8_t {
        Inside,
        Outside,
    };

    // How a group draws once it ends: the opacity of the whole image and its place among the draws of its canvas.
    struct GroupOptions {
        float opacity = 1.0F;
        DrawOrder order{};
    };

    struct MetaballStyle {
        math::Color color = math::Color::white();
        math::Color outlineColor = math::Color::transparent();
        float outlineWidth = 0.0F;
        float threshold = 0.5F;
    };

    // A textured draw that a canvas holds: the corners of a sprite, of a piece of a nine-slice or of a baked batch, or the bounds of a block of text, with the label of the texture of the first quad of a draw.
    struct Drawn {
        std::array<math::Vec2, 4> corners{};
        bool text = false;
        std::string_view label;
    };

    using DrawnVisitor = std::function<void(const Drawn& drawn)>;

    struct Stats {
        std::size_t canvases = 0;
        std::size_t passes = 0;
        std::size_t drawCalls = 0;
        std::size_t sprites = 0;
        std::size_t instances = 0;
        std::size_t vertices = 0;
        std::size_t indices = 0;
        std::size_t lights = 0;
        std::size_t occluders = 0;
        std::size_t shadows = 0;
        std::size_t textureSwitches = 0;
        std::size_t uploadedBytes = 0;
    };

    Renderer(graphics::Device& device, core::JobSystem& jobs);
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // World canvases draw through the camera into its viewport, or across the visible area when it has none. Screen canvases use design coordinates of the visible area. Render target canvases draw through the camera into the target, where a camera viewport is in target pixels. World and render target canvases take lighting and post-processing.
    void beginWorld(const Camera& camera, const CanvasOptions& options = kDefaultCanvas);
    void beginScreen(const CanvasOptions& options = kDefaultCanvas);
    void beginTarget(const graphics::RenderTarget& target, const Camera& camera, const CanvasOptions& options = kDefaultCanvas);

    // Sends the world and screen canvases that begin until the capture ends into the target instead of the screen, cleared to the color first. The visible area covers the whole target, so a target with the pixel size of the viewport captures the frame as the screen would show it. A capture can begin inside another one, which takes the canvases back once it ends. A capture left open ends with the frame.
    void beginCapture(const graphics::RenderTarget& target, math::Color clear = math::Color::transparent());
    void endCapture();

    // A sprite or a batch whose order has a part mask recolors its parts with the part colors of each sprite, where a batch gives white to the sprites past the end of its part colors.
    void draw(const Sprite& sprite);
    void drawBatch(const graphics::Texture& texture, std::span<const SpriteInstance> sprites, const DrawOrder& order = {}, std::span<const PartColors> partColors = {});

    // Draws the sprites a buffer of floats holds, each one the template of the layout with the fields the values give it, without a sprite list in between.
    void drawBatch(const graphics::Texture& texture, std::span<const float> values, const SpriteLayout& layout, const DrawOrder& order = {});

    // Draws sprites that pick their texture by their data among up to `SpriteData::kMaxTextures` textures with the same filter, wrap and mipmaps, in one draw call, and hand the material of the order the numbers of their data, where sprites past the end of the data take the first texture and zeros. A batch with several textures draws without a part mask, a normal map or a distortion.
    void drawBatch(std::span<const graphics::Texture> textures, std::span<const SpriteInstance> sprites, std::span<const SpriteData> data, const DrawOrder& order = {}, std::span<const PartColors> partColors = {});
    void drawBatch(std::span<const graphics::Texture> textures, std::span<const float> values, const SpriteLayout& layout, const DrawOrder& order = {});
    // Draws a baked batch shifted by an offset in world units, which lets parallax layers reuse one batch. A baked batch takes no part mask.
    void drawStatic(const StaticSpriteBatch& batch, const DrawOrder& order = {}, math::Vec2 offset = {});
    void drawNineSlice(const NineSlice& slice, const math::Rect& area, math::Color color = math::Color::white(), const DrawOrder& order = {}, float borderScale = 1.0F);
    // Draws a vector image in the place, size and look of the sprite, whose texture and source come from the raster of the image at the scale it covers on the screen, made on the task pool and kept in the vector atlas of the renderer. A sprite without a size takes the size of the image. A vector image draws without a part mask.
    void drawVector(const graphics::VectorImage& image, const Sprite& sprite);
    // Draws plain text with a font alone or with a family and its fallbacks, turned and stretched by the style around the position, where its anchor lands.
    void drawText(text::Font& font, std::string_view content, math::Vec2 position, const text::Style& style = {}, const DrawOrder& order = {});
    void drawText(text::FontFamily& family, std::string_view content, math::Vec2 position, const text::Style& style = {}, const DrawOrder& order = {});
    // Draws rich text with the top-left of its block at the position, scaled from that corner and with its colors multiplied by the tint. A y-sorted canvas sorts it by the bottom of the block.
    void drawRichText(text::RichText& richText, math::Vec2 position, const DrawOrder& order = {}, math::Vec2 scale = {1.0F, 1.0F}, math::Color tint = math::Color::white());
    void drawMesh(const graphics::Texture& texture, std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices, const DrawOrder& order = {});

    // Draws a kept mesh with its vertices placed by the transform.
    void drawMesh(const Mesh& mesh, const DrawOrder& order = {}, const math::Transform2D& transform = {});

    // Draws a line through the points with the joins, caps, widths, colors, texture and dashes of the stroke, whose edges fade over one pixel of the destination. Throws `std::invalid_argument` for a stroke whose values cannot draw.
    void drawStroke(std::span<const math::Vec2> points, const Stroke& stroke, const DrawOrder& order = {});

    // Draws a stroke along the spline, flattened so the straight lines between its points stay within a quarter pixel of the curve, closed when the spline is. Throws `std::invalid_argument` for a stroke with widths or colors for each point, which a curve does not have.
    void drawStroke(const math::Spline& spline, const Stroke& stroke, const DrawOrder& order = {});
    void drawImageBlend(const ImageBlend& blend, const DrawOrder& order = {});

    // Lights and occluders belong to lit canvases, and a light draws in the order it was drawn over the lights before it. Disabled lights draw nothing.
    void drawLight(const lighting2d::Light& light);
    void drawOccluder(const lighting2d::Occluder& occluder);

    // Adds up a soft circle of the radius around every point into a field and draws the surface where the field reaches the threshold, which merges nearby circles into one smooth shape, such as a liquid made of particles.
    void drawMetaballs(std::span<const math::Vec2> points, float radius, const MetaballStyle& style = kDefaultMetaballs, const DrawOrder& order = {});

    void drawRect(const math::Rect& rect, math::Color color, const DrawOrder& order = {});
    void drawRectOutline(const math::Rect& rect, float thickness, math::Color color, const DrawOrder& order = {});
    void drawLine(math::Vec2 from, math::Vec2 to, float thickness, math::Color color, const DrawOrder& order = {});
    void drawPolyline(std::span<const math::Vec2> points, float thickness, math::Color color, bool closed = false, const DrawOrder& order = {});

    // Circles, ellipses, rings, arcs and shapes draw as one quad each, whose edges the shader covers, so they fade over one pixel of the destination at any size.
    void drawCircle(math::Vec2 center, float radius, math::Color color, const DrawOrder& order = {});
    void drawEllipse(math::Vec2 center, math::Vec2 radii, math::Color color, const DrawOrder& order = {});
    void drawRing(math::Vec2 center, float radius, float thickness, math::Color color, const DrawOrder& order = {});
    void drawArc(math::Vec2 center, float radius, float thickness, float startAngle, float endAngle, math::Color color, const DrawOrder& order = {});
    void drawShape(const Shape& shape, const DrawOrder& order = {});

    // Fills a circle of the radius at every point, all of the one color of `colors` or each of its own, as one quad each of the shape program, which merge into one draw call. Throws `std::invalid_argument` for colors of another count.
    void drawPoints(std::span<const math::Vec2> points, float radius, std::span<const math::Color> colors, const DrawOrder& order = {});

    // A polygon fills the triangles of its outline and fades its edge out over one pixel of the destination.
    void drawPolygon(std::span<const math::Vec2> points, math::Color color, const DrawOrder& order = {});

    // Outlines every textured quad and text block the open canvas holds so far, in the color, and names each sprite after the path of its texture with the font, at a size in units of the destination: sprites, batches, nine-slice pieces, baked batches and glyphs drawn together.
    void drawBounds(text::Font& font, math::Color color, float labelSize);

    // Calls the visitor with every textured quad and text block the open canvas holds so far, in canvas coordinates, the way `drawBounds` outlines them. The visitor may draw, and what it draws is not visited.
    void visitDrawn(const DrawnVisitor& visitor) const;

    // Runs an overlay in every canvas just before it closes, with the clips and layer offsets of the canvas cleared, such as the debug drawings of the engine. Overlays never run while an overlay draws. The returned id removes the overlay.
    std::uint64_t addCanvasOverlay(CanvasOverlay overlay);
    void removeCanvasOverlay(std::uint64_t id);

    // Returns the kind of the open canvas. It needs an active canvas.
    [[nodiscard]] CanvasKind getCanvasKind() const;

    // Names the canvas kinds `world`, `screen` and `target`.
    [[nodiscard]] static std::string_view canvasKindName(CanvasKind value) noexcept;

    // Restricts following draws of the current canvas to a rectangle in canvas coordinates until popped.
    void pushClip(const math::Rect& rect);
    void popClip();

    // Draws what follows until `endGroup` into an image of the open canvas, which then draws as one with the opacity and the draw order of the options, so overlapping parts fade together. A group sorts its draws among themselves like a canvas of its own and composites with the blend of its order, unlit and without post-processing of its own, a blend that mixes with what lies below reads a copy of what its canvas drew before it, and a canvas that ends closes the groups and masks it has open.
    void beginGroup(const GroupOptions& options);
    void endGroup();

    // Records what follows until `endMaskShape` as the shape of a mask, drawn with anything, and draws what follows from there until `popMask` only inside the shape or outside it, through the coverage of the shape, soft edges included, as one image with the draw order. Masks and groups nest.
    void pushMask(MaskMode mode = MaskMode::Inside, const DrawOrder& order = {});
    void endMaskShape();
    void popMask();

    [[nodiscard]] static std::optional<MaskMode> maskModeFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view maskModeName(MaskMode value) noexcept;

    // Adds the offset to the layer of the following draws of the current canvas until popped. Nested offsets add up.
    void pushLayerOffset(int offset);
    void popLayerOffset();

    [[nodiscard]] StaticSpriteBatch createStaticBatch(const graphics::Texture& texture, std::span<const SpriteInstance> sprites);
    [[nodiscard]] const graphics::Texture& getLightTexture() const noexcept;

    // Tells whether light maps store light in floating point, where the backend can render and blend it, so lights brighter than 1 brighten the scene beyond its unlit colors. Otherwise light saturates at 1.
    [[nodiscard]] bool isHdrLighting() const noexcept;
    [[nodiscard]] const Stats& getStats() const noexcept;
    [[nodiscard]] math::Rect getCanvasBounds() const noexcept;

    // Returns the length in canvas coordinates of one unit of the destination, a design unit on the screen or a pixel of a render target, so outlines keep their thickness at any zoom. It needs an active canvas.
    [[nodiscard]] float getCanvasUnitSize() const;

    // Tells whether the open canvas is lit, the only kind of canvas that takes lights. It needs an active canvas.
    [[nodiscard]] bool isCanvasLit() const;
    [[nodiscard]] bool isCapturing() const noexcept;

    // The engine begins and ends every frame, and submits everything drawn in between at its end into the swapchain, whose alpha a transparent window composes with the desktop.
    void beginFrame(const graphics::Viewport& viewport, math::Color clearColor);
    void endFrame(bool transparent);

  private:
    static const CanvasOptions kDefaultCanvas;
    static const std::array<std::pair<std::string_view, CanvasKind>, 3> kCanvasKindNames;
    static const MetaballStyle kDefaultMetaballs;

    // A soft circle whose kernel spans this many radii reaches the threshold of 0.5 at its radius, since `(1 - d^2)^2` is 0.5 at `d = sqrt(1 - sqrt(0.5))`.
    static constexpr float kMetaballReach = 1.8477591F;

    static void validatePostProcess(const CanvasOptions& options);

    [[nodiscard]] static std::vector<std::uint8_t> radialFalloff(int size);
    [[nodiscard]] static std::vector<std::uint8_t> metaballKernel(int size);
    [[nodiscard]] static float lowestPoint(std::span<const math::Vec2> points) noexcept;

    void drawTextLayout(const text::Layout& layout, math::Vec2 position, const text::Style& style, const DrawOrder& order);
    void addMesh(const graphics::Texture& texture, std::span<const MeshVertex> vertices, std::span<const std::uint32_t> indices, const DrawOrder& order, const math::Transform2D* transform);

    static constexpr std::array<std::pair<std::string_view, MaskMode>, 2> kMaskModeNames{{{"inside", MaskMode::Inside}, {"outside", MaskMode::Outside}}};

    // Opens the canvas of a group over the open canvas, and closes it again, moving its draws ahead of those of the canvas it drew over, which then opens again, and returns its image.
    void openGroup(GroupScope scope);
    graphics::Texture closeGroup();
    void composite(const GroupScope& scope, const graphics::Texture& image);

    // Ends every group and mask the open canvas has.
    void closeGroups();

    // Runs the overlays in the open canvas and closes it.
    void finishCanvas();

    // Adds `count` sprites of the textures, where `spriteAt` returns the sprite at an index from any worker thread, `partsAt` its part colors when the order recolors them, and `dataAt` its data when the sprites have any.
    template <typename SpriteAt, typename PartsAt, typename DataAt> void addBatch(std::span<const graphics::Texture> textures, std::size_t count, const DrawOrder& order, const SpriteAt& spriteAt, const PartsAt& partsAt, const DataAt& dataAt, bool hasData);

    // Throws when the textures cannot share one draw call, or when a sprite names a texture the batch lacks.
    template <typename DataAt> static void checkBatch(std::span<const graphics::Texture> textures, std::size_t count, const DataAt& dataAt, bool hasData);

    std::unique_ptr<RendererState> state;
};

} // namespace haylen::graphics2d
