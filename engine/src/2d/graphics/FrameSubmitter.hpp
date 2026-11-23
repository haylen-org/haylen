#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "2d/graphics/Canvas.hpp"
#include "2d/graphics/DrawItem.hpp"
#include "2d/graphics/GpuInstance.hpp"
#include "2d/graphics/LitTargets.hpp"
#include "2d/graphics/Program.hpp"
#include "graphics/PassTarget.hpp"
#include "haylen/2d/graphics/PostProcess.hpp"
#include "haylen/2d/lighting/Light.hpp"
#include "haylen/gpu/Device.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/graphics/RenderTarget.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics {
struct TextureResource;
} // namespace haylen::graphics

namespace haylen::graphics2d {

struct Command;
struct RendererState;
struct Shade;

// Turns the canvases recorded during a frame into GPU passes: it casts the shadow maps, orders and merges the draws, uploads the instance data and renders the metaball fields, the offscreen, light, post-processing and capture passes, the passes of glass and the swapchain pass.
class FrameSubmitter final {
  public:
    // A transparent window composes the alpha of the frame with the desktop, and every other frame keeps alpha 1 whatever it draws.
    FrameSubmitter(RendererState& rendererState, bool transparentFrame) : state(rendererState), transparent(transparentFrame) {}

    // Every render target is created before the first pass, and a failure inside a pass ends it and the frame before it goes on.
    void submit();

    // Returns the pixels of a pass that a clip rectangle of a canvas covers, those whose centers lie inside it.
    [[nodiscard]] static math::Rect getScissor(const Canvas& canvas, const math::Rect& clip, const math::Rect& passRect) noexcept;

  private:
    using Matrix = std::array<float, 16>;
    using Vector = std::array<float, 4>;

    static constexpr Matrix kIdentity{1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F, 0.0F, 1.0F};

    // The uniform blocks of the programs of the renderer, laid out as their sources declare them under the std140 rules.
    struct VertexParams {
        Matrix viewProjection{};
        float premultiply = 0.0F;
        float padding = 0.0F;
        std::array<float, 2> viewport{};
    };

    // The parameters of the fragment stage of a draw as the shader library reads them: the slopes along x, the slopes along y and the offsets of the first four edges of its fade, then of the next four, where the slots past the last edge cover every point, the count of its edges and the form of its texture.
    struct DrawParams {
        Vector x{};
        Vector y{};
        Vector offset{1.0F, 1.0F, 1.0F, 1.0F};
        Vector moreX{};
        Vector moreY{};
        Vector moreOffset{1.0F, 1.0F, 1.0F, 1.0F};
        float fadeCount = 0.0F;
        float textureForm = 0.0F;
        std::array<float, 2> padding{};
    };

    // The form of the texture of a draw, which lets the lookups of the shader library skip work: a texture with mip levels, or the several textures of a batch, whose lookups pick a level, a texture of a single level, which they read without slopes, and the white texture, whose texels are all 1.
    enum class TextureForm : std::uint8_t {
        Levels,
        SingleLevel,
        White,
    };

    struct LitParams {
        Vector surface{};
        Vector info{};
    };

    struct LightParams {
        Matrix viewProjection{};
    };

    struct CompositeParams {
        Vector tint{};
        Vector fade{};
        Vector grading{};
        Vector vignette{};
        Vector flags{};
        Vector view{};
        Vector shape{};
        Vector visionRed{};
        Vector visionGreen{};
        Vector visionBlue{};
        Vector grain{};
    };

    struct FilterParams {
        Vector settings{};
        Vector offset{};
    };

    struct TurnParams {
        Vector turn{};
    };

    struct BlendVertexParams {
        Matrix viewProjection{};
        Vector placement{};
        Vector columns{};
        float premultiply = 0.0F;
        std::array<float, 3> padding{};
    };

    struct BlendFragmentParams {
        Vector settings{};
        Vector shape{};
        Vector color{};
        Vector image{};
        Vector fade{};
    };

    struct BackdropVertexParams {
        Matrix viewProjection{};
        Vector placement{};
        Vector columns{};
    };

    struct BackdropFragmentParams {
        Vector settings{};
        Vector flips{};
    };

    struct MetaballVertexParams {
        Matrix viewProjection{};
        Vector area{};
        Vector field{};
    };

    struct MetaballFragmentParams {
        Vector fill{};
        Vector outlineColor{};
        Vector levels{};
    };

    struct GlassParams {
        Vector look{};
        Vector tint{};
        Vector rim{};
    };

    // The parts of the composite of a canvas: everything in one pass, the image before bloom and blur, or what comes after them.
    enum class Stage : std::uint8_t {
        Whole,
        First,
        Finish,
    };

