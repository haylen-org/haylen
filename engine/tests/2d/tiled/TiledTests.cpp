#include <gtest/gtest.h>

#include <zlib.h>
#include <zstd.h>

#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/2d/graphics/Renderer.hpp"
#include "haylen/2d/physics/World.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/MapRenderer.hpp"
#include "haylen/2d/tiled/ObjectFactories.hpp"
#include "haylen/2d/tiled/World.hpp"
#include "haylen/assets/Manager.hpp"
#include "haylen/core/SceneManager.hpp"
#include "haylen/graphics/Device.hpp"
#include "haylen/graphics/Image.hpp"
#include "haylen/math/Math.hpp"
#include "support/EngineFixture.hpp"

namespace haylen {

namespace {

std::string base64(const std::vector<std::uint8_t>& bytes) {
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string text;
    std::size_t index = 0;
    for (; index + 2 < bytes.size(); index += 3) {
        const std::uint32_t value = (static_cast<std::uint32_t>(bytes[index]) << 16U) | (static_cast<std::uint32_t>(bytes[index + 1]) << 8U) | bytes[index + 2];
        for (const unsigned shift : {18U, 12U, 6U, 0U}) {
            text.push_back(alphabet[(value >> shift) & 63U]);
        }
    }
    if (index < bytes.size()) {
        std::uint32_t value = static_cast<std::uint32_t>(bytes[index]) << 16U;
        if (index + 1 < bytes.size()) {
            value |= static_cast<std::uint32_t>(bytes[index + 1]) << 8U;
        }
        text.push_back(alphabet[(value >> 18U) & 63U]);
        text.push_back(alphabet[(value >> 12U) & 63U]);
        text.push_back(index + 1 < bytes.size() ? alphabet[(value >> 6U) & 63U] : '=');
        text.push_back('=');
    }
    return text;
}

// Encodes gids the way Tiled stores base64 tile data with the given compression.
std::string encodeTiles(const std::vector<std::uint32_t>& gids, const std::string& compression) {
    std::vector<std::uint8_t> raw;
    for (const std::uint32_t gid : gids) {
        for (const unsigned shift : {0U, 8U, 16U, 24U}) {
            raw.push_back(static_cast<std::uint8_t>((gid >> shift) & 0xFFU));
        }
    }
    if (compression == "zstd") {
        std::vector<std::uint8_t> packed(ZSTD_compressBound(raw.size()));
        packed.resize(ZSTD_compress(packed.data(), packed.size(), raw.data(), raw.size(), 1));
        return base64(packed);
    }
    if (compression == "zlib" || compression == "gzip") {
        std::vector<std::uint8_t> packed(raw.size() + 64);
        z_stream stream{};
        deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, compression == "gzip" ? 16 + MAX_WBITS : MAX_WBITS, 8, Z_DEFAULT_STRATEGY);
        stream.next_in = raw.data();
        stream.avail_in = static_cast<uInt>(raw.size());
        stream.next_out = packed.data();
        stream.avail_out = static_cast<uInt>(packed.size());
        deflate(&stream, Z_FINISH);
        packed.resize(stream.total_out);
        deflateEnd(&stream);
        return base64(packed);
    }
    return base64(raw);
}

// A 4 by 3 orthogonal map exercising every layer type, both tileset kinds, templates and all tile data encodings.
core::Json orthogonalMap() {
    const std::vector<std::uint32_t> ground{1, 2, 1, 2, 3, 3, 3, 3, 5, 0, 5, 5};
    return core::Json::parse(R"({
        "type": "map", "orientation": "orthogonal", "renderorder": "right-down", "width": 4, "height": 3,
        "tilewidth": 16, "tileheight": 16, "infinite": false, "class": "level", "backgroundcolor": "#FF102030",
        "parallaxoriginx": 0, "parallaxoriginy": 0,
        "properties": [
            {"name": "title", "type": "string", "value": "Island"},
            {"name": "level", "type": "int", "value": 3},
            {"name": "gravity", "type": "float", "value": 9.5},
            {"name": "safe", "type": "bool", "value": true},
            {"name": "sky", "type": "color", "value": "#FF0000FF"},
            {"name": "unset", "type": "color", "value": ""},
            {"name": "music", "type": "file", "value": "../audio/theme.ogg"},
            {"name": "boss", "type": "object", "value": 7},
            {"name": "spawn", "type": "class", "propertytype": "Spawn", "value": {"count": 2, "kind": "goblin"}},
            {"name": "loot", "type": "list", "value": [
                {"type": "int", "value": 3}, {"type": "color", "value": "#FF00FF00"}, {"type": "file", "value": "../audio/coin.ogg"},
                {"type": "list", "value": [{"type": "bool", "value": true}, {"type": "file", "value": "gem.png"}]},
                {"type": "class", "propertytype": "Spawn", "value": {"kind": "bat"}}
            ]}
        ],
        "tilesets": [
            {"firstgid": 1, "source": "tiles/terrain.tsj"},
            {"firstgid": 100, "name": "props", "tilewidth": 16, "tileheight": 32, "tilecount": 2, "columns": 0, "objectalignment": "bottom",
             "tiles": [
                {"id": 0, "image": "props/tree.png", "imagewidth": 16, "imageheight": 32, "type": "tree"},
                {"id": 1, "image": "props/rock.png", "imagewidth": 16, "imageheight": 16, "x": 0, "y": 0, "width": 16, "height": 16}
             ]}
        ],
        "layers": [
            {"id": 1, "name": "ground", "type": "tilelayer", "width": 4, "height": 3, "encoding": "base64", "compression": "zlib", "data": ")" +
                             encodeTiles(ground, "zlib") + R"("},
            {"id": 2, "name": "decor", "type": "tilelayer", "width": 4, "height": 3, "encoding": "base64", "compression": "gzip", "data": ")" +
                             encodeTiles({0, 0, 0, 0, 0, 2147483652, 0, 0, 0, 0, 0, 4}, "gzip") + R"(",
             "properties": [{"name": "collision", "type": "bool", "value": false}]},
            {"id": 3, "name": "packed", "type": "tilelayer", "width": 4, "height": 3, "encoding": "base64", "compression": "zstd", "data": ")" +
                             encodeTiles(std::vector<std::uint32_t>(12, 0), "zstd") + R"("},
            {"id": 4, "name": "raw", "type": "tilelayer", "width": 4, "height": 3, "encoding": "base64", "data": ")" +
                             encodeTiles(std::vector<std::uint32_t>(12, 0), "") + R"("},
            {"id": 5, "name": "things", "type": "objectgroup", "draworder": "topdown", "class": "entities", "objects": [
                {"id": 1, "name": "spawn", "type": "player", "x": 10, "y": 20, "point": true, "properties": [{"name": "hp", "type": "int", "value": 3}]},
                {"id": 2, "name": "zone", "type": "collision", "x": 0, "y": 0, "width": 8, "height": 8, "rotation": 90, "properties": [{"name": "sensor", "type": "bool", "value": true}]},
                {"id": 3, "name": "pond", "type": "collision", "x": 20, "y": 20, "width": 10, "height": 6, "ellipse": true},
                {"id": 4, "name": "wall", "type": "collision", "x": 30, "y": 0, "polygon": [{"x": 0, "y": 0}, {"x": 10, "y": 0}, {"x": 5, "y": 5}]},
                {"id": 5, "name": "fence", "type": "collision", "x": 0, "y": 40, "polyline": [{"x": 0, "y": 0}, {"x": 10, "y": 0}, {"x": 10, "y": 10}]},
                {"id": 6, "name": "sign", "x": 5, "y": 5, "width": 40, "height": 20, "text": {"text": "Hi", "pixelsize": 12, "wrap": true, "color": "#FF00FF00", "halign": "center", "valign": "bottom", "bold": true}},
                {"id": 7, "name": "tree", "x": 16, "y": 48, "width": 16, "height": 32, "gid": 100},
                {"id": 8, "template": "templates/rock.tj", "x": 40, "y": 48},
                {"id": 9, "template": "templates/rock.tj", "x": 44, "y": 48, "name": "big rock", "properties": [{"name": "size", "type": "int", "value": 3}]}
            ]},
            {"id": 6, "name": "sky", "type": "imagelayer", "image": "backgrounds/sky.png", "imagewidth": 8, "imageheight": 8, "repeatx": true, "repeaty": false, "parallaxx": 0.5, "parallaxy": 0.5, "transparentcolor": "#ff00ff"},
            {"id": 7, "name": "group", "type": "group", "offsetx": 4, "offsety": 2, "opacity": 0.5, "tintcolor": "#FFFF0000", "layers": [
                {"id": 8, "name": "inner", "type": "tilelayer", "width": 4, "height": 3, "data": [0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0], "visible": true}
            ]}
        ]
    })");
}

