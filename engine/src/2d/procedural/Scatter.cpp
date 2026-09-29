#include "haylen/2d/procedural/Scatter.hpp"

#include <algorithm>
#include <cmath>
#include <optional>
#include <stdexcept>

#include "haylen/math/Math.hpp"
#include "haylen/math/PoissonDisk.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/WeightedChoice.hpp"

namespace haylen::procedural2d {

bool Scatter::isExcluded(const Options& options, math::Vec2 point) noexcept {
    return std::any_of(options.exclusions.begin(), options.exclusions.end(), [point](const Region& zone) { return zone.contains(point); });
}

bool Scatter::keepsByDensity(const Options& options, math::Vec2 point, math::Random& random) {
    return !options.densityMap || random.chance(math::Math::saturate(options.densityMap(point)));
}

void Scatter::requirePointCount(double count) {
    if (count > kMaxPoints) {
        throw std::invalid_argument("Scattering over this region would need more than 16777216 points, so the density must be lower or the spacing larger.");
    }
}

// The fraction of the expected count becomes one more point with that probability, so the count matches the area on average.
std::vector<math::Vec2> Scatter::placeRandom(const Region& region, const Options& options, math::Random& random) {
    const float expected = region.getArea() * options.density;
    requirePointCount(expected);
    const float whole = std::floor(expected);
    const auto count = static_cast<std::size_t>(whole) + (random.chance(expected - whole) ? 1U : 0U);

    std::vector<math::Vec2> points;
    points.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        const math::Vec2 point = region.getRandomPoint(random);
        if (!isExcluded(options, point) && keepsByDensity(options, point, random)) {
            points.push_back(point);
        }
    }
    return points;
}

std::vector<math::Vec2> Scatter::placeGrid(const Region& region, const Options& options, math::Random& random) {
    const math::Rect bounds = region.getBounds();
    const float columns = std::ceil(bounds.width / options.spacing);
    const float rows = std::ceil(bounds.height / options.spacing);
    if (!(columns > 0.0F && rows > 0.0F)) {
        return {};
    }
    requirePointCount(static_cast<double>(columns) * rows);
    const float reach = options.jitter * options.spacing * 0.5F;

    std::vector<math::Vec2> points;
    for (int row = 0; row < static_cast<int>(rows); ++row) {
        for (int column = 0; column < static_cast<int>(columns); ++column) {
            const math::Vec2 center = bounds.getMin() + math::Vec2{static_cast<float>(column) + 0.5F, static_cast<float>(row) + 0.5F} * options.spacing;
            const math::Vec2 point = center + math::Vec2{random.range(-reach, reach), random.range(-reach, reach)};
            if (region.contains(point) && !isExcluded(options, point) && keepsByDensity(options, point, random)) {
                points.push_back(point);
            }
        }
    }
    return points;
}

std::vector<math::Vec2> Scatter::placePoisson(const Region& region, const Options& options, math::Random& random) {
    math::PoissonDisk::Options poisson{.area = region.getBounds(), .minimumDistance = options.spacing, .attempts = options.attempts};
    poisson.accept = [&](math::Vec2 point) { return region.contains(point) && !isExcluded(options, point); };
    if (options.densityMap) {
        poisson.maximumDistance = options.maximumSpacing;
        poisson.distance = [&](math::Vec2 point) { return math::Math::lerp(options.maximumSpacing, options.spacing, math::Math::saturate(options.densityMap(point))); };
    }
    return math::PoissonDisk::sample(poisson, random);
}

std::vector<math::Vec2> Scatter::place(const Region& region, const Options& options, math::Random& random) {
    switch (options.method) {
    case Method::Grid:
        return placeGrid(region, options, random);
    case Method::Poisson:
        return placePoisson(region, options, random);
    case Method::Random:
        break;
    }
    return placeRandom(region, options, random);
}

std::vector<Scatter::Point> Scatter::generate(const Region& region, const Options& options, math::Random& random) {
    if (!(options.density >= 0.0F) || !std::isfinite(options.density) || !(options.spacing > 0.0F) || !std::isfinite(options.spacing)) {
        throw std::invalid_argument("Scattering needs a finite density of at least zero and a finite positive spacing.");
    }
    if (options.method == Method::Poisson && options.densityMap && !(options.maximumSpacing >= options.spacing && std::isfinite(options.maximumSpacing))) {
        throw std::invalid_argument("Poisson scattering with a density map needs a finite maximumSpacing of at least the spacing.");
    }
    if (!options.layers.empty() && !options.biome) {
        throw std::invalid_argument("Scatter layers need a biome function.");
    }

    std::optional<math::WeightedChoice> types;
    if (!options.weights.empty()) {
        types.emplace(options.weights);
    }
    std::vector<math::WeightedChoice> layerTypes;
    for (const Layer& layer : options.layers) {
        layerTypes.emplace_back(layer.weights);
    }

    std::vector<Point> result;
    for (const math::Vec2 position : place(region, options, random)) {
        if (options.layers.empty()) {
            result.push_back({position, types ? static_cast<std::uint32_t>(types->pick(random)) : 0U});
            continue;
        }

        const float value = options.biome(position);
        const auto layer = std::find_if(options.layers.begin(), options.layers.end(), [value](const Layer& candidate) { return value >= candidate.minimum && value <= candidate.maximum; });
        if (layer != options.layers.end()) {
            const auto index = static_cast<std::size_t>(layer - options.layers.begin());
            result.push_back({position, static_cast<std::uint32_t>(layerTypes[index].pick(random))});
        }
    }
    return result;
}

} // namespace haylen::procedural2d
