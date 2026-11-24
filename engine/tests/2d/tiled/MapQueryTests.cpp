#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/MapQuery.hpp"
#include "haylen/math/Geometry2.hpp"
#include "support/TiledHelper.hpp"

namespace haylen::tiled {

class MapQueryTest : public ::testing::Test {
  protected:
    // An 8 by 6 map of 16 pixel tiles with a wall down column 5 that leaves row 3 open, and objects in a group moved by 4, 2.
    [[nodiscard]] static Map queryMap(Map::Orientation orientation) {
        Map map;
        map.orientation = orientation;
        map.width = 8;
        map.height = 6;
        map.tileSize = orientation == Map::Orientation::Isometric ? haylen::math::Vec2{32.0F, 16.0F} : haylen::math::Vec2{16.0F, 16.0F};
        map.skew = orientation == Map::Orientation::Oblique ? haylen::math::Vec2{8.0F, 0.0F} : haylen::math::Vec2{};

        Layer walls{.name = "walls", .kind = Layer::Kind::Tile, .width = 8, .height = 6};
        walls.gids.assign(48, 0);

        for (int row = 0; row < 6; ++row) {
            walls.gids[static_cast<std::size_t>(row * 8 + 5)] = row == 3 ? 0U : (row == 4 ? 2U : 1U);
        }

        Layer rocks{.name = "rocks", .kind = Layer::Kind::Object};
        rocks.objects = {
            {.id = 1, .name = "crate", .type = "block", .position = {20.0F, 20.0F}, .size = {10.0F, 10.0F}}, {.id = 2, .name = "pond", .position = {40.0F, 20.0F}, .size = {20.0F, 10.0F}, .shape = Object::Shape::Ellipse}, {.id = 3, .name = "wedge", .position = {70.0F, 20.0F}, .shape = Object::Shape::Polygon, .points = {{0.0F, 0.0F}, {10.0F, 5.0F}, {0.0F, 10.0F}}}, {.id = 4, .name = "fence", .position = {90.0F, 10.0F}, .shape = Object::Shape::Polyline, .points = {{0.0F, 0.0F}, {0.0F, 30.0F}}}, {.id = 5, .name = "tree", .position = {100.0F, 40.0F}, .size = {16.0F, 32.0F}, .shape = Object::Shape::Tile, .gid = 1}, {.id = 6, .name = "spawn", .position = {5.0F, 5.0F}, .shape = Object::Shape::Point}, {.id = 7, .name = "log", .position = {120.0F, 20.0F}, .size = {30.0F, 10.0F}, .shape = Object::Shape::Capsule},
        };
        Layer group{.name = "things", .kind = Layer::Kind::Group, .offset = {4.0F, 2.0F}};
        group.layers.push_back(rocks);
        map.layers = {walls, group};
        map.tilesets = {{.firstGid = 1, .tileset = std::make_shared<Tileset>(Tileset{.image = "tiles.png", .tileSize = {16.0F, 16.0F}, .columns = 2, .tileCount = 2})}};
        return map;
    }
};

TEST_F(MapQueryTest, CastsRaysOverTileLayers) {
    const Map map = queryMap(Map::Orientation::Orthogonal);
    const MapQuery query(map);

    const std::optional<MapQuery::TileHit> wall = query.castTiles("walls", haylen::math::Ray2::between({8.0F, 8.0F}, {200.0F, 8.0F}));
    ASSERT_TRUE(wall.has_value());
    EXPECT_EQ(wall->column, 5);
    EXPECT_EQ(wall->row, 0);
    EXPECT_EQ(wall->gid, 1U);
    EXPECT_NEAR(wall->point.x, 80.0F, 1e-3F);
    EXPECT_NEAR(wall->distance, 72.0F, 1e-3F);
    EXPECT_EQ(wall->normal, (haylen::math::Vec2{-1.0F, 0.0F}));

    // Row 3 is open, and a filter can let chosen tiles through.
    EXPECT_FALSE(query.castTiles("walls", haylen::math::Ray2::between({8.0F, 56.0F}, {200.0F, 56.0F})).has_value());
    EXPECT_FALSE(query.castTiles("walls", haylen::math::Ray2::between({8.0F, 72.0F}, {200.0F, 72.0F}), [](std::uint32_t gid) { return gid != 2; }).has_value());
    const std::optional<MapQuery::TileHit> fromBelow = query.castTiles("walls", haylen::math::Ray2::between({88.0F, 150.0F}, {88.0F, 0.0F}));
    ASSERT_TRUE(fromBelow.has_value());
    EXPECT_EQ(fromBelow->row, 5);
    EXPECT_EQ(fromBelow->normal, (haylen::math::Vec2{0.0F, 1.0F}));

    EXPECT_THROW((void)query.castTiles("rocks", haylen::math::Ray2::between({}, {1.0F, 0.0F})), std::invalid_argument);
    EXPECT_THROW((void)query.castTiles("missing", haylen::math::Ray2::between({}, {1.0F, 0.0F})), std::invalid_argument);
    const Map staggered = queryMap(Map::Orientation::Staggered);
    EXPECT_THROW((void)MapQuery(staggered).castTiles("walls", haylen::math::Ray2::between({}, {1.0F, 0.0F})), std::invalid_argument);
}

TEST_F(MapQueryTest, CastsTileRaysOnlyOverTheCellsOfTheLayer) {
    const Map map = queryMap(Map::Orientation::Orthogonal);
    const MapQuery query(map);

    // Rays of any length walk at most across the layer, so far ends and infinite rays cost no more than short rays.
    EXPECT_FALSE(query.castTiles("walls", haylen::math::Ray2::between({8.0F, 56.0F}, {1.0e12F, 56.0F})).has_value());
    EXPECT_FALSE(query.castTiles("walls", haylen::math::Ray2{{8.0F, 56.0F}, {1.0F, 0.0F}}).has_value());
    const std::optional<MapQuery::TileHit> endless = query.castTiles("walls", haylen::math::Ray2{{8.0F, 8.0F}, {1.0F, 0.0F}});
    ASSERT_TRUE(endless.has_value());
    EXPECT_EQ(endless->column, 5);
    EXPECT_NEAR(endless->distance, 72.0F, 1e-3F);

    // Rays from outside enter the layer through its sides, from the left and from the right.
    const std::optional<MapQuery::TileHit> fromLeft = query.castTiles("walls", haylen::math::Ray2::between({-1000.0F, 8.0F}, {200.0F, 8.0F}));
    ASSERT_TRUE(fromLeft.has_value());
    EXPECT_EQ(fromLeft->column, 5);
    EXPECT_NEAR(fromLeft->distance, 1080.0F, 1e-2F);
    EXPECT_EQ(fromLeft->normal, (haylen::math::Vec2{-1.0F, 0.0F}));
    const std::optional<MapQuery::TileHit> fromRight = query.castTiles("walls", haylen::math::Ray2::between({1000.0F, 72.0F}, {-1000.0F, 72.0F}));
    ASSERT_TRUE(fromRight.has_value());
    EXPECT_EQ(fromRight->column, 5);
    EXPECT_EQ(fromRight->gid, 2U);
    EXPECT_NEAR(fromRight->point.x, 96.0F, 1e-3F);
    EXPECT_EQ(fromRight->normal, (haylen::math::Vec2{1.0F, 0.0F}));
    EXPECT_FALSE(query.castTiles("walls", haylen::math::Ray2::between({-1000.0F, -8.0F}, {1000.0F, -8.0F})).has_value());

    // An infinite layer spans its chunks, which may start left of the origin.
    Map endlessMap = map;
    Layer& walls = endlessMap.layers.front();
    walls.chunks = {{.x = -4, .y = 0, .width = 2, .height = 1, .gids = {0, 1}}, {.x = 2, .y = 0, .width = 2, .height = 1, .gids = {1, 0}}};
    walls.infinite = true;
    const std::optional<MapQuery::TileHit> chunk = MapQuery(endlessMap).castTiles("walls", haylen::math::Ray2::between({-1000.0F, 8.0F}, {1000.0F, 8.0F}));
    ASSERT_TRUE(chunk.has_value());
    EXPECT_EQ(chunk->column, -3);
    EXPECT_NEAR(chunk->point.x, -48.0F, 1e-3F);
}

TEST_F(MapQueryTest, CastsRaysOverIsometricAndObliqueTiles) {
    for (const Map::Orientation orientation : {Map::Orientation::Isometric, Map::Orientation::Oblique}) {
        const Map map = queryMap(orientation);
        const MapQuery query(map);

        // A ray from the middle of cell 1, 0 toward the middle of cell 7, 0 stops in the wall at cell 5, 0.
        const haylen::math::Vec2 start = map.cellToWorld(1, 0) + map.objectToWorld({8.0F, 8.0F}) - map.objectToWorld({});
        const haylen::math::Vec2 end = map.cellToWorld(7, 0) + map.objectToWorld({8.0F, 8.0F}) - map.objectToWorld({});
        const std::optional<MapQuery::TileHit> hit = query.castTiles("walls", haylen::math::Ray2::between(start, end));
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(hit->column, 5);
        EXPECT_EQ(hit->row, 0);
        EXPECT_NEAR(hit->normal.getLength(), 1.0F, 1e-4F);
        EXPECT_LT(haylen::math::Vec2::dot(hit->normal, end - start), 0.0F);
        EXPECT_NEAR(hit->distance, haylen::math::Vec2::distance(start, hit->point), 1e-3F);
    }
}

TEST_F(MapQueryTest, OutlinesObjectsAndCastsRaysAgainstThem) {
    const Map map = queryMap(Map::Orientation::Orthogonal);
    const MapQuery query(map);
    const std::vector<Object>& objects = test::TiledHelper::layer(map, "rocks").objects;
    const haylen::math::Vec2 offset{4.0F, 2.0F};

    std::vector<haylen::math::Vec2> outline;
    query.getOutline(objects[0], offset, outline);
    EXPECT_EQ(outline, (std::vector<haylen::math::Vec2>{{24.0F, 22.0F}, {34.0F, 22.0F}, {34.0F, 32.0F}, {24.0F, 32.0F}}));
    query.getOutline(objects[4], offset, outline);
    EXPECT_EQ(outline.front(), (haylen::math::Vec2{104.0F, 10.0F}));
    query.getOutline(objects[1], offset, outline);
    EXPECT_EQ(outline.size(), 16U);

    for (const haylen::math::Vec2 point : outline) {
        const haylen::math::Vec2 local = point - haylen::math::Vec2{54.0F, 27.0F};
        EXPECT_NEAR(local.x * local.x / 100.0F + local.y * local.y / 25.0F, 1.0F, 1e-3F);
    }

    query.getOutline(objects[6], offset, outline);
    EXPECT_EQ(outline.size(), 18U);
    EXPECT_NEAR(std::fabs(haylen::math::Geometry2::signedArea(outline)), 20.0F * 10.0F + std::numbers::pi_v<float> * 25.0F, 3.0F);
    query.getOutline(objects[5], offset, outline);
    EXPECT_TRUE(outline.empty());

    std::vector<std::vector<haylen::math::Vec2>> outlines;
    query.getOutlines("rocks", outlines);
    EXPECT_EQ(outlines.size(), 5U);
    query.getOutlines("", outlines);
    EXPECT_EQ(outlines.size(), 5U);

    // The ray crosses the objects from left to right, and a polyline blocks it from either side.
    const std::optional<MapQuery::ObjectHit> crate = query.castObjects("rocks", haylen::math::Ray2::between({0.0F, 27.0F}, {200.0F, 27.0F}));
    ASSERT_TRUE(crate.has_value());
    EXPECT_EQ(crate->object->name, "crate");
    EXPECT_NEAR(crate->point.x, 24.0F, 1e-3F);
    EXPECT_EQ(crate->normal, (haylen::math::Vec2{-1.0F, 0.0F}));
    const std::optional<MapQuery::ObjectHit> fenceFromLeft = query.castObjects("", haylen::math::Ray2::between({85.0F, 27.0F}, {200.0F, 27.0F}));
    ASSERT_TRUE(fenceFromLeft.has_value());
    EXPECT_EQ(fenceFromLeft->object->name, "fence");
    const std::optional<MapQuery::ObjectHit> fenceFromRight = query.castObjects("", haylen::math::Ray2::between({100.0F, 40.0F}, {60.0F, 40.0F}));
    ASSERT_TRUE(fenceFromRight.has_value());
    EXPECT_EQ(fenceFromRight->object->name, "fence");
    const std::optional<MapQuery::ObjectHit> fromInside = query.castObjects("", haylen::math::Ray2::between({150.0F, 27.0F}, {88.0F, 27.0F}));
    ASSERT_TRUE(fromInside.has_value());
    EXPECT_EQ(fromInside->distance, 0.0F);
    EXPECT_FALSE(query.castObjects("rocks", haylen::math::Ray2::between({0.0F, 100.0F}, {200.0F, 100.0F})).has_value());
    EXPECT_THROW((void)query.castObjects("walls", haylen::math::Ray2::between({}, {1.0F, 0.0F})), std::invalid_argument);
}

TEST_F(MapQueryTest, OutlinesTileObjectsWhereTheirImagesDraw) {
    // The tree tileset aligns its objects by their bottom center, so the tree at 100, 40 covers 92 to 108 across.
    Map map = queryMap(Map::Orientation::Orthogonal);
    map.tilesets.front().tileset->objectAlignment = "bottom";
    const Object& tree = test::TiledHelper::layer(map, "rocks").objects[4];
    std::vector<haylen::math::Vec2> outline;
    MapQuery(map).getOutline(tree, {}, outline);
    EXPECT_EQ(outline, (std::vector<haylen::math::Vec2>{{92.0F, 8.0F}, {108.0F, 8.0F}, {108.0F, 40.0F}, {92.0F, 40.0F}}));

    // On isometric maps the image stands upright on the world position of the object, centered by the unspecified alignment.
    const Map isometric = queryMap(Map::Orientation::Isometric);
    const haylen::math::Vec2 anchor = isometric.objectToWorld(tree.position);
    MapQuery(isometric).getOutline(test::TiledHelper::layer(isometric, "rocks").objects[4], {}, outline);
    EXPECT_EQ(outline.front(), anchor - haylen::math::Vec2(8.0F, 32.0F));
    EXPECT_EQ(outline[2], anchor + haylen::math::Vec2(8.0F, 0.0F));

    Object lost = tree;
    lost.gid = 9;
    EXPECT_THROW(MapQuery(map).getOutline(lost, {}, outline), std::invalid_argument);
}

} // namespace haylen::tiled
