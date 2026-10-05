#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include "2d/graphics/Canvas.hpp"
#include "2d/graphics/DrawItem.hpp"
#include "2d/graphics/LitTargets.hpp"
#include "2d/graphics/Program.hpp"
#include "graphics/PassTarget.hpp"
#include "haylen/2d/graphics/PostProcess.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"
#include "sokol_gfx.h"

namespace haylen::graphics {
struct FrameTarget;
struct TextureResource;
} // namespace haylen::graphics

namespace haylen::graphics2d {

struct Command;
struct RendererState;
struct Shade;

// Turns the canvases recorded during a frame into GPU passes: it casts the shadow maps, orders and merges the draws, uploads the instance data and renders the metaball fields, the offscreen, light, post-processing and capture passes and the swapchain pass.
class FrameSubmitter final {
  public:
    FrameSubmitter(RendererState& rendererState, const graphics::FrameTarget& frameTarget) : state(rendererState), target(frameTarget) {}

    // Every render target is created before the first pass, and a failure inside a pass ends it and the frame before it goes on.
    void submit();

  private:
    using Matrix = std::array<float, 16>;

    // The parts of the composite of a canvas: everything in one pass, the image before bloom and blur, or what comes after them.
    enum class Stage : std::uint8_t {
        Whole,
        First,
        Finish,
    };

    // The composite of a lit canvas without post-processing grades nothing.
    static const PostProcess kNoPostProcess;

    // Moves everything drawn with the matrix by an offset in world units.
    [[nodiscard]] static Matrix translated(Matrix matrix, math::Vec2 offset) noexcept;
    [[nodiscard]] static Matrix projection(const math::Transform2D& view, math::Vec2 viewSize) noexcept;
    [[nodiscard]] static bool isInstanced(const DrawItem& item) noexcept;

    void prepareTargets();
    LitTargets& getLitTargets(std::size_t index, math::Vec2 size, const Canvas& canvas);
    const graphics::RenderTarget& getField(std::size_t index, math::Vec2 size);
    void castShadows();
    void writeShadowAtlas(int rows);
    void buildCommands();
    void append(const DrawItem& item);
    void appendCanvas(const std::vector<std::uint32_t>& order, bool distortion, std::uint32_t& expected, bool& inOrder);
    void upload();
    void writeBuffer(sg_buffer& buffer, std::size_t& capacity, const void* data, std::size_t count, std::size_t elementSize, bool indexBuffer);

    void renderOffscreen();
    void renderFields(const Canvas& canvas);
    void renderCanvasOffscreen(Canvas& canvas);
    void renderLights(const Canvas& canvas, const LitTargets& targets);
    [[nodiscard]] const graphics::TextureResource& getLightShape(const lighting2d::Light& light) const;
    void renderPostChain(Canvas& canvas, const LitTargets& targets);
    void renderCapture(std::size_t index);
    void renderSwapchain();
    void drawCanvases(std::size_t capture, graphics::PassTarget pass);

    // Draws the finished image of a composited canvas into the pass: the composite, or the last post-processing material over the image before it.
    void drawFinal(const Canvas& canvas, graphics::PassTarget pass);
    // The clear colors of color images are premultiplied by their alpha, like everything drawn into them.
    void beginOffscreenPass(const graphics::RenderTarget& renderTarget, math::Color clear);
    void beginLitPass(const LitTargets& targets, math::Color clear);
    void endPass();
    void composite(const Canvas& canvas, graphics::PassTarget pass, Stage stage);

    // Draws the source into the target with the filter program, which shrinks it to half its size, keeping what passes the threshold when it is not negative, or blurs it along the step in texels of the source.
    void filter(const graphics::RenderTarget& source, const graphics::RenderTarget& destination, float threshold, math::Vec2 step);

    // Blurs the image of a stage at half its size over the radius in units of the canvas, into the first of the pair of targets.
    void blurInto(const Canvas& canvas, const graphics::RenderTarget& image, const std::array<graphics::RenderTarget, 2>& pair, float threshold, float radius);
    void drawPostMaterial(std::size_t shadeIndex, const graphics::Texture& image, graphics::PassTarget pass);

    void applyClip(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect);
    void drawCommands(const Canvas& canvas, std::size_t begin, std::size_t end, graphics::PassTarget pass, const math::Rect& passRect);
    void drawImageBlend(const Command& command, const Matrix& matrix);
    void drawMetaball(const Command& command, const Matrix& matrix);
    void applyUniforms(const Matrix& matrix, graphics::BlendMode::Type blend);

    // Applies the uniforms of how a command is shaded: the lighting of lit passes and the values of its material.
    void applyShade(const Shade& shade, graphics::PassTarget pass, Program program);

    // Binds the textures and samplers of a material program, where slot 0 takes the texture of the draw.
    void bindMaterial(sg_bindings& bindings, const Shade& shade, const graphics::TextureResource& texture, graphics::PassTarget pass, Program program);

    RendererState& state;
    const graphics::FrameTarget& target;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> placements;
    std::size_t instanceCursor = 0;
    std::size_t splatBase = 0;
    std::size_t commandFloor = 0;
    bool identity = true;
    bool passOpen = false;
};

} // namespace haylen::graphics2d
