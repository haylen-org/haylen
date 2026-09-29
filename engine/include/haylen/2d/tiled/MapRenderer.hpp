#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/SpriteFlip.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/graphics/StaticSpriteBatch.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/2d/physics/CollisionFilter.hpp"
#include "haylen/2d/physics/Shape.hpp"
#include "haylen/2d/tiled/Layer.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/2d/tiled/Tile.hpp"
#include "haylen/2d/tiled/Tileset.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::physics2d {
class World;
}

namespace haylen::text {
class Font;
}

namespace haylen::tiled {

// A playable instance of a Tiled map. It owns a copy of the map data, so edits never touch the cached asset, and it bakes each layer into static batches the first time it draws them.
class MapRenderer final {
  public:
    // What the camera sees. Parallax layers move by their factor relative to the center, and repeated image layers and culling use the visible area.
    struct View {
        math::Vec2 center{};
        math::Rect visible{};
    };

    // How a map draws: the draw order of its layers and the place of the map origin in the world, so the maps of a Tiled world share one camera. With ysort, tile layers draw row by row and object layers object by object, each sorted by the y it stands on, so entities of a canvas that sorts by y or by depth pass behind and in front of map objects.
    struct DrawOptions {
        graphics2d::DrawOrder order{};
        math::Vec2 offset{};
        bool ysort = false;
    };

    using ObjectVisitor = std::function<void(const Object& object, math::Vec2 position)>;

    explicit MapRenderer(Map data, std::shared_ptr<text::Font> textFont = nullptr);

    [[nodiscard]] const Map& getMap() const noexcept {
        return map;
    }

    [[nodiscard]] std::uint32_t getTile(std::string_view layer, int column, int row) const;
    void setTile(std::string_view layer, int column, int row, std::uint32_t gid);
    void setLayerVisible(std::string_view layer, bool visible);

    // Advances animated tiles.
    void update(float deltaSeconds) noexcept;

    // Draws every visible layer in map order with the same draw order, so later layers cover earlier ones, unless ysort sets the depth of each row and object to the y it stands on. Each layer blends with its own Tiled blend mode.
    void draw(graphics2d::Renderer& renderer, const View& view, const DrawOptions& options = kDefaultOptions);
    void drawLayer(graphics2d::Renderer& renderer, std::string_view layer, const View& view, const DrawOptions& options = kDefaultOptions);

    // Visits the objects of the named object layer, or of every object layer when the name is empty, in map order. The position is the object origin in world coordinates, including the offsets of its layer and groups.
    void forEachObject(std::string_view layer, const ObjectVisitor& visit) const;

    // Creates one static body per collision source. Tile layers use the collision shapes of their tiles unless their collision property is false, and object layers or objects whose class is collision become shapes. Objects with a true sensor property become sensors.
    std::vector<physics2d::Body> buildCollision(physics2d::World& world) const;

  private:
    struct AnimatedTile {
        const Tileset* tileset = nullptr;
        const Tile* tile = nullptr;
        graphics2d::SpriteInstance instance;
    };

    struct Run {
        graphics::Texture texture;
        std::vector<graphics2d::SpriteInstance> instances;
        graphics2d::StaticSpriteBatch batch;
        std::vector<AnimatedTile> animated;
    };

    // A block of cells, or with ysort a stretch of cells that stand on the same y, the ground of the region.
    struct Region {
        math::Rect bounds{};
        float ground = 0.0F;
        std::vector<Run> runs;
    };

    struct LayerCache {
        std::vector<Region> regions;
        bool baked = false;
    };

    // Returns the draw order that makes a draw standing on standing sort by the ground in canvases that sort by depth and in canvases that sort by y.
    [[nodiscard]] static graphics2d::DrawOrder groundOrder(const graphics2d::DrawOrder& order, float ground, float standing) noexcept;

    struct Inherited {
        math::Vec2 origin{};
        math::Vec2 offset{};
        math::Vec2 parallax{1.0F, 1.0F};
        math::Color tint = math::Color::white();
        bool visible = true;
    };

    static const DrawOptions kDefaultOptions;
    static constexpr int kRegionCells = 32;
    static constexpr int kEllipseSegments = 16;

    [[nodiscard]] static graphics2d::SpriteFlip flipsOf(std::uint32_t gid, bool diagonal) noexcept;
    [[nodiscard]] static math::Vec2 alignmentPivot(std::string_view alignment, Map::Orientation orientation) noexcept;

    // Size a tile covers when drawn, which is its image size or the map grid when the tileset asks for grid sized tiles.
    [[nodiscard]] static math::Vec2 drawSize(const Tileset& tileset, math::Rect source, math::Vec2 grid) noexcept;
    [[nodiscard]] static math::Rect instanceBounds(const graphics2d::SpriteInstance& instance) noexcept;
    [[nodiscard]] static math::Color fade(math::Color color, float opacity) noexcept;
    [[nodiscard]] static std::vector<math::Vec2> ellipseOutline(math::Vec2 center, math::Vec2 radii);

    // Outline of a capsule filling its box, with round caps on the two short sides.
    [[nodiscard]] static std::vector<math::Vec2> capsuleOutline(math::Vec2 topLeft, math::Vec2 size);
    [[nodiscard]] static math::Vec2 rotateAround(math::Vec2 point, math::Vec2 origin, float angle) noexcept;

    // Mirrors a point inside a tile the way Tiled flips the tile image, with the diagonal swap first.
    [[nodiscard]] static math::Vec2 flipInTile(math::Vec2 point, math::Vec2 tileSize, std::uint32_t gid) noexcept;

    // Adds one Tiled object to a body. Transform maps object coordinates to world positions.
    template <typename Transform> static void addObjectShape(physics2d::Body& body, const Object& object, const physics2d::Shape::Options& options, Transform&& transform);
    [[nodiscard]] static physics2d::CollisionFilter layerFilter(const Properties& properties);
    [[nodiscard]] static bool isFullCell(const Object& object, math::Vec2 tileSize) noexcept;
    [[nodiscard]] static Inherited inherit(const Inherited& parent, const Layer& layer) noexcept;

    // Bottom-left corner where a cell places its tile image.
    [[nodiscard]] math::Vec2 cellAnchor(int column, int row) const noexcept;
    [[nodiscard]] graphics2d::SpriteInstance tileInstance(const Tileset& tileset, std::uint32_t localId, std::uint32_t gid, math::Vec2 anchor) const;

    // Visits the cells of a tile layer in the order the map renders them, calling visit with each cell and gid.
    template <typename Visit> void forEachCell(const Layer& layer, Visit&& visit) const;

    void drawTree(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& parent, const View& view, const DrawOptions& options);
    void drawTiles(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const DrawOptions& options);
    void drawObjects(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const DrawOptions& options) const;
    void drawImage(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, const View& view, const graphics2d::DrawOrder& order) const;

    // Bakes a tile layer into regions of 32 by 32 cells, or into rows of cells that stand on the same y for ysort.
    void bake(graphics2d::Renderer& renderer, const Layer& layer, const Inherited& state, LayerCache& cache, bool rows) const;
    [[nodiscard]] math::Vec2 parallaxOffset(const Inherited& state, const View& view) const noexcept;
    [[nodiscard]] std::uint32_t animatedTileId(const Tile& tile) const noexcept;

    Map map;
    std::shared_ptr<text::Font> font;
    std::map<std::pair<std::uint32_t, bool>, LayerCache> caches;
    float time = 0.0F;
};

} // namespace haylen::tiled
