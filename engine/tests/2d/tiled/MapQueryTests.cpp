#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <memory>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/MapQuery.hpp"
#include "haylen/math/Geometry.hpp"
#include "support/EngineFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen {

namespace {

class MapQueryTest : public ::testing::Test {
  protected:
    // An 8 by 6 map of 16 pixel tiles with a wall down column 5 that leaves row 3 open, and objects in a group moved by 4, 2.
    [[nodiscard]] static tiled::Map queryMap(tiled::Map::Orientation orientation) {
        tiled::Map map;
        map.orientation = orientation;
        map.width = 8;
        map.height = 6;
        map.tileSize = orientation == tiled::Map::Orientation::Isometric ? math::Vec2{32.0F, 16.0F} : math::Vec2{16.0F, 16.0F};
        map.skew = orientation == tiled::Map::Orientation::Oblique ? math::Vec2{8.0F, 0.0F} : math::Vec2{};

        tiled::Layer walls{.name = "walls", .kind = tiled::Layer::Kind::Tile, .width = 8, .height = 6};
        walls.gids.assign(48, 0);
        for (int row = 0; row < 6; ++row) {
            walls.gids[static_cast<std::size_t>(row * 8 + 5)] = row == 3 ? 0U : (row == 4 ? 2U : 1U);
        }

        tiled::Layer rocks{.name = "rocks", .kind = tiled::Layer::Kind::Object};
        rocks.objects = {
            {.id = 1, .name = "crate", .type = "block", .position = {20.0F, 20.0F}, .size = {10.0F, 10.0F}}, {.id = 2, .name = "pond", .position = {40.0F, 20.0F}, .size = {20.0F, 10.0F}, .shape = tiled::Object::Shape::Ellipse}, {.id = 3, .name = "wedge", .position = {70.0F, 20.0F}, .shape = tiled::Object::Shape::Polygon, .points = {{0.0F, 0.0F}, {10.0F, 5.0F}, {0.0F, 10.0F}}}, {.id = 4, .name = "fence", .position = {90.0F, 10.0F}, .shape = tiled::Object::Shape::Polyline, .points = {{0.0F, 0.0F}, {0.0F, 30.0F}}}, {.id = 5, .name = "tree", .position = {100.0F, 40.0F}, .size = {16.0F, 32.0F}, .shape = tiled::Object::Shape::Tile, .gid = 1}, {.id = 6, .name = "spawn", .position = {5.0F, 5.0F}, .shape = tiled::Object::Shape::Point}, {.id = 7, .name = "log", .position = {120.0F, 20.0F}, .size = {30.0F, 10.0F}, .shape = tiled::Object::Shape::Capsule},
        };
        tiled::Layer group{.name = "things", .kind = tiled::Layer::Kind::Group, .offset = {4.0F, 2.0F}};
        group.layers.push_back(rocks);
        map.layers = {walls, group};
        map.tilesets = {{.firstGid = 1, .tileset = std::make_shared<tiled::Tileset>(tiled::Tileset{.image = "tiles.png", .tileSize = {16.0F, 16.0F}, .columns = 2, .tileCount = 2})}};
        return map;
    }
};

} // namespace

TEST_F(MapQueryTest, CastsRaysOverTileLayers) {
    const tiled::Map map = queryMap(tiled::Map::Orientation::Orthogonal);
    const tiled::MapQuery query(map);

    const std::optional<tiled::MapQuery::TileHit> wall = query.castTiles("walls", math::Ray::between({8.0F, 8.0F}, {200.0F, 8.0F}));
    ASSERT_TRUE(wall.has_value());
    EXPECT_EQ(wall->column, 5);
    EXPECT_EQ(wall->row, 0);
    EXPECT_EQ(wall->gid, 1U);
    EXPECT_NEAR(wall->point.x, 80.0F, 1e-3F);
    EXPECT_NEAR(wall->distance, 72.0F, 1e-3F);
    EXPECT_EQ(wall->normal, (math::Vec2{-1.0F, 0.0F}));

    // Row 3 is open, and a filter can let chosen tiles through.
    EXPECT_FALSE(query.castTiles("walls", math::Ray::between({8.0F, 56.0F}, {200.0F, 56.0F})).has_value());
    EXPECT_FALSE(query.castTiles("walls", math::Ray::between({8.0F, 72.0F}, {200.0F, 72.0F}), [](std::uint32_t gid) { return gid != 2; }).has_value());
    const std::optional<tiled::MapQuery::TileHit> fromBelow = query.castTiles("walls", math::Ray::between({88.0F, 150.0F}, {88.0F, 0.0F}));
    ASSERT_TRUE(fromBelow.has_value());
    EXPECT_EQ(fromBelow->row, 5);
    EXPECT_EQ(fromBelow->normal, (math::Vec2{0.0F, 1.0F}));

    EXPECT_THROW((void)query.castTiles("rocks", math::Ray::between({}, {1.0F, 0.0F})), std::invalid_argument);
    EXPECT_THROW((void)query.castTiles("missing", math::Ray::between({}, {1.0F, 0.0F})), std::invalid_argument);
    const tiled::Map staggered = queryMap(tiled::Map::Orientation::Staggered);
    EXPECT_THROW((void)tiled::MapQuery(staggered).castTiles("walls", math::Ray::between({}, {1.0F, 0.0F})), std::invalid_argument);
}

