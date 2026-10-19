#include "haylen/2d/tiled/AutoMapper.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <utility>

#include "2d/tiled/MapParser.hpp"
#include "haylen/io/Path.hpp"

namespace haylen::tiled {

struct AutoMapper::Target {
    Map& map;
    Result& result;
    std::uint32_t nextLayerId = 1;
    std::uint32_t nextObjectId = 1;
    std::map<const Tileset*, std::uint32_t> firstGids;

    // The layers by name and kind, found once, until a new layer moves them.
    mutable std::map<std::pair<std::string, Layer::Kind>, Layer*, std::less<>> layers;

    // The bottom-most layer of the name and kind, which a rule reads and writes.
    [[nodiscard]] Layer* findLayer(std::string_view name, Layer::Kind kind) const {
        const auto cached = layers.find(std::pair{std::string(name), kind});
        if (cached != layers.end()) {
            return cached->second;
        }
        std::vector<Layer*> all;
        AutoMapper::collectLayers(map.layers, all);
        const auto found = std::ranges::find_if(all, [&](const Layer* layer) { return layer->name == name && layer->kind == kind; });
        Layer* layer = found != all.end() ? *found : nullptr;
        layers.emplace(std::pair{std::string(name), kind}, layer);
        return layer;
    }

    // A missing output layer joins on top of the map, as large as a finite map.
    [[nodiscard]] Layer& requireLayer(std::string_view name, Layer::Kind kind) {
        if (Layer* layer = findLayer(name, kind)) {
            return *layer;
        }
        Layer created{.id = nextLayerId++, .name = std::string(name), .kind = kind};
        if (kind == Layer::Kind::Tile && !map.infinite) {
            created.width = map.width;
            created.height = map.height;
            created.gids.assign(static_cast<std::size_t>(map.width) * static_cast<std::size_t>(map.height), 0U);
        }
        map.layers.push_back(std::move(created));
        layers.clear();
        return map.layers.back();
    }

    // Cells outside a finite map are empty, unless its border repeats the cells along the edge or wraps around to the other side.
    [[nodiscard]] std::uint32_t getGid(const CompiledMap& rules, const Layer* layer, Cell cell) const {
        if (layer == nullptr) {
            return 0;
        }
        if (!map.infinite && (cell.x < 0 || cell.y < 0 || cell.x >= map.width || cell.y >= map.height)) {
            if (rules.wrapBorder) {
                cell = {((cell.x % map.width) + map.width) % map.width, ((cell.y % map.height) + map.height) % map.height};
            } else if (rules.overflowBorder) {
                cell = {std::clamp(cell.x, 0, map.width - 1), std::clamp(cell.y, 0, map.height - 1)};
            } else {
                return 0;
            }
        }
        return layer->getGid(cell.x, cell.y);
    }

    // A finite map keeps its size, and an infinite one grows a chunk of 16 cells around a cell it lacks.
    void setGid(Layer& layer, Cell cell, std::uint32_t gid) {
        if (!map.infinite) {
            if (cell.x >= 0 && cell.y >= 0 && cell.x < layer.width && cell.y < layer.height) {
                layer.setGid(cell.x, cell.y, gid);
            }
            return;
        }
        if (!layer.contains(cell.x, cell.y)) {
            constexpr int kChunk = 16;
            const int left = cell.x >= 0 ? cell.x / kChunk * kChunk : (cell.x - kChunk + 1) / kChunk * kChunk;
            const int top = cell.y >= 0 ? cell.y / kChunk * kChunk : (cell.y - kChunk + 1) / kChunk * kChunk;
            layer.chunks.push_back({.x = left, .y = top, .width = kChunk, .height = kChunk, .gids = std::vector<std::uint32_t>(kChunk * kChunk, 0U)});
        }
        layer.setGid(cell.x, cell.y, gid);
    }

    // The first global id of a tileset of a rule map in the map, when the map holds the same tileset, by its file or by its name, image and size.
    [[nodiscard]] std::optional<std::uint32_t> findFirstGid(const Tileset* tileset) const {
        if (const auto known = firstGids.find(tileset); known != firstGids.end()) {
            return known->second;
        }
        // clang-format off
        const auto same = std::ranges::find_if(map.tilesets, [tileset](const Map::TilesetReference& candidate) {
            const Tileset& other = *candidate.tileset;
            return !tileset->path.empty() ? other.path == tileset->path : other.name == tileset->name && other.image == tileset->image && other.tileCount == tileset->tileCount;
        });
        // clang-format on
        return same != map.tilesets.end() ? std::optional(same->firstGid) : std::nullopt;
    }

