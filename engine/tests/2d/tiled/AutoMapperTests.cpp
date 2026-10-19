#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "haylen/2d/tiled/AutoMapper.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::tiled {

namespace {

// Small maps of a terrain tileset, whose tiles 1 to 8 are grass, edge, flower, rock, sand, water, mark and tree, and rule maps that also hold the special tiles of automapping from 101: empty, ignore, non-empty, other and negate.
class AutoMapperTest : public ::testing::Test {
  protected:
    static constexpr std::uint32_t kGrass = 1;
    static constexpr std::uint32_t kEdge = 2;
    static constexpr std::uint32_t kFlower = 3;
    static constexpr std::uint32_t kRock = 4;
    static constexpr std::uint32_t kSand = 5;
    static constexpr std::uint32_t kWater = 6;
    static constexpr std::uint32_t kMark = 7;
    static constexpr std::uint32_t kEmpty = 101;
    static constexpr std::uint32_t kIgnore = 102;
    static constexpr std::uint32_t kNonEmpty = 103;
    static constexpr std::uint32_t kOther = 104;
    static constexpr std::uint32_t kNegate = 105;

    [[nodiscard]] static core::Json layer(std::string name, int width, int height, std::vector<std::uint32_t> data, core::Json properties = core::Json::array()) {
        return {{"type", "tilelayer"}, {"name", std::move(name)}, {"width", width}, {"height", height}, {"data", std::move(data)}, {"properties", std::move(properties)}};
    }

    [[nodiscard]] static core::Json special(const char* type) {
        return {{"name", "MatchType"}, {"type", "string"}, {"value", type}};
    }

    [[nodiscard]] static core::Json document(int width, int height, core::Json layers, core::Json properties = core::Json::array()) {
        core::Json tilesets = core::Json::array();
        tilesets.push_back({{"firstgid", 1}, {"name", "terrain"}, {"tilewidth", 16}, {"tileheight", 16}, {"tilecount", 8}, {"columns", 8}, {"image", "terrain.png"}, {"imagewidth", 128}, {"imageheight", 16}});
        core::Json specials = core::Json::array();
        int id = 0;
        for (const char* type : {"Empty", "Ignore", "NonEmpty", "Other", "Negate"}) {
            specials.push_back({{"id", id++}, {"properties", core::Json::array({special(type)})}});
        }
        tilesets.push_back({{"firstgid", 101}, {"name", "automap"}, {"tilewidth", 16}, {"tileheight", 16}, {"tilecount", 5}, {"columns", 5}, {"image", "automap.png"}, {"imagewidth", 80}, {"imageheight", 16}, {"tiles", specials}});
        std::uint32_t layerId = 1;
        for (core::Json& entry : layers) {
            entry["id"] = layerId++;
        }
        return {{"type", "map"}, {"orientation", "orthogonal"}, {"renderorder", "right-down"}, {"width", width}, {"height", height}, {"tilewidth", 16}, {"tileheight", 16}, {"tilesets", tilesets}, {"layers", std::move(layers)}, {"properties", std::move(properties)}};
    }

    [[nodiscard]] static Map parse(const core::Json& json, const std::string& path = "maps/level.tmj") {
        return Map::parse(json, path, [](const std::string& file) -> core::Json { throw std::runtime_error("The file \"" + file + "\" is not here."); });
    }

    [[nodiscard]] static AutoMapper mapper(const core::Json& rules, std::string filter = {}) {
        return AutoMapper({{.map = std::make_shared<Map>(parse(rules, "maps/rule.tmj")), .filter = std::move(filter)}});
    }

    [[nodiscard]] static std::vector<std::uint32_t> row(const Map& map, std::string_view name, int y) {
        std::vector<std::uint32_t> cells;
        const Layer* found = map.findLayer(name);
        for (int x = 0; x < map.width; ++x) {
            cells.push_back(found != nullptr ? found->getGid(x, y) : 0U);
        }
        return cells;
    }

    math::Random random{7};
};

} // namespace