core::Json terrainTileset() {
    return core::Json::parse(R"({
        "type": "tileset", "name": "terrain", "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 4,
        "image": "../images/terrain.png", "imagewidth": 64, "imageheight": 32, "margin": 0, "spacing": 0,
        "tileoffset": {"x": 0, "y": 0}, "properties": [{"name": "theme", "type": "string", "value": "grass"}],
        "tiles": [
            {"id": 1, "type": "water", "properties": [{"name": "deep", "type": "bool", "value": true}],
             "objectgroup": {"objects": [{"id": 1, "x": 0, "y": 0, "width": 16, "height": 16}]}},
            {"id": 2, "animation": [{"tileid": 2, "duration": 100}, {"tileid": 6, "duration": 100}]},
            {"id": 3, "objectgroup": {"objects": [{"id": 1, "x": 0, "y": 8, "width": 8, "height": 8}]}},
            {"id": 4, "objectgroup": {"objects": [{"id": 1, "x": 0, "y": 0, "width": 16, "height": 16}]}}
        ],
        "wangsets": [{"name": "coast", "class": "shore", "type": "corner", "tile": 0, "properties": [{"name": "wet", "type": "bool", "value": true}],
                      "colors": [{"name": "sand", "class": "ground", "color": "#ffe0c080", "tile": 0, "probability": 0.5, "properties": [{"name": "speed", "type": "float", "value": 0.8}]}],
                      "wangtiles": [{"tileid": 0, "wangid": [0, 1, 0, 1, 0, 1, 0, 1]}]}]
    })");
}

core::Json rockTemplate() {
    return core::Json::parse(R"({"type": "template", "tileset": {"firstgid": 1, "source": "../tiles/terrain.tsj"},
        "object": {"name": "rock", "type": "rock", "width": 16, "height": 16, "gid": 5, "properties": [{"name": "size", "type": "int", "value": 1}, {"name": "hard", "type": "bool", "value": true}]}})");
}

// The test map as an infinite map whose ground layer is stored in two chunks, one of them left of the origin.
core::Json infiniteMap() {
    core::Json map = orthogonalMap();
    map["infinite"] = true;
    map["layers"] = core::Json::parse(R"([{"id": 1, "name": "ground", "type": "tilelayer", "width": 4, "height": 2, "startx": -2, "starty": 0,
        "chunks": [{"x": -2, "y": 0, "width": 2, "height": 2, "data": [1, 2, 3, 4]}, {"x": 0, "y": 0, "width": 2, "height": 2, "data": [5, 6, 7, 8]}]}])");
    return map;
}

tiled::Map::JsonReader reader() {
    // clang-format off
    return [](const std::string& path) {
        if (path == "maps/tiles/terrain.tsj") {
            return terrainTileset();
        }
        if (path == "maps/templates/rock.tj") {
            return rockTemplate();
        }
        throw std::runtime_error("Unexpected read: " + path);
    };
    // clang-format on
}

std::map<std::string, std::string> packageFiles() {
    // clang-format off
    const auto png = [](int width, int height, std::uint32_t color) {
        const std::vector<std::uint8_t> image = test::pngImage(width, height, color);
        return std::string(image.begin(), image.end());
    };
    // clang-format on
    std::string map = orthogonalMap().dump();
    return {
        {"content/maps/island.tmj", map}, {"content/maps/tiles/terrain.tsj", terrainTileset().dump()}, {"content/maps/templates/rock.tj", rockTemplate().dump()}, {"content/maps/images/terrain.png", png(64, 32, 0xFFFFFFFFU)}, {"content/maps/props/tree.png", png(16, 32, 0x00FF00FFU)}, {"content/maps/props/rock.png", png(16, 16, 0x808080FFU)}, {"content/maps/backgrounds/sky.png", png(8, 8, 0xFF00FFFFU)}, {"content/maps/infinite.tmj", infiniteMap().dump()}, {"content/maps/world/level_0_0.tmj", map}, {"content/maps/world/level_1_0.tmj", map}, {"content/maps/world/level.world", R"({"type": "world", "maps": [{"fileName": "../island.tmj", "x": -64, "y": 0, "width": 64, "height": 48}], "patterns": [{"regexp": "level_(\\d+)_(\\d+)\\.tmj", "multiplierX": 64, "multiplierY": 48, "offsetX": 0, "offsetY": 0, "mapWidth": 64, "mapHeight": 48}]})"},
    };
}

} // namespace

