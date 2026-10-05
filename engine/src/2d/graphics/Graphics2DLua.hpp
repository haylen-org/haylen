#pragma once

#include <array>
#include <string_view>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/math/Vec2.hpp"

struct lua_State;

namespace haylen::text {
class Font;
}

namespace haylen::graphics2d {

// Installs `haylen.graphics2d` with the canvases, captures and draw functions of the 2D renderer, and the `Sprite`, `SpriteBatch`, `StaticSpriteBatch`, `Camera`, `Parallax`, `NineSlice`, `Material` and `RichText` classes.
class Graphics2DLua final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 6> kCanvasFields{"sort", "order", "visibilityMask", "ambientLight", "clear", "postProcess"};
    static constexpr std::array<std::string_view, 8> kImageBlendFields{"pattern", "progress", "center", "cellSize", "blockSize", "color", "reversed", "angle"};
    static constexpr std::array<std::string_view, 18> kPostProcessFields{"tint", "saturation", "brightness", "contrast", "vignetteStrength", "vignetteRadius", "vignetteSoftness", "fade", "distortion", "chromaticAberration", "pixelate", "blur", "bloomStrength", "bloomThreshold", "bloomRadius", "colorLut", "colorLutStrength", "materials"};
    static constexpr std::array<std::string_view, 1> kEffectFields{"effect"};
    static constexpr std::array<std::string_view, 4> kMetaballFields{"color", "outlineColor", "outlineWidth", "threshold"};
    static constexpr std::array<std::string_view, 2> kScaleFields{"scaleX", "scaleY"};
    static constexpr std::array<std::string_view, 5> kMeshVertexFields{"x", "y", "u", "v", "color"};
    static constexpr std::array<std::string_view, 4> kNineSliceFields{"source", "borders", "pieces", "fill"};
    static constexpr std::array<std::string_view, 14> kVectorFields{"x", "y", "width", "height", "scaleX", "scaleY", "pivotX", "pivotY", "rotation", "color", "flash", "flipHorizontal", "flipVertical", "flipDiagonal"};

    [[nodiscard]] static Renderer& getRenderer(lua_State* L);

    // Reads the scale of a text draw from its style table, which stretches the block from its anchor.
    [[nodiscard]] static text::Font& fontArgument(lua_State* L, int index);
    [[nodiscard]] static Renderer::CanvasOptions readCanvasOptions(lua_State* L, int index);

    // Assigns every field of the options table at index to the userdata on top of the stack through its properties, so each one is validated like a later assignment.
    static void assignFields(lua_State* L, int options);

    static int newSprite(lua_State* L);
    static int newSpriteBatch(lua_State* L);
    static int newCamera(lua_State* L);
    static int newNineSlice(lua_State* L);
    static int newParallax(lua_State* L);
    static int blendCameras(lua_State* L);

    static int beginWorld(lua_State* L);
    static int beginScreen(lua_State* L);
    static int beginTarget(lua_State* L);
    static int beginCapture(lua_State* L);
    static int endCapture(lua_State* L);

    static int draw(lua_State* L);
    static int drawVector(lua_State* L);
    static int drawBatch(lua_State* L);
    static int drawStatic(lua_State* L);
    static int drawRect(lua_State* L);
    static int drawRectOutline(lua_State* L);
    static int drawLine(lua_State* L);
    static int drawCircle(lua_State* L);
    static int drawRing(lua_State* L);
    static int drawArc(lua_State* L);
    static int drawPolygon(lua_State* L);
    static int drawPolyline(lua_State* L);
    static int drawMesh(lua_State* L);
    static int drawText(lua_State* L);
    static int measureText(lua_State* L);
    static int drawNineSlice(lua_State* L);
    static int drawLight(lua_State* L);
    static int drawOccluder(lua_State* L);
    static int drawMetaballs(lua_State* L);
    static int drawImageBlend(lua_State* L);
    static int pushClip(lua_State* L);
    static int popClip(lua_State* L);
    static int pushLayerOffset(lua_State* L);
    static int popLayerOffset(lua_State* L);

    static int stats(lua_State* L);
    static int drawn(lua_State* L);
    static int canvasBounds(lua_State* L);
    static int canvasUnitSize(lua_State* L);
    static int canvasLit(lua_State* L);
    static int capturing(lua_State* L);
    static int lightTexture(lua_State* L);
    static int hdrLighting(lua_State* L);
    static int defaultFont(lua_State* L);
    static int open(lua_State* L);
};

} // namespace haylen::graphics2d