    // A tile of the input of a rule map takes the global id of its tileset in the map, and a tile of a tileset the map lacks matches nothing there.
    [[nodiscard]] std::uint32_t lookup(const Map& rules, std::uint32_t gid) const {
        const Map::TilesetReference* reference = rules.findTileset(gid);
        const std::optional<std::uint32_t> firstGid = reference != nullptr ? findFirstGid(reference->tileset.get()) : std::nullopt;
        return firstGid ? (*firstGid + (Map::tileId(gid) - reference->firstGid)) | (gid & Map::kFlagMask) : kMissingTile;
    }

    // A tile of the output of a rule map takes the global id of its tileset in the map, which joins the map when the map lacks it.
    [[nodiscard]] std::uint32_t translate(const Map& rules, std::uint32_t gid) {
        const Map::TilesetReference* reference = rules.findTileset(gid);
        if (reference == nullptr) {
            throw std::invalid_argument("A rule map outputs the tile \"" + std::to_string(Map::tileId(gid)) + "\", which no tileset of the rule map holds.");
        }
        const Tileset* tileset = reference->tileset.get();
        auto found = firstGids.find(tileset);
        if (found == firstGids.end()) {
            std::uint32_t firstGid = 0;
            if (const std::optional<std::uint32_t> same = findFirstGid(tileset)) {
                firstGid = *same;
            } else {
                firstGid = 1;
                for (const Map::TilesetReference& existing : map.tilesets) {
                    const std::uint32_t highest = existing.tileset->tiles.empty() ? 0U : existing.tileset->tiles.rbegin()->first + 1U;
                    firstGid = std::max(firstGid, existing.firstGid + std::max(static_cast<std::uint32_t>(existing.tileset->tileCount), highest));
                }
                map.tilesets.push_back({.firstGid = firstGid, .tileset = reference->tileset});
            }
            found = firstGids.emplace(tileset, firstGid).first;
        }
        return (found->second + (Map::tileId(gid) - reference->firstGid)) | (gid & Map::kFlagMask);
    }

