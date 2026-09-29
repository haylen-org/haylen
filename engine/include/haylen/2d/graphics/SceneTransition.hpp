#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/ImageBlend.hpp"
#include "haylen/core/TransitionEffect.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

class Renderer;

// The built-in scene transition effects. Each one draws the outgoing and incoming images over the visible area, and the direction tells where motion, wipes, waves and page turns go. Effects that show one scene at a time change the stack halfway, and effects that show both scenes change it at the start and let the outgoing scenes exit at the end.
class SceneTransition final : public core::TransitionEffect {
  public:
    enum class Kind : std::uint8_t {
        // Fades the outgoing scene to the color and the incoming one back from it.
        Fade,
        // Fades the outgoing scene into the incoming one.
        CrossFade,
        // The incoming scene moves in toward the direction and covers the outgoing one, which stays still.
        MoveIn,
        // The incoming scene slides in toward the direction while the outgoing one slides a third of the way and darkens, like a navigation stack.
        SlideIn,
        // Both scenes move together toward the direction, the incoming one pushing the outgoing one out.
        Push,
        // The outgoing scene shrinks away while the incoming one grows in.
        ShrinkGrow,
        // The outgoing scene turns around the vertical axis toward the direction, and the incoming one turns in on its back.
        FlipX,
        // The outgoing scene turns around the horizontal axis toward the direction, and the incoming one turns in on its back.
        FlipY,
        // A flip that moves away while it turns, around the vertical axis for horizontal directions and the horizontal axis otherwise.
        ZoomFlip,
        // The outgoing scene spins and shrinks away, and the incoming one spins and grows back.
        RotoZoom,
        // The outgoing scene shrinks and jumps out toward the direction, and the incoming one jumps in and grows.
        JumpZoom,
        // The outgoing scene splits into columns that slide up and down out of view, and the incoming one closes back from them.
        SplitColumns,
        // The outgoing scene splits into rows that slide left and right out of view, and the incoming one closes back from them.
        SplitRows,
        // The tiles of the outgoing scene turn off in random order and uncover the incoming one.
        TurnOffTiles,
        // The tiles of the outgoing scene shrink away in a wave that travels toward the direction.
        FadeTiles,
        // The outgoing scene curls away like a page turned toward the direction.
        PageTurn,
        // A hand sweeps clockwise from twelve o'clock and uncovers the incoming scene.
        RadialClockwise,
        // A hand sweeps counterclockwise from twelve o'clock and uncovers the incoming scene.
        RadialCounterclockwise,
        // An edge moves toward the direction and uncovers the incoming scene.
        Wipe,
        // The incoming scene grows out of the center.
        InOut,
        // The outgoing scene shrinks into the center and uncovers the incoming one.
        OutIn,
        // A circle closes on the outgoing scene to the color and opens on the incoming one.
        Iris,
        // The incoming scene appears in random order, a few pixels at a time.
        Dissolve,
        // The outgoing scene breaks into growing blocks and the incoming one comes back out of them.
        Pixelate,
    };

    enum class Direction : std::uint8_t {
        Left,
        Right,
        Up,
        Down,
        UpLeft,
        UpRight,
        DownLeft,
        DownRight,
    };

    struct Options {
        Kind kind = Kind::Fade;
        Direction direction = Direction::Left;

        // The color fades and irises pass through, which also shows behind effects that uncover the screen.
        math::Color color = math::Color::black();
    };

    explicit SceneTransition(Options value) noexcept : options(value) {}

    [[nodiscard]] float getSwitchProgress() const noexcept override;
    [[nodiscard]] float getExitProgress() const noexcept override;
    void render(Renderer& renderer, const Frames& frames, float progress) override;

  private:
    // A turn draws as a grid whose corners follow the perspective, fine enough that the image stays straight inside each cell.
    static constexpr int kTurnColumns = 48;
    static constexpr int kTurnRows = 12;
    static constexpr int kTileColumns = 16;
    static constexpr int kSplits = 3;

    // The captured scenes hold premultiplied colors, because they render over the clear color, which a transparent window keeps transparent.
    static const DrawOrder& kCapturedOrder;

    // Returns the steps toward the direction on each axis, from -1 to 1.
    [[nodiscard]] static math::Vec2 getSteps(Direction direction) noexcept;
    [[nodiscard]] static bool isVertical(Direction direction) noexcept;

    // Returns the part of the area that a rectangle of the unit square covers.
    [[nodiscard]] static math::Rect getPlace(const math::Rect& area, const math::Rect& part) noexcept;

    // Returns the part of the texture, in pixels, that a rectangle of the unit square covers.
    [[nodiscard]] static math::Rect getSource(const graphics::Texture& texture, const math::Rect& part) noexcept;

    // Returns a random number from 0 to 1 that only depends on the index.
    [[nodiscard]] static float getHash(int index) noexcept;

    // Cuts the part of a convex polygon where the dot product with the normal stays below the limit.
    [[nodiscard]] static std::vector<math::Vec2> clip(std::span<const math::Vec2> polygon, math::Vec2 normal, float limit);

    // Draws a part of the texture, a rectangle of the unit square, over the target rectangle.
    static void drawImage(Renderer& renderer, const graphics::Texture& texture, const math::Rect& target, math::Color tint = math::Color::white(), const math::Rect& part = {0.0F, 0.0F, 1.0F, 1.0F});

    // Draws the whole texture over the area scaled and turned around the pivot, a point of the unit square.
    static void drawTransformed(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, math::Vec2 pivot, float scale, float rotation);
    static void drawTurn(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, float angle, bool vertical, float scale);

    // Draws the part of the texture inside a convex polygon of the unit square.
    static void drawRegion(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, std::span<const math::Vec2> polygon);

    void drawGrid(Renderer& renderer, const math::Rect& area, const graphics::Texture& texture, float progress, bool wave) const;
    void drawSplit(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress, bool columns) const;
    void drawJump(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress) const;
    void drawMotion(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress) const;
    void drawBlend(Renderer& renderer, const math::Rect& area, const Frames& frames, float progress, ImageBlend::Pattern pattern) const;

    Options options;
};

} // namespace haylen::graphics2d
