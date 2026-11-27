#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <zlib.h>
#include <zstd.h>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/2d/tiled/MapWriter.hpp"
#include "haylen/2d/tiled/ObjectFactories.hpp"
#include "haylen/2d/tiled/PropertyTypes.hpp"
#include "haylen/2d/tiled/World.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Geometry2D.hpp"
#include "haylen/math/Math.hpp"
#include "support/AllocationTracker.hpp"
#include "support/DrawingScene.hpp"
#include "support/EngineFixture.hpp"
#include "support/Physics2DHelper.hpp"
#include "support/TestFiles.hpp"
#include "support/TiledHelper.hpp"
#include "support/TiledMaps.hpp"

namespace haylen::tiled {

TEST(MapTest, ParsesMapsTilesetsLayersAndTemplates) {
    const Map map = Map::parse(test::TiledMaps::orthogonalMap(), "maps/island.tmj", test::TiledMaps::reader());

    EXPECT_EQ(map.path, "maps/island.tmj");
    EXPECT_EQ(map.type, "level");
    EXPECT_EQ(map.orientation, Map::Orientation::Orthogonal);
    EXPECT_EQ(map.getPixelBounds(), (haylen::math::Rect{0.0F, 0.0F, 64.0F, 48.0F}));
    EXPECT_EQ(map.backgroundColor, haylen::math::Color::parse("#FF102030"));
    EXPECT_EQ(map.properties.getString("title", ""), "Island");
    EXPECT_EQ(map.properties.getNumber("level", 0.0), 3.0);
    EXPECT_TRUE(map.properties.getBool("safe", false));
    EXPECT_EQ(test::TiledHelper::property(map.properties, "music").value, "audio/theme.ogg");
    EXPECT_EQ(test::TiledHelper::property(map.properties, "spawn").propertyType, "Spawn");
    EXPECT_EQ(test::TiledHelper::property(map.properties, "spawn").value.at("kind"), "goblin");
    const haylen::core::Json& loot = test::TiledHelper::property(map.properties, "loot").value;
    EXPECT_EQ(test::TiledHelper::property(map.properties, "loot").type, "list");
    EXPECT_EQ(loot[2].at("value"), "audio/coin.ogg");
    EXPECT_EQ(loot[3].at("value")[1].at("value"), "maps/gem.png");
    EXPECT_EQ(loot[4].at("propertytype"), "Spawn");
    EXPECT_FALSE(map.properties.has("missing"));
    EXPECT_EQ(map.properties.getNumber("missing", 4.0), 4.0);

    ASSERT_EQ(map.tilesets.size(), 2U);
    const Tileset& terrain = *map.tilesets[0].tileset;
    EXPECT_EQ(terrain.path, "maps/tiles/terrain.tsj");
    EXPECT_EQ(terrain.image, "maps/images/terrain.png");
    EXPECT_EQ(terrain.getSource(5), (haylen::math::Rect{16.0F, 16.0F, 16.0F, 16.0F}));
    EXPECT_EQ(test::TiledHelper::tile(terrain, 1).type, "water");
    EXPECT_TRUE(test::TiledHelper::tile(terrain, 1).properties.getBool("deep", false));
    EXPECT_EQ(test::TiledHelper::tile(terrain, 2).animation.size(), 2U);
    EXPECT_FLOAT_EQ(test::TiledHelper::tile(terrain, 2).animation[1].duration, 0.1F);
    EXPECT_EQ(terrain.wangSets.front().tiles.front().wangId[1], 1U);
    EXPECT_EQ(terrain.wangSets.front().type, "shore");
    EXPECT_EQ(terrain.wangSets.front().colors.front().type, "ground");
    EXPECT_FLOAT_EQ(terrain.wangSets.front().colors.front().probability, 0.5F);
    EXPECT_EQ(terrain.properties.getString("theme", ""), "grass");
    EXPECT_EQ(terrain.findTile(7), nullptr);

    const Tileset& props = *map.tilesets[1].tileset;
    EXPECT_EQ(test::TiledHelper::tile(props, 0).image, "maps/props/tree.png");
    EXPECT_EQ(props.getSource(0), (haylen::math::Rect{0.0F, 0.0F, 16.0F, 32.0F}));
    EXPECT_EQ(test::TiledHelper::tileset(map, 101).firstGid, 100U);
    EXPECT_EQ(test::TiledHelper::tileset(map, 8).firstGid, 1U);
    EXPECT_EQ(map.findTileset(99), nullptr);
    EXPECT_EQ(map.findTileset(102), nullptr);
    EXPECT_EQ(map.findTileset(0), nullptr);

    const Layer& ground = test::TiledHelper::layer(map, "ground");
    EXPECT_EQ(ground.getGid(1, 0), 2U);
    EXPECT_EQ(ground.getGid(3, 2), 5U);
    EXPECT_EQ(ground.getGid(9, 9), 0U);
    EXPECT_EQ(test::TiledHelper::layer(map, "decor").getGid(1, 1), 2147483652U);
    EXPECT_EQ(test::TiledHelper::layer(map, "inner").getGid(1, 1), 1U);
    EXPECT_EQ(map.findLayer("missing"), nullptr);
    EXPECT_EQ(test::TiledHelper::layer(map, "group").opacity, 0.5F);

    const Layer& things = test::TiledHelper::layer(map, "things");
    ASSERT_EQ(things.objects.size(), 9U);
    EXPECT_EQ(things.objects[0].shape, Object::Shape::Point);
    EXPECT_NEAR(things.objects[1].rotation, haylen::math::Math::kPi / 2.0F, 0.0001F);
    EXPECT_EQ(things.objects[2].shape, Object::Shape::Ellipse);
    EXPECT_EQ(things.objects[3].points.size(), 3U);
    EXPECT_EQ(things.objects[4].shape, Object::Shape::Polyline);
    EXPECT_EQ(things.objects[5].text.horizontalAlign, "center");
    EXPECT_EQ(things.objects[5].text.color, haylen::math::Color::fromHex(0x00FF00FFU));
    EXPECT_EQ(things.objects[6].shape, Object::Shape::Tile);

    const Object& rock = things.objects[7];
    EXPECT_EQ(rock.name, "rock");
    EXPECT_EQ(rock.gid, 5U);
    EXPECT_EQ(rock.templatePath, "maps/templates/rock.tj");
    EXPECT_EQ(rock.position, haylen::math::Vec2(40.0F, 48.0F));
    EXPECT_EQ(things.objects[8].name, "big rock");
    EXPECT_EQ(things.objects[8].properties.getNumber("size", 0.0), 3.0);
    EXPECT_TRUE(things.objects[8].properties.getBool("hard", false));

    const Layer& sky = test::TiledHelper::layer(map, "sky");
    EXPECT_EQ(sky.kind, Layer::Kind::Image);
    EXPECT_EQ(sky.transparentColor, haylen::math::Color::fromHex(0xFF00FFFFU));
    EXPECT_EQ(sky.parallax, haylen::math::Vec2(0.5F, 0.5F));

    const std::vector<Map::Image> images = map.getImages();
    ASSERT_EQ(images.size(), 4U);
    EXPECT_EQ(images.back().path, "maps/backgrounds/sky.png");
}

TEST(MapTest, RejectsBrokenData) {
    const auto parse = [](haylen::core::Json document) { return Map::parse(document, "maps/broken.tmj", test::TiledMaps::reader()); };
    haylen::core::Json map = test::TiledMaps::orthogonalMap();
    map["orientation"] = "diagonal";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = test::TiledMaps::orthogonalMap();
    map["renderorder"] = "down";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = test::TiledMaps::orthogonalMap();
    map["layers"][0]["compression"] = "lz4";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = test::TiledMaps::orthogonalMap();
    map["layers"][0]["data"] = "!!!!";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = test::TiledMaps::orthogonalMap();
    map["layers"][0]["compression"] = "zstd";
    EXPECT_THROW((void)parse(map), std::runtime_error);

    // A layer that claims far more cells than its compressed data holds fails without allocating them.
    for (const std::string compression : {"zlib", "zstd"}) {
        map = test::TiledMaps::orthogonalMap();
        map["layers"][0]["width"] = 4096;
        map["layers"][0]["height"] = 4096;
        map["layers"][0]["compression"] = compression;
        map["layers"][0]["data"] = test::TiledMaps::encodeTiles({1, 2, 3}, compression);
        const test::AllocationTracker allocations;
        EXPECT_THROW((void)parse(map), std::runtime_error);
        EXPECT_LT(allocations.getLargest(), std::size_t{1} << 20U);
    }

    map = test::TiledMaps::orthogonalMap();
    map["layers"][0]["data"] = haylen::core::Json::array({1, 2});
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = test::TiledMaps::orthogonalMap();
    map["layers"][0]["type"] = "shapelayer";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = test::TiledMaps::orthogonalMap();
    map["backgroundcolor"] = "blue";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = test::TiledMaps::orthogonalMap();
    map["tilesets"].erase(0);
    EXPECT_THROW((void)parse(map), std::invalid_argument) << "The rock template needs the terrain tileset.";

    map = test::TiledMaps::orthogonalMap();
    map["layers"][2].erase("id");
    EXPECT_THROW((void)parse(map), haylen::core::Json::out_of_range) << "Layer caches tell layers apart by their ids.";

    // Sizes are checked before any cell is read, so a layer whose cell count overflows cannot pass with empty data.
    // clang-format off
    const auto failure = [&parse](const haylen::core::Json& document) {
        try {
            (void)parse(document);
        } catch (const std::invalid_argument& error) {
            return std::string(error.what());
        }

        return std::string("no error");
    };
    // clang-format on
    map = test::TiledMaps::orthogonalMap();
    map["layers"][3]["width"] = 65536;
    map["layers"][3]["height"] = 65536;
    map["layers"][3]["data"] = haylen::core::Json::array();
    EXPECT_EQ(failure(map), "The Tiled tile layer \"raw\" has more cells than a layer can hold.");
    map["layers"][3]["width"] = -4;
    map["layers"][3]["height"] = -3;
    EXPECT_EQ(failure(map), "The Tiled tile layer \"raw\" has a negative size.");
    map = test::TiledMaps::infiniteMap();
    map["layers"][0]["chunks"][1]["width"] = -2;
    EXPECT_EQ(failure(map), "The Tiled tile layer \"ground\" has a negative size.");
    // A chunk whose cells reach far from the origin would overflow the arithmetic over its cells, so it is refused like any hostile file.
    map = test::TiledMaps::infiniteMap();
    map["layers"][0]["chunks"][1]["x"] = 2147483646;
    EXPECT_EQ(failure(map), "The chunk at 2147483646, 0 of the Tiled tile layer \"ground\" reaches farther than 16777216 cells from the origin.");
    map["layers"][0]["chunks"][1]["x"] = 16777215;
    EXPECT_EQ(failure(map), "The chunk at 16777215, 0 of the Tiled tile layer \"ground\" reaches farther than 16777216 cells from the origin.");
    map["layers"][0]["chunks"][1]["x"] = -9223372036854775807LL;
    EXPECT_EQ(failure(map), "The chunk at -9223372036854775807, 0 of the Tiled tile layer \"ground\" reaches farther than 16777216 cells from the origin.");
    map["layers"][0]["chunks"][1]["x"] = 18446744073709551615ULL;
    EXPECT_EQ(failure(map), "The chunk at 18446744073709551615, 0 of the Tiled tile layer \"ground\" reaches farther than 16777216 cells from the origin.");
    map["layers"][0]["chunks"][1]["x"] = 1e20;
    EXPECT_EQ(failure(map), "A chunk of the Tiled tile layer \"ground\" has an \"x\" that is not an integer.");
    map["layers"][0]["chunks"][1]["x"] = 16777214;
    EXPECT_EQ(failure(map), "no error");
    map = test::TiledMaps::orthogonalMap();
    map["height"] = -1;
    EXPECT_EQ(failure(map), "The Tiled map \"maps/broken.tmj\" has a negative size.");
    map = test::TiledMaps::orthogonalMap();
    map["tilesets"][1]["wangsets"] = haylen::core::Json::parse(R"([{"name": "cliff", "type": "corner", "colors": [{"name": "rock"}], "wangtiles": [{"tileid": 1, "wangid": [0, 1, 0, -1, 0, 1, 0, 1]}]}])");
    EXPECT_EQ(failure(map), "The Wang set \"cliff\" gives the tile 1 the color -1, which is not a whole number from 0 to 255.");
    map["tilesets"][1]["wangsets"][0]["wangtiles"][0]["wangid"][3] = 300;
    EXPECT_EQ(failure(map), "The Wang set \"cliff\" gives the tile 1 the color 300, which is not a whole number from 0 to 255.");

    map = test::TiledMaps::orthogonalMap();
    map["tilewidth"] = 0;
    EXPECT_EQ(failure(map), "The Tiled map \"maps/broken.tmj\" needs a positive tile size.");

    // Group layers and lists of properties nest only so deep, so a hostile file never runs out of stack in the code that walks them.
    haylen::core::Json group{{"id", 100}, {"name", "deepest"}, {"type", "group"}};
    for (int level = 1; level < Map::kMaxNesting; ++level) {
        group = haylen::core::Json{{"id", 100 + level}, {"name", "level"}, {"type", "group"}, {"layers", haylen::core::Json::array({group})}};
    }

    map = test::TiledMaps::orthogonalMap();
    map["layers"].push_back(group);
    EXPECT_EQ(failure(map), "no error");
    map["layers"].back() = haylen::core::Json{{"id", 200}, {"name", "outer"}, {"type", "group"}, {"layers", haylen::core::Json::array({group})}};
    EXPECT_EQ(failure(map), "The Tiled map \"maps/broken.tmj\" nests its group layers deeper than 64 levels.");

    haylen::core::Json items = haylen::core::Json::array();
    for (int level = 1; level < Map::kMaxNesting; ++level) {
        items = haylen::core::Json::array({haylen::core::Json{{"type", "list"}, {"value", items}}});
    }

    map = test::TiledMaps::orthogonalMap();
    map["properties"].push_back(haylen::core::Json{{"name", "nested"}, {"type", "list"}, {"value", items}});
    EXPECT_EQ(failure(map), "no error");
    map["properties"].back()["value"] = haylen::core::Json::array({haylen::core::Json{{"type", "list"}, {"value", items}}});
    EXPECT_EQ(failure(map), "The list properties of a Tiled map nest deeper than 64 levels.");

    // Every layer may hold the most cells a layer holds, but the map refuses more cells in all before it decompresses them.
    const std::string zeros = test::TiledMaps::encodeTiles(std::vector<std::uint32_t>(static_cast<std::size_t>(Map::kMaxCells), 0), "zstd");
    map = test::TiledMaps::orthogonalMap();
    for (int index = 0; index < 4; ++index) {
        map["layers"].push_back(haylen::core::Json{{"id", 300 + index}, {"name", "large"}, {"type", "tilelayer"}, {"width", 4096}, {"height", 4096}, {"encoding", "base64"}, {"compression", "zstd"}, {"data", zeros}});
    }

    EXPECT_EQ(failure(map), "The tile layers of the Tiled map \"maps/broken.tmj\" hold more than the 67108864 cells that a map holds in all.");

    Layer layer{.width = 2, .height = 2, .gids = std::vector<std::uint32_t>(4, 0)};
    EXPECT_THROW(layer.setGid(2, 0, 1), std::out_of_range);
    Layer chunked{.chunks = {{.x = 0, .y = 0, .width = 1, .height = 1, .gids = {0}}}, .infinite = true};
    chunked.setGid(0, 0, 3);
    EXPECT_EQ(chunked.getGid(0, 0), 3U);
    EXPECT_THROW(chunked.setGid(5, 5, 1), std::out_of_range);
    EXPECT_THROW((void)Tileset{}.getSource(0), std::out_of_range);
}

TEST(MapTest, FindsTheTilesOfImageCollectionsPastTheirTileCount) {
    // Tiled keeps the ids of the tiles removed from a collection unused, so the rock keeps the id 3 in a collection of two tiles.
    haylen::core::Json document = test::TiledMaps::orthogonalMap();
    document["tilesets"][1]["tiles"][1]["id"] = 3;
    MapRenderer map(Map::parse(document, "maps/island.tmj", test::TiledMaps::reader()));

    EXPECT_EQ(test::TiledHelper::tileset(map.getMap(), 103).firstGid, 100U);
    EXPECT_EQ(map.getMap().findTileset(101), nullptr);
    EXPECT_EQ(map.getMap().findTileset(104), nullptr);
    map.setTile("inner", 0, 0, 103);
    EXPECT_EQ(map.getTile("inner", 0, 0), 103U);
    EXPECT_THROW(map.setTile("inner", 0, 0, 101), std::invalid_argument);
}

TEST(MapTest, ConvertsCellsForEveryOrientation) {
    Map map;
    map.width = 10;
    map.height = 10;
    map.tileSize = {64.0F, 32.0F};

    map.orientation = Map::Orientation::Orthogonal;
    EXPECT_EQ(map.cellToWorld(2, 3), haylen::math::Vec2(128.0F, 96.0F));
    EXPECT_EQ(map.worldToCell({130.0F, 100.0F}), (std::array<int, 2>{2, 3}));
    EXPECT_EQ(map.objectToWorld({5.0F, 6.0F}), haylen::math::Vec2(5.0F, 6.0F));

    map.orientation = Map::Orientation::Isometric;
    EXPECT_EQ(map.cellToWorld(0, 0), haylen::math::Vec2(320.0F, 0.0F));
    EXPECT_EQ(map.cellToWorld(1, 0), haylen::math::Vec2(352.0F, 16.0F));
    EXPECT_EQ(map.worldToCell(map.cellToWorld(3, 4) + haylen::math::Vec2{0.0F, 16.0F}), (std::array<int, 2>{3, 4}));
    EXPECT_EQ(map.objectToWorld({32.0F, 0.0F}), haylen::math::Vec2(352.0F, 16.0F));
    EXPECT_EQ(map.getPixelBounds(), (haylen::math::Rect{0.0F, 0.0F, 640.0F, 320.0F}));

    for (const bool staggerX : {false, true}) {
        for (const bool even : {false, true}) {
            for (const Map::Orientation orientation : {Map::Orientation::Staggered, Map::Orientation::Hexagonal}) {
                map.orientation = orientation;
                map.staggerX = staggerX;
                map.staggerEven = even;
                map.hexSideLength = 16;
                for (const auto& [column, row] : std::vector<std::pair<int, int>>{{0, 0}, {3, 4}, {4, 3}, {-1, 2}}) {
                    const haylen::math::Vec2 center = map.cellToWorld(column, row) + map.tileSize * 0.5F;
                    EXPECT_EQ(map.worldToCell(center), (std::array<int, 2>{column, row})) << column << "," << row;
                }

                EXPECT_GT(map.getPixelBounds().width, 0.0F);
            }
        }
    }

    EXPECT_EQ(Map::orientationName(Map::Orientation::Hexagonal), "hexagonal");
    EXPECT_EQ(Map::renderOrderName(Map::RenderOrder::RightDown), "right-down");
    EXPECT_EQ(Map::renderOrderName(Map::RenderOrder::RightUp), "right-up");
    EXPECT_EQ(Map::renderOrderName(Map::RenderOrder::LeftDown), "left-down");
    EXPECT_EQ(Map::renderOrderName(Map::RenderOrder::LeftUp), "left-up");
    EXPECT_EQ(Layer::kindName(Layer::Kind::Group), "group");
    EXPECT_EQ(Object::shapeName(Object::Shape::Polyline), "polyline");
}

TEST(MapTest, ReadsObliqueMapsBlendModesAndCapsules) {
    Map map = Map::parse(test::TiledMaps::obliqueMap(), "maps/oblique.tmj", test::TiledMaps::reader());
    EXPECT_EQ(map.orientation, Map::Orientation::Oblique);
    EXPECT_EQ(Map::orientationName(map.orientation), "oblique");
    EXPECT_EQ(map.skew, haylen::math::Vec2(8.0F, 4.0F));
    EXPECT_EQ(test::TiledHelper::layer(map, "ground").blend, haylen::graphics::BlendMode::Type::Multiply);
    EXPECT_EQ(test::TiledHelper::layer(map, "decor").blend, haylen::graphics::BlendMode::Type::Additive);
    EXPECT_EQ(test::TiledHelper::layer(map, "things").blend, haylen::graphics::BlendMode::Type::Screen);
    EXPECT_EQ(test::TiledHelper::layer(map, "inner").blend, haylen::graphics::BlendMode::Type::Alpha);

    const std::vector<Object>& objects = test::TiledHelper::layer(map, "things").objects;
    EXPECT_EQ(objects[6].opacity, 0.25F);
    EXPECT_EQ(objects[0].opacity, 1.0F);
    EXPECT_EQ(objects[9].shape, Object::Shape::Capsule);
    EXPECT_EQ(Object::shapeName(Object::Shape::Capsule), "capsule");

    // Rows slide right by the horizontal skew and columns slide down by the vertical skew.
    EXPECT_EQ(map.cellToWorld(2, 3), haylen::math::Vec2(56.0F, 56.0F));
    EXPECT_EQ(map.objectToWorld({16.0F, 32.0F}), haylen::math::Vec2(32.0F, 36.0F));
    EXPECT_EQ(map.worldToCell(map.objectToWorld({40.0F, 56.0F})), (std::array<int, 2>{2, 3}));
    EXPECT_EQ(map.worldToCell(map.objectToWorld({-4.0F, 2.0F})), (std::array<int, 2>{-1, 0}));
    EXPECT_EQ(map.getPixelBounds(), (haylen::math::Rect{0.0F, 0.0F, 88.0F, 64.0F}));
    map.skew = {-8.0F, 4.0F};
    EXPECT_EQ(map.getPixelBounds(), (haylen::math::Rect{-24.0F, 0.0F, 88.0F, 64.0F}));
    EXPECT_EQ(map.worldToCell(map.cellToWorld(3, 1) + haylen::math::Vec2{1.0F, 1.0F}), (std::array<int, 2>{3, 1}));

    haylen::core::Json folded = test::TiledMaps::obliqueMap();
    folded["skewx"] = 16;
    folded["skewy"] = 16;
    EXPECT_THROW((void)Map::parse(folded, "maps/folded.tmj", test::TiledMaps::reader()), std::invalid_argument);

    // The modes that mix with what lies below read as they are, and a mode Tiled does not offer stops the map.
    haylen::core::Json blended = test::TiledMaps::orthogonalMap();
    for (const auto& [mode, type] : {std::pair{"overlay", haylen::graphics::BlendMode::Type::Overlay}, std::pair{"color-dodge", haylen::graphics::BlendMode::Type::ColorDodge}, std::pair{"soft-light", haylen::graphics::BlendMode::Type::SoftLight}, std::pair{"exclusion", haylen::graphics::BlendMode::Type::Exclusion}}) {
        blended["layers"][6]["layers"][0]["mode"] = mode;
        EXPECT_EQ(test::TiledHelper::layer(Map::parse(blended, "maps/blended.tmj", test::TiledMaps::reader()), "inner").blend, type) << mode;
    }

    blended["layers"][6]["layers"][0]["mode"] = "glow";

    try {
        (void)Map::parse(blended, "maps/blended.tmj", test::TiledMaps::reader());
        FAIL() << "The glow blend mode was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The Tiled layer \"inner\" uses the blend mode \"glow\", which Tiled does not offer. Layers can use \"normal\", \"add\", \"multiply\", \"screen\", \"overlay\", \"darken\", \"lighten\", \"color-dodge\", \"color-burn\", \"hard-light\", \"soft-light\", \"difference\" or \"exclusion\".");
    }

