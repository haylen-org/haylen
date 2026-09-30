#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/2d/procedural/Region.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::math {
class Random;
}

namespace haylen::procedural2d {

// Places things such as trees, rocks and enemies over a region, keeps them out of exclusion zones and gives each one a type by weight, optionally with the weights of the biome layer found at its position. The same generator state always gives the same points.
class Scatter final {
  public:
    // The method `Random` places the area times the density in points, `Grid` places one point per cell of `spacing` size moved randomly by up to `jitter` times half the spacing, and `Poisson` keeps points at least `spacing` apart.
    enum class Method : std::uint8_t {
        Random,
        Grid,
        Poisson,
    };

    // A layer gives its type weights to the points whose biome value lies between its minimum and maximum. Points outside every layer are dropped.
    struct Layer {
        float minimum = -1.0F;
        float maximum = 1.0F;
        std::vector<float> weights;
    };

    // The density map returns 0 to 1 at a point. The methods `Random` and `Grid` keep each point with that probability, and `Poisson` spaces points from `spacing` where the map is 1 to `maximumSpacing` where it is 0, so `Poisson` with a density map needs a `maximumSpacing` of at least the spacing. Layers need a biome function, such as fractal noise.
    struct Options {
        Method method = Method::Random;
        float density = 0.001F;
        float spacing = 32.0F;
        float maximumSpacing = 0.0F;
        float jitter = 1.0F;
        int attempts = 30;
        std::function<float(math::Vec2)> densityMap;
        std::vector<Region> exclusions;
        std::vector<float> weights;
        std::function<float(math::Vec2)> biome;
        std::vector<Layer> layers;
    };

    struct Point {
        math::Vec2 position{};
        std::uint32_t type = 0;
    };

    // Throws `std::invalid_argument` for a density that is negative or not finite, a spacing that is not positive and finite, a Poisson density map without a `maximumSpacing` of at least the spacing, a region that would need more than 16777216 points or Poisson grid cells, layers without a biome function, or weights that `math::WeightedChoice` rejects.
    [[nodiscard]] static std::vector<Point> generate(const Region& region, const Options& options, math::Random& random);

    [[nodiscard]] static std::optional<Method> methodFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view methodName(Method value) noexcept;

  private:
    static const std::array<std::pair<std::string_view, Method>, 3> kMethodNames;
    static constexpr double kMaxPoints = 16777216.0;

    [[nodiscard]] static std::vector<math::Vec2> place(const Region& region, const Options& options, math::Random& random);
    [[nodiscard]] static std::vector<math::Vec2> placeRandom(const Region& region, const Options& options, math::Random& random);
    [[nodiscard]] static std::vector<math::Vec2> placeGrid(const Region& region, const Options& options, math::Random& random);
    [[nodiscard]] static std::vector<math::Vec2> placePoisson(const Region& region, const Options& options, math::Random& random);
    [[nodiscard]] static bool isExcluded(const Options& options, math::Vec2 point) noexcept;
    [[nodiscard]] static bool keepsByDensity(const Options& options, math::Vec2 point, math::Random& random);
    static void requirePointCount(double count);
};

} // namespace haylen::procedural2d