TEST(MapTest, ParsesMapsTilesetsLayersAndTemplates) {
    const tiled::Map map = tiled::Map::parse(orthogonalMap(), "maps/island.tmj", reader());

    EXPECT_EQ(map.path, "maps/island.tmj");
    EXPECT_EQ(map.type, "level");
    EXPECT_EQ(map.orientation, tiled::Map::Orientation::Orthogonal);
    EXPECT_EQ(map.getPixelBounds(), (math::Rect{0.0F, 0.0F, 64.0F, 48.0F}));
    EXPECT_EQ(map.backgroundColor, math::Color::parse("#FF102030"));
    EXPECT_EQ(map.properties.getString("title", ""), "Island");
    EXPECT_EQ(map.properties.getNumber("level", 0.0), 3.0);
    EXPECT_TRUE(map.properties.getBool("safe", false));
    EXPECT_EQ(map.properties.find("music")->value, "audio/theme.ogg");
    EXPECT_EQ(map.properties.find("spawn")->propertyType, "Spawn");
    EXPECT_EQ(map.properties.find("spawn")->value.at("kind"), "goblin");
    const core::Json& loot = map.properties.find("loot")->value;
    EXPECT_EQ(map.properties.find("loot")->type, "list");
    EXPECT_EQ(loot[2].at("value"), "audio/coin.ogg");
    EXPECT_EQ(loot[3].at("value")[1].at("value"), "maps/gem.png");
    EXPECT_EQ(loot[4].at("propertytype"), "Spawn");
    EXPECT_FALSE(map.properties.has("missing"));
    EXPECT_EQ(map.properties.getNumber("missing", 4.0), 4.0);

    ASSERT_EQ(map.tilesets.size(), 2U);
    const tiled::Tileset& terrain = *map.tilesets[0].tileset;
    EXPECT_EQ(terrain.path, "maps/tiles/terrain.tsj");
    EXPECT_EQ(terrain.image, "maps/images/terrain.png");
    EXPECT_EQ(terrain.getSource(5), (math::Rect{16.0F, 16.0F, 16.0F, 16.0F}));
    EXPECT_EQ(terrain.findTile(1)->type, "water");
    EXPECT_TRUE(terrain.findTile(1)->properties.getBool("deep", false));
    EXPECT_EQ(terrain.findTile(2)->animation.size(), 2U);
    EXPECT_FLOAT_EQ(terrain.findTile(2)->animation[1].duration, 0.1F);
    EXPECT_EQ(terrain.wangSets.front().tiles.front().wangId[1], 1U);
    EXPECT_EQ(terrain.wangSets.front().type, "shore");
    EXPECT_EQ(terrain.wangSets.front().colors.front().type, "ground");
    EXPECT_FLOAT_EQ(terrain.wangSets.front().colors.front().probability, 0.5F);
    EXPECT_EQ(terrain.properties.getString("theme", ""), "grass");
    EXPECT_EQ(terrain.findTile(7), nullptr);

    const tiled::Tileset& props = *map.tilesets[1].tileset;
    EXPECT_EQ(props.findTile(0)->image, "maps/props/tree.png");
    EXPECT_EQ(props.getSource(0), (math::Rect{0.0F, 0.0F, 16.0F, 32.0F}));
    EXPECT_EQ(map.findTileset(101)->firstGid, 100U);
    EXPECT_EQ(map.findTileset(8)->firstGid, 1U);
    EXPECT_EQ(map.findTileset(99), nullptr);
    EXPECT_EQ(map.findTileset(102), nullptr);
    EXPECT_EQ(map.findTileset(0), nullptr);

    const tiled::Layer& ground = *map.findLayer("ground");
    EXPECT_EQ(ground.getGid(1, 0), 2U);
    EXPECT_EQ(ground.getGid(3, 2), 5U);
    EXPECT_EQ(ground.getGid(9, 9), 0U);
    EXPECT_EQ(map.findLayer("decor")->getGid(1, 1), 2147483652U);
    EXPECT_EQ(map.findLayer("inner")->getGid(1, 1), 1U);
    EXPECT_EQ(map.findLayer("missing"), nullptr);
    EXPECT_EQ(map.findLayer("group")->opacity, 0.5F);

    const tiled::Layer& things = *map.findLayer("things");
    ASSERT_EQ(things.objects.size(), 9U);
    EXPECT_EQ(things.objects[0].shape, tiled::Object::Shape::Point);
    EXPECT_NEAR(things.objects[1].rotation, math::Math::kPi / 2.0F, 0.0001F);
    EXPECT_EQ(things.objects[2].shape, tiled::Object::Shape::Ellipse);
    EXPECT_EQ(things.objects[3].points.size(), 3U);
    EXPECT_EQ(things.objects[4].shape, tiled::Object::Shape::Polyline);
    EXPECT_EQ(things.objects[5].text.horizontalAlign, "center");
    EXPECT_EQ(things.objects[5].text.color, math::Color::fromHex(0x00FF00FFU));
    EXPECT_EQ(things.objects[6].shape, tiled::Object::Shape::Tile);

    const tiled::Object& rock = things.objects[7];
    EXPECT_EQ(rock.name, "rock");
    EXPECT_EQ(rock.gid, 5U);
    EXPECT_EQ(rock.templatePath, "maps/templates/rock.tj");
    EXPECT_EQ(rock.position, math::Vec2(40.0F, 48.0F));
    EXPECT_EQ(things.objects[8].name, "big rock");
    EXPECT_EQ(things.objects[8].properties.getNumber("size", 0.0), 3.0);
    EXPECT_TRUE(things.objects[8].properties.getBool("hard", false));

    const tiled::Layer& sky = *map.findLayer("sky");
    EXPECT_EQ(sky.kind, tiled::Layer::Kind::Image);
    EXPECT_EQ(sky.transparentColor, math::Color::fromHex(0xFF00FFFFU));
    EXPECT_EQ(sky.parallax, math::Vec2(0.5F, 0.5F));

    const std::vector<tiled::Map::Image> images = map.getImages();
    ASSERT_EQ(images.size(), 4U);
    EXPECT_EQ(images.back().path, "maps/backgrounds/sky.png");
}

TEST(MapTest, RejectsBrokenData) {
    const auto parse = [](core::Json document) { return tiled::Map::parse(document, "maps/broken.tmj", reader()); };
    core::Json map = orthogonalMap();
    map["orientation"] = "diagonal";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = orthogonalMap();
    map["renderorder"] = "down";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = orthogonalMap();
    map["layers"][0]["compression"] = "lz4";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = orthogonalMap();
    map["layers"][0]["data"] = "!!!!";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = orthogonalMap();
    map["layers"][0]["compression"] = "zstd";
    EXPECT_THROW((void)parse(map), std::runtime_error);

    map = orthogonalMap();
    map["layers"][0]["data"] = core::Json::array({1, 2});
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = orthogonalMap();
    map["layers"][0]["type"] = "shapelayer";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = orthogonalMap();
    map["backgroundcolor"] = "blue";
    EXPECT_THROW((void)parse(map), std::invalid_argument);

    map = orthogonalMap();
    map["tilesets"].erase(0);
    EXPECT_THROW((void)parse(map), std::invalid_argument) << "the rock template needs the terrain tileset";

    tiled::Layer layer{.width = 2, .height = 2, .gids = std::vector<std::uint32_t>(4, 0)};
    EXPECT_THROW(layer.setGid(2, 0, 1), std::out_of_range);
    tiled::Layer chunked{.chunks = {{.x = 0, .y = 0, .width = 1, .height = 1, .gids = {0}}}};
    chunked.setGid(0, 0, 3);
    EXPECT_EQ(chunked.getGid(0, 0), 3U);
    EXPECT_THROW(chunked.setGid(5, 5, 1), std::out_of_range);
    EXPECT_THROW((void)tiled::Tileset{}.getSource(0), std::out_of_range);
}