    // Tiled draws the layers of a group with their own modes, so a group mode would silently change nothing.
    haylen::core::Json blendedGroup = test::TiledMaps::orthogonalMap();
    blendedGroup["layers"][6]["mode"] = "add";

    try {
        (void)Map::parse(blendedGroup, "maps/group.tmj", test::TiledMaps::reader());
        FAIL() << "The group blend mode was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The Tiled group layer \"group\" uses the blend mode \"add\", which Tiled does not apply to the layers inside it. Set the blend mode on those layers.");
    }

    blendedGroup["layers"][6]["mode"] = "normal";
    EXPECT_EQ(test::TiledHelper::layer(Map::parse(blendedGroup, "maps/group.tmj", test::TiledMaps::reader()), "group").blend, haylen::graphics::BlendMode::Type::Alpha);
}

TEST(MapTest, ReadsInfiniteMapsAndWorlds) {
    const Map infinite = Map::parse(test::TiledMaps::infiniteMap(), "maps/infinite.tmj", test::TiledMaps::reader());
    EXPECT_TRUE(infinite.infinite);
    EXPECT_EQ(test::TiledHelper::layer(infinite, "ground").getGid(-1, 1), 4U);
    EXPECT_EQ(test::TiledHelper::layer(infinite, "ground").getGid(1, 0), 6U);
    EXPECT_EQ(test::TiledHelper::layer(infinite, "ground").getGid(5, 5), 0U);

    const std::vector<std::string> files{"maps/world/level_0_0.tmj", "maps/world/level_1_0.tmj", "maps/world/notes.txt", "maps/other/level_9_9.tmj"};
    const World world = World::parse(haylen::core::Json::parse(test::TiledMaps::packageFiles().at("content/maps/world/level.world")), "maps/world/level.world", files);
    ASSERT_EQ(world.maps.size(), 3U);
    EXPECT_EQ(world.maps[0].path, "maps/island.tmj");
    EXPECT_EQ(world.maps[0].bounds, (haylen::math::Rect{-64.0F, 0.0F, 64.0F, 48.0F}));
    EXPECT_EQ(world.maps[2].path, "maps/world/level_1_0.tmj");
    EXPECT_EQ(world.maps[2].bounds, (haylen::math::Rect{64.0F, 0.0F, 64.0F, 48.0F}));

    const haylen::core::Json named = haylen::core::Json::parse(R"({"patterns": [{"regexp": "level_(\\w+)_(\\d+)\\.tmj"}]})");
    try {
        (void)World::parse(named, "maps/world/level.world", {"maps/world/level_east_0.tmj"});
        FAIL() << "A capture that is not a number was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), R"(The pattern "level_(\w+)_(\d+)\.tmj" of a Tiled world captures "east", which is not a number.)");
    }
}

