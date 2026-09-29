#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/tiled/Layer.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/math/Ray.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// Answers geometric questions about a map without a physics world: the world outlines of its objects, which also feed navigation meshes, and ray casts against its tile layers and object layers. Positions include the offsets of layers and their groups, like the collision that MapRenderer builds.
class MapQuery final {
  public:
    struct TileHit {
        int column = 0;
        int row = 0;
        std::uint32_t gid = 0;
        math::Vec2 point{};
        math::Vec2 normal{};
        float distance = 0.0F;
    };

    struct ObjectHit {
        const Object* object = nullptr;
        math::Vec2 point{};
        math::Vec2 normal{};
        float distance = 0.0F;
    };

    using SolidTile = std::function<bool(std::uint32_t gid)>;

    explicit MapQuery(const Map& target) noexcept : map(target) {}

    // Fills points with the world outline of an object whose layers shift it by the offset: the corners of rectangles and tile objects, the points of polygons and polylines, and many-sided outlines of ellipses and capsules. Points and text have no outline and leave points empty.
    void getOutline(const Object& object, math::Vec2 offset, std::vector<math::Vec2>& points) const;

    // Fills outlines with the closed outlines of the objects of the named object layer, or of every object layer when the name is empty, in map order. Polylines stay out because they enclose no area.
    void getOutlines(std::string_view layer, std::vector<std::vector<math::Vec2>>& outlines) const;

    // Casts a ray of finite length over the cells of a tile layer and returns the first cell whose tile is solid, which is every tile unless solid decides. Orthogonal, isometric and oblique maps take tile casts.
    [[nodiscard]] std::optional<TileHit> castTiles(std::string_view layer, const math::Ray& ray, const SolidTile& solid = {}) const;

    // Casts a ray against the objects of the named object layer, or of every object layer when the name is empty, and returns the closest hit. Closed shapes are solid and polylines are hit from both sides.
    [[nodiscard]] std::optional<ObjectHit> castObjects(std::string_view layer, const math::Ray& ray) const;

  private:
    static constexpr int kCurveSegments = 16;

    [[nodiscard]] static math::Vec2 rotateAround(math::Vec2 point, math::Vec2 origin, float angle) noexcept;

    // Finds a layer by name depth first, together with the sum of its own offset and those of its groups.
    [[nodiscard]] std::optional<std::pair<const Layer*, math::Vec2>> findLayer(std::string_view name) const;

    // Calls visit(object, offset) for the objects of the named object layer, or of every object layer when the name is empty.
    template <typename Visit> void forEachObject(std::string_view layer, Visit&& visit) const;

    // Maps world points to continuous cell coordinates, where each cell spans one unit, for the map orientations whose cells form a sheared grid, and cell vectors back to world vectors.
    [[nodiscard]] math::Vec2 worldToCell(math::Vec2 point) const noexcept;
    [[nodiscard]] math::Vec2 cellToWorldVector(math::Vec2 vector) const noexcept;

    const Map& map;
};

} // namespace haylen::tiled
