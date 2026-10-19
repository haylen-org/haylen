#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::tiled {

// Applies the automapping rules of Tiled to a map at run time, the way the editor does, so a level painted with plain tiles, or made by code, gets its borders, corners and decorations. A rule map holds `input`, `inputnot` and `output` layers, optionally with an index, the special tiles of the automapping rules tileset by their `MatchType`, a `rule_options` layer and the map properties `DeleteTiles`, `MatchOutsideMap`, `OverflowBorder`, `WrapBorder`, `MatchInOrder` and `NoOverlappingOutput`, and a rules file lists rule maps with filters of map names.
class AutoMapper final {
  public:
    // Reads the text of a file of the package by its resolved path.
    using TextReader = std::function<std::string(const std::string& path)>;

    // What one call changed: the ids of the layers it wrote or created, the tilesets it added to the map and how many times rules matched.
    struct Result {
        std::vector<std::uint32_t> layers;
        std::vector<std::uint32_t> objectLayers;
        std::size_t matches = 0;
    };

    // A rule map with the filter of map names that the rules file gave it, empty for every map.
    struct RuleMap {
        std::shared_ptr<Map> map;
        std::string filter;
    };

    explicit AutoMapper(std::vector<RuleMap> ruleMaps);

    // Loads a rules file, whose lines name rule maps, other rules files and `[filters]`, or one rule map, by its path inside the package content folder. Throws `std::invalid_argument` for rule maps without input or output layers and for rule maps of the legacy `regions` layers.
    [[nodiscard]] static AutoMapper load(std::string_view path, const TextReader& readText, const Map::JsonReader& readJson);

    // Tells whether a filter of a rules file, where `*` stands for any text, matches the file name of a map.
    [[nodiscard]] static bool matchesFilter(std::string_view filter, std::string_view mapPath) noexcept;

    [[nodiscard]] const std::vector<RuleMap>& getRuleMaps() const noexcept {
        return ruleMaps;
    }

    // Applies every rule map whose filter matches the map, in order, so later rule maps see what earlier ones wrote. The random generator picks the outputs and the rules with a probability.
    Result apply(Map& map, math::Random& random) const;

  private:
    // The flips and rotation a layer of input ignores when it compares tiles, as gid flag bits.
    static constexpr std::uint32_t kAllFlags = Map::kFlagMask;

    // The global id of an input tile whose tileset the map lacks, which no cell holds.
    static constexpr std::uint32_t kMissingTile = 0xFFFFFFFFU;

    enum class Match : std::uint8_t {
        Tile,
        Empty,
        Ignore,
        NonEmpty,
        Other,
        Negate,
    };

    struct Cell {
        int x = 0;
        int y = 0;

        friend bool operator==(const Cell&, const Cell&) = default;
    };

    struct Entry {
        Match match = Match::Tile;
        std::uint32_t gid = 0;
        std::uint32_t ignoredFlags = 0;
    };

    // The condition of one target layer for one input index at one cell: the tiles one of which must be there, and the tiles none of which may be.
    struct CellCondition {
        Cell cell;
        std::vector<Entry> wanted;
        std::vector<Entry> unwanted;
        bool negated = false;
    };

    struct TargetCondition {
        std::string layer;
        std::vector<CellCondition> cells;
        std::vector<std::uint32_t> used;
        bool usesEmpty = false;
    };

    struct InputIndex {
        std::string index;
        std::vector<TargetCondition> targets;
    };

    struct OutputLayer {
        std::string layer;
        bool objects = false;
        std::vector<std::pair<Cell, Entry>> cells;
        std::vector<Object> placed;
        Properties properties;
    };

    struct OutputIndex {
        std::string index;
        float probability = 1.0F;
        std::vector<OutputLayer> layers;
    };

    struct Options {
        int modX = 1;
        int modY = 1;
        int offsetX = 0;
        int offsetY = 0;
        float probability = 1.0F;
        bool disabled = false;
        bool noOverlappingOutput = false;
    };

    struct Rule {
        Cell origin;
        std::vector<Cell> region;
        std::vector<Cell> inputCells;
        std::vector<InputIndex> inputs;
        std::vector<OutputIndex> outputs;
        Options options;
    };

    struct CompiledMap {
        std::shared_ptr<Map> source;
        std::string filter;
        std::vector<Rule> rules;
        bool deleteTiles = false;
        bool matchOutsideMap = false;
        bool overflowBorder = false;
        bool wrapBorder = false;
        bool matchInOrder = false;
    };

    // The working map with its edits counted.
    struct Target;

    static void collectLayers(std::vector<Layer>& layers, std::vector<Layer*>& found);
    static void loadRules(std::string_view path, const TextReader& readText, const Map::JsonReader& readJson, std::string filter, std::vector<RuleMap>& found, int depth);
    [[nodiscard]] static CompiledMap compile(const RuleMap& ruleMap);
    [[nodiscard]] static Match matchOf(const Map& map, std::uint32_t gid);
    [[nodiscard]] static Cell objectCell(const Object& object, math::Vec2 tileSize) noexcept;
    [[nodiscard]] static Options readOptions(const Properties& properties, Options defaults);
    [[nodiscard]] static bool matches(const Entry& entry, std::uint32_t gid, const TargetCondition& target) noexcept;
    [[nodiscard]] static bool satisfies(const CompiledMap& compiled, const Rule& rule, const Target& target, Cell offset);

    [[nodiscard]] static CompiledMap translateInputs(const CompiledMap& compiled, const Target& target);
    void applyMap(const CompiledMap& source, Target& target, math::Random& random) const;
    static void write(const CompiledMap& compiled, const Rule& rule, Target& target, Cell offset, math::Random& random, std::vector<std::pair<std::string, Cell>>& written);

    static constexpr int kMaxRulesDepth = 16;

    std::vector<RuleMap> ruleMaps;
    std::vector<CompiledMap> compiled;
};

} // namespace haylen::tiled
