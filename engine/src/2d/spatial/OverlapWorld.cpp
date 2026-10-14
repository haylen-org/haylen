#include "haylen/2d/spatial/OverlapWorld.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <utility>

#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/math/Color.hpp"

namespace haylen::spatial2d {

OverlapWorld::OverlapWorld(Options value) : options(std::move(value)) {
    if (options.kinds.size() > kMaxKinds) {
        throw std::invalid_argument("An overlap world has at most " + std::to_string(kMaxKinds) + " kinds of boxes.");
    }
    if (!std::isfinite(options.cellSize) || options.cellSize < 0.0F) {
        throw std::invalid_argument("The cell size of an overlap world must be 0 or positive.");
    }
    if (options.cellSize > 0.0F) {
        grid.emplace(options.cellSize);
    }
}

std::uint32_t OverlapWorld::defineSet(std::span<const Box> boxes) {
    Set set;
    for (const Box& box : boxes) {
        if (!(box.rect.width > 0.0F && box.rect.height > 0.0F)) {
            throw std::invalid_argument("A box needs a positive width and height.");
        }
        if (box.kind >= std::max<std::size_t>(options.kinds.size(), 1)) {
            throw std::invalid_argument("A box names a kind that the overlap world does not have.");
        }
        set.bounds = set.boxes.empty() ? box.rect : set.bounds.merged(box.rect);
        set.boxes.push_back(box);
    }
    sets.push_back(std::move(set));
    return static_cast<std::uint32_t>(sets.size());
}

std::uint32_t OverlapWorld::add() {
    std::uint32_t id = 0;
    if (freeIds.empty()) {
        entities.emplace_back();
        ranks.push_back(0);
        id = static_cast<std::uint32_t>(entities.size());
    } else {
        id = freeIds.back();
        freeIds.pop_back();
    }
    entities[id - 1] = Entity{.alive = true};
    ranks[id - 1] = static_cast<std::uint32_t>(order.size());
    order.push_back(id);
    return id;
}

OverlapWorld::Entity& OverlapWorld::find(std::uint32_t entity) {
    if (entity == 0 || entity > entities.size() || !entities[entity - 1].alive) {
        throw std::out_of_range("The overlap world has no entity " + std::to_string(entity) + ".");
    }
    return entities[entity - 1];
}

// A mirrored entity places the box `x, y, width, height` at `-x - width, y`, which flips it around the origin of the entity.
math::Rect OverlapWorld::placeBox(const Entity& entity, const math::Rect& rect) const noexcept {
    const float x = entity.mirrored ? -rect.x - rect.width : rect.x;
    return {entity.position.x + x, entity.position.y + rect.y, rect.width, rect.height};
}

void OverlapWorld::place(std::uint32_t entity, math::Vec2 position, float depth, bool mirrored, std::uint32_t set) {
    if (set > sets.size()) {
        throw std::out_of_range("The overlap world has no box set " + std::to_string(set) + ".");
    }
    Entity& placed = find(entity);
    placed.position = position;
    placed.depth = depth;
    placed.mirrored = mirrored;
    placed.set = set;
    if (!grid) {
        return;
    }
    if (set == 0) {
        grid->remove(entity);
        return;
    }
    grid->set(entity, placeBox(placed, sets[set - 1].bounds));
}

void OverlapWorld::remove(std::uint32_t entity) {
    find(entity) = {};
    if (grid) {
        grid->remove(entity);
    }
    order.erase(order.begin() + static_cast<std::ptrdiff_t>(ranks[entity - 1]));
    for (std::size_t rank = ranks[entity - 1]; rank < order.size(); ++rank) {
        ranks[order[rank] - 1] = static_cast<std::uint32_t>(rank);
    }
    std::erase_if(rests, [entity](const auto& rest) { return rest.first >> 32U == entity || (rest.first & 0xFFFFFFFFU) == entity; });
    freeIds.push_back(entity);
}

void OverlapWorld::clear() noexcept {
    entities.clear();
    order.clear();
    freeIds.clear();
    ranks.clear();
    rests.clear();
    if (grid) {
        grid->clear();
    }
}

std::uint64_t OverlapWorld::pairKey(std::uint32_t source, std::uint32_t target) noexcept {
    return (static_cast<std::uint64_t>(source) << 32U) | target;
}

void OverlapWorld::rest(std::uint32_t source, std::uint32_t target, std::uint32_t ticks) {
    (void)find(source);
    (void)find(target);
    if (ticks == 0) {
        rests.erase(pairKey(source, target));
        return;
    }
    rests[pairKey(source, target)] = ticks;
}

bool OverlapWorld::isResting(std::uint32_t source, std::uint32_t target) const noexcept {
    return rests.contains(pairKey(source, target));
}

void OverlapWorld::step() noexcept {
    std::erase_if(rests, [](auto& rest) { return --rest.second == 0; });
}

bool OverlapWorld::overlaps(const math::Rect& first, const math::Rect& second) const noexcept {
    if (options.touching) {
        return first.getLeft() <= second.getRight() && first.getRight() >= second.getLeft() && first.getTop() <= second.getBottom() && first.getBottom() >= second.getTop();
    }
    return first.intersects(second);
}

void OverlapWorld::collectPair(std::uint32_t source, std::uint32_t target, std::vector<Contact>& contacts) const {
    const Entity& from = entities[source - 1];
    const Entity& to = entities[target - 1];
    if (to.set == 0 || isResting(source, target)) {
        return;
    }
    const std::vector<Box>& fromBoxes = sets[from.set - 1].boxes;
    const std::vector<Box>& toBoxes = sets[to.set - 1].boxes;
    const float distance = std::fabs(from.depth - to.depth);
    for (std::size_t first = 0; first < fromBoxes.size(); ++first) {
        const Box& hitting = fromBoxes[first];
        if (hitting.mask == 0) {
            continue;
        }
        const math::Rect hitRect = placeBox(from, hitting.rect);
        for (std::size_t second = 0; second < toBoxes.size(); ++second) {
            const Box& hit = toBoxes[second];
            if ((hitting.mask & hit.layers) == 0 || distance > hitting.depth + hit.depth) {
                continue;
            }
            const math::Rect hitBy = placeBox(to, hit.rect);
            if (!overlaps(hitRect, hitBy)) {
                continue;
            }
            const math::Vec2 minimum = math::Vec2::max(hitRect.getMin(), hitBy.getMin());
            const math::Vec2 maximum = math::Vec2::min(hitRect.getMax(), hitBy.getMax());
            contacts.push_back({.source = source, .sourceBox = static_cast<std::uint32_t>(first + 1), .target = target, .targetBox = static_cast<std::uint32_t>(second + 1), .overlap = math::Rect::fromMinMax(minimum, maximum)});
        }
    }
}

void OverlapWorld::collect(std::vector<Contact>& contacts) const {
    contacts.clear();
    for (const std::uint32_t source : order) {
        const Entity& from = entities[source - 1];
        if (from.set == 0) {
            continue;
        }
        if (!grid) {
            for (const std::uint32_t target : order) {
                if (target != source) {
                    collectPair(source, target, contacts);
                }
            }
            continue;
        }

        // Candidates of the hash grid come in the order the entities were added, the same order the test of every pair uses.
        candidates.clear();
        const math::Rect bounds = placeBox(from, sets[from.set - 1].bounds);
        grid->query(options.touching ? bounds.expanded(1.0e-3F) : bounds, candidates);
        std::ranges::sort(candidates, [this](std::uint64_t first, std::uint64_t second) { return ranks[static_cast<std::size_t>(first - 1)] < ranks[static_cast<std::size_t>(second - 1)]; });
        candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
        for (const std::uint64_t target : candidates) {
            if (target != source) {
                collectPair(source, static_cast<std::uint32_t>(target), contacts);
            }
        }
    }
}

void OverlapWorld::debugDraw(graphics2d::Renderer& renderer, const graphics2d::DrawOrder& drawOrder) const {
    for (const std::uint32_t id : order) {
        const Entity& entity = entities[id - 1];
        if (entity.set == 0) {
            continue;
        }
        for (const Box& box : sets[entity.set - 1].boxes) {
            const float hue = static_cast<float>(box.kind) * 0.61803398875F;
            renderer.drawRectOutline(placeBox(entity, box.rect), 2.0F, math::Color::fromHsv(hue - std::floor(hue), 0.75F, 1.0F), drawOrder);
        }
    }
}

} // namespace haylen::spatial2d
