#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/spatial/HashGrid.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::spatial2d {

// Keeps entities that each show one set of boxes, such as the body, hit and grab boxes of a frame of a fighter, and lists the pairs of boxes that reach each other, for games that resolve their own hits without physics. A box reaches another when its mask shares a bit with the layers of the other, their rectangles overlap and their entities stand within the sum of the depth reaches of the boxes, so a world of depth rows works like plain 2D with every depth at 0. Boxes are relative to the origin of an entity that faces right, and a mirrored entity flips them around its origin. Contacts come in the order the entities were added, then by box, whatever the broad phase, so results never depend on how entities moved. Entity and set ids start at 1.
class OverlapWorld final {
  public:
    struct Box {
        math::Rect rect{};
        std::uint8_t kind = 0;
        std::uint32_t layers = 1;
        std::uint32_t mask = 0;
        float depth = 0.0F;
    };

    struct Contact {
        std::uint32_t source = 0;
        std::uint32_t sourceBox = 0;
        std::uint32_t target = 0;
        std::uint32_t targetBox = 0;
        math::Rect overlap{};
    };

    // The kinds name the boxes for debug drawing and the app. Touching edges count as an overlap unless `touching` is false, and a cell size above 0 finds candidate pairs through a hash grid instead of testing every pair.
    struct Options {
        std::vector<std::string> kinds;
        bool touching = true;
        float cellSize = 0.0F;
    };

    // Throws `std::invalid_argument` for more kinds than a box can name or a cell size that is negative or not finite.
    explicit OverlapWorld(Options value);

    // Throws `std::invalid_argument` for a box without a positive width and height or with a kind the world does not have.
    [[nodiscard]] std::uint32_t defineSet(std::span<const Box> boxes);
    [[nodiscard]] std::uint32_t add();

    // Shows a set of boxes at a position, or nothing with the set 0. Throws `std::out_of_range` for an entity or a set the world does not have.
    void place(std::uint32_t entity, math::Vec2 position, float depth, bool mirrored, std::uint32_t set);
    void remove(std::uint32_t entity);
    void clear() noexcept;

    // Keeps the boxes of the source from reaching the target for that many steps, as a hit that lands once in a window does.
    void rest(std::uint32_t source, std::uint32_t target, std::uint32_t ticks);
    [[nodiscard]] bool isResting(std::uint32_t source, std::uint32_t target) const noexcept;
    void step() noexcept;

    // Replaces the contents of `contacts` with every contact, which reuses its memory from step to step.
    void collect(std::vector<Contact>& contacts) const;

    [[nodiscard]] const Options& getOptions() const noexcept {
        return options;
    }
    [[nodiscard]] std::size_t getEntityCount() const noexcept {
        return order.size();
    }
    [[nodiscard]] std::size_t getSetCount() const noexcept {
        return sets.size();
    }

    // Outlines every placed box with the color of its kind in the active canvas of the renderer.
    void debugDraw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& drawOrder) const;

  private:
    static constexpr std::size_t kMaxKinds = 256;

    struct Set {
        std::vector<Box> boxes;
        math::Rect bounds{};
    };

    struct Entity {
        bool alive = false;
        math::Vec2 position{};
        float depth = 0.0F;
        bool mirrored = false;
        std::uint32_t set = 0;
    };

    [[nodiscard]] static std::uint64_t pairKey(std::uint32_t source, std::uint32_t target) noexcept;
    [[nodiscard]] Entity& find(std::uint32_t entity);
    [[nodiscard]] math::Rect placeBox(const Entity& entity, const math::Rect& rect) const noexcept;
    [[nodiscard]] bool overlaps(const math::Rect& first, const math::Rect& second) const noexcept;
    void collectPair(std::uint32_t source, std::uint32_t target, std::vector<Contact>& contacts) const;

    Options options;
    std::vector<Set> sets;
    std::vector<Entity> entities;
    std::vector<std::uint32_t> order;
    std::vector<std::uint32_t> freeIds;
    std::vector<std::uint32_t> ranks;
    std::unordered_map<std::uint64_t, std::uint32_t> rests;
    std::optional<HashGrid> grid;
    mutable std::vector<std::uint64_t> candidates;
};

} // namespace haylen::spatial2d
