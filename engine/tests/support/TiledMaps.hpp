#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include <zlib.h>
#include <zstd.h>

#include "haylen/2d/tiled/Map.hpp"
#include "haylen/core/Json.hpp"

namespace haylen::test {

// The maps, tilesets, templates and projects of Tiled that the tests of maps in C++ and in Lua load, with the encoders of the tile data of a layer.
class TiledMaps final {
  public:
    [[nodiscard]] static std::string base64(const std::vector<std::uint8_t>& bytes) {
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
    [[nodiscard]] static std::string encodeTiles(const std::vector<std::uint32_t>& gids, const std::string& compression) {
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
    [[nodiscard]] static haylen::core::Json orthogonalMap() {
        const std::vector<std::uint32_t> ground{1, 2, 1, 2, 3, 3, 3, 3, 5, 0, 5, 5};
        // clang-format off
        return haylen::core::Json::parse(R"({
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
            {"id": 5, "name": "things", "type": "objectgroup", "draworder": "topdown", "color": "#FFA0C040", "class": "entities", "objects": [
                {"id": 1, "name": "spawn", "type": "player", "x": 10, "y": 20, "point": true, "properties": [{"name": "hp", "type": "int", "value": 3}]},
                {"id": 2, "name": "zone", "type": "collision", "x": 0, "y": 0, "width": 8, "height": 8, "rotation": 90, "properties": [{"name": "sensor", "type": "bool", "value": true}]},
                {"id": 3, "name": "pond", "type": "collision", "x": 20, "y": 20, "width": 10, "height": 6, "ellipse": true},
                {"id": 4, "name": "wall", "type": "collision", "x": 30, "y": 0, "polygon": [{"x": 0, "y": 0}, {"x": 10, "y": 0}, {"x": 5, "y": 5}]},
                {"id": 5, "name": "fence", "type": "collision", "x": 0, "y": 40, "polyline": [{"x": 0, "y": 0}, {"x": 10, "y": 0}, {"x": 10, "y": 10}]},
                {"id": 6, "name": "sign", "x": 5, "y": 5, "width": 40, "height": 20, "text": {"text": "Hi", "pixelsize": 12, "wrap": true, "color": "#FF00FF00", "halign": "center", "valign": "bottom", "bold": true, "underline": true, "strikeout": true, "kerning": false}},
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
        // clang-format on
    }

    [[nodiscard]] static haylen::core::Json terrainTileset() {
        // clang-format off
        return haylen::core::Json::parse(R"({
        "type": "tileset", "name": "terrain", "tilewidth": 16, "tileheight": 16, "tilecount": 8, "columns": 4,
        "image": "../images/terrain.png", "imagewidth": 64, "imageheight": 32, "margin": 0, "spacing": 0,
        "tileoffset": {"x": 0, "y": 0}, "properties": [{"name": "theme", "type": "string", "value": "grass"}],
        "grid": {"orientation": "isometric", "width": 32, "height": 16}, "transformations": {"hflip": true, "vflip": false, "rotate": true, "preferuntransformed": true},
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
        // clang-format on
    }

    [[nodiscard]] static haylen::core::Json rockTemplate() {
        // clang-format off
        return haylen::core::Json::parse(R"({"type": "template", "tileset": {"firstgid": 1, "source": "../tiles/terrain.tsj"},
        "object": {"name": "rock", "type": "rock", "width": 16, "height": 16, "gid": 5, "properties": [{"name": "size", "type": "int", "value": 1}, {"name": "hard", "type": "bool", "value": true}]}})");
        // clang-format on
    }

    // The test map as an infinite map whose ground layer is stored in two chunks, one of them left of the origin.
    [[nodiscard]] static haylen::core::Json infiniteMap() {
        haylen::core::Json map = orthogonalMap();
        map["infinite"] = true;
        // clang-format off
        map["layers"] = haylen::core::Json::parse(R"([{"id": 1, "name": "ground", "type": "tilelayer", "width": 4, "height": 2, "startx": -2, "starty": 0,
        "chunks": [{"x": -2, "y": 0, "width": 2, "height": 2, "data": [1, 2, 3, 4]}, {"x": 0, "y": 0, "width": 2, "height": 2, "data": [5, 6, 7, 8]}]}])");
        // clang-format on
        return map;
    }

    [[nodiscard]] static haylen::tiled::Map::JsonReader reader() {
        // clang-format off
        return [](const std::string& path) {
            if (path == "maps/tiles/terrain.tsj") {
                return terrainTileset();
            }

            if (path == "maps/templates/rock.tj") {
                return rockTemplate();
            }

            throw std::runtime_error("The file \"" + path + "\" was read unexpectedly.");
        };
        // clang-format on
    }

    // A rule map that shades every ground tile of the island with the terrain tileset of the island, matching with the special tiles of automapping, whose image is an image of the editor that the package lacks.
    [[nodiscard]] static haylen::core::Json automapRules() {
        // clang-format off
        return haylen::core::Json::parse(R"({"type": "map", "orientation": "orthogonal", "renderorder": "right-down", "width": 1, "height": 1, "tilewidth": 16, "tileheight": 16,
            "tilesets": [{"firstgid": 1, "source": "tiles/terrain.tsj"}, {"firstgid": 101, "name": "automap", "tilewidth": 16, "tileheight": 16, "tilecount": 1, "columns": 1, "image": ":/automap-tiles.png", "imagewidth": 16, "imageheight": 16,
                "tiles": [{"id": 0, "properties": [{"name": "MatchType", "type": "string", "value": "NonEmpty"}]}]}],
            "layers": [{"id": 1, "type": "tilelayer", "name": "input_ground", "width": 1, "height": 1, "data": [101]}, {"id": 2, "type": "tilelayer", "name": "output_shade", "width": 1, "height": 1, "data": [3]}]})");
        // clang-format on
    }

    // The classes of the project of the island: the class of the map, the spawn of its properties and the reward each spawn holds, of which the map writes only what differs.
    [[nodiscard]] static haylen::core::Json gameProject() {
        // clang-format off
        return haylen::core::Json::parse(R"({"propertyTypes": [
            {"id": 1, "name": "level", "type": "class", "useAs": ["map"], "members": [{"name": "difficulty", "type": "int", "value": 1}, {"name": "title", "type": "string", "value": "Untitled"}]},
            {"id": 2, "name": "Spawn", "type": "class", "useAs": ["property"], "members": [{"name": "count", "type": "int", "value": 1}, {"name": "kind", "type": "string", "value": "slime"}, {"name": "reward", "type": "class", "propertyType": "Reward", "value": {}}]},
            {"id": 3, "name": "Reward", "type": "class", "useAs": ["property"], "members": [{"name": "coins", "type": "int", "value": 5}, {"name": "gem", "type": "bool", "value": false}]},
            {"id": 4, "name": "Kind", "type": "enum", "storageType": "string", "values": ["slime", "goblin"]}
        ]})");
        // clang-format on
    }