TEST(MapRendererTest, LoadsDrawsAndAnimatesMaps) {
    test::EngineFixture fixture(test::TiledMaps::packageFiles());
    haylen::assets::Manager& assets = fixture.engine().getAssets();
    const auto data = std::static_pointer_cast<Map>(assets.load("tiled", "maps/island.tmj"));
    EXPECT_EQ(data->tilesets[0].tileset->texture, assets.texture("maps/images/terrain.png"));
    EXPECT_TRUE(test::TiledHelper::layer(*data, "sky").texture.isValid());
    EXPECT_NE(test::TiledHelper::layer(*data, "sky").texture, assets.texture("maps/backgrounds/sky.png")) << "Keyed images stay private.";

    MapRenderer map(*data, fixture.engine().getDefaultFont());
    // clang-format off
    const auto render = [&](const MapRenderer::View& view) {
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
            engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
            map.draw(engine.getRenderer2D(), view);
        }));
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats();
    };
    // clang-format on

    // Eleven ground tiles, two decor tiles, one nested tile, three tile objects, eight sky copies, and two glyphs of text with the lines under and through them.
    const MapRenderer::View view{.center = {32.0F, 24.0F}, .visible = {0.0F, 0.0F, 64.0F, 48.0F}};
    const haylen::graphics2d::Renderer::Stats all = render(view);
    EXPECT_EQ(all.sprites, 29U);

    map.setLayerVisible("ground", false);
    EXPECT_EQ(render(view).sprites, 18U);
    map.setLayerVisible("ground", true);

    EXPECT_EQ(map.getTile("ground", 0, 1), 3U);
    map.setTile("ground", 0, 1, 1);
    EXPECT_EQ(map.getTile("ground", 0, 1), 1U);
    map.setTile("ground", 0, 1, 0);
    map.update(0.15F);
    const haylen::graphics2d::Renderer::Stats culled = render({.center = {1000.0F, 1000.0F}, .visible = {900.0F, 900.0F, 10.0F, 10.0F}});
    EXPECT_LT(culled.sprites, all.sprites);

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
        map.drawLayer(engine.getRenderer2D(), "inner", {});
        map.drawLayer(engine.getRenderer2D(), "things", {});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);

    EXPECT_THROW(map.setTile("things", 0, 0, 1), std::invalid_argument);
    EXPECT_THROW(map.setTile("ground", 0, 0, 5000), std::invalid_argument);
    EXPECT_THROW((void)map.getTile("missing", 0, 0), std::invalid_argument);
    EXPECT_THROW(map.setLayerVisible("missing", true), std::invalid_argument);
    EXPECT_THROW(map.drawLayer(fixture.engine().getRenderer2D(), "missing", {}), std::invalid_argument);
    EXPECT_THROW((void)assets.load("tiled", "maps/island.tmj", {{"scale", 2}}), std::invalid_argument);
}