TEST_F(MapQueryTest, CastsTileRaysOnlyOverTheCellsOfTheLayer) {
    const tiled::Map map = queryMap(tiled::Map::Orientation::Orthogonal);
    const tiled::MapQuery query(map);

    // Rays of any length walk at most across the layer, so far ends and infinite rays cost no more than short rays.
    EXPECT_FALSE(query.castTiles("walls", math::Ray::between({8.0F, 56.0F}, {1.0e12F, 56.0F})).has_value());
    EXPECT_FALSE(query.castTiles("walls", math::Ray{{8.0F, 56.0F}, {1.0F, 0.0F}}).has_value());
    const std::optional<tiled::MapQuery::TileHit> endless = query.castTiles("walls", math::Ray{{8.0F, 8.0F}, {1.0F, 0.0F}});
    ASSERT_TRUE(endless.has_value());
    EXPECT_EQ(endless->column, 5);
    EXPECT_NEAR(endless->distance, 72.0F, 1e-3F);

    // Rays from outside enter the layer through its sides, from the left and from the right.
    const std::optional<tiled::MapQuery::TileHit> fromLeft = query.castTiles("walls", math::Ray::between({-1000.0F, 8.0F}, {200.0F, 8.0F}));
    ASSERT_TRUE(fromLeft.has_value());
    EXPECT_EQ(fromLeft->column, 5);
    EXPECT_NEAR(fromLeft->distance, 1080.0F, 1e-2F);
    EXPECT_EQ(fromLeft->normal, (math::Vec2{-1.0F, 0.0F}));
    const std::optional<tiled::MapQuery::TileHit> fromRight = query.castTiles("walls", math::Ray::between({1000.0F, 72.0F}, {-1000.0F, 72.0F}));
    ASSERT_TRUE(fromRight.has_value());
    EXPECT_EQ(fromRight->column, 5);
    EXPECT_EQ(fromRight->gid, 2U);
    EXPECT_NEAR(fromRight->point.x, 96.0F, 1e-3F);
    EXPECT_EQ(fromRight->normal, (math::Vec2{1.0F, 0.0F}));
    EXPECT_FALSE(query.castTiles("walls", math::Ray::between({-1000.0F, -8.0F}, {1000.0F, -8.0F})).has_value());

    // An infinite layer spans its chunks, which may start left of the origin.
    tiled::Map endlessMap = map;
    tiled::Layer& walls = endlessMap.layers.front();
    walls.chunks = {{.x = -4, .y = 0, .width = 2, .height = 1, .gids = {0, 1}}, {.x = 2, .y = 0, .width = 2, .height = 1, .gids = {1, 0}}};
    const std::optional<tiled::MapQuery::TileHit> chunk = tiled::MapQuery(endlessMap).castTiles("walls", math::Ray::between({-1000.0F, 8.0F}, {1000.0F, 8.0F}));
    ASSERT_TRUE(chunk.has_value());
    EXPECT_EQ(chunk->column, -3);
    EXPECT_NEAR(chunk->point.x, -48.0F, 1e-3F);
}