    void markLayer(const Layer& layer) {
        std::vector<std::uint32_t>& list = layer.kind == Layer::Kind::Object ? result.objectLayers : result.layers;
        if (std::ranges::find(list, layer.id) == list.end()) {
            list.push_back(layer.id);
        }
    }
};

void AutoMapper::collectLayers(std::vector<Layer>& layers, std::vector<Layer*>& found) {
    for (Layer& layer : layers) {
        if (layer.kind == Layer::Kind::Group) {
            collectLayers(layer.layers, found);
        } else {
            found.push_back(&layer);
        }
    }
}

AutoMapper::AutoMapper(std::vector<RuleMap> maps) : ruleMaps(std::move(maps)) {
    for (const RuleMap& ruleMap : ruleMaps) {
        compiled.push_back(compile(ruleMap));
    }
}

AutoMapper AutoMapper::load(std::string_view path, const TextReader& readText, const Map::JsonReader& readJson) {
    std::vector<RuleMap> found;
    loadRules(path, readText, readJson, {}, found, 0);
    return AutoMapper(std::move(found));
}

// A rules file lists rule maps and other rules files relative to itself, a line in square brackets filters the map names of the lines after it, and lines that start with "#" or "//" are comments.
void AutoMapper::loadRules(std::string_view path, const TextReader& readText, const Map::JsonReader& readJson, std::string filter, std::vector<RuleMap>& found, int depth) {
    if (depth > kMaxRulesDepth) {
        throw std::invalid_argument("The automapping rules file \"" + std::string(path) + "\" includes itself.");
    }
    const std::string file = io::Path::normalize(path);
    if (!file.ends_with(".txt")) {
        found.push_back({.map = std::make_shared<Map>(Map::parse(readJson(file), file, readJson)), .filter = std::move(filter)});
        return;
    }

    const std::string directory{io::Path::directory(file)};
    std::istringstream lines(readText(file));
    std::string line;
    while (std::getline(lines, line)) {
        const auto first = line.find_first_not_of(" \t\r");
        if (first == std::string::npos) {
            continue;
        }
        line = line.substr(first, line.find_last_not_of(" \t\r") - first + 1);
        if (line.starts_with('#') || line.starts_with("//")) {
            continue;
        }
        if (line.starts_with('[') && line.ends_with(']')) {
            filter = line.substr(1, line.size() - 2);
            continue;
        }
        loadRules(MapParser::resolve(directory, line), readText, readJson, filter, found, depth + 1);
    }
}

bool AutoMapper::matchesFilter(std::string_view filter, std::string_view mapPath) noexcept {
    const std::string_view name = mapPath.substr(mapPath.find_last_of('/') == std::string_view::npos ? 0 : mapPath.find_last_of('/') + 1);
    // A star matches any text, so the name is matched piece by piece between the stars, from the left.
    std::size_t position = 0;
    std::size_t start = 0;
    bool anchored = true;
    while (start <= filter.size()) {
        const std::size_t star = filter.find('*', start);
        const std::string_view piece = filter.substr(start, star == std::string_view::npos ? std::string_view::npos : star - start);
        if (star == std::string_view::npos) {
            return anchored ? name.substr(position) == piece : name.size() >= position + piece.size() && name.ends_with(piece);
        }
        if (anchored) {
            if (name.substr(position, piece.size()) != piece) {
                return false;
            }
            position += piece.size();
        } else {
            const std::size_t at = name.find(piece, position);
            if (at == std::string_view::npos) {
                return false;
            }
            position = at + piece.size();
        }
        anchored = false;
        start = star + 1;
    }
    return true;
}

// A tile object stands on its origin at its bottom edge, so it lies in the cell above its origin, while other objects lie in the cell of their origin.
AutoMapper::Cell AutoMapper::objectCell(const Object& object, math::Vec2 tileSize) noexcept {
    const float y = object.gid != 0 ? object.position.y - tileSize.y * 0.5F : object.position.y;
    return {static_cast<int>(std::floor(object.position.x / tileSize.x)), static_cast<int>(std::floor(y / tileSize.y))};
}

AutoMapper::Match AutoMapper::matchOf(const Map& map, std::uint32_t gid) {
    const Map::TilesetReference* reference = map.findTileset(gid);
    const Tile* tile = reference != nullptr ? reference->tileset->findTile(Map::tileId(gid) - reference->firstGid) : nullptr;
    if (tile == nullptr || !tile->properties.has("MatchType")) {
        return Match::Tile;
    }
    const std::string type = tile->properties.getString("MatchType", "");
    for (const auto& [name, match] : {std::pair{"Empty", Match::Empty}, std::pair{"Ignore", Match::Ignore}, std::pair{"NonEmpty", Match::NonEmpty}, std::pair{"Other", Match::Other}, std::pair{"Negate", Match::Negate}}) {
        if (type == name) {
            return match;
        }
    }
    throw std::invalid_argument("The automapping tile type \"" + type + "\" is unknown. It is \"Empty\", \"Ignore\", \"NonEmpty\", \"Other\" or \"Negate\".");
}

AutoMapper::Options AutoMapper::readOptions(const Properties& properties, Options defaults) {
    defaults.modX = std::max(1, static_cast<int>(properties.getNumber("ModX", defaults.modX)));
    defaults.modY = std::max(1, static_cast<int>(properties.getNumber("ModY", defaults.modY)));
    defaults.offsetX = static_cast<int>(properties.getNumber("OffsetX", defaults.offsetX));
    defaults.offsetY = static_cast<int>(properties.getNumber("OffsetY", defaults.offsetY));
    defaults.probability = static_cast<float>(properties.getNumber("Probability", defaults.probability));
    defaults.disabled = properties.getBool("Disabled", defaults.disabled);
    defaults.noOverlappingOutput = properties.getBool("NoOverlappingOutput", defaults.noOverlappingOutput);
    return defaults;
}

// A rule is a group of cells that touch, on any side or corner, and hold a tile of an input or output layer, or the origin of an output object.
AutoMapper::CompiledMap AutoMapper::compile(const RuleMap& ruleMap) {
    Map& map = *ruleMap.map;
    CompiledMap result{.source = ruleMap.map, .filter = ruleMap.filter};
    result.deleteTiles = map.properties.getBool("DeleteTiles", false);
    result.overflowBorder = map.properties.getBool("OverflowBorder", false);
    result.wrapBorder = map.properties.getBool("WrapBorder", false);
    result.matchOutsideMap = map.properties.getBool("MatchOutsideMap", map.infinite) || result.overflowBorder || result.wrapBorder;
    result.matchInOrder = map.properties.getBool("MatchInOrder", false);
    const Options defaults = readOptions(map.properties, {});

    struct Input {
        const Layer* layer = nullptr;
        bool inverted = false;
        std::string index;
        std::string target;
    };
    struct Output {
        const Layer* layer = nullptr;
        std::string index;
        std::string target;
    };
    std::vector<Input> inputs;
    std::vector<Output> outputs;
    const Layer* options = nullptr;
    std::vector<Layer*> layers;
    collectLayers(map.layers, layers);
    for (const Layer* layer : layers) {
        std::string lower = layer->name;
        std::ranges::transform(lower, lower.begin(), [](unsigned char letter) { return static_cast<char>(std::tolower(letter)); });
        const std::size_t underscore = layer->name.find('_');
        if (lower == "regions" || lower.starts_with("regions_")) {
            throw std::invalid_argument("The rule map \"" + map.path + "\" uses the legacy \"regions\" layers. Remove them, so its rules follow the tiles of their input and output layers.");
        }
        if (lower == "rule_options") {
            options = layer;
        } else if (lower.starts_with("input") && underscore != std::string::npos && layer->kind == Layer::Kind::Tile) {
            std::string index = lower.substr(5, underscore - 5);
            const bool inverted = index.starts_with("not");
            inputs.push_back({.layer = layer, .inverted = inverted, .index = inverted ? index.substr(3) : index, .target = layer->name.substr(underscore + 1)});
        } else if (lower.starts_with("output") && underscore != std::string::npos && layer->kind != Layer::Kind::Image) {
            outputs.push_back({.layer = layer, .index = lower.substr(6, underscore - 6), .target = layer->name.substr(underscore + 1)});
        }
    }
    if (inputs.empty() || outputs.empty()) {
        throw std::invalid_argument("The rule map \"" + map.path + "\" needs an input layer and an output layer.");
    }

    // The cells of every rule, and the cells input tiles cover, which deleting tiles clears.
    std::set<std::pair<int, int>> filled;
    std::set<std::pair<int, int>> covered;
    // clang-format off
    const auto markArea = [&](const Layer& layer, bool input, int left, int top, int width, int height) {
        for (int row = top; row < top + height; ++row) {
            for (int column = left; column < left + width; ++column) {
                if (layer.getGid(column, row) != 0) {
                    filled.insert({row, column});
                    if (input) {
                        covered.insert({row, column});
                    }
                }
            }
        }
    };
    const auto mark = [&](const Layer& layer, bool input) {
        if (!map.infinite) {
            markArea(layer, input, 0, 0, map.width, map.height);
        }
        for (const Layer::Chunk& chunk : layer.chunks) {
            markArea(layer, input, chunk.x, chunk.y, chunk.width, chunk.height);
        }
    };
    // clang-format on
    for (const Input& input : inputs) {
        mark(*input.layer, true);
    }
    for (const Output& output : outputs) {
        if (output.layer->kind == Layer::Kind::Tile) {
            mark(*output.layer, false);
        }
        for (const Object& object : output.layer->objects) {
            const Cell cell = objectCell(object, map.tileSize);
            filled.insert({cell.y, cell.x});
        }
    }

    std::set<std::pair<int, int>> left = filled;
    while (!left.empty()) {
        Rule rule;
        std::vector<std::pair<int, int>> pending{*left.begin()};
        left.erase(left.begin());
        while (!pending.empty()) {
            const auto [row, column] = pending.back();
            pending.pop_back();
            rule.region.push_back({column, row});
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (const auto next = left.find({row + dy, column + dx}); next != left.end()) {
                        pending.push_back(*next);
                        left.erase(next);
                    }
                }
            }
        }
        rule.origin = {std::ranges::min(rule.region, {}, &Cell::x).x, std::ranges::min(rule.region, {}, &Cell::y).y};
        std::ranges::sort(rule.region, [](Cell a, Cell b) { return a.y != b.y ? a.y < b.y : a.x < b.x; });