TEST(MapRendererTest, DrawsTheSameMapAgainWithoutAllocating) {
    test::EngineFixture fixture(test::TiledMaps::packageFiles());
    MapRenderer map(*std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());
    const MapRenderer::View view{.center = {32.0F, 24.0F}, .visible = {0.0F, 0.0F, 64.0F, 48.0F}};
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
        map.draw(engine.getRenderer2D(), view);
    }));
    // clang-format on
    fixture.frames(3);

    // The sky repeats across the view, and its copies reuse the storage of the frame before.
    const test::AllocationTracker tracker;
    fixture.frames(5);
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(MapRendererTest, BakesTheRegionsOfChangedCellsAgain) {
    // The ground is 40 cells wide, which makes two regions across, with one tile on the left and none on the right.
    haylen::core::Json document = test::TiledMaps::orthogonalMap();
    document["width"] = 40;
    std::vector<std::uint32_t> ground(120, 0);
    ground[0] = 5;
    document["layers"] = haylen::core::Json::array({{{"id", 1}, {"name", "ground"}, {"type", "tilelayer"}, {"width", 40}, {"height", 3}, {"data", ground}}});
    std::map<std::string, std::string> files = test::TiledMaps::packageFiles();
    files["content/maps/island.tmj"] = document.dump();
    test::EngineFixture fixture(files);
    MapRenderer map(*std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")));
    // clang-format off
    const auto sprites = [&](const MapRenderer::View& view, bool ysort) {
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
            engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
            map.drawLayer(engine.getRenderer2D(), "ground", view, {.ysort = ysort});
        }));
        fixture.frames(1);
        EXPECT_EQ(fixture.engine().getError(), nullptr);
        return fixture.engine().getRenderer2D().getStats().sprites;
    };
    // clang-format on

    // A tile in an empty region makes a region that culling finds, and emptying it again removes the region, in both kinds of cache.
    const MapRenderer::View right{.center = {570.0F, 24.0F}, .visible = {520.0F, 0.0F, 100.0F, 48.0F}};
    for (const bool ysort : {false, true}) {
        EXPECT_EQ(sprites({}, ysort), 1U);
        map.setTile("ground", 35, 2, 5);
        EXPECT_EQ(sprites({}, ysort), 2U);
        EXPECT_EQ(sprites(right, ysort), 1U);
        map.setTile("ground", 1, 0, 5);
        EXPECT_EQ(sprites({}, ysort), 3U);
        map.setTile("ground", 35, 2, 0);
        EXPECT_EQ(sprites(right, ysort), 0U);
        map.setTile("ground", 1, 0, 0);
        EXPECT_EQ(sprites({}, ysort), 1U);
    }
}