TEST(MapTest, ConvertsCellsForEveryOrientation) {
    tiled::Map map;
    map.width = 10;
    map.height = 10;
    map.tileSize = {64.0F, 32.0F};

    map.orientation = tiled::Map::Orientation::Orthogonal;
    EXPECT_EQ(map.cellToWorld(2, 3), math::Vec2(128.0F, 96.0F));
    EXPECT_EQ(map.worldToCell({130.0F, 100.0F}), (std::array<int, 2>{2, 3}));
    EXPECT_EQ(map.objectToWorld({5.0F, 6.0F}), math::Vec2(5.0F, 6.0F));

    map.orientation = tiled::Map::Orientation::Isometric;
    EXPECT_EQ(map.cellToWorld(0, 0), math::Vec2(320.0F, 0.0F));
    EXPECT_EQ(map.cellToWorld(1, 0), math::Vec2(352.0F, 16.0F));
    EXPECT_EQ(map.worldToCell(map.cellToWorld(3, 4) + math::Vec2{0.0F, 16.0F}), (std::array<int, 2>{3, 4}));
    EXPECT_EQ(map.objectToWorld({32.0F, 0.0F}), math::Vec2(352.0F, 16.0F));
    EXPECT_EQ(map.getPixelBounds(), (math::Rect{0.0F, 0.0F, 640.0F, 320.0F}));

    for (const bool staggerX : {false, true}) {
        for (const bool even : {false, true}) {
            for (const tiled::Map::Orientation orientation : {tiled::Map::Orientation::Staggered, tiled::Map::Orientation::Hexagonal}) {
                map.orientation = orientation;
                map.staggerX = staggerX;
                map.staggerEven = even;
                map.hexSideLength = 16;
                for (const auto& [column, row] : std::vector<std::pair<int, int>>{{0, 0}, {3, 4}, {4, 3}, {-1, 2}}) {
                    const math::Vec2 center = map.cellToWorld(column, row) + map.tileSize * 0.5F;
                    EXPECT_EQ(map.worldToCell(center), (std::array<int, 2>{column, row})) << column << "," << row;
                }
                EXPECT_GT(map.getPixelBounds().width, 0.0F);
            }
        }
    }
    EXPECT_EQ(tiled::Map::orientationName(tiled::Map::Orientation::Hexagonal), "hexagonal");
    EXPECT_EQ(tiled::Map::renderOrderName(tiled::Map::RenderOrder::RightDown), "right-down");
    EXPECT_EQ(tiled::Map::renderOrderName(tiled::Map::RenderOrder::RightUp), "right-up");
    EXPECT_EQ(tiled::Map::renderOrderName(tiled::Map::RenderOrder::LeftDown), "left-down");
    EXPECT_EQ(tiled::Map::renderOrderName(tiled::Map::RenderOrder::LeftUp), "left-up");
    EXPECT_EQ(tiled::Layer::kindName(tiled::Layer::Kind::Group), "group");
    EXPECT_EQ(tiled::Object::shapeName(tiled::Object::Shape::Polyline), "polyline");
}

// The test map skewed into an oblique map, with Tiled 1.12 blend modes, a capsule and a faded tree.
core::Json obliqueMap() {
    core::Json map = orthogonalMap();
    map["orientation"] = "oblique";
    map["skewx"] = 8;
    map["skewy"] = 4;
    map["layers"][0]["mode"] = "multiply";
    map["layers"][1]["mode"] = "add";
    map["layers"][4]["mode"] = "screen";
    map["layers"][5]["visible"] = false;
    map["layers"][4]["objects"][6]["opacity"] = 0.25;
    map["layers"][4]["objects"].push_back(core::Json::parse(R"({"id": 10, "name": "log", "type": "collision", "x": 100, "y": 0, "width": 40, "height": 10, "capsule": true})"));
    return map;
}

TEST(MapTest, ReadsObliqueMapsBlendModesAndCapsules) {
    tiled::Map map = tiled::Map::parse(obliqueMap(), "maps/oblique.tmj", reader());
    EXPECT_EQ(map.orientation, tiled::Map::Orientation::Oblique);
    EXPECT_EQ(tiled::Map::orientationName(map.orientation), "oblique");
    EXPECT_EQ(map.skew, math::Vec2(8.0F, 4.0F));
    EXPECT_EQ(map.findLayer("ground")->blend, graphics::BlendMode::Type::Multiply);
    EXPECT_EQ(map.findLayer("decor")->blend, graphics::BlendMode::Type::Additive);
    EXPECT_EQ(map.findLayer("things")->blend, graphics::BlendMode::Type::Screen);
    EXPECT_EQ(map.findLayer("inner")->blend, graphics::BlendMode::Type::Alpha);

    const std::vector<tiled::Object>& objects = map.findLayer("things")->objects;
    EXPECT_EQ(objects[6].opacity, 0.25F);
    EXPECT_EQ(objects[0].opacity, 1.0F);
    EXPECT_EQ(objects[9].shape, tiled::Object::Shape::Capsule);
    EXPECT_EQ(tiled::Object::shapeName(tiled::Object::Shape::Capsule), "capsule");

    // Rows slide right by the horizontal skew and columns slide down by the vertical skew.
    EXPECT_EQ(map.cellToWorld(2, 3), math::Vec2(56.0F, 56.0F));
    EXPECT_EQ(map.objectToWorld({16.0F, 32.0F}), math::Vec2(32.0F, 36.0F));
    EXPECT_EQ(map.worldToCell(map.objectToWorld({40.0F, 56.0F})), (std::array<int, 2>{2, 3}));
    EXPECT_EQ(map.worldToCell(map.objectToWorld({-4.0F, 2.0F})), (std::array<int, 2>{-1, 0}));
    EXPECT_EQ(map.getPixelBounds(), (math::Rect{0.0F, 0.0F, 88.0F, 64.0F}));
    map.skew = {-8.0F, 4.0F};
    EXPECT_EQ(map.getPixelBounds(), (math::Rect{-24.0F, 0.0F, 88.0F, 64.0F}));
    EXPECT_EQ(map.worldToCell(map.cellToWorld(3, 1) + math::Vec2{1.0F, 1.0F}), (std::array<int, 2>{3, 1}));

    core::Json folded = obliqueMap();
    folded["skewx"] = 16;
    folded["skewy"] = 16;
    EXPECT_THROW((void)tiled::Map::parse(folded, "maps/folded.tmj", reader()), std::invalid_argument);

    core::Json overlay = orthogonalMap();
    overlay["layers"][6]["layers"][0]["mode"] = "overlay";
    try {
        (void)tiled::Map::parse(overlay, "maps/overlay.tmj", reader());
        FAIL() << "The overlay blend mode was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The Tiled layer 'inner' uses the blend mode 'overlay', which Haylen cannot draw. Layers can use normal, add, multiply or screen.");
    }

    // Tiled draws the layers of a group with their own modes, so a group mode would silently change nothing.
    core::Json blendedGroup = orthogonalMap();
    blendedGroup["layers"][6]["mode"] = "add";
    try {
        (void)tiled::Map::parse(blendedGroup, "maps/group.tmj", reader());
        FAIL() << "The group blend mode was accepted.";
    } catch (const std::invalid_argument& error) {
        EXPECT_EQ(std::string(error.what()), "The Tiled group layer 'group' uses the blend mode 'add', which Tiled does not apply to the layers inside it. Set the blend mode on those layers.");
    }
    blendedGroup["layers"][6]["mode"] = "normal";
    EXPECT_EQ(tiled::Map::parse(blendedGroup, "maps/group.tmj", reader()).findLayer("group")->blend, graphics::BlendMode::Type::Alpha);
}

