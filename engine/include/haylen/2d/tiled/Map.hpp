#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/tiled/Layer.hpp"
#include "haylen/2d/tiled/Properties.hpp"
#include "haylen/2d/tiled/Tileset.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::tiled {

// A Tiled map in the current JSON format, with external tilesets and templates already merged in.
class Map final {
  public:
    enum class Orientation : std::uint8_t {
        Orthogonal,
        Isometric,
        Staggered,
        Hexagonal,
        Oblique,
    };

    enum class RenderOrder : std::uint8_t {
        RightDown,
        RightUp,
        LeftDown,
        LeftUp,
    };

    struct TilesetReference {
        std::uint32_t firstGid = 0;
        std::shared_ptr<Tileset> tileset;
    };

    // An image the map needs, with the color that Tiled treats as transparent in it.
    struct Image {
        std::string path;
        std::optional<math::Color> transparentColor;
    };

    using JsonReader = std::function<core::Json(const std::string& path)>;
    using TextureLoader = std::function<graphics::Texture(const std::string& path)>;

    static constexpr std::uint32_t kFlipHorizontal = 0x80000000U;
    static constexpr std::uint32_t kFlipVertical = 0x40000000U;
    static constexpr std::uint32_t kFlipDiagonal = 0x20000000U;
    static constexpr std::uint32_t kRotateHexagonal = 0x10000000U;
    static constexpr std::uint32_t kFlagMask = 0xF0000000U;

    // Returns the tile id of a global tile id without its flip flags.
    [[nodiscard]] static constexpr std::uint32_t tileId(std::uint32_t gid) noexcept {
        return gid & ~kFlagMask;
    }

    // The orientation and render order names are the ones of the Tiled JSON format.
    [[nodiscard]] static std::optional<Orientation> orientationFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view orientationName(Orientation value) noexcept;
    [[nodiscard]] static std::optional<RenderOrder> renderOrderFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view renderOrderName(RenderOrder value) noexcept;

    // Parses a map stored at `file` inside the package content folder. The reader loads external tilesets and templates by their resolved path.
    [[nodiscard]] static Map parse(const core::Json& document, std::string_view file, const JsonReader& read);

    // Lists every image the map needs once, resolved against the package content folder.
    [[nodiscard]] std::vector<Image> getImages() const;

    // Hands every tileset, tile and image layer its texture.
    void attachTextures(const TextureLoader& load);

    // Finds the tileset that holds a global tile id, ignoring its flip flags, or returns null when no tileset holds it.
    [[nodiscard]] const TilesetReference* findTileset(std::uint32_t gid) const noexcept;
    [[nodiscard]] const Layer* findLayer(std::string_view name) const noexcept;
    [[nodiscard]] Layer* findLayer(std::string_view name) noexcept;

    // Converts between tile cells and world positions. The world position of a cell is its top-left corner, or the top corner of the diamond on isometric maps.
    [[nodiscard]] math::Vec2 cellToWorld(int column, int row) const noexcept;
    [[nodiscard]] std::array<int, 2> worldToCell(math::Vec2 position) const noexcept;

    // Converts object coordinates to world positions, which differ on isometric maps and on skewed oblique maps.
    [[nodiscard]] math::Vec2 objectToWorld(math::Vec2 position) const noexcept;

    // Returns the area the map grid covers in world coordinates, which starts left of or above the origin on oblique maps with a negative skew.
    [[nodiscard]] math::Rect getPixelBounds() const noexcept;

    std::string path;
    std::string type;
    Orientation orientation = Orientation::Orthogonal;
    RenderOrder renderOrder = RenderOrder::RightDown;
    int width = 0;
    int height = 0;
    math::Vec2 tileSize{};
    bool infinite = false;
    int hexSideLength = 0;
    bool staggerX = false;
    bool staggerEven = false;
    math::Vec2 skew{};
    math::Vec2 parallaxOrigin{};
    std::optional<math::Color> backgroundColor;
    Properties properties;
    std::vector<TilesetReference> tilesets;
    std::vector<Layer> layers;

  private:
    static const std::array<std::pair<std::string_view, Orientation>, 5> kOrientationNames;
    static const std::array<std::pair<std::string_view, RenderOrder>, 4> kRenderOrderNames;

    [[nodiscard]] static int positiveModulo(int value, int divisor) noexcept;
    static void collectImages(const Layer& layer, std::vector<Image>& images);
    static void attachLayer(Layer& layer, const TextureLoader& load);

    // Searches groups depth first and keeps the constness of the layers it walks.
    template <typename Layers> [[nodiscard]] static auto findIn(Layers& candidates, std::string_view name) noexcept -> decltype(&candidates.front());
};

} // namespace haylen::tiled