// A rule writes its output wherever its input matches, onto an output layer it creates when the map lacks it.
TEST_F(AutoMapperTest, WritesTheOutputWhereTheInputMatches) {
    Map map = parse(document(5, 1, core::Json::array({layer("ground", 5, 1, {kGrass, 0, kGrass, kGrass, kRock})})));
    const AutoMapper rules = mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output_decor", 1, 1, {kFlower})})));
    const AutoMapper::Result result = rules.apply(map, random);
    EXPECT_EQ(result.matches, 3U);
    EXPECT_EQ(row(map, "decor", 0), (std::vector<std::uint32_t>{kFlower, 0, kFlower, kFlower, 0}));
    EXPECT_EQ(result.layers.size(), 1U);
}

// The special tiles match an empty cell, any tile, and any tile the rule does not use, and the empty tile also erases.
TEST_F(AutoMapperTest, MatchesTheSpecialTiles) {
    Map map = parse(document(5, 1, core::Json::array({layer("ground", 5, 1, {kGrass, kGrass, kGrass, 0, kRock})})));
    // A grass cell whose right neighbour is empty becomes an edge.
    mapper(document(2, 1, core::Json::array({layer("input_ground", 2, 1, {kGrass, kEmpty}), layer("output_ground", 2, 1, {kEdge, 0})}))).apply(map, random);
    EXPECT_EQ(row(map, "ground", 0), (std::vector<std::uint32_t>{kGrass, kGrass, kEdge, 0, kRock}));

    // Any tile next to an edge is marked, and a tile that is neither grass nor edge, the tiles the rule uses, is erased, while the empty cell stays empty.
    mapper(document(2, 1, core::Json::array({layer("input_ground", 2, 1, {kNonEmpty, kEdge}), layer("output_marks", 2, 1, {kMark, 0})}))).apply(map, random);
    EXPECT_EQ(row(map, "marks", 0), (std::vector<std::uint32_t>{0, kMark, 0, 0, 0}));
    mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kOther}), layer("inputnot_ground", 1, 1, {kGrass}), layer("inputnot_ground", 1, 1, {kEdge}), layer("output_ground", 1, 1, {kEmpty})}))).apply(map, random);
    EXPECT_EQ(row(map, "ground", 0), (std::vector<std::uint32_t>{kGrass, kGrass, kEdge, 0, 0}));
}

// An inverted input forbids its tiles, a negated cell turns the conditions around, and inputs of different indexes are alternatives.
TEST_F(AutoMapperTest, CombinesInvertedNegatedAndAlternativeInputs) {
    Map map = parse(document(4, 1, core::Json::array({layer("ground", 4, 1, {kGrass, kRock, kSand, kWater})})));
    mapper(document(1, 1, core::Json::array({layer("inputnot_ground", 1, 1, {kGrass}), layer("output_marks", 1, 1, {kMark})}))).apply(map, random);
    EXPECT_EQ(row(map, "marks", 0), (std::vector<std::uint32_t>{0, kMark, kMark, kMark}));

    mapper(document(1, 1, core::Json::array({layer("input1_ground", 1, 1, {kRock}), layer("input2_ground", 1, 1, {kWater}), layer("output_decor", 1, 1, {kFlower})}))).apply(map, random);
    EXPECT_EQ(row(map, "decor", 0), (std::vector<std::uint32_t>{0, kFlower, 0, kFlower}));

    mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kSand}), layer("input_ground", 1, 1, {kNegate}), layer("output_trees", 1, 1, {kMark})}))).apply(map, random);
    EXPECT_EQ(row(map, "trees", 0), (std::vector<std::uint32_t>{kMark, kMark, 0, kMark}));
}

// One indexed output is picked at random by its probability, and outputs without an index always write.
TEST_F(AutoMapperTest, PicksOneIndexedOutputByItsProbability) {
    Map map = parse(document(64, 1, core::Json::array({layer("ground", 64, 1, std::vector<std::uint32_t>(64, kGrass))})));
    const core::Json heavy = core::Json::array({{{"name", "Probability"}, {"type", "float"}, {"value", 3.0}}});
    mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output1_decor", 1, 1, {kFlower}), layer("output2_decor", 1, 1, {kRock}, heavy), layer("output_marks", 1, 1, {kMark})}))).apply(map, random);
    std::map<std::uint32_t, int> counts;
    for (const std::uint32_t gid : row(map, "decor", 0)) {
        ++counts[gid];
    }
    EXPECT_EQ(counts[kFlower] + counts[kRock], 64);
    EXPECT_GT(counts[kRock], counts[kFlower]);
    EXPECT_GT(counts[kFlower], 0);
    EXPECT_EQ(row(map, "marks", 0), std::vector<std::uint32_t>(64, kMark));
}

