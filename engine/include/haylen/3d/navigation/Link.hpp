#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "haylen/core/EnumNames.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::navigation3d {

// A way across a gap that the mesh does not cover, from `start` to `end`, such as a jump down a ledge, a ladder or a teleporter. Each end joins the mesh within `radius` of it, a link that is not `bidirectional` goes from its start to its end only, and its `area` gives it the cost that a query filter sets for that area. The `kind` tells a crowd how its agents cross it.
struct Link {
    enum class Kind : std::uint8_t {
        Jump,
        Ladder,
        Teleport,
    };

    math::Vec3 start;
    math::Vec3 end;
    float radius = 0.5F;
    bool bidirectional = true;
    Kind kind = Kind::Jump;
    std::string area = "ground";

    friend bool operator==(const Link&, const Link&) = default;
};

} // namespace haylen::navigation3d

namespace haylen::core {

template <> struct EnumNames<navigation3d::Link::Kind> {
    using Kind = navigation3d::Link::Kind;

    static constexpr std::array<std::pair<std::string_view, Kind>, 3> kNames{{{"jump", Kind::Jump}, {"ladder", Kind::Ladder}, {"teleport", Kind::Teleport}}};

    static std::optional<Kind> fromName(std::string_view name) {
        const auto found = std::ranges::find(kNames, name, &std::pair<std::string_view, Kind>::first);
        return found != kNames.end() ? std::optional(found->second) : std::nullopt;
    }
    static std::string_view name(Kind value) {
        return std::ranges::find(kNames, value, &std::pair<std::string_view, Kind>::second)->first;
    }
};

} // namespace haylen::core
