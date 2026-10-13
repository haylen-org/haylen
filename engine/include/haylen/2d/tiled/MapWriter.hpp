#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/tiled/Map.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

struct Layer;
struct Object;
struct Tileset;
class Properties;

// Writes a map as the Tiled JSON that the map loader and the Tiled editor read, with every tileset embedded and every path relative to the folder of the file it will be saved as, a package path such as `maps/cave.tmj`.
class MapWriter final {
  public:
    [[nodiscard]] static core::Json write(const Map& map, std::string_view file);

  private:
    [[nodiscard]] static std::vector<std::string_view> split(std::string_view path);
    [[nodiscard]] static core::Json writePoints(const std::vector<math::Vec2>& points);

    // Returns the path of a package file relative to a folder of the package.
    [[nodiscard]] static std::string relative(std::string_view directory, std::string_view path);
    [[nodiscard]] static core::Json writeValue(std::string_view type, core::Json value, std::string_view directory);
    [[nodiscard]] static core::Json writeProperties(const Properties& properties, std::string_view directory);
    [[nodiscard]] static core::Json writeObject(const Object& object, std::string_view directory);
    [[nodiscard]] static core::Json writeTileset(const Tileset& tileset, std::uint32_t firstGid, std::string_view directory);
    [[nodiscard]] static core::Json writeLayer(const Layer& layer, std::string_view directory);
    static void collectIds(const Layer& layer, std::uint32_t& layerId, std::uint32_t& objectId) noexcept;
};

} // namespace haylen::tiled
