#include "haylen/2d/procedural/Dungeon.hpp"

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

#include "haylen/math/Random.hpp"

namespace haylen::procedural2d {

const std::array<std::pair<std::string_view, Dungeon::Method>, 2> Dungeon::kMethodNames{{{"bsp", Method::Bsp}, {"placement", Method::Placement}}};

std::optional<Dungeon::Method> Dungeon::methodFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kMethodNames, name, &std::pair<std::string_view, Method>::first);
    return found != kMethodNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view Dungeon::methodName(Method value) noexcept {
    return std::ranges::find(kMethodNames, value, &std::pair<std::string_view, Method>::second)->first;
}

void Dungeon::carveRoom(const Room& room, spatial2d::CellGrid& grid) {
    for (int y = room.y; y < room.y + room.height; ++y) {
        for (int x = room.x; x < room.x + room.width; ++x) {
            grid.set({x, y}, kFloor);
        }
    }
}

// Carves the straight run between two cells that share a row or a column.
void Dungeon::carveLine(int fromX, int fromY, int toX, int toY, spatial2d::CellGrid& grid) {
    for (int y = std::min(fromY, toY); y <= std::max(fromY, toY); ++y) {
        for (int x = std::min(fromX, toX); x <= std::max(fromX, toX); ++x) {
            grid.set({x, y}, kFloor);
        }
    }
}

// Joins two room centers with an L-shaped corridor whose bend goes either way.
void Dungeon::connect(std::size_t first, std::size_t second, math::Random& random, Result& result) {
    const Room& from = result.rooms[first];
    const Room& to = result.rooms[second];
    const int startX = from.getCenterX();
    const int startY = from.getCenterY();
    const int endX = to.getCenterX();
    const int endY = to.getCenterY();
    if (random.chance(0.5F)) {
        carveLine(startX, startY, endX, startY, result.grid);
        carveLine(endX, startY, endX, endY, result.grid);
    } else {
        carveLine(startX, startY, startX, endY, result.grid);
        carveLine(startX, endY, endX, endY, result.grid);
    }
    result.connections.emplace_back(first, second);
}

// Returns the rooms of the leaf after splitting it, joining the closest rooms of its two halves.
std::vector<std::size_t> Dungeon::splitLeaf(const Leaf& leaf, const Options& options, math::Random& random, Result& result) {
    const bool splitsAcross = leaf.width >= options.minimumLeafSize * 2;
    const bool splitsDown = leaf.height >= options.minimumLeafSize * 2;
    if (!splitsAcross && !splitsDown) {
        const int room = options.minimumRoomSize;
        const int width = random.range(room, std::min(options.maximumRoomSize, leaf.width - options.padding * 2));
        const int height = random.range(room, std::min(options.maximumRoomSize, leaf.height - options.padding * 2));
        const int x = leaf.x + options.padding + random.range(0, leaf.width - options.padding * 2 - width);
        const int y = leaf.y + options.padding + random.range(0, leaf.height - options.padding * 2 - height);
        result.rooms.push_back({x, y, width, height});
        carveRoom(result.rooms.back(), result.grid);
        return {result.rooms.size() - 1};
    }

    // Long parts split across their long side, and square ones either way.
    bool across = splitsAcross;
    if (splitsAcross && splitsDown) {
        across = leaf.width * 4 > leaf.height * 5 || (leaf.height * 4 <= leaf.width * 5 && random.chance(0.5F));
    }

    Leaf first = leaf;
    Leaf second = leaf;
    if (across) {
        const int at = random.range(options.minimumLeafSize, leaf.width - options.minimumLeafSize);
        first.width = at;
        second.x = leaf.x + at;
        second.width = leaf.width - at;
    } else {
        const int at = random.range(options.minimumLeafSize, leaf.height - options.minimumLeafSize);
        first.height = at;
        second.y = leaf.y + at;
        second.height = leaf.height - at;
    }

    std::vector<std::size_t> rooms = splitLeaf(first, options, random, result);
    const std::vector<std::size_t> others = splitLeaf(second, options, random, result);

    std::pair<std::size_t, std::size_t> closest{rooms.front(), others.front()};
    long best = std::numeric_limits<long>::max();
    for (const std::size_t room : rooms) {
        for (const std::size_t other : others) {
            const long dx = result.rooms[room].getCenterX() - result.rooms[other].getCenterX();
            const long dy = result.rooms[room].getCenterY() - result.rooms[other].getCenterY();
            if (dx * dx + dy * dy < best) {
                best = dx * dx + dy * dy;
                closest = {room, other};
            }
        }
    }
    connect(closest.first, closest.second, random, result);

    rooms.insert(rooms.end(), others.begin(), others.end());
    return rooms;
}