// Rules match the map as it was unless the rule map matches in order, where each rule sees what the rules before it wrote.
TEST_F(AutoMapperTest, MatchesInOrderOnlyWhenAsked) {
    const auto rules = [](core::Json properties) { return document(3, 1, core::Json::array({layer("input_ground", 3, 1, {kGrass, 0, kSand}), layer("output_ground", 3, 1, {kSand, 0, kWater})}), std::move(properties)); };
    Map concurrent = parse(document(2, 1, core::Json::array({layer("ground", 2, 1, {kGrass, kRock})})));
    mapper(rules(core::Json::array())).apply(concurrent, random);
    EXPECT_EQ(row(concurrent, "ground", 0), (std::vector<std::uint32_t>{kSand, kRock}));

    Map ordered = parse(document(2, 1, core::Json::array({layer("ground", 2, 1, {kGrass, kRock})})));
    mapper(rules(core::Json::array({{{"name", "MatchInOrder"}, {"type", "bool"}, {"value", true}}}))).apply(ordered, random);
    EXPECT_EQ(row(ordered, "ground", 0), (std::vector<std::uint32_t>{kWater, kRock}));
}

// Options place a rule every few cells and keep its outputs apart, and the border of the map matches outside it when asked.
TEST_F(AutoMapperTest, PlacesRulesByTheirOptionsAndTheBorderOfTheMap) {
    Map map = parse(document(6, 1, core::Json::array({layer("ground", 6, 1, std::vector<std::uint32_t>(6, kGrass))})));
    const core::Json every = core::Json::array({{{"name", "ModX"}, {"type", "int"}, {"value", 2}}, {{"name", "OffsetX"}, {"type", "int"}, {"value", 1}}});
    mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output_decor", 1, 1, {kFlower})}), every)).apply(map, random);
    EXPECT_EQ(row(map, "decor", 0), (std::vector<std::uint32_t>{0, kFlower, 0, kFlower, 0, kFlower}));

    // A pair of cells overlaps the one before it, so keeping outputs apart writes every other pair.
    const core::Json apart = core::Json::array({{{"name", "NoOverlappingOutput"}, {"type", "bool"}, {"value", true}}});
    mapper(document(2, 1, core::Json::array({layer("input_ground", 2, 1, {kGrass, kGrass}), layer("output_marks", 2, 1, {kMark, kMark})}), apart)).apply(map, random);
    EXPECT_EQ(row(map, "marks", 0), std::vector<std::uint32_t>(6, kMark));

    // The left edge has an empty cell outside it, and a wrapping border has the right edge there.
    const core::Json edge = document(2, 1, core::Json::array({layer("input_ground", 2, 1, {kEmpty, kGrass}), layer("output_edges", 2, 1, {0, kEdge})}), core::Json::array({{{"name", "MatchOutsideMap"}, {"type", "bool"}, {"value", true}}}));
    mapper(edge).apply(map, random);
    EXPECT_EQ(row(map, "edges", 0), (std::vector<std::uint32_t>{kEdge, 0, 0, 0, 0, 0}));
    Map wrapped = parse(document(3, 1, core::Json::array({layer("ground", 3, 1, {kGrass, kGrass, 0})})));
    core::Json wrapping = edge;
    wrapping["properties"] = core::Json::array({{{"name", "WrapBorder"}, {"type", "bool"}, {"value", true}}});
    mapper(wrapping).apply(wrapped, random);
    EXPECT_EQ(row(wrapped, "edges", 0), (std::vector<std::uint32_t>{kEdge, 0, 0}));
}