        // Conditions read the cells of the rule relative to its origin.
        for (const Input& input : inputs) {
            auto index = std::ranges::find(rule.inputs, input.index, &InputIndex::index);
            if (index == rule.inputs.end()) {
                rule.inputs.push_back({.index = input.index});
                index = rule.inputs.end() - 1;
            }
            auto target = std::ranges::find(index->targets, input.target, &TargetCondition::layer);
            if (target == index->targets.end()) {
                index->targets.push_back({.layer = input.target});
                target = index->targets.end() - 1;
            }
            const std::uint32_t ignored = (input.layer->properties.getBool("IgnoreHorizontalFlip", false) ? Map::kFlipHorizontal : 0U) | (input.layer->properties.getBool("IgnoreVerticalFlip", false) ? Map::kFlipVertical : 0U) | (input.layer->properties.getBool("IgnoreDiagonalFlip", false) ? Map::kFlipDiagonal : 0U) | (input.layer->properties.getBool("IgnoreHexRotate120", false) ? Map::kRotateHexagonal : 0U);
            const bool strictEmpty = input.layer->properties.getBool("AutoEmpty", false) || input.layer->properties.getBool("StrictEmpty", false);
            for (const Cell cell : rule.region) {
                const std::uint32_t gid = input.layer->getGid(cell.x, cell.y);
                const Cell relative{cell.x - rule.origin.x, cell.y - rule.origin.y};
                auto condition = std::ranges::find(target->cells, relative, &CellCondition::cell);
                if (condition == target->cells.end()) {
                    target->cells.push_back({.cell = relative});
                    condition = target->cells.end() - 1;
                }
                if (gid == 0) {
                    if (strictEmpty && covered.contains({cell.y, cell.x}) && !input.inverted) {
                        condition->wanted.push_back({.match = Match::Empty});
                    }
                    continue;
                }
                const Match match = matchOf(map, gid);
                if (match == Match::Negate) {
                    condition->negated = true;
                } else if (match != Match::Ignore) {
                    (input.inverted ? condition->unwanted : condition->wanted).push_back({.match = match, .gid = gid, .ignoredFlags = ignored});
                }
            }
        }