TEST(MapRendererTest, PlacesMapsAtAWorldOffset) {
    test::EngineFixture fixture(test::TiledMaps::packageFiles());
    MapRenderer map(*std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());
    // clang-format off
    const auto render = [&](const MapRenderer::View& view, haylen::math::Vec2 offset) {
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
            engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
            map.draw(engine.getRenderer2D(), view, {.offset = offset});
        }));
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats().sprites;
    };
    // clang-format on

    // A map moved by an offset draws the same way when the view moves with it, parallax included, and its tiles leave the view it left.
    const MapRenderer::View home{.center = {32.0F, 24.0F}, .visible = {0.0F, 0.0F, 64.0F, 48.0F}};
    const MapRenderer::View away{.center = {1032.0F, 24.0F}, .visible = {1000.0F, 0.0F, 64.0F, 48.0F}};
    const std::size_t all = render(home, {});
    EXPECT_EQ(all, 29U);
    EXPECT_EQ(render(away, {1000.0F, 0.0F}), all);
    EXPECT_LT(render(home, {1000.0F, 0.0F}), 16U);
}

TEST(MapRendererTest, SortsRowsAndObjectsByTheYTheyStandOn) {
    test::EngineFixture fixture(test::TiledMaps::packageFiles());
    MapRenderer map(*std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());
    const haylen::graphics::Texture unit = fixture.engine().getGraphics().createTexture(haylen::graphics::Image(4, 4, haylen::math::Color::white()));

    // clang-format off
    const auto switches = [&](haylen::graphics2d::Renderer::SortMode mode, bool ysort, float standing) {
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&, mode, ysort, standing](haylen::core::Engine& engine) {
            haylen::graphics2d::Renderer& renderer = engine.getRenderer2D();
            renderer.beginWorld(haylen::graphics2d::Camera{}, {.sort = mode});
            map.drawLayer(renderer, "ground", {}, {.ysort = ysort});
            renderer.draw({.texture = unit, .position = {8.0F, standing}, .order = {.depth = standing}});
        }));
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats().textureSwitches;
    };
    // clang-format on

    // The ground rows stand on 16, 32 and 48, so a unit between two rows splits the terrain draws, and one below the map comes last.
    EXPECT_EQ(switches(haylen::graphics2d::Renderer::SortMode::Depth, true, 24.0F), 3U);
    EXPECT_EQ(switches(haylen::graphics2d::Renderer::SortMode::Depth, true, 100.0F), 2U);
    EXPECT_EQ(switches(haylen::graphics2d::Renderer::SortMode::Depth, false, 24.0F), 2U);
    EXPECT_EQ(switches(haylen::graphics2d::Renderer::SortMode::Y, true, 40.0F), 3U);
    EXPECT_EQ(switches(haylen::graphics2d::Renderer::SortMode::Y, true, 100.0F), 2U);

    // Tile objects and text stand on their bottom edge.
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{}, {.sort = haylen::graphics2d::Renderer::SortMode::Y});
        map.draw(engine.getRenderer2D(), {.center = {32.0F, 24.0F}, .visible = {0.0F, 0.0F, 64.0F, 48.0F}}, {.ysort = true});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 29U);
}

TEST(MapRendererTest, DrawsObliqueMapsWithLayerBlendModes) {
    std::map<std::string, std::string> files = test::TiledMaps::packageFiles();
    files["content/maps/island.tmj"] = test::TiledMaps::obliqueMap().dump();
    test::EngineFixture fixture(files);
    MapRenderer map(*std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
        map.draw(engine.getRenderer2D(), {});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    // Eleven ground tiles, two decor tiles, one nested tile, three tile objects and two glyphs with their two lines, with the sky hidden.
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 21U);
}

TEST(MapRendererTest, DrawsALayerThatMixesWithWhatLiesBelowAsAGroup) {
    std::map<std::string, std::string> files = test::TiledMaps::packageFiles();
    haylen::core::Json oblique = test::TiledMaps::obliqueMap();
    for (haylen::core::Json& layer : oblique["layers"]) {
        if (layer.value("name", std::string{}) == "decor") {
            layer["mode"] = "overlay";
        }
    }

    files["content/maps/island.tmj"] = oblique.dump();
    test::EngineFixture fixture(files);
    MapRenderer map(*std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});
        map.draw(engine.getRenderer2D(), {});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    const haylen::graphics2d::Renderer::Stats stats = fixture.engine().getRenderer2D().getStats();
    EXPECT_EQ(stats.sprites, 21U);

    // The decor renders into a group, the map draws offscreen up to it, copies what it drew below, goes on and reaches the screen.
    EXPECT_EQ(stats.passes, 5U);
    const haylen::core::Json written = MapWriter::write(map.getMap(), "maps/island.tmj");
    for (const haylen::core::Json& layer : written["layers"]) {
        if (layer.value("name", std::string{}) == "decor") {
            EXPECT_EQ(layer.value("mode", std::string{}), "overlay");
        }
    }
}

