#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/2d/tiled/Layer.hpp"
#include "haylen/2d/tiled/Map.hpp"
#include "haylen/2d/tiled/Object.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/2d/tiled/Tileset.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/BlendMode.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// Reads maps in the current Tiled JSON format, loading external tilesets and templates through the reader of the map.
class MapParser final {
  public:
    [[nodiscard]] static Map parse(const core::Json& document, std::string_view file, const Map::JsonReader& reader);

    // Resolves a path relative to the file in `directory`, keeping an empty path empty.
    [[nodiscard]] static std::string resolve(std::string_view directory, std::string_view relative);

  private:
    // Cells are indexed with `int` and their data takes four bytes each, so a layer holds as many cells as both allow.
    static constexpr std::size_t kMaxCells = std::min<std::size_t>(std::numeric_limits<int>::max(), std::numeric_limits<std::size_t>::max() / 4);

    MapParser(const Map::JsonReader& reader, Map& target) noexcept : read(reader), map(target) {}

    [[nodiscard]] static std::size_t countCells(int width, int height, std::string_view layer);
    [[nodiscard]] static std::vector<std::uint8_t> decodeBase64(std::string_view text);
    [[nodiscard]] static std::vector<std::uint8_t> inflate(const std::vector<std::uint8_t>& compressed, bool gzip, std::size_t expected);
    [[nodiscard]] static std::vector<std::uint32_t> readTileData(const core::Json& layer, const core::Json& data, std::size_t cells);
    [[nodiscard]] static std::optional<math::Color> readOptionalColor(const core::Json& object, const char* key);
    [[nodiscard]] static graphics::BlendMode::Type readBlendMode(const std::string& mode, std::string_view layer);
    [[nodiscard]] static core::Json resolveValue(std::string_view type, core::Json value, std::string_view directory);
    [[nodiscard]] static Properties readProperties(const core::Json& owner, std::string_view directory);
    [[nodiscard]] static Map::Orientation readOrientation(const std::string& name);
    [[nodiscard]] static Map::RenderOrder readRenderOrder(const std::string& name);
    [[nodiscard]] static std::vector<math::Vec2> readPoints(const core::Json& points);

    [[nodiscard]] Object readObject(const core::Json& source, std::string_view directory) const;
    [[nodiscard]] std::uint32_t readTemplateGid(const core::Json& document, std::uint32_t gid, std::string_view directory) const;
    [[nodiscard]] std::shared_ptr<Tileset> readTileset(const core::Json& document, std::string_view directory, std::string path) const;
    [[nodiscard]] Layer readLayer(const core::Json& entry, std::string_view directory) const;

    const Map::JsonReader& read;
    Map& map;
};

} // namespace haylen::tiled