        // The tiles a rule uses on a target layer are what "Other" stays away from, over every index of the rule.
        std::map<std::string, std::pair<std::vector<std::uint32_t>, bool>> used;
        for (const InputIndex& index : rule.inputs) {
            for (const TargetCondition& target : index.targets) {
                for (const CellCondition& condition : target.cells) {
                    for (const std::vector<Entry>* entries : {&condition.wanted, &condition.unwanted}) {
                        for (const Entry& entry : *entries) {
                            if (entry.match == Match::Tile) {
                                used[target.layer].first.push_back(Map::tileId(entry.gid));
                            } else if (entry.match == Match::Empty) {
                                used[target.layer].second = true;
                            }
                        }
                    }
                }
            }
        }
        // An input layer without tiles in the rule sets no condition, and an index without conditions is no alternative, since it would hold everywhere.
        for (InputIndex& index : rule.inputs) {
            for (TargetCondition& target : index.targets) {
                std::erase_if(target.cells, [](const CellCondition& condition) { return condition.wanted.empty() && condition.unwanted.empty(); });
                target.used = used[target.layer].first;
                target.usesEmpty = used[target.layer].second;
            }
            std::erase_if(index.targets, [](const TargetCondition& target) { return target.cells.empty(); });
        }
        std::erase_if(rule.inputs, [](const InputIndex& index) { return index.targets.empty(); });