    // The composite of a lit canvas without post-processing grades nothing.
    static const PostProcess kNoPostProcess;

    // The grain repeats its pattern after this many frames, which keeps the seed of its noise exact as a float.
    static constexpr std::uint64_t kGrainFrames = 1024;

    // The image bends along the slope of the coverage of its distortion draws blurred by a Gaussian whose deviation is this share of the bend distance, which keeps the slope of the bend below 1, so the image never folds into copies of itself. The blur of a radius has a deviation of about this share of it.
    static constexpr float kBendDeviation = 0.6F;
    static constexpr float kBlurDeviation = 0.42F;

    // A blur shrinks the image until its radius spans at most this many texels, through at most this many sizes, none smaller than this many texels across.
    static constexpr float kBlurReach = 4.0F;
    static constexpr std::size_t kBlurLevels = 8;
    static constexpr int kBlurSmallest = 4;

    // A blur of a region works on the region moved out by this many texels of its smallest size, which holds every texel that its taps reach from the region.
    static constexpr float kBlurMargin = 8.0F;

    // The light mask and the layer of the pixels of a lit canvas where no draw covers more than half, which the lights reach as they reach a draw with them.
    static constexpr std::uint8_t kBackgroundLightMask = 1;
    static constexpr int kBackgroundLayer = 0;

    // Moves everything drawn with the matrix by an offset in world units.
    [[nodiscard]] static Matrix translated(Matrix matrix, math::Vec2 offset) noexcept;
    [[nodiscard]] static Matrix projection(const math::Transform2D& view, math::Vec2 viewSize) noexcept;
    [[nodiscard]] static bool isInstanced(const DrawItem& item) noexcept;

    // Returns the pixels of a pass that the corners of an area of a canvas cover, the ones whose centers lie inside it, or every pixel the area touches.
    [[nodiscard]] static math::Rect getPixels(const Canvas& canvas, const std::array<math::Vec2, 4>& corners, const math::Rect& passRect, bool touched) noexcept;

    void prepareTargets();

    // Tells whether every draw of a lit canvas takes the lights as its background does: without a material, a normal map, emission or unshaded colors, and with a light mask and a layer that every light of the canvas reaches or misses as it does the background.
    [[nodiscard]] bool takesLightsAlike(const Canvas& canvas) const;
    LitTargets& getLitTargets(std::size_t index, math::Vec2 size, const Canvas& canvas);

    // Gives the glass draws of the canvas the images of glass of the size of its destination, and returns whether the canvas draws on the screen, whose image the glass reads.
    bool prepareGlass(const Canvas& canvas, std::size_t& glassTargetsUsed);
    [[nodiscard]] math::Vec2 getDestinationSize(const Canvas& canvas) const;
    const graphics::RenderTarget& getField(std::size_t index, math::Vec2 size);
    void castShadows();
    void buildCommands();
    void append(const DrawItem& item);
    void appendCanvas(const std::vector<std::uint32_t>& order, bool distortion, std::uint32_t& expected, bool& inOrder);
    void upload();
    void writeInstances(std::span<const GpuInstance> data, std::size_t offset);
    void writeBuffer(gpu::Device::Buffer& buffer, std::size_t& capacity, std::span<const std::byte> data, std::size_t elementSize, bool indexBuffer);

    // Applies the values of a uniform block of the applied pipeline.
    template <typename Block> void applyBlock(int slot, const Block& block);

    void renderOffscreen();
    void renderFields(const Canvas& canvas);
    void renderCanvasOffscreen(Canvas& canvas);
    void renderLights(const Canvas& canvas, const LitTargets& targets);
    [[nodiscard]] const graphics::TextureResource& getLightShape(const lighting2d::Light& light) const;
    void renderPostChain(Canvas& canvas, LitTargets& targets);
    void renderCapture(std::size_t index);
    void renderSwapchain();
    void renderPresentations();

    // Draws the world and screen canvases of a destination, the screen or a capture, into the pass, which draws into the image, or into the swapchain without one.
    void drawCanvases(std::size_t capture, graphics::PassTarget pass, const graphics::RenderTarget* image);

    // Draws the image of the screen into the swapchain one pixel to one pixel.
    void drawScreenImage(graphics::PassTarget pass);

    // Draws the finished image of a composited canvas into the pass: the composite, or the last post-processing material over the image before it.
    void drawFinal(const Canvas& canvas, graphics::PassTarget pass);

    // The clear colors of color images are premultiplied by their alpha, like everything drawn into them.
    void beginOffscreenPass(const graphics::RenderTarget& renderTarget, std::optional<math::Color> clear);
    void beginLitPass(const LitTargets& targets, math::Color clear);
    void endPass();
    void composite(const Canvas& canvas, graphics::PassTarget pass, Stage stage);

