#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <optional>

#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::debug {

// Picks what the app draws under a press on the screen, for the selection of the inspector in development, and follows where the selected thing draws. Whatever draws things that the app can edit offers each of them while a pick or a selection is active, by a tag that names it: the renderer of 2D offers sprites and the nodes of trees, and every other drawer, the ones of plugins too, offers its own the same way. The frame after a press finds the hit that draws over the others, and the selection then takes its placement every frame it draws.
class Picking final {
  public:
    // Names a thing that draws: an owner, such as an object of the app, and an item inside it, such as the key of a node of a tree, or 0 for the owner itself.
    struct Tag {
        std::uint64_t owner = 0;
        std::uint64_t item = 0;

        friend bool operator==(const Tag&, const Tag&) = default;
    };

    // Where a thing draws and what moves it: the position, rotation and scale that the app edits, the transform from the space of that position to the design units of the screen, and the corners of what it draws on the screen.
    struct Placement {
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 scale{1.0F, 1.0F};
        math::Transform2D frame{};
        std::array<math::Vec2, 4> outline{};
    };

    // Which of two hits draws over the other: a canvas of a higher order over a lower one, a later canvas over an earlier one and a higher layer over a lower one, while of two hits of the same depth the one offered later draws over the other.
    struct Depth {
        int canvasOrder = 0;
        std::size_t canvas = 0;
        int layer = 0;

        friend auto operator<=>(const Depth&, const Depth&) = default;
    };

    // Looks for what draws under the point of the screen, in design units, through the draws of the next frame.
    void request(math::Vec2 point) noexcept;

    // Starts a frame: a pick that was requested looks for its hit through the draws of this frame, and the selection waits for its placement.
    void beginFrame() noexcept;

    // Whether the draws of this frame offer themselves, which they do while a pick looks for its hit or a selection follows its thing.
    [[nodiscard]] bool isActive() const noexcept {
        return searching.has_value() || selected.has_value();
    }

    // Whether a pick looks for its hit in this frame, so the drawers record what their tags name.
    [[nodiscard]] bool isSearching() const noexcept {
        return searching.has_value();
    }

    // Offers a thing that draws in this frame, which becomes the hit when its outline holds the point of the pick and it draws over the hit before it, and the placement of the selection when its tag is the selected one.
    void offer(const Tag& tag, const Placement& placement, const Depth& depth) noexcept;

    // Ends the search of the frame and returns whether a pick looked for its hit, with the tag of the hit, or no tag when nothing drew under the point.
    [[nodiscard]] bool takeResult(std::optional<Tag>& hit) noexcept;

    void select(const Tag& tag) noexcept;
    void clear() noexcept;
    [[nodiscard]] const std::optional<Tag>& getSelected() const noexcept {
        return selected;
    }

    // Where the selection drew in this frame, or nothing while it did not draw.
    [[nodiscard]] const std::optional<Placement>& getPlacement() const noexcept {
        return placement;
    }

  private:
    // A hit of the search, with the depth that ranks it.
    struct Hit {
        Tag tag;
        Depth depth;
    };

    std::optional<math::Vec2> requested;
    std::optional<math::Vec2> searching;
    std::optional<Hit> best;
    std::optional<Tag> selected;
    std::optional<Placement> placement;
};

} // namespace haylen::debug