TEST(MapRendererTest, RejectsDrawingWhatItCannotPlace) {
    test::EngineFixture fixture(test::TiledMaps::packageFiles());
    const auto data = std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj"));

    MapRenderer textless(*data);
    std::string error;
    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&](haylen::core::Engine& engine) {
        engine.getRenderer2D().beginWorld(haylen::graphics2d::Camera{});

        try {
            textless.drawLayer(engine.getRenderer2D(), "things", {});
        } catch (const std::logic_error& failure) {
            error = failure.what();
        }

        try {
            textless.drawLayer(engine.getRenderer2D(), "sky", {});
        } catch (const std::invalid_argument& failure) {
            error += failure.what();
        }
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_NE(error.find("needs a font"), std::string::npos);
    EXPECT_NE(error.find("visible area"), std::string::npos);
}

TEST(MapRendererTest, BuildsCollisionBodies) {
    test::EngineFixture fixture(test::TiledMaps::packageFiles());
    const auto data = std::static_pointer_cast<Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj"));
    MapRenderer map(*data);
    haylen::physics2d::World world({.gravity = {}});

    // Each tile layer that collides keeps a body, empty or not, and the object layer adds one more.
    const std::vector<haylen::physics2d::Body> bodies = map.buildCollision(world);
    ASSERT_EQ(bodies.size(), 5U);
    EXPECT_TRUE(bodies[1].getShapes().empty());

    // Solid cells that touch merge into one chain loop, which gives two single cells on the first row and a single cell and a pair on the last row.
    const std::vector<haylen::physics2d::Shape::Outline> outlines = bodies[0].getOutlines();
    ASSERT_EQ(outlines.size(), 4U);

    for (const haylen::physics2d::Shape::Outline& outline : outlines) {
        EXPECT_TRUE(outline.closed);
        EXPECT_EQ(outline.points.size(), 4U);
        EXPECT_GT(haylen::math::Geometry2D::signedArea(outline.points), 0.0F);
    }

    EXPECT_NEAR(test::Physics2DHelper::requireHit(world.raycast({24.0F, -10.0F}, {24.0F, 30.0F})).point.y, 0.0F, 0.01F);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(world.raycast({70.0F, 40.0F}, {30.0F, 40.0F})).point.x, 64.0F, 0.01F);
    EXPECT_FALSE(world.raycast({20.0F, 40.0F}, {28.0F, 40.0F}).has_value());

    const std::vector<haylen::physics2d::Shape> zone = world.queryPoint({-4.0F, 4.0F});
    ASSERT_EQ(zone.size(), 1U);
    EXPECT_TRUE(zone.front().isSensor());
    EXPECT_GE(bodies[3].getShapes().size(), 5U);
}

TEST(MapRendererTest, TracesTileCollisionAgainAfterSetTile) {
    // A tileset of a slope and a one-way ledge joins the terrain tiles.
    haylen::core::Json document = test::TiledMaps::orthogonalMap();
    // clang-format off
    document["tilesets"].push_back(haylen::core::Json::parse(R"({"firstgid": 200, "name": "shapes", "tilewidth": 16, "tileheight": 16, "tilecount": 2, "columns": 0, "tiles": [
        {"id": 0, "image": "props/rock.png", "imagewidth": 16, "imageheight": 16, "objectgroup": {"objects": [{"id": 1, "x": 0, "y": 0, "polygon": [{"x": 0, "y": 16}, {"x": 16, "y": 0}, {"x": 16, "y": 16}]}]}},
        {"id": 1, "image": "props/rock.png", "imagewidth": 16, "imageheight": 16, "objectgroup": {"objects": [{"id": 1, "x": 0, "y": 0, "width": 16, "height": 4, "properties": [{"name": "oneWay", "type": "bool", "value": true}]}]}}
    ]})"));
    // clang-format on
    MapRenderer map(Map::parse(document, "maps/island.tmj", test::TiledMaps::reader()));
    haylen::physics2d::World world({.gravity = {}});
    const haylen::physics2d::Body ground = map.buildCollision(world).front();
    ASSERT_EQ(ground.getOutlines().size(), 4U);

    // Filling the gap of the last row joins its cells into one loop, and clearing a cell of the first row takes its loop away.
    map.setTile("ground", 1, 2, 5);
    EXPECT_EQ(ground.getOutlines().size(), 3U);
    map.setTile("ground", 1, 0, 0);
    EXPECT_EQ(ground.getOutlines().size(), 2U);
    EXPECT_FALSE(world.raycast({24.0F, -10.0F}, {24.0F, 15.0F}).has_value());

    // A slope that stands on the last row merges into its loop, and a one-way ledge stays a shape of its own.
    map.setTile("ground", 1, 1, 200);
    EXPECT_EQ(ground.getOutlines().size(), 2U);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(world.raycast({31.0F, 2.0F}, {31.0F, 40.0F})).point.y, 17.0F, 0.01F);
    map.setTile("ground", 0, 0, 201);
    const std::vector<haylen::physics2d::Shape> ledge = world.queryPoint({8.0F, 2.0F});
    ASSERT_EQ(ledge.size(), 1U);
    EXPECT_NEAR(ledge.front().getOneWay().value_or(haylen::math::Vec2{}).y, -1.0F, 1e-4F);

    // Once the world is gone the map still changes its tiles.
    std::optional<haylen::physics2d::World> gone(std::in_place);
    (void)map.buildCollision(*gone);
    gone.reset();
    map.setTile("ground", 0, 0, 0);
    EXPECT_EQ(map.getTile("ground", 0, 0), 0U);
}

TEST(MapRendererTest, BuildsObliqueAndCapsuleCollision) {
    MapRenderer map(Map::parse(test::TiledMaps::obliqueMap(), "maps/oblique.tmj", test::TiledMaps::reader()));
    haylen::physics2d::World world({.gravity = {}});
    ASSERT_EQ(map.buildCollision(world).size(), 5U);

    // The solid tile in the second column of the first row hangs from the skewed corner of the cell below it.
    EXPECT_FALSE(world.queryPoint({26.0F, 18.0F}).empty());
    EXPECT_TRUE(world.queryPoint({18.0F, 2.0F}).empty());

    // The capsule keeps its round caps under the shear.
    EXPECT_FALSE(world.queryPoint(map.getMap().objectToWorld({120.0F, 5.0F})).empty());
    EXPECT_FALSE(world.queryPoint(map.getMap().objectToWorld({101.0F, 5.0F})).empty());
    EXPECT_TRUE(world.queryPoint(map.getMap().objectToWorld({100.5F, 0.5F})).empty());

    haylen::core::Json upright = test::TiledMaps::orthogonalMap();
    upright["layers"][4]["objects"] = haylen::core::Json::parse(R"([{"id": 1, "type": "collision", "x": 100, "y": 0, "width": 10, "height": 40, "capsule": true}, {"id": 2, "type": "collision", "x": 200, "y": 0, "width": 12, "height": 12, "capsule": true}])");
    MapRenderer straight(Map::parse(upright, "maps/upright.tmj", test::TiledMaps::reader()));
    haylen::physics2d::World other({.gravity = {}});
    (void)straight.buildCollision(other);
    EXPECT_FALSE(other.queryPoint({105.0F, 1.0F}).empty());
    EXPECT_FALSE(other.queryPoint({105.0F, 39.0F}).empty());
    EXPECT_TRUE(other.queryPoint({100.5F, 0.5F}).empty());
    EXPECT_FALSE(other.queryPoint({206.0F, 6.0F}).empty());
    EXPECT_TRUE(other.queryPoint({200.5F, 0.5F}).empty());
}