// Deleting tiles clears the output layers where the input lies before the rule writes, and objects of an output layer are placed with the rule.
TEST_F(AutoMapperTest, DeletesTilesAndPlacesObjects) {
    Map map = parse(document(3, 1, core::Json::array({layer("ground", 3, 1, {kGrass, kRock, kGrass}), layer("decor", 3, 1, {kFlower, kFlower, kFlower})})));
    core::Json rules = document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output_decor", 1, 1, {kIgnore})}), core::Json::array({{{"name", "DeleteTiles"}, {"type", "bool"}, {"value", true}}}));
    rules["layers"].push_back({{"id", 9}, {"type", "objectgroup"}, {"name", "output_spawns"}, {"objects", core::Json::array({{{"id", 1}, {"name", "slime"}, {"x", 4}, {"y", 8}, {"width", 0}, {"height", 0}, {"point", true}}, {{"id", 2}, {"name", "post"}, {"gid", kMark}, {"x", 0}, {"y", 16}, {"width", 16}, {"height", 16}}})}});
    const AutoMapper::Result result = mapper(rules).apply(map, random);
    EXPECT_EQ(row(map, "decor", 0), (std::vector<std::uint32_t>{0, kFlower, 0}));
    const Layer* spawns = map.findLayer("spawns");
    ASSERT_NE(spawns, nullptr);
    // The tile object stands on the bottom edge of the cell of the rule, so it belongs to the rule and keeps its tile.
    ASSERT_EQ(spawns->objects.size(), 4U);
    EXPECT_EQ(spawns->objects[2].position, math::Vec2(36.0F, 8.0F));
    EXPECT_EQ(spawns->objects[3].position, math::Vec2(32.0F, 16.0F));
    EXPECT_EQ(spawns->objects[3].gid, kMark);
    EXPECT_NE(spawns->objects[0].id, spawns->objects[2].id);
    EXPECT_EQ(result.objectLayers.size(), 1U);
}

// A tile from a tileset the map lacks brings that tileset along after the tilesets of the map.
TEST_F(AutoMapperTest, BringsTheTilesetOfAnOutputTheMapLacks) {
    core::Json lonely = document(2, 1, core::Json::array({layer("ground", 2, 1, {kGrass, 0})}));
    lonely["tilesets"][0]["name"] = "meadow";
    lonely["tilesets"][0]["image"] = "meadow.png";
    Map map = parse(lonely);
    mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kNonEmpty}), layer("output_decor", 1, 1, {kRock})}))).apply(map, random);
    ASSERT_EQ(map.tilesets.size(), 3U);
    EXPECT_EQ(map.tilesets[2].tileset->name, "terrain");
    EXPECT_EQ(map.findLayer("decor")->getGid(0, 0), map.tilesets[2].firstGid + kRock - 1);
}

// Each rule takes only the input layers that hold its tiles, so the input layers of other rules, of other indexes, never let it match everywhere.
TEST_F(AutoMapperTest, KeepsTheInputsOfEachRuleToItsOwnCells) {
    Map map = parse(document(3, 1, core::Json::array({layer("ground", 3, 1, {kGrass, kRock, kSand})})));
    const AutoMapper rules = mapper(document(3, 1, core::Json::array({layer("input1_ground", 3, 1, {kGrass, 0, 0}), layer("input2_ground", 3, 1, {0, 0, kSand}), layer("output_marks", 3, 1, {kMark, 0, kWater})})));
    EXPECT_EQ(rules.apply(map, random).matches, 2U);
    EXPECT_EQ(row(map, "marks", 0), (std::vector<std::uint32_t>{kMark, 0, kWater}));
}

// A rule map names its tiles by its own tilesets, so a map whose tilesets start at other global ids matches the same tiles and writes them in its own ids.
TEST_F(AutoMapperTest, MatchesTilesByTheirTilesetsWhateverTheirGlobalIds) {
    core::Json shifted = document(3, 1, core::Json::array({layer("ground", 3, 1, {kGrass + 20, kRock + 20, kGrass + 20})}));
    shifted["tilesets"][0]["firstgid"] = 21;
    shifted["tilesets"].push_back({{"firstgid", 1}, {"name", "extra"}, {"tilewidth", 16}, {"tileheight", 16}, {"tilecount", 20}, {"columns", 20}, {"image", "extra.png"}, {"imagewidth", 320}, {"imageheight", 16}});
    Map map = parse(shifted);
    const AutoMapper::Result result = mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output_decor", 1, 1, {kFlower})}))).apply(map, random);
    EXPECT_EQ(result.matches, 2U);
    EXPECT_EQ(row(map, "decor", 0), (std::vector<std::uint32_t>{kFlower + 20, 0, kFlower + 20}));
}