TEST_F(MapQueryTest, CastsRaysOverIsometricAndObliqueTiles) {
    for (const tiled::Map::Orientation orientation : {tiled::Map::Orientation::Isometric, tiled::Map::Orientation::Oblique}) {
        const tiled::Map map = queryMap(orientation);
        const tiled::MapQuery query(map);

        // A ray from the middle of cell 1, 0 toward the middle of cell 7, 0 stops in the wall at cell 5, 0.
        const math::Vec2 start = map.cellToWorld(1, 0) + map.objectToWorld({8.0F, 8.0F}) - map.objectToWorld({});
        const math::Vec2 end = map.cellToWorld(7, 0) + map.objectToWorld({8.0F, 8.0F}) - map.objectToWorld({});
        const std::optional<tiled::MapQuery::TileHit> hit = query.castTiles("walls", math::Ray::between(start, end));
        ASSERT_TRUE(hit.has_value());
        EXPECT_EQ(hit->column, 5);
        EXPECT_EQ(hit->row, 0);
        EXPECT_NEAR(hit->normal.getLength(), 1.0F, 1e-4F);
        EXPECT_LT(math::Vec2::dot(hit->normal, end - start), 0.0F);
        EXPECT_NEAR(hit->distance, math::Vec2::distance(start, hit->point), 1e-3F);
    }
}

TEST_F(MapQueryTest, OutlinesObjectsAndCastsRaysAgainstThem) {
    const tiled::Map map = queryMap(tiled::Map::Orientation::Orthogonal);
    const tiled::MapQuery query(map);
    const std::vector<tiled::Object>& objects = map.findLayer("rocks")->objects;
    const math::Vec2 offset{4.0F, 2.0F};

    std::vector<math::Vec2> outline;
    query.getOutline(objects[0], offset, outline);
    EXPECT_EQ(outline, (std::vector<math::Vec2>{{24.0F, 22.0F}, {34.0F, 22.0F}, {34.0F, 32.0F}, {24.0F, 32.0F}}));
    query.getOutline(objects[4], offset, outline);
    EXPECT_EQ(outline.front(), (math::Vec2{104.0F, 10.0F}));
    query.getOutline(objects[1], offset, outline);
    EXPECT_EQ(outline.size(), 16U);
    for (const math::Vec2 point : outline) {
        const math::Vec2 local = point - math::Vec2{54.0F, 27.0F};
        EXPECT_NEAR(local.x * local.x / 100.0F + local.y * local.y / 25.0F, 1.0F, 1e-3F);
    }
    query.getOutline(objects[6], offset, outline);
    EXPECT_EQ(outline.size(), 18U);
    EXPECT_NEAR(std::fabs(math::Geometry::signedArea(outline)), 20.0F * 10.0F + std::numbers::pi_v<float> * 25.0F, 3.0F);
    query.getOutline(objects[5], offset, outline);
    EXPECT_TRUE(outline.empty());

    std::vector<std::vector<math::Vec2>> outlines;
    query.getOutlines("rocks", outlines);
    EXPECT_EQ(outlines.size(), 5U);
    query.getOutlines("", outlines);
    EXPECT_EQ(outlines.size(), 5U);

    // The ray crosses the objects from left to right, and a polyline blocks it from either side.
    const std::optional<tiled::MapQuery::ObjectHit> crate = query.castObjects("rocks", math::Ray::between({0.0F, 27.0F}, {200.0F, 27.0F}));
    ASSERT_TRUE(crate.has_value());
    EXPECT_EQ(crate->object->name, "crate");
    EXPECT_NEAR(crate->point.x, 24.0F, 1e-3F);
    EXPECT_EQ(crate->normal, (math::Vec2{-1.0F, 0.0F}));
    EXPECT_EQ(query.castObjects("", math::Ray::between({85.0F, 27.0F}, {200.0F, 27.0F}))->object->name, "fence");
    EXPECT_EQ(query.castObjects("", math::Ray::between({100.0F, 40.0F}, {60.0F, 40.0F}))->object->name, "fence");
    EXPECT_EQ(query.castObjects("", math::Ray::between({150.0F, 27.0F}, {88.0F, 27.0F}))->distance, 0.0F);
    EXPECT_FALSE(query.castObjects("rocks", math::Ray::between({0.0F, 100.0F}, {200.0F, 100.0F})).has_value());
    EXPECT_THROW((void)query.castObjects("walls", math::Ray::between({}, {1.0F, 0.0F})), std::invalid_argument);
}