        // An output index with nothing in the rule is never chosen, and the top layer of an index sets its probability.
        for (const Output& output : outputs) {
            OutputLayer written{.layer = output.target, .objects = output.layer->kind == Layer::Kind::Object};
            std::vector<Property> kept;
            for (const Property& property : output.layer->properties.getItems()) {
                if (property.name != "Probability") {
                    kept.push_back(property);
                }
            }
            written.properties = Properties(std::move(kept));
            for (const Cell cell : rule.region) {
                if (output.layer->kind == Layer::Kind::Tile) {
                    if (const std::uint32_t gid = output.layer->getGid(cell.x, cell.y); gid != 0) {
                        written.cells.push_back({{cell.x - rule.origin.x, cell.y - rule.origin.y}, {.match = matchOf(map, gid), .gid = gid}});
                    }
                }
            }
            for (const Object& object : output.layer->objects) {
                if (std::ranges::find(rule.region, objectCell(object, map.tileSize)) != rule.region.end()) {
                    Object placed = object;
                    placed.position -= math::Vec2{static_cast<float>(rule.origin.x) * map.tileSize.x, static_cast<float>(rule.origin.y) * map.tileSize.y};
                    written.placed.push_back(std::move(placed));
                }
            }
            if (written.cells.empty() && written.placed.empty()) {
                continue;
            }
            auto index = std::ranges::find(rule.outputs, output.index, &OutputIndex::index);
            if (index == rule.outputs.end()) {
                rule.outputs.push_back({.index = output.index});
                index = rule.outputs.end() - 1;
            }
            index->probability = static_cast<float>(output.layer->properties.getNumber("Probability", index->probability));
            index->layers.push_back(std::move(written));
        }

        // The options of a rule come from the map and from every object of the options layer that holds the whole rule.
        rule.options = defaults;
        if (options != nullptr) {
            for (const Object& object : options->objects) {
                const math::Rect area{object.position.x / map.tileSize.x, object.position.y / map.tileSize.y, object.size.x / map.tileSize.x, object.size.y / map.tileSize.y};
                const bool holds = std::ranges::all_of(rule.region, [&area](Cell cell) { return area.contains(math::Rect{static_cast<float>(cell.x), static_cast<float>(cell.y), 1.0F, 1.0F}); });
                if (holds) {
                    rule.options = readOptions(object.properties, rule.options);
                }
            }
        }
        rule.inputCells.clear();
        for (const Cell cell : rule.region) {
            if (covered.contains({cell.y, cell.x})) {
                rule.inputCells.push_back({cell.x - rule.origin.x, cell.y - rule.origin.y});
            }
        }
        for (Cell& cell : rule.region) {
            cell = {cell.x - rule.origin.x, cell.y - rule.origin.y};
        }
        result.rules.push_back(std::move(rule));
    }
    std::ranges::sort(result.rules, [](const Rule& a, const Rule& b) { return a.origin.y != b.origin.y ? a.origin.y < b.origin.y : a.origin.x < b.origin.x; });
    return result;
}

bool AutoMapper::matches(const Entry& entry, std::uint32_t gid, const TargetCondition& target) noexcept {
    switch (entry.match) {
    case Match::Tile:
        return gid != 0 && (gid & ~entry.ignoredFlags) == (entry.gid & ~entry.ignoredFlags);
    case Match::Empty:
        return gid == 0;
    case Match::NonEmpty:
        return gid != 0;
    case Match::Other:
        return gid == 0 ? !target.usesEmpty : std::ranges::find(target.used, Map::tileId(gid)) == target.used.end();
    case Match::Ignore:
    case Match::Negate:
        break;
    }
    return true;
}

// A rule matches where every target layer of any one of its input indexes holds one of the wanted tiles and none of the unwanted ones at each of its cells, with the roles swapped at a negated cell.
bool AutoMapper::satisfies(const CompiledMap& compiled, const Rule& rule, const Target& target, Cell offset) {
    for (const InputIndex& index : rule.inputs) {
        // clang-format off
        const bool all = std::ranges::all_of(index.targets, [&](const TargetCondition& condition) {
            const Layer* layer = target.findLayer(condition.layer, Layer::Kind::Tile);
            return std::ranges::all_of(condition.cells, [&](const CellCondition& cell) {
                const std::uint32_t gid = target.getGid(compiled, layer, {offset.x + cell.cell.x, offset.y + cell.cell.y});
                const std::vector<Entry>& wanted = cell.negated ? cell.unwanted : cell.wanted;
                const std::vector<Entry>& unwanted = cell.negated ? cell.wanted : cell.unwanted;
                const bool present = wanted.empty() || std::ranges::any_of(wanted, [&](const Entry& entry) { return matches(entry, gid, condition); });
                return present && std::ranges::none_of(unwanted, [&](const Entry& entry) { return matches(entry, gid, condition); });
            });
        });
        // clang-format on
        if (all) {
            return true;
        }
    }
    return false;
}