void Dungeon::placeRooms(const Options& options, math::Random& random, Result& result) {
    for (int attempt = 0; attempt < options.roomAttempts && static_cast<int>(result.rooms.size()) < options.maximumRooms; ++attempt) {
        const int width = random.range(options.minimumRoomSize, std::min(options.maximumRoomSize, options.width - options.padding * 2));
        const int height = random.range(options.minimumRoomSize, std::min(options.maximumRoomSize, options.height - options.padding * 2));
        const Room room{random.range(options.padding, options.width - options.padding - width), random.range(options.padding, options.height - options.padding - height), width, height};

        // clang-format off
        const bool free = std::none_of(result.rooms.begin(), result.rooms.end(), [&](const Room& other) {
            return room.x < other.x + other.width + options.padding && other.x < room.x + room.width + options.padding && room.y < other.y + other.height + options.padding && other.y < room.y + room.height + options.padding;
        });
        // clang-format on
        if (free) {
            result.rooms.push_back(room);
            carveRoom(room, result.grid);
        }
    }

    // Prim's algorithm joins every room through the shortest set of corridors between centers.
    std::vector<bool> joined(result.rooms.size(), false);
    if (!joined.empty()) {
        joined.front() = true;
    }
    for (std::size_t added = 1; added < result.rooms.size(); ++added) {
        std::pair<std::size_t, std::size_t> closest{0, 0};
        long best = std::numeric_limits<long>::max();
        for (std::size_t from = 0; from < result.rooms.size(); ++from) {
            for (std::size_t to = 0; to < result.rooms.size(); ++to) {
                if (!joined[from] || joined[to]) {
                    continue;
                }
                const long dx = result.rooms[from].getCenterX() - result.rooms[to].getCenterX();
                const long dy = result.rooms[from].getCenterY() - result.rooms[to].getCenterY();
                if (dx * dx + dy * dy < best) {
                    best = dx * dx + dy * dy;
                    closest = {from, to};
                }
            }
        }
        joined[closest.second] = true;
        connect(closest.first, closest.second, random, result);
    }
}

Dungeon::Result Dungeon::generate(const Options& options, math::Random& random) {
    if (options.minimumRoomSize < 1 || options.maximumRoomSize < options.minimumRoomSize || options.padding < 0) {
        throw std::invalid_argument("Dungeon rooms need a positive minimum size, a maximum size of at least the minimum and a padding of at least zero.");
    }
    const int smallest = options.minimumRoomSize + options.padding * 2;
    if (options.width < smallest || options.height < smallest || (options.method == Method::Bsp && options.minimumLeafSize < smallest)) {
        throw std::invalid_argument("The dungeon and its leaves must fit the smallest room with its padding.");
    }

    Result result{.grid = spatial2d::CellGrid(options.width, options.height, kWall)};
    if (options.method == Method::Bsp) {
        (void)splitLeaf({0, 0, options.width, options.height}, options, random, result);
    } else {
        placeRooms(options, random, result);
    }
    return result;
}

} // namespace haylen::procedural2d