TEST_F(MapQueryTest, OutlinesTileObjectsWhereTheirImagesDraw) {
    // The tree tileset aligns its objects by their bottom center, so the tree at 100, 40 covers 92 to 108 across.
    tiled::Map map = queryMap(tiled::Map::Orientation::Orthogonal);
    map.tilesets.front().tileset->objectAlignment = "bottom";
    const tiled::Object& tree = map.findLayer("rocks")->objects[4];
    std::vector<math::Vec2> outline;
    tiled::MapQuery(map).getOutline(tree, {}, outline);
    EXPECT_EQ(outline, (std::vector<math::Vec2>{{92.0F, 8.0F}, {108.0F, 8.0F}, {108.0F, 40.0F}, {92.0F, 40.0F}}));

    // On isometric maps the image stands upright on the world position of the object, centered by the unspecified alignment.
    const tiled::Map isometric = queryMap(tiled::Map::Orientation::Isometric);
    const math::Vec2 anchor = isometric.objectToWorld(tree.position);
    tiled::MapQuery(isometric).getOutline(isometric.findLayer("rocks")->objects[4], {}, outline);
    EXPECT_EQ(outline.front(), anchor - math::Vec2(8.0F, 32.0F));
    EXPECT_EQ(outline[2], anchor + math::Vec2(8.0F, 0.0F));

    tiled::Object lost = tree;
    lost.gid = 9;
    EXPECT_THROW(tiled::MapQuery(map).getOutline(lost, {}, outline), std::invalid_argument);
}

TEST(MapQueryLuaTest, CastsRaysOverMapsFromLua) {
    const std::vector<std::uint8_t> image = test::TestFiles::pngImage(32, 16, 0xFFFFFFFFU);
    // clang-format off
    const std::string map = R"({
        "type": "map", "orientation": "orthogonal", "renderorder": "right-down", "width": 8, "height": 6, "tilewidth": 16, "tileheight": 16, "infinite": false,
        "tilesets": [{"firstgid": 1, "name": "tiles", "tilewidth": 16, "tileheight": 16, "tilecount": 2, "columns": 2, "image": "tiles.png", "imagewidth": 32, "imageheight": 16}],
        "layers": [
            {"id": 1, "name": "walls", "type": "tilelayer", "width": 8, "height": 6, "data": [0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0, 0,0,0,0,0,1,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,2,0,0, 0,0,0,0,0,1,0,0]},
            {"id": 2, "name": "rocks", "type": "objectgroup", "offsetx": 4, "offsety": 2, "objects": [
                {"id": 1, "name": "crate", "type": "block", "x": 20, "y": 20, "width": 10, "height": 10},
                {"id": 2, "name": "fence", "x": 90, "y": 10, "polyline": [{"x": 0, "y": 0}, {"x": 0, "y": 30}]}
            ]}
        ]
    })";
    test::EngineFixture fixture({{"content/maps/query.tmj", map}, {"content/maps/tiles.png", std::string(image.begin(), image.end())}});
    fixture.runLua(R"(
        tiled = require('haylen.tiled')
        map = tiled.newMapRenderer(require('haylen.assets').load('maps/query.tmj'))
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("local hit = map:raycastTiles('walls', 8, 8, 200, 8) return hit.column .. ':' .. hit.row .. ' ' .. hit.gid .. ' ' .. hit.x .. ' ' .. hit.normalX .. ' ' .. hit.distance"), "5:0 1 80.0 -1.0 72.0");
    EXPECT_EQ(fixture.lua("return tostring(map:raycastTiles('walls', 8, 56, 200, 56)) .. ' ' .. tostring(map:raycastTiles('walls', 8, 72, 200, 72, function(gid) return gid ~= 2 end))"), "nil nil");
    EXPECT_EQ(fixture.lua("local hit = map:raycastObjects('rocks', 0, 27, 200, 27) return hit.name .. ' ' .. hit.id .. ' ' .. hit.type .. ' ' .. hit.x"), "crate 1 block 24.0");
    EXPECT_EQ(fixture.lua("return map:raycastObjects(nil, 60, 27, 200, 27).name .. ' ' .. tostring(map:raycastObjects('rocks', 0, 100, 200, 100))"), "fence nil");
    EXPECT_EQ(fixture.lua("local outlines = map:objectOutlines('rocks') return #outlines .. ' ' .. #outlines[1] .. ' ' .. outlines[1][1].x .. ' ' .. #map:objectOutlines()"), "1 4 24.0 1");
    EXPECT_NE(fixture.lua("map:raycastTiles('rocks', 0, 0, 1, 0)").find("no tile layer named 'rocks'"), std::string::npos);

    // The solid callback runs once the ray has gathered its tiles, so a tile it places behind the wall does not stop the ray.
    EXPECT_EQ(fixture.lua("local hit = map:raycastTiles('walls', 8, 8, 200, 8, function(gid) map:setTile('walls', 6, 0, 2) return gid == 2 end) return tostring(hit) .. ' ' .. map:tileAt('walls', 6, 0)"), "nil 2");
}

} // namespace haylen