    // Draws the source into the target with the filter program, which shrinks it to half its size, keeping what passes the threshold when it is not negative, or blurs it along the step in texels of the source, writing only the pixels of the region of the target when it has one.
    void filter(const graphics::RenderTarget& source, const graphics::RenderTarget& destination, float threshold, math::Vec2 step, const std::optional<math::Rect>& region = std::nullopt);
    void copy(const graphics::RenderTarget& source, const graphics::RenderTarget& destination, const std::optional<math::Rect>& region = std::nullopt);

    // Blurs the image over the radius in its pixels into the first target of the chain, at half its size, through the smaller sizes the radius needs, keeping to what the region of the image needs when it has one.
    void blurInto(const graphics::RenderTarget& image, std::vector<std::array<graphics::RenderTarget, 2>>& chain, float threshold, float radius, const std::optional<math::Rect>& region = std::nullopt);
    void growChain(std::vector<std::array<graphics::RenderTarget, 2>>& chain, std::size_t levels, int width, int height);

    // Returns the texels of a size of the chain that a region of the image covers, the ones it touches.
    [[nodiscard]] static std::optional<math::Rect> getChainRegion(const std::optional<math::Rect>& region, const std::vector<std::array<graphics::RenderTarget, 2>>& chain, std::size_t index) noexcept;
    void drawPostMaterial(std::size_t shadeIndex, const graphics::Texture& image, graphics::PassTarget pass);

    [[nodiscard]] math::Rect getClipScissor(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect) const noexcept;
    [[nodiscard]] bool showsPixels(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect) const noexcept;
    void applyClip(const Canvas& canvas, std::uint32_t clip, const math::Rect& passRect);

    // Draws the commands of a canvas into the pass, which draws into the image, or into the swapchain without one, where no glass draws.
    void drawCommands(const Canvas& canvas, std::size_t begin, std::size_t end, graphics::PassTarget pass, const math::Rect& passRect, const graphics::RenderTarget* image);
    [[nodiscard]] static bool isPaired(Program program) noexcept;
    [[nodiscard]] gpu::Device::Bindings makeBindings(const Command& command, const Shade& shade, graphics::PassTarget pass);
    void drawImageBlend(const Command& command, const Matrix& matrix);
    void drawBackdrop(const Canvas& canvas, const Command& command, const Matrix& matrix, const math::Rect& passRect);
    void drawGlass(const Canvas& canvas, const Command& command, const Matrix& matrix, const math::Rect& passRect, const graphics::RenderTarget& image);
    void drawMetaball(const Command& command, const Matrix& matrix);
    void applyUniforms(const Matrix& matrix, graphics::BlendMode::Type blend, math::Vec2 viewport);
    void applyTurn();

    // Applies the fragment parameters of a draw: the edges of a fade of the frame, or of none for 0, moved with draws that the offset moves, and the form of its texture.
    void applyDraw(std::uint32_t fade, math::Vec2 offset, TextureForm form);
    [[nodiscard]] TextureForm getTextureForm(const graphics::TextureResource& texture) const noexcept;

    // Applies the uniforms of how a command is shaded: the lighting of lit passes, with how its blend covers the emission, the surface and the info, and the values of its material.
    void applyShade(const Shade& shade, graphics::PassTarget pass, Program program, graphics::BlendMode::Type blend);

    // Binds the textures of a batch program without a material, every one with the sampler of the first.
    void bindBatch(gpu::Device::Bindings& bindings, const Shade& shade, const graphics::TextureResource& texture, graphics::PassTarget pass);

    // Binds the textures and samplers of a material program, where slot 0 takes the texture of the draw and the slots of the textures of a batch take the others.
    void bindMaterial(gpu::Device::Bindings& bindings, const Shade& shade, const graphics::TextureResource& texture, graphics::PassTarget pass, Program program);

    // Returns the texture a slot of a material program takes: a texture of the material, a texture of the batch, the texture of the draw for slot 0 or white.
    [[nodiscard]] const graphics::TextureResource& getMaterialTexture(const Shade& shade, const graphics::TextureResource& texture, Program program, int slot) const;

    RendererState& state;
    bool transparent = false;
    std::size_t instanceCursor = 0;
    std::size_t splatBase = 0;
    std::size_t glassBase = 0;
    std::size_t commandFloor = 0;
    bool identity = true;
    bool passOpen = false;

    // The pixels of the shadow atlas that the shadowed lights of the frame cast, which reach the GPU with the dynamic textures.
    std::optional<gpu::Device::ImageWrite> shadowWrite;
};

} // namespace haylen::graphics2d