TEST(MapTest, ReadsInfiniteMapsAndWorlds) {
    const tiled::Map infinite = tiled::Map::parse(infiniteMap(), "maps/infinite.tmj", reader());
    EXPECT_TRUE(infinite.infinite);
    EXPECT_EQ(infinite.findLayer("ground")->getGid(-1, 1), 4U);
    EXPECT_EQ(infinite.findLayer("ground")->getGid(1, 0), 6U);
    EXPECT_EQ(infinite.findLayer("ground")->getGid(5, 5), 0U);

    const std::vector<std::string> files{"maps/world/level_0_0.tmj", "maps/world/level_1_0.tmj", "maps/world/notes.txt", "maps/other/level_9_9.tmj"};
    const tiled::World world = tiled::World::parse(core::Json::parse(packageFiles().at("content/maps/world/level.world")), "maps/world/level.world", files);
    ASSERT_EQ(world.maps.size(), 3U);
    EXPECT_EQ(world.maps[0].path, "maps/island.tmj");
    EXPECT_EQ(world.maps[0].bounds, (math::Rect{-64.0F, 0.0F, 64.0F, 48.0F}));
    EXPECT_EQ(world.maps[2].path, "maps/world/level_1_0.tmj");
    EXPECT_EQ(world.maps[2].bounds, (math::Rect{64.0F, 0.0F, 64.0F, 48.0F}));
}

TEST(MapRendererTest, LoadsDrawsAndAnimatesMaps) {
    test::EngineFixture fixture(packageFiles());
    assets::Manager& assets = fixture.engine().getAssets();
    const auto data = std::static_pointer_cast<tiled::Map>(assets.load("tiled", "maps/island.tmj"));
    EXPECT_EQ(data->tilesets[0].tileset->texture, assets.texture("maps/images/terrain.png"));
    EXPECT_TRUE(data->findLayer("sky")->texture.isValid());
    EXPECT_NE(data->findLayer("sky")->texture, assets.texture("maps/backgrounds/sky.png")) << "keyed images stay private";

    tiled::MapRenderer map(*data, fixture.engine().getDefaultFont());
    // clang-format off
    const auto render = [&](const tiled::MapRenderer::View& view) {
        // clang-format off
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
            engine.getRenderer2D().beginWorld(graphics2d::Camera{});
            map.draw(engine.getRenderer2D(), view);
        }));
        // clang-format on
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats();
    };
    // clang-format on

    // Eleven ground tiles, two decor tiles, one nested tile, three tile objects, eight sky copies and two glyphs of text.
    const tiled::MapRenderer::View view{.center = {32.0F, 24.0F}, .visible = {0.0F, 0.0F, 64.0F, 48.0F}};
    const graphics2d::Renderer::Stats all = render(view);
    EXPECT_EQ(all.sprites, 27U);

    map.setLayerVisible("ground", false);
    EXPECT_EQ(render(view).sprites, 16U);
    map.setLayerVisible("ground", true);

    EXPECT_EQ(map.getTile("ground", 0, 1), 3U);
    map.setTile("ground", 0, 1, 1);
    EXPECT_EQ(map.getTile("ground", 0, 1), 1U);
    map.setTile("ground", 0, 1, 0);
    map.update(0.15F);
    const graphics2d::Renderer::Stats culled = render({.center = {1000.0F, 1000.0F}, .visible = {900.0F, 900.0F, 10.0F, 10.0F}});
    EXPECT_LT(culled.sprites, all.sprites);

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(graphics2d::Camera{});
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

TEST(MapRendererTest, PlacesMapsAtAWorldOffset) {
    test::EngineFixture fixture(packageFiles());
    tiled::MapRenderer map(*std::static_pointer_cast<tiled::Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());
    // clang-format off
    const auto render = [&](const tiled::MapRenderer::View& view, math::Vec2 offset) {
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
            engine.getRenderer2D().beginWorld(graphics2d::Camera{});
            map.draw(engine.getRenderer2D(), view, {.offset = offset});
        }));
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats().sprites;
    };
    // clang-format on

    // A map moved by an offset draws the same way when the view moves with it, parallax included, and its tiles leave the view it left.
    const tiled::MapRenderer::View home{.center = {32.0F, 24.0F}, .visible = {0.0F, 0.0F, 64.0F, 48.0F}};
    const tiled::MapRenderer::View away{.center = {1032.0F, 24.0F}, .visible = {1000.0F, 0.0F, 64.0F, 48.0F}};
    const std::size_t all = render(home, {});
    EXPECT_EQ(all, 27U);
    EXPECT_EQ(render(away, {1000.0F, 0.0F}), all);
    EXPECT_LT(render(home, {1000.0F, 0.0F}), 16U);
}

TEST(MapRendererTest, SortsRowsAndObjectsByTheYTheyStandOn) {
    test::EngineFixture fixture(packageFiles());
    tiled::MapRenderer map(*std::static_pointer_cast<tiled::Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());
    const graphics::Texture unit = fixture.engine().getGraphics().createTexture(graphics::Image(4, 4, math::Color::white()));

    // clang-format off
    const auto switches = [&](graphics2d::Renderer::SortMode mode, bool ysort, float standing) {
        fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&, mode, ysort, standing](core::Engine& engine) {
            graphics2d::Renderer& renderer = engine.getRenderer2D();
            renderer.beginWorld(graphics2d::Camera{}, {.sort = mode});
            map.drawLayer(renderer, "ground", {}, {.ysort = ysort});
            renderer.draw({.texture = unit, .position = {8.0F, standing}, .order = {.depth = standing}});
        }));
        fixture.frames(1);
        return fixture.engine().getRenderer2D().getStats().textureSwitches;
    };
    // clang-format on

    // The ground rows stand on 16, 32 and 48, so a unit between two rows splits the terrain draws, and one below the map comes last.
    EXPECT_EQ(switches(graphics2d::Renderer::SortMode::Depth, true, 24.0F), 3U);
    EXPECT_EQ(switches(graphics2d::Renderer::SortMode::Depth, true, 100.0F), 2U);
    EXPECT_EQ(switches(graphics2d::Renderer::SortMode::Depth, false, 24.0F), 2U);
    EXPECT_EQ(switches(graphics2d::Renderer::SortMode::Y, true, 40.0F), 3U);
    EXPECT_EQ(switches(graphics2d::Renderer::SortMode::Y, true, 100.0F), 2U);

    // Tile objects and text stand on their bottom edge.
    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(graphics2d::Camera{}, {.sort = graphics2d::Renderer::SortMode::Y});
        map.draw(engine.getRenderer2D(), {.center = {32.0F, 24.0F}, .visible = {0.0F, 0.0F, 64.0F, 48.0F}}, {.ysort = true});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 27U);
}