    [[nodiscard]] static std::map<std::string, std::string> packageFiles() {
        // clang-format off
        const auto png = [](int width, int height, std::uint32_t color) {
            const std::vector<std::uint8_t> image = test::TestFiles::pngImage(width, height, color);
            return std::string(image.begin(), image.end());
        };
        // clang-format on
        std::string map = orthogonalMap().dump();
        return {
            {"content/maps/island.tmj", map}, {"content/maps/tiles/terrain.tsj", terrainTileset().dump()}, {"content/maps/templates/rock.tj", rockTemplate().dump()}, {"content/maps/images/terrain.png", png(64, 32, 0xFFFFFFFFU)}, {"content/maps/props/tree.png", png(16, 32, 0x00FF00FFU)}, {"content/maps/props/rock.png", png(16, 16, 0x808080FFU)}, {"content/maps/backgrounds/sky.png", png(8, 8, 0xFF00FFFFU)}, {"content/maps/infinite.tmj", infiniteMap().dump()}, {"content/maps/game.tiled-project", gameProject().dump()}, {"content/maps/automap.tmj", automapRules().dump()}, {"content/maps/rules.txt", "# Shade every ground tile\nautomap.tmj\n"}, {"content/maps/world/level_0_0.tmj", map}, {"content/maps/world/level_1_0.tmj", map}, {"content/maps/world/level.world", R"({"type": "world", "maps": [{"fileName": "../island.tmj", "x": -64, "y": 0, "width": 64, "height": 48}], "patterns": [{"regexp": "level_(\\d+)_(\\d+)\\.tmj", "multiplierX": 64, "multiplierY": 48, "offsetX": 0, "offsetY": 0, "mapWidth": 64, "mapHeight": 48}]})"},
        };
    }

    // The test map skewed into an oblique map, with Tiled 1.12 blend modes, a capsule and a faded tree.
    [[nodiscard]] static haylen::core::Json obliqueMap() {
        haylen::core::Json map = orthogonalMap();
        map["orientation"] = "oblique";
        map["skewx"] = 8;
        map["skewy"] = 4;
        map["layers"][0]["mode"] = "multiply";
        map["layers"][1]["mode"] = "add";
        map["layers"][4]["mode"] = "screen";
        map["layers"][5]["visible"] = false;
        map["layers"][4]["objects"][6]["opacity"] = 0.25;
        map["layers"][4]["objects"].push_back(haylen::core::Json::parse(R"({"id": 10, "name": "log", "type": "collision", "x": 100, "y": 0, "width": 40, "height": 10, "capsule": true})"));
        return map;
    }
};

} // namespace haylen::test