AutoMapper::Result AutoMapper::apply(Map& map, math::Random& random) const {
    Result result;
    Target target{.map = map, .result = result};
    std::vector<Layer*> layers;
    collectLayers(map.layers, layers);
    // clang-format off
    const auto highestLayer = [](const std::vector<Layer>& list, const auto& self) -> std::uint32_t {
        std::uint32_t highest = 0;
        for (const Layer& layer : list) {
            highest = std::max({highest, layer.id, self(layer.layers, self)});
        }
        return highest;
    };
    // clang-format on
    target.nextLayerId = highestLayer(map.layers, highestLayer) + 1;
    for (const Layer* layer : layers) {
        for (const Object& object : layer->objects) {
            target.nextObjectId = std::max(target.nextObjectId, object.id + 1);
        }
    }
    for (const CompiledMap& rules : compiled) {
        if (rules.filter.empty() || matchesFilter(rules.filter, map.path)) {
            applyMap(rules, target, random);
        }
    }
    return result;
}

// Rules match against the map as it was before the rule map and then write in order, or match and write one by one in order when the rule map matches in order. Finite maps place rules inside themselves unless they match outside, and infinite maps place them around the chunks they have.
void AutoMapper::applyMap(const CompiledMap& source, Target& target, math::Random& random) const {
    Map& map = target.map;
    const CompiledMap rules = translateInputs(source, target);
    int left = 0;
    int top = 0;
    int right = map.width - 1;
    int bottom = map.height - 1;
    if (map.infinite) {
        std::vector<Layer*> layers;
        collectLayers(map.layers, layers);
        left = top = std::numeric_limits<int>::max();
        right = bottom = std::numeric_limits<int>::min();
        for (const Layer* layer : layers) {
            for (const Layer::Chunk& chunk : layer->chunks) {
                left = std::min(left, chunk.x);
                top = std::min(top, chunk.y);
                right = std::max(right, chunk.x + chunk.width - 1);
                bottom = std::max(bottom, chunk.y + chunk.height - 1);
            }
        }
        if (left > right) {
            return;
        }
    }

    // clang-format off
    const auto find = [&](const Rule& rule) {
        std::vector<Cell> found;
        if (rule.options.disabled) {
            return found;
        }
        const int width = std::ranges::max(rule.region, {}, &Cell::x).x;
        const int height = std::ranges::max(rule.region, {}, &Cell::y).y;
        const bool outside = rules.matchOutsideMap || map.infinite;
        for (int y = outside ? top - height : top; y <= (outside ? bottom : bottom - height); ++y) {
            for (int x = outside ? left - width : left; x <= (outside ? right : right - width); ++x) {
                const bool placed = ((x - rule.options.offsetX) % rule.options.modX + rule.options.modX) % rule.options.modX == 0 && ((y - rule.options.offsetY) % rule.options.modY + rule.options.modY) % rule.options.modY == 0;
                if (placed && satisfies(rules, rule, target, {x, y}) && (rule.options.probability >= 1.0F || random.nextFloat() < rule.options.probability)) {
                    found.push_back({x, y});
                }
            }
        }
        return found;
    };
    // clang-format on

    if (rules.matchInOrder) {
        for (std::size_t index = 0; index < rules.rules.size(); ++index) {
            std::vector<std::pair<std::string, Cell>> written;
            for (const Cell offset : find(rules.rules[index])) {
                if (satisfies(rules, rules.rules[index], target, offset)) {
                    write(source, source.rules[index], target, offset, random, written);
                }
            }
        }
        return;
    }
    std::vector<std::vector<Cell>> matches;
    matches.reserve(rules.rules.size());
    for (const Rule& rule : rules.rules) {
        matches.push_back(find(rule));
    }
    for (std::size_t index = 0; index < rules.rules.size(); ++index) {
        std::vector<std::pair<std::string, Cell>> written;
        for (const Cell offset : matches[index]) {
            write(source, source.rules[index], target, offset, random, written);
        }
    }
}

