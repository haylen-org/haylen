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
#include "haylen/2d/graphics/MeshVertex.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/PartColors.hpp"
#include "haylen/2d/graphics/PostProcess.hpp"
#include "haylen/2d/graphics/Sprite.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/SpriteLayout.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/graphics/VectorImage.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/text/Style.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics {
class Device;
class Viewport;
struct FrameTarget;
} // namespace haylen::graphics

namespace haylen::lighting2d {
struct Light;
struct Occluder;
} // namespace haylen::lighting2d

namespace haylen::text {
class Font;
class FontFamily;
class RichText;
struct Layout;
} // namespace haylen::text

namespace haylen::graphics2d {

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
    void drawCircle(math::Vec2 center, float radius, math::Color color, const DrawOrder& order = {}, int segments = 0);
    void drawRing(math::Vec2 center, float radius, float thickness, math::Color color, const DrawOrder& order = {}, int segments = 0);
    void drawArc(math::Vec2 center, float radius, float thickness, float startAngle, float endAngle, math::Color color, const DrawOrder& order = {}, int segments = 0);
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

    // The engine begins and ends every frame, and submits everything drawn in between at its end.
    void beginFrame(const graphics::Viewport& viewport, math::Color clearColor);
    void endFrame(const graphics::FrameTarget& target);

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

    // Runs the overlays in the open canvas and closes it.
    void finishCanvas();

    // Adds `count` sprites of one texture, where `spriteAt` returns the sprite at an index from any worker thread, and `partsAt` its part colors when the order recolors them.
    template <typename SpriteAt, typename PartsAt> void addBatch(const graphics::Texture& texture, std::size_t count, const DrawOrder& order, const SpriteAt& spriteAt, const PartsAt& partsAt);

    std::unique_ptr<RendererState> state;
};

} // namespace haylen::graphics2d
