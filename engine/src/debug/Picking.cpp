#include "haylen/debug/Picking.hpp"

#include "haylen/math/Geometry2D.hpp"

namespace haylen::debug {

void Picking::request(math::Vec2 point) noexcept {
    requested = point;
}

void Picking::beginFrame() noexcept {
    searching = requested;
    requested.reset();
    best.reset();
    placement.reset();
}

void Picking::offer(const Tag& tag, const Placement& drawn, const Depth& depth) noexcept {
    if (selected && *selected == tag) {
        placement = drawn;
    }

    if (!searching || !math::Geometry2D::contains(drawn.outline, *searching)) {
        return;
    }

    if (!best || !(depth < best->depth)) {
        best = Hit{.tag = tag, .depth = depth};
    }
}

bool Picking::takeResult(std::optional<Tag>& hit) noexcept {
    if (!searching) {
        return false;
    }

    hit = best ? std::optional(best->tag) : std::nullopt;
    searching.reset();
    best.reset();
    return true;
}

void Picking::select(const Tag& tag) noexcept {
    selected = tag;
    placement.reset();
}

void Picking::clear() noexcept {
    selected.reset();
    placement.reset();
}

} // namespace haylen::debug