TEST(MapRendererTest, DrawsObliqueMapsWithLayerBlendModes) {
    std::map<std::string, std::string> files = packageFiles();
    files["content/maps/island.tmj"] = obliqueMap().dump();
    test::EngineFixture fixture(files);
    tiled::MapRenderer map(*std::static_pointer_cast<tiled::Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj")), fixture.engine().getDefaultFont());

    // clang-format off
    fixture.engine().getScenes().replace(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(graphics2d::Camera{});
        map.draw(engine.getRenderer2D(), {});
    }));
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    // Eleven ground tiles, two decor tiles, one nested tile, three tile objects and two glyphs, with the sky hidden.
    EXPECT_EQ(fixture.engine().getRenderer2D().getStats().sprites, 19U);
}

TEST(MapRendererTest, RejectsDrawingWhatItCannotPlace) {
    test::EngineFixture fixture(packageFiles());
    const auto data = std::static_pointer_cast<tiled::Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj"));

    tiled::MapRenderer textless(*data);
    std::string error;
    // clang-format off
    fixture.engine().getScenes().push(std::make_shared<test::DrawingScene>([&](core::Engine& engine) {
        engine.getRenderer2D().beginWorld(graphics2d::Camera{});
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
    test::EngineFixture fixture(packageFiles());
    const auto data = std::static_pointer_cast<tiled::Map>(fixture.engine().getAssets().load("tiled", "maps/island.tmj"));
    tiled::MapRenderer map(*data);
    physics2d::World world({.gravity = {}});

    const std::vector<physics2d::Body> bodies = map.buildCollision(world);
    ASSERT_EQ(bodies.size(), 2U);
    // Solid cells merge into one box per run, which gives two single cells on the first row and a single cell and a pair on the last row.
    EXPECT_EQ(bodies[0].getShapes().size(), 4U);
    EXPECT_FALSE(world.queryPoint({24.0F, 8.0F}).empty());
    EXPECT_FALSE(world.queryPoint({4.0F, 44.0F}).empty());
    EXPECT_FALSE(world.queryPoint({56.0F, 40.0F}).empty());
    EXPECT_TRUE(world.queryPoint({20.0F, 40.0F}).empty());

    const std::vector<physics2d::Shape> zone = world.queryPoint({-4.0F, 4.0F});
    ASSERT_EQ(zone.size(), 1U);
    EXPECT_TRUE(zone.front().isSensor());
    EXPECT_GE(bodies[1].getShapes().size(), 5U);
}

TEST(MapRendererTest, BuildsObliqueAndCapsuleCollision) {
    const tiled::MapRenderer map(tiled::Map::parse(obliqueMap(), "maps/oblique.tmj", reader()));
    physics2d::World world({.gravity = {}});
    ASSERT_EQ(map.buildCollision(world).size(), 2U);

    // The solid tile in the second column of the first row hangs from the skewed corner of the cell below it.
    EXPECT_FALSE(world.queryPoint({26.0F, 18.0F}).empty());
    EXPECT_TRUE(world.queryPoint({18.0F, 2.0F}).empty());

    // The capsule keeps its round caps under the shear.
    EXPECT_FALSE(world.queryPoint(map.getMap().objectToWorld({120.0F, 5.0F})).empty());
    EXPECT_FALSE(world.queryPoint(map.getMap().objectToWorld({101.0F, 5.0F})).empty());
    EXPECT_TRUE(world.queryPoint(map.getMap().objectToWorld({100.5F, 0.5F})).empty());

    core::Json upright = orthogonalMap();
    upright["layers"][4]["objects"] = core::Json::parse(R"([{"id": 1, "type": "collision", "x": 100, "y": 0, "width": 10, "height": 40, "capsule": true}, {"id": 2, "type": "collision", "x": 200, "y": 0, "width": 12, "height": 12, "capsule": true}])");
    const tiled::MapRenderer straight(tiled::Map::parse(upright, "maps/upright.tmj", reader()));
    physics2d::World other({.gravity = {}});
    (void)straight.buildCollision(other);
    EXPECT_FALSE(other.queryPoint({105.0F, 1.0F}).empty());
    EXPECT_FALSE(other.queryPoint({105.0F, 39.0F}).empty());
    EXPECT_TRUE(other.queryPoint({100.5F, 0.5F}).empty());
    EXPECT_FALSE(other.queryPoint({206.0F, 6.0F}).empty());
    EXPECT_TRUE(other.queryPoint({200.5F, 0.5F}).empty());
}

TEST(MapRendererTest, SpawnsObjectsThroughFactories) {
    core::Json document = orthogonalMap();
    document["layers"][6]["layers"].push_back(core::Json::parse(R"({"id": 9, "name": "camp", "type": "objectgroup", "offsetx": 10, "offsety": 20, "objects": [{"id": 20, "type": "player", "x": 1, "y": 2}]})"));
    const tiled::MapRenderer map(tiled::Map::parse(document, "maps/island.tmj", reader()));

    std::vector<std::pair<std::string, math::Vec2>> spawned;
    tiled::ObjectFactories factories;
    factories.add("player", [&spawned](const tiled::Object& object, math::Vec2 position) { spawned.emplace_back("player " + object.name, position); });
    factories.add("rock", [&spawned](const tiled::Object& object, math::Vec2 position) { spawned.emplace_back("rock " + object.name, position); });
    EXPECT_TRUE(factories.has("rock"));
    EXPECT_THROW(factories.add("rock", [](const tiled::Object&, math::Vec2) {}), std::invalid_argument);
    EXPECT_THROW(factories.add("", [](const tiled::Object&, math::Vec2) {}), std::invalid_argument);

    // The nested layer adds its own offset to the one of its group.
    EXPECT_EQ(factories.spawn(map), 4U);
    ASSERT_EQ(spawned.size(), 4U);
    EXPECT_EQ(spawned[0], std::make_pair(std::string("player spawn"), math::Vec2(10.0F, 20.0F)));
    EXPECT_EQ(spawned[1], std::make_pair(std::string("rock rock"), math::Vec2(40.0F, 48.0F)));
    EXPECT_EQ(spawned[2], std::make_pair(std::string("rock big rock"), math::Vec2(44.0F, 48.0F)));
    EXPECT_EQ(spawned[3], std::make_pair(std::string("player "), math::Vec2(15.0F, 24.0F)));

    spawned.clear();
    factories.remove("rock");
    EXPECT_FALSE(factories.has("rock"));
    EXPECT_EQ(factories.spawn(map, "camp"), 1U);
    EXPECT_EQ(spawned.front().second, math::Vec2(15.0F, 24.0F));
    EXPECT_THROW((void)factories.spawn(map, "ground"), std::invalid_argument);
    EXPECT_THROW((void)factories.spawn(map, "missing"), std::invalid_argument);

    // A factory may remove itself while it runs, which leaves the later objects of its class alone.
    spawned.clear();
    // clang-format off
    factories.add("rock", [&spawned, &factories](const tiled::Object& object, math::Vec2 position) {
        factories.remove("rock");
        spawned.emplace_back("once " + object.name, position);
    });
    // clang-format on
    EXPECT_EQ(factories.spawn(map), 3U);
    ASSERT_EQ(spawned.size(), 3U);
    EXPECT_EQ(spawned[1].first, "once rock");
}

TEST(TiledLuaTest, HandsDeeplyNestedGroupsToLua) {
    core::Json document = orthogonalMap();
    core::Json group = core::Json::parse(R"({"id": 100, "name": "deepest", "type": "group", "layers": []})");
    for (int level = 1; level <= 60; ++level) {
        group = core::Json{{"id", 100 + level}, {"name", "level"}, {"type", "group"}, {"layers", core::Json::array({group})}};
    }
    document["layers"].push_back(group);
    std::map<std::string, std::string> files = packageFiles();
    files["content/maps/island.tmj"] = document.dump();

    test::EngineFixture fixture(files);
    EXPECT_EQ(fixture.lua("local layers = require('haylen.tiled').newMap(require('haylen.assets').load('maps/island.tmj')):layers() local layer = layers[#layers] for level = 1, 60 do layer = layer.layers[1] end return layer.name"), "deepest");
}

TEST(TiledLuaTest, UsesMapsFromLua) {
    test::EngineFixture fixture(packageFiles());
    // clang-format off
    fixture.runLua(R"(
        tiled = require('haylen.tiled')
        assets = require('haylen.assets')
        graphics2d = require('haylen.graphics2d')
        physics2d = require('haylen.physics2d')
        data = assets.load('maps/island.tmj')
        map = tiled.newMap(data)
        camera = graphics2d.newCamera()
        require('haylen.scene').push({render = function()
            graphics2d.beginWorld(camera)
            map:draw(camera, {layer = 1})
            map:drawLayer('inner', nil, {layer = 2})
        end})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();

    EXPECT_EQ(fixture.lua("return data.path .. ' ' .. map.path .. ' ' .. tostring(data == assets.load('maps/island.tmj'))"), "maps/island.tmj maps/island.tmj true");
    EXPECT_EQ(fixture.lua("return map.width .. 'x' .. map.height .. ' ' .. map.tileWidth .. ' ' .. map.tileHeight .. ' ' .. map.bounds.width .. ' ' .. map.bounds.height .. ' ' .. map.skewX"), "4x3 16.0 16.0 64.0 48.0 0.0");
    EXPECT_EQ(fixture.lua("return map.orientation .. ' ' .. map.type .. ' ' .. map.backgroundColor:toHex()"), "orthogonal level #FF102030");
    EXPECT_EQ(fixture.lua("local p = map.properties return p.title .. p.level .. p.gravity .. tostring(p.safe) .. p.sky:toHex() .. tostring(p.unset) .. p.music .. p.boss .. p.spawn.kind"), "Island39.5true#FF0000FFnilaudio/theme.ogg7goblin");
    EXPECT_EQ(fixture.lua("return map:tileAt('ground', 1, 0) .. ' ' .. #map:layers() .. ' ' .. map:layer('group').layers[1].name .. ' ' .. map:layer('ground').width"), "2 7 inner 4");
    EXPECT_EQ(fixture.lua("local layer = map:layer('things') return layer.kind .. ' ' .. layer.type .. ' ' .. #layer.objects .. ' ' .. layer.objects[1].properties.hp"), "object entities 9 3");
    EXPECT_EQ(fixture.lua("local sky = map:layer('sky') return sky.kind .. ' ' .. sky.image .. ' ' .. tostring(sky.repeatX) .. ' ' .. sky.parallaxX"), "image maps/backgrounds/sky.png true 0.5");
    EXPECT_EQ(fixture.lua("local o = map:objects('things')[6] return o.shape .. ' ' .. o.text.text .. ' ' .. o.text.horizontalAlign .. ' ' .. o.text.color:toHex()"), "text Hi center #FF00FF00");
    EXPECT_EQ(fixture.lua("local o = map:objects()[4] return o.shape .. ' ' .. #o.points .. ' ' .. o.points[2].x .. ' ' .. o.opacity .. ' ' .. map:layer('ground').blend"), "polygon 3 10.0 1.0 alpha");
    EXPECT_EQ(fixture.lua("local t = map:tileInfo(2) return t.id .. ' ' .. t.tileset .. ' ' .. t.type .. ' ' .. tostring(t.properties.deep) .. ' ' .. #t.collision .. ' ' .. t.source.x"), "1 terrain water true 1 16.0");
    EXPECT_EQ(fixture.lua("local t = map:tileInfo(3 | 0x80000000) return tostring(t.flippedX) .. tostring(t.flippedY) .. tostring(t.flippedDiagonally) .. ' ' .. #t.animation"), "truefalsefalse 2");
    EXPECT_EQ(fixture.lua("return tostring(map:tileInfo(0))"), "nil");
    EXPECT_EQ(fixture.lua("local sets = map:tilesets() return #sets .. ' ' .. sets[1].name .. ' ' .. sets[2].firstGid .. ' ' .. sets[1].wangSets[1].name .. ' ' .. sets[1].wangSets[1].colors[1].name .. ' ' .. sets[1].wangSets[1].tiles[1].wangId[2]"), "2 terrain 100 coast sand 1");
    // clang-format off
    EXPECT_EQ(fixture.lua(R"(
        local spawned = map:spawn({
            player = function(object) return object.name .. '@' .. object.worldX .. ',' .. object.worldY end,
            rock = function(object) return object.properties.size end,
            collision = function() end,
        })
        return table.concat(spawned, ' ') .. ' ' .. #map:spawn({player = function() return 1 end}, 'things')
    )"), "spawn@10.0,20.0 1 3 1");
    // clang-format on
    EXPECT_NE(fixture.lua("map:spawn({rock = 5})").find("The factory for the Tiled class 'rock' is not a function."), std::string::npos);
    EXPECT_NE(fixture.lua("map:spawn({}, 'ground')").find("Unknown object layer: ground"), std::string::npos);
    EXPECT_EQ(fixture.lua("local x, y = map:cellToWorld(2, 1) local c, r = map:worldToCell(40, 20) local ox, oy = map:objectToWorld(3, 4) return x .. ',' .. y .. ' ' .. c .. ',' .. r .. ' ' .. ox .. ',' .. oy"), "32.0,16.0 2,1 3.0,4.0");

    fixture.runLua("map:setTile('ground', 0, 0, 3) map:setLayerVisible('decor', false) map:update(0.1)");
    EXPECT_EQ(fixture.lua("return map:tileAt('ground', 0, 0) .. ' ' .. tostring(map:layer('decor').visible)"), "3 false");
    EXPECT_EQ(fixture.lua("local world = physics2d.newWorld({gravity = {0, 0}}) local bodies = map:buildCollision(world) return #bodies .. ' ' .. bodies[1].type .. ' ' .. tostring(bodies[1].world == world)"), "2 static true");
    EXPECT_EQ(fixture.lua("local w = assets.load('maps/world/level.world') return #w .. ' ' .. w[1].path .. ' ' .. w[3].x .. ' ' .. w[3].width"), "3 maps/island.tmj 64.0 64.0");

    EXPECT_NE(fixture.lua("map:layer('missing')").find("Unknown layer: missing"), std::string::npos);
    EXPECT_NE(fixture.lua("map:draw(camera, {layer = 1, z = 2})").find("Unknown option 'z'"), std::string::npos);
    EXPECT_NE(fixture.lua("map:objects('ground')").find("Unknown object layer: ground"), std::string::npos);
    EXPECT_NE(fixture.lua("map:setTile('things', 0, 0, 1)").find("Unknown tile layer: things"), std::string::npos);
    EXPECT_NE(fixture.lua("tiled.newMap('map')").find("error: "), std::string::npos);
}

TEST(TiledLuaTest, CullsWithTheActiveCanvasAndPlacesWorldMaps) {
    test::EngineFixture fixture(packageFiles());
    // clang-format off
    fixture.runLua(R"(
        tiled = require('haylen.tiled')
        graphics = require('haylen.graphics')
        graphics2d = require('haylen.graphics2d')
        map = tiled.newMap(require('haylen.assets').load('maps/island.tmj'))
        camera = graphics2d.newCamera()
        target = graphics.newRenderTarget(64, 64)
        require('haylen.scene').push({render = function()
            if canvas == 'target' then
                graphics2d.beginTarget(target, camera)
            else
                graphics2d.beginWorld(camera)
            end
            map:drawLayer('ground', camera, {x = offsetX, ysort = ysort})
        end})
    )");
    const auto groundSprites = [&](const std::string& setup) {
        fixture.runLua(setup);
        fixture.frames(1);
        EXPECT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
        return fixture.lua("return graphics2d.stats().sprites");
    };
    // clang-format on

    // The render target shows 64 units around the camera, so the ground far to its left is culled, while the wide world canvas still sees it.
    EXPECT_EQ(groundSprites("canvas = 'target' offsetX = 0 camera.position = {600, 24}"), "0");
    EXPECT_EQ(groundSprites("canvas = 'world'"), "11");
    EXPECT_EQ(groundSprites("canvas = 'target' offsetX = 600 camera.position = {632, 24}"), "11");
    EXPECT_EQ(groundSprites("canvas = 'world' offsetX = 3000"), "0");
    EXPECT_EQ(groundSprites("canvas = 'world' offsetX = 0 ysort = true"), "11");
    EXPECT_NE(fixture.lua("map:drawLayer('ground', camera, {ysort = 1})").find("bad option 'ysort'"), std::string::npos);
}

TEST(TiledLuaTest, ExposesMapTilesetLayerAndPropertyDetails) {
    test::EngineFixture fixture(packageFiles());
    fixture.runLua("tiled = require('haylen.tiled') assets = require('haylen.assets') map = tiled.newMap(assets.load('maps/island.tmj')) endless = tiled.newMap(assets.load('maps/infinite.tmj'))");

    EXPECT_EQ(fixture.lua("return tostring(map.infinite) .. ' ' .. map.renderOrder .. ' ' .. map.hexSideLength .. ' ' .. tostring(map.staggerX) .. ' ' .. tostring(map.staggerEven) .. ' ' .. map.parallaxOrigin.x .. ' ' .. tostring(endless.infinite)"), "false right-down 0 false false 0.0 true");
    EXPECT_EQ(fixture.lua("local types = map.propertyTypes return types.spawn .. ' ' .. tostring(types.title)"), "Spawn nil");
    EXPECT_EQ(fixture.lua("local loot = map.properties.loot return #loot .. ' ' .. loot[1] .. ' ' .. loot[2]:toHex() .. ' ' .. loot[3] .. ' ' .. tostring(loot[4][1]) .. ' ' .. loot[4][2] .. ' ' .. loot[5].kind"), "5 3 #FF00FF00 audio/coin.ogg true maps/gem.png bat");

    EXPECT_EQ(fixture.lua("local t = map:tilesets()[1] return t.path .. ' ' .. t.margin .. ' ' .. t.spacing .. ' ' .. t.tileOffset.x .. ' ' .. t.objectAlignment .. ' ' .. t.imageSize.x .. 'x' .. t.imageSize.y .. ' ' .. tostring(t.transparentColor) .. ' ' .. tostring(t.renderGridSize) .. ' ' .. tostring(t.preserveAspect) .. ' ' .. tostring(t.texture == assets.texture('maps/images/terrain.png'))"), "maps/tiles/terrain.tsj 0 0 0.0 unspecified 64.0x32.0 nil false false true");
    EXPECT_EQ(fixture.lua("local t = map:tilesets()[2] return t.path .. '|' .. t.objectAlignment .. ' ' .. t.image .. '|' .. tostring(t.texture)"), "|bottom |nil");
    EXPECT_EQ(fixture.lua("local set = map:tilesets()[1].wangSets[1] local color = set.colors[1] return set.type .. ' ' .. set.tile .. ' ' .. tostring(set.properties.wet) .. ' ' .. color.type .. ' ' .. color.probability .. ' ' .. string.format('%.1f', color.properties.speed) .. ' ' .. tostring(next(color.propertyTypes))"), "shore 0 true ground 0.5 0.8 nil");
    EXPECT_EQ(fixture.lua("local t = map:tileInfo(100) local rock = map:tileInfo(2) return t.image .. ' ' .. t.probability .. ' ' .. rock.image .. '|' .. rock.probability"), "maps/props/tree.png 1.0 |1.0");

    EXPECT_EQ(fixture.lua("return tostring(map:layer('things').indexDrawOrder) .. ' ' .. #map:layer('ground').chunks .. ' ' .. map:layer('sky').imageSize.x .. ' ' .. map:layer('sky').transparentColor:toHex() .. ' ' .. tostring(map:layer('things').objects[1].propertyTypes.hp)"), "false 0 8.0 #FFFF00FF nil");
    EXPECT_EQ(fixture.lua("local chunks = endless:layer('ground').chunks return #chunks .. ' ' .. chunks[1].x .. ',' .. chunks[1].y .. ' ' .. chunks[1].width .. 'x' .. chunks[1].height .. ' ' .. table.concat(chunks[1].gids, ',') .. ' ' .. chunks[2].gids[4]"), "2 -2,0 2x2 1,2,3,4 8");

    EXPECT_EQ(fixture.lua("return string.format('%X %X %X %X %X', tiled.flipHorizontal, tiled.flipVertical, tiled.flipDiagonal, tiled.rotateHexagonal, tiled.flagMask)"), "80000000 40000000 20000000 10000000 F0000000");
    EXPECT_EQ(fixture.lua("return tiled.tileId(5 | tiled.flipHorizontal | tiled.flipDiagonal) .. ' ' .. tiled.tileId(7)"), "5 7");
}

} // namespace haylen