// The input tiles of a rule map name tiles by its own tilesets, so they take the global ids of the same tilesets in the map before they match.
AutoMapper::CompiledMap AutoMapper::translateInputs(const CompiledMap& compiled, const Target& target) {
    CompiledMap translated = compiled;
    for (Rule& rule : translated.rules) {
        for (InputIndex& index : rule.inputs) {
            for (TargetCondition& condition : index.targets) {
                for (CellCondition& cell : condition.cells) {
                    for (std::vector<Entry>* entries : {&cell.wanted, &cell.unwanted}) {
                        for (Entry& entry : *entries) {
                            if (entry.match == Match::Tile) {
                                entry.gid = target.lookup(*compiled.source, entry.gid);
                            }
                        }
                    }
                }
                for (std::uint32_t& used : condition.used) {
                    used = Map::tileId(target.lookup(*compiled.source, used));
                }
            }
        }
    }
    return translated;
}

// One indexed output is picked by the weight of its probability, and the outputs without an index always write. A rule that keeps its outputs apart skips a match that would write over a cell it already wrote.
void AutoMapper::write(const CompiledMap& rules, const Rule& rule, Target& target, Cell offset, math::Random& random, std::vector<std::pair<std::string, Cell>>& written) {
    std::vector<const OutputIndex*> chosen;
    float total = 0.0F;
    for (const OutputIndex& output : rule.outputs) {
        if (output.index.empty()) {
            chosen.push_back(&output);
        } else {
            total += std::max(output.probability, 0.0F);
        }
    }
    if (total > 0.0F) {
        float pick = random.nextFloat() * total;
        for (const OutputIndex& output : rule.outputs) {
            if (!output.index.empty() && output.probability > 0.0F) {
                pick -= output.probability;
                if (pick < 0.0F) {
                    chosen.push_back(&output);
                    break;
                }
            }
        }
    }

    std::vector<std::pair<std::string, Cell>> cells;
    for (const OutputIndex* output : chosen) {
        for (const OutputLayer& layer : output->layers) {
            for (const auto& [cell, entry] : layer.cells) {
                if (entry.match == Match::Tile || entry.match == Match::Empty) {
                    cells.push_back({layer.layer, {offset.x + cell.x, offset.y + cell.y}});
                }
            }
        }
    }
    if (rule.options.noOverlappingOutput && std::ranges::any_of(cells, [&written](const auto& cell) { return std::ranges::find(written, cell) != written.end(); })) {
        return;
    }
    written.insert(written.end(), cells.begin(), cells.end());
    ++target.result.matches;

    const Map& source = *rules.source;
    const math::Vec2 shift{static_cast<float>(offset.x) * target.map.tileSize.x, static_cast<float>(offset.y) * target.map.tileSize.y};
    if (rules.deleteTiles) {
        for (const OutputIndex& output : rule.outputs) {
            for (const OutputLayer& layer : output.layers) {
                Layer& cleared = target.requireLayer(layer.layer, layer.objects ? Layer::Kind::Object : Layer::Kind::Tile);
                for (const Cell cell : rule.inputCells) {
                    const Cell at{offset.x + cell.x, offset.y + cell.y};
                    if (layer.objects) {
                        std::erase_if(cleared.objects, [&](const Object& object) { return objectCell(object, target.map.tileSize) == at; });
                    } else {
                        target.setGid(cleared, at, 0);
                    }
                }
                target.markLayer(cleared);
            }
        }
    }
    for (const OutputIndex* output : chosen) {
        for (const OutputLayer& layer : output->layers) {
            Layer& destination = target.requireLayer(layer.layer, layer.objects ? Layer::Kind::Object : Layer::Kind::Tile);
            for (const auto& [cell, entry] : layer.cells) {
                if (entry.match == Match::Tile || entry.match == Match::Empty) {
                    target.setGid(destination, {offset.x + cell.x, offset.y + cell.y}, entry.match == Match::Tile ? target.translate(source, entry.gid) : 0U);
                }
            }
            for (const Object& object : layer.placed) {
                Object placed = object;
                placed.id = target.nextObjectId++;
                placed.position += shift;
                if (placed.gid != 0) {
                    placed.gid = target.translate(source, placed.gid);
                }
                destination.objects.push_back(std::move(placed));
            }
            destination.properties = destination.properties.merged(layer.properties);
            target.markLayer(destination);
        }
    }
}

} // namespace haylen::tiled
