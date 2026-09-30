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
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// Answers geometric questions about a map without a physics world: the world outlines of its objects, which also feed navigation meshes and the collision that `MapRenderer` builds, and ray casts against its tile layers and object layers. Positions include the offsets of layers and their groups.
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

    // Fills `points` with the world outline of an object whose layers shift it by the offset: the corners of rectangles, the corners of tile objects where their image draws, placed by the object alignment of their tileset, the points of polygons and polylines, and many-sided outlines of ellipses and capsules. Points and text have no outline and leave `points` empty.
    void getOutline(const Object& object, math::Vec2 offset, std::vector<math::Vec2>& points) const;

    // Fills `outlines` with the closed outlines of the objects of the named object layer, or of every object layer when the name is empty, in map order. Polylines stay out because they enclose no area.
    void getOutlines(std::string_view layer, std::vector<std::vector<math::Vec2>>& outlines) const;

    // Casts a ray over the cells of a tile layer and returns the first cell whose tile is solid, which is every tile unless `solid` decides. The ray only travels over the cells the layer holds, so it may be infinite, and `solid` runs once the walk has gathered the tiles the ray crosses, so it may change the map. Orthogonal, isometric and oblique maps take tile casts.
    [[nodiscard]] std::optional<TileHit> castTiles(std::string_view layer, const math::Ray& ray, const SolidTile& solid = {}) const;

    // Casts a ray against the objects of the named object layer, or of every object layer when the name is empty, and returns the closest hit. Closed shapes are solid and polylines are hit from both sides.
    [[nodiscard]] std::optional<ObjectHit> castObjects(std::string_view layer, const math::Ray& ray) const;

  private:
    friend class MapRenderer;

    using ObjectVisitor = std::function<void(const Object& object, math::Vec2 offset)>;

    struct CrossedTile {
        int column = 0;
        int row = 0;
        std::uint32_t gid = 0;
        float distance = 0.0F;
        math::Vec2 normal{};
    };

    static constexpr int kCurveSegments = 16;

    [[nodiscard]] static math::Vec2 rotateAround(math::Vec2 point, math::Vec2 origin, float angle) noexcept;

    // Returns the point of a tile object image that sits on the object position, as fractions of the image size. Unspecified alignment is bottom left on most maps and bottom center on isometric ones.
    [[nodiscard]] static math::Vec2 alignmentPivot(std::string_view alignment, Map::Orientation orientation) noexcept;

    // Returns the cells a tile layer holds as a rectangle in cell coordinates, which spans every chunk of an infinite map.
    [[nodiscard]] static math::Rect getCellBounds(const Layer& layer) noexcept;

    // Fills `points` with the outline of an object in the coordinates of the map file, turned around the object position by its rotation. Tile objects stand on their position the way their image draws.
    void traceOutline(const Object& object, std::vector<math::Vec2>& points) const;

    // Finds a layer by name depth first, together with the sum of its own offset and those of its groups.
    [[nodiscard]] std::optional<std::pair<const Layer*, math::Vec2>> findLayer(std::string_view name) const;

    // Calls `visit(object, offset)` for the objects of the object layer that the name finds, or of every object layer when the name is empty, in map order.
    void forEachObject(std::string_view layer, const ObjectVisitor& visit) const;

    // Maps world points to continuous cell coordinates, where each cell spans one unit, for the map orientations whose cells form a sheared grid, and cell vectors back to world vectors.
    [[nodiscard]] math::Vec2 worldToCell(math::Vec2 point) const noexcept;
    [[nodiscard]] math::Vec2 cellToWorldVector(math::Vec2 vector) const noexcept;

    const Map& map;
};

} // namespace haylen::tiled
