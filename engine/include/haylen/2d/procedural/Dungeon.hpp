#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/spatial/CellGrid.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::procedural2d {

// Lays out rooms joined by corridors, where 1 marks walls and 0 floors, and every room can reach every other one. The method `Bsp` splits the map in two again and again and puts one room in each part. The method `Placement` drops rooms at random free spots and joins them with a minimum spanning tree.
class Dungeon final {
  public:
    static constexpr std::int32_t kFloor = 0;
    static constexpr std::int32_t kWall = 1;

    enum class Method : std::uint8_t {
        Bsp,
        Placement,
    };

    struct Room {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        [[nodiscard]] int getCenterX() const noexcept {
            return x + width / 2;
        }
        [[nodiscard]] int getCenterY() const noexcept {
            return y + height / 2;
        }
    };

    // Rooms keep `padding` wall cells from their part or from other rooms. The method `Bsp` stops splitting parts smaller than twice `minimumLeafSize`, and `Placement` tries `roomAttempts` positions for up to `maximumRooms` rooms.
    struct Options {
        Method method = Method::Bsp;
        int width = 64;
        int height = 48;
        int minimumRoomSize = 4;
        int maximumRoomSize = 10;
        int minimumLeafSize = 10;
        int maximumRooms = 12;
        int roomAttempts = 200;
        int padding = 1;
    };

    // Connections list the rooms each corridor joins.
    struct Result {
        spatial2d::CellGrid grid;
        std::vector<Room> rooms;
        std::vector<std::pair<std::size_t, std::size_t>> connections;
    };

    // Throws `std::invalid_argument` when the room sizes are not positive and ordered or the map cannot hold one room.
    [[nodiscard]] static Result generate(const Options& options, math::Random& random);

    [[nodiscard]] static std::optional<Method> methodFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view methodName(Method value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Method>, 2> kMethodNames;

    struct Leaf {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
    };

    [[nodiscard]] static std::vector<std::size_t> splitLeaf(const Leaf& leaf, const Options& options, math::Random& random, Result& result);
    static void placeRooms(const Options& options, math::Random& random, Result& result);
    static void connect(std::size_t first, std::size_t second, math::Random& random, Result& result);
    static void carveRoom(const Room& room, spatial2d::CellGrid& grid);
    static void carveLine(int fromX, int fromY, int toX, int toY, spatial2d::CellGrid& grid);
};

} // namespace haylen::procedural2d