// Rule maps list their filters of map names, and rule maps without input or output, with the legacy regions or with an unknown special tile are refused.
TEST_F(AutoMapperTest, FiltersMapsAndRefusesBrokenRuleMaps) {
    EXPECT_TRUE(AutoMapper::matchesFilter("town*", "maps/town-hall.tmj"));
    EXPECT_TRUE(AutoMapper::matchesFilter("*hall*", "maps/town-hall.tmj"));
    EXPECT_TRUE(AutoMapper::matchesFilter("*", "maps/cave.tmj"));
    EXPECT_FALSE(AutoMapper::matchesFilter("town*", "maps/cave.tmj"));
    EXPECT_FALSE(AutoMapper::matchesFilter("*.tmx", "maps/cave.tmj"));

    Map cave = parse(document(1, 1, core::Json::array({layer("ground", 1, 1, {kGrass})})), "maps/cave.tmj");
    EXPECT_EQ(mapper(document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output_decor", 1, 1, {kFlower})})), "town*").apply(cave, random).matches, 0U);

    EXPECT_THROW((void)mapper(document(1, 1, core::Json::array({layer("output_decor", 1, 1, {kFlower})}))), std::invalid_argument);
    EXPECT_THROW((void)mapper(document(1, 1, core::Json::array({layer("regions", 1, 1, {kGrass}), layer("input_ground", 1, 1, {kGrass}), layer("output_decor", 1, 1, {kFlower})}))), std::invalid_argument);
    core::Json strange = document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kEmpty}), layer("output_decor", 1, 1, {kFlower})}));
    strange["tilesets"][1]["tiles"][0]["properties"][0]["value"] = "Maybe";
    EXPECT_THROW((void)mapper(strange), std::invalid_argument);
}

// A rules file lists rule maps and other rules files relative to itself, with comments and filters that last until the next filter.
TEST_F(AutoMapperTest, LoadsRulesFilesWithFilters) {
    const std::map<std::string, std::string> texts{
        {"rules/rules.txt", "# Rules of the world\nedges.tmj\n[town*]\n  more/rules.txt\n"},
        {"rules/more/rules.txt", "// Town only\n../decor.tmj\n"},
    };
    // The rule maps sit in a folder of their own and reach the image of the terrain of the maps from there.
    std::map<std::string, core::Json> maps{
        {"rules/edges.tmj", document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output_edges", 1, 1, {kEdge})}))},
        {"rules/decor.tmj", document(1, 1, core::Json::array({layer("input_ground", 1, 1, {kGrass}), layer("output_decor", 1, 1, {kFlower})}))},
    };
    for (auto& [path, rules] : maps) {
        rules["tilesets"][0]["image"] = "../maps/terrain.png";
    }
    const AutoMapper loaded = AutoMapper::load("rules/rules.txt", [&texts](const std::string& path) { return texts.at(path); }, [&maps](const std::string& path) { return maps.at(path); });
    ASSERT_EQ(loaded.getRuleMaps().size(), 2U);
    EXPECT_EQ(loaded.getRuleMaps()[1].filter, "town*");

    Map town = parse(document(1, 1, core::Json::array({layer("ground", 1, 1, {kGrass})})), "maps/town-square.tmj");
    Map cave = parse(document(1, 1, core::Json::array({layer("ground", 1, 1, {kGrass})})), "maps/cave.tmj");
    EXPECT_EQ(loaded.apply(town, random).matches, 2U);
    EXPECT_EQ(loaded.apply(cave, random).matches, 1U);
    EXPECT_EQ(cave.findLayer("decor"), nullptr);
}

} // namespace haylen::tiled