TEST(MapRendererTest, PlacesTileObjectCollisionLikeTheirImagesAndReadsLayerFilters) {
    // The tree tileset aligns its objects by their bottom center, so the tree at 16, 48 covers 8 to 24 across and 16 to 48 down.
    haylen::core::Json document = test::TiledMaps::orthogonalMap();
    document["layers"][4]["objects"] = haylen::core::Json::parse(R"([{"id": 1, "type": "collision", "x": 16, "y": 48, "width": 16, "height": 32, "gid": 100}])");
    document["layers"][4]["properties"] = haylen::core::Json::parse(R"([{"name": "category", "type": "int", "value": 4}, {"name": "mask", "type": "int", "value": 6}])");
    MapRenderer map(Map::parse(document, "maps/island.tmj", test::TiledMaps::reader()));
    haylen::physics2d::World world({.gravity = {}});
    const std::vector<haylen::physics2d::Body> bodies = map.buildCollision(world);
    ASSERT_EQ(bodies.size(), 5U);

    const haylen::physics2d::CollisionFilter probe{.category = 2};
    const std::vector<haylen::physics2d::Shape> tree = world.queryPoint({10.0F, 24.0F}, probe);
    ASSERT_EQ(tree.size(), 1U);
    EXPECT_EQ(tree.front().getFilter().category, 4U);
    EXPECT_EQ(tree.front().getFilter().mask, 6U);
    EXPECT_TRUE(world.queryPoint({28.0F, 24.0F}, probe).empty());
    EXPECT_EQ(bodies[0].getShapes().front().getFilter().category, 1U);

    // Collision bits are integers of at least 0, which a negative mask is not.
    document["layers"][4]["properties"][1]["value"] = -1;
    MapRenderer negative(Map::parse(document, "maps/island.tmj", test::TiledMaps::reader()));
    haylen::physics2d::World other({.gravity = {}});
    try {
        (void)negative.buildCollision(other);
        FAIL() << "The negative mask was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The collision property \"mask\" of the Tiled layer \"things\" needs an integer of at least 0.");
    }
}

TEST(MapRendererTest, ScalesTheOffsetsAndCollisionShapesOfGridSizedTiles) {
    // Tiles painted at 32 pixels draw on a grid of 16, so their offset of 8 pixels moves them by 4 and their collision shapes shrink with them, as Tiled draws them.
    // clang-format off
    const haylen::core::Json document = haylen::core::Json::parse(R"({
        "type": "map", "orientation": "orthogonal", "renderorder": "right-down", "width": 3, "height": 2, "tilewidth": 16, "tileheight": 16, "infinite": false,
        "tilesets": [{"firstgid": 1, "name": "painted", "tilewidth": 32, "tileheight": 32, "tilecount": 2, "columns": 2, "image": "images/painted.png", "imagewidth": 64, "imageheight": 32,
            "tilerendersize": "grid", "tileoffset": {"x": 8, "y": 0}, "tiles": [
                {"id": 0, "objectgroup": {"objects": [{"id": 1, "x": 0, "y": 0, "width": 32, "height": 32}]}},
                {"id": 1, "objectgroup": {"objects": [{"id": 1, "x": 0, "y": 16, "width": 32, "height": 16}]}}
            ]}],
        "layers": [{"id": 1, "name": "ground", "type": "tilelayer", "width": 3, "height": 2, "data": [1, 1, 0, 0, 0, 2]}]
    })");
    // clang-format on
    MapRenderer map(Map::parse(document, "maps/painted.tmj", test::TiledMaps::reader()));
    haylen::physics2d::World world({.gravity = {}});
    const haylen::physics2d::Body ground = map.buildCollision(world).front();

    // The two full tiles of the first row fill their cells and merge into one loop, and the half tile covers the lower half of its cell.
    EXPECT_EQ(ground.getOutlines().size(), 2U);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(world.raycast({-10.0F, 8.0F}, {50.0F, 8.0F})).point.x, 4.0F, 0.01F);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(world.raycast({50.0F, 8.0F}, {-10.0F, 8.0F})).point.x, 36.0F, 0.01F);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(world.raycast({44.0F, 17.0F}, {44.0F, 40.0F})).point.y, 24.0F, 0.01F);
    EXPECT_NEAR(test::Physics2DHelper::requireHit(world.raycast({60.0F, 28.0F}, {30.0F, 28.0F})).point.x, 52.0F, 0.01F);
}

TEST(MapRendererTest, SpawnsObjectsThroughFactories) {
    haylen::core::Json document = test::TiledMaps::orthogonalMap();
    document["layers"][6]["layers"].push_back(haylen::core::Json::parse(R"({"id": 9, "name": "camp", "type": "objectgroup", "offsetx": 10, "offsety": 20, "objects": [{"id": 20, "type": "player", "x": 1, "y": 2}]})"));
    const MapRenderer map(Map::parse(document, "maps/island.tmj", test::TiledMaps::reader()));

    std::vector<std::pair<std::string, haylen::math::Vec2>> spawned;
    ObjectFactories factories;
    factories.add("player", [&spawned](const Object& object, haylen::math::Vec2 position) { spawned.emplace_back("player " + object.name, position); });
    factories.add("rock", [&spawned](const Object& object, haylen::math::Vec2 position) { spawned.emplace_back("rock " + object.name, position); });
    EXPECT_TRUE(factories.has("rock"));
    EXPECT_THROW(factories.add("rock", [](const Object&, haylen::math::Vec2) {}), std::invalid_argument);
    EXPECT_THROW(factories.add("", [](const Object&, haylen::math::Vec2) {}), std::invalid_argument);

    // The nested layer adds its own offset to the one of its group.
    EXPECT_EQ(factories.spawn(map), 4U);
    ASSERT_EQ(spawned.size(), 4U);
    EXPECT_EQ(spawned[0], std::make_pair(std::string("player spawn"), haylen::math::Vec2(10.0F, 20.0F)));
    EXPECT_EQ(spawned[1], std::make_pair(std::string("rock rock"), haylen::math::Vec2(40.0F, 48.0F)));
    EXPECT_EQ(spawned[2], std::make_pair(std::string("rock big rock"), haylen::math::Vec2(44.0F, 48.0F)));
    EXPECT_EQ(spawned[3], std::make_pair(std::string("player "), haylen::math::Vec2(15.0F, 24.0F)));

    spawned.clear();
    factories.remove("rock");
    EXPECT_FALSE(factories.has("rock"));
    EXPECT_EQ(factories.spawn(map, "camp"), 1U);
    EXPECT_EQ(spawned.front().second, haylen::math::Vec2(15.0F, 24.0F));
    EXPECT_THROW((void)factories.spawn(map, "ground"), std::invalid_argument);
    EXPECT_THROW((void)factories.spawn(map, "missing"), std::invalid_argument);

    // A factory may remove itself while it runs, which leaves the later objects of its class alone.
    spawned.clear();
    // clang-format off
    factories.add("rock", [&spawned, &factories](const Object& object, haylen::math::Vec2 position) {
        factories.remove("rock");
        spawned.emplace_back("once " + object.name, position);
    });
    // clang-format on
    EXPECT_EQ(factories.spawn(map), 3U);
    ASSERT_EQ(spawned.size(), 3U);
    EXPECT_EQ(spawned[1].first, "once rock");
}

TEST(MapTest, WritesMapsThatReadBackUnchanged) {
    const Map original = Map::parse(test::TiledMaps::orthogonalMap(), "maps/island.tmj", test::TiledMaps::reader());
    const haylen::core::Json written = MapWriter::write(original, "maps/copies/island.tmj");
    EXPECT_EQ(written.at("tilesets")[0].at("image"), "../images/terrain.png");
    EXPECT_FALSE(written.at("tilesets")[0].contains("source"));

    // The copy embeds its tilesets and resolves its templates, so reading it back reads no other file.
    // clang-format off
    const Map copy = Map::parse(written, "maps/copies/island.tmj", [](const std::string& path) -> haylen::core::Json {
        throw std::runtime_error("The file \"" + path + "\" was read unexpectedly.");
    });
    // clang-format on
    EXPECT_EQ(copy.type, original.type);
    EXPECT_EQ(copy.getPixelBounds(), original.getPixelBounds());
    EXPECT_EQ(copy.backgroundColor, original.backgroundColor);
    EXPECT_EQ(copy.properties.getString("title", ""), "Island");
    EXPECT_EQ(test::TiledHelper::property(copy.properties, "spawn").value.at("kind"), "goblin");
    EXPECT_EQ(test::TiledHelper::property(copy.properties, "loot").value[3].at("value")[1].at("value"), "maps/gem.png");

    ASSERT_EQ(copy.tilesets.size(), original.tilesets.size());

    for (std::size_t index = 0; index < copy.tilesets.size(); ++index) {
        const Tileset& from = *original.tilesets[index].tileset;
        const Tileset& to = *copy.tilesets[index].tileset;
        EXPECT_EQ(copy.tilesets[index].firstGid, original.tilesets[index].firstGid);
        EXPECT_EQ(to.image, from.image);
        EXPECT_EQ(to.tileCount, from.tileCount);
        EXPECT_EQ(to.wangSets.size(), from.wangSets.size());
    }

    EXPECT_EQ(test::TiledHelper::tile(*copy.tilesets[1].tileset, 0).image, "maps/props/tree.png");
    EXPECT_EQ(test::TiledHelper::tile(*copy.tilesets[0].tileset, 2).animation.size(), 2U);
    EXPECT_EQ(copy.tilesets[0].tileset->wangSets.front().tiles.front().wangId, original.tilesets[0].tileset->wangSets.front().tiles.front().wangId);

    for (const std::string_view name : {"ground", "decor", "inner"}) {
        EXPECT_EQ(test::TiledHelper::layer(copy, name).gids, test::TiledHelper::layer(original, name).gids) << name;
        EXPECT_EQ(test::TiledHelper::layer(copy, name).id, test::TiledHelper::layer(original, name).id) << name;
    }

    EXPECT_EQ(test::TiledHelper::layer(copy, "group").opacity, 0.5F);
    EXPECT_EQ(test::TiledHelper::layer(copy, "sky").image, "maps/backgrounds/sky.png");
    EXPECT_EQ(test::TiledHelper::layer(copy, "sky").parallax, haylen::math::Vec2(0.5F, 0.5F));

    const Layer& things = test::TiledHelper::layer(copy, "things");
    const Layer& before = test::TiledHelper::layer(original, "things");
    ASSERT_EQ(things.objects.size(), before.objects.size());

    for (std::size_t index = 0; index < things.objects.size(); ++index) {
        EXPECT_EQ(things.objects[index].shape, before.objects[index].shape) << index;
        EXPECT_EQ(things.objects[index].position, before.objects[index].position) << index;
        EXPECT_EQ(things.objects[index].points.size(), before.objects[index].points.size()) << index;
        EXPECT_EQ(things.objects[index].gid, before.objects[index].gid) << index;
    }

    EXPECT_EQ(things.objects[8].name, "big rock");
    EXPECT_TRUE(things.objects[8].properties.getBool("hard", false));

    // The color of an object layer, the grid and the turns of a tileset and the decorations of text read back too.
    EXPECT_EQ(things.color, before.color);
    EXPECT_EQ(copy.tilesets[0].tileset->grid.size, haylen::math::Vec2(32.0F, 16.0F));
    EXPECT_TRUE(copy.tilesets[0].tileset->grid.isometric);
    EXPECT_TRUE(copy.tilesets[0].tileset->transformations.preferUntransformed);
    EXPECT_FALSE(copy.tilesets[0].tileset->transformations.flipVertical);
    EXPECT_EQ(copy.tilesets[1].tileset->grid.size, copy.tilesets[1].tileset->tileSize);
    const auto sign = std::ranges::find(things.objects, std::string("sign"), &Object::name);
    ASSERT_NE(sign, things.objects.end());
    EXPECT_TRUE(sign->text.underline && sign->text.strikeout);
    EXPECT_FALSE(sign->text.kerning);

    const Map infinite = Map::parse(MapWriter::write(Map::parse(test::TiledMaps::infiniteMap(), "maps/infinite.tmj", test::TiledMaps::reader()), "maps/infinite.tmj"), "maps/infinite.tmj", test::TiledMaps::reader());
    EXPECT_TRUE(infinite.infinite);
    EXPECT_EQ(test::TiledHelper::layer(infinite, "ground").getGid(-1, 1), 4U);
    EXPECT_EQ(test::TiledHelper::layer(infinite, "ground").getGid(1, 0), 6U);
}

// A project fills in the members the map leaves at their defaults, in the properties of its class and in class values, nested classes and the class items of lists alike, and keeps what the map sets.
TEST(MapTest, FillsInTheClassMembersOfItsProject) {
    Map map = Map::parse(test::TiledMaps::orthogonalMap(), "maps/island.tmj", test::TiledMaps::reader());
    map.applyPropertyTypes(PropertyTypes::parse(test::TiledMaps::gameProject()));
    EXPECT_EQ(map.properties.getNumber("difficulty", 0.0), 1.0);
    EXPECT_EQ(map.properties.getString("title", ""), "Island");
    const haylen::core::Json& spawn = test::TiledHelper::property(map.properties, "spawn").value;
    EXPECT_EQ(spawn.at("count"), 2);
    EXPECT_EQ(spawn.at("kind"), "goblin");
    EXPECT_EQ(spawn.at("reward").at("coins"), 5);
    EXPECT_EQ(spawn.at("reward").at("gem"), false);
    const haylen::core::Json& bat = test::TiledHelper::property(map.properties, "loot").value[4].at("value");
    EXPECT_EQ(bat.at("kind"), "bat");
    EXPECT_EQ(bat.at("count"), 1);

    const PropertyTypes types = PropertyTypes::parse(test::TiledMaps::gameProject());
    EXPECT_EQ(types.getClasses().size(), 3U);
    EXPECT_EQ(types.findClass("Kind"), nullptr);
    const PropertyTypes looping = PropertyTypes::parse(haylen::core::Json::parse(R"({"propertyTypes": [{"name": "Node", "type": "class", "members": [{"name": "next", "type": "class", "propertyType": "Node", "value": {}}]}]})"));
    EXPECT_THROW((void)looping.complete(Properties({{.name = "head", .type = "class", .propertyType = "Node", .value = haylen::core::Json::object()}}), ""), std::invalid_argument);
    EXPECT_THROW((void)PropertyTypes::parse(haylen::core::Json::parse(R"({"propertyTypes": [{"type": "class", "members": []}]})")), std::invalid_argument);
}

} // namespace haylen::tiled
