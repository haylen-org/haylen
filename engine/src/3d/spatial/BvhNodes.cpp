#include "haylen/3d/spatial/BvhNodes.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace haylen::spatial3d {

void BvhNodes::build(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order) {
    if (boxes.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("A bounding volume hierarchy holds at most 4294967295 boxes.");
    }

    nodes.clear();
    depth = 0;
    order.resize(boxes.size());
    std::iota(order.begin(), order.end(), std::uint32_t{0});
    if (boxes.empty()) {
        return;
    }

    nodes.reserve(boxes.size() * 2);
    split(boxes, order, 0, static_cast<std::uint32_t>(boxes.size()), 0);
}

void BvhNodes::clear() noexcept {
    nodes.clear();
    depth = 0;
}

std::uint32_t BvhNodes::split(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order, std::uint32_t first, std::uint32_t count, int level) {
    const auto index = static_cast<std::uint32_t>(nodes.size());
    nodes.emplace_back();
    depth = std::max(depth, level);

    math::Box box = boxes[order[first]];
    for (std::uint32_t position = first + 1; position < first + count; ++position) {
        box = box.merged(boxes[order[position]]);
    }

    nodes[index].box = box;
    nodes[index].first = first;
    nodes[index].count = count;
    if (count <= kLeafSize) {
        return index;
    }

    const std::uint32_t leftCount = partition(boxes, order, first, count, level);
    split(boxes, order, first, leftCount, level + 1);
    const std::uint32_t second = split(boxes, order, first + leftCount, count - leftCount, level + 1);
    nodes[index].second = second;
    return index;
}

std::uint32_t BvhNodes::splitAtMedian(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order, std::uint32_t first, std::uint32_t count, int axis) {
    const std::uint32_t half = count / 2;
    const auto offset = static_cast<std::ptrdiff_t>(first);
    const auto middle = static_cast<std::ptrdiff_t>(half);
    const auto length = static_cast<std::ptrdiff_t>(count);
    const auto begin = order.begin() + offset;
    std::nth_element(begin, begin + middle, begin + length, [&boxes, axis](std::uint32_t lhs, std::uint32_t rhs) { return along(centroidOf(boxes[lhs]), axis) < along(centroidOf(boxes[rhs]), axis); });
    return half;
}

std::uint32_t BvhNodes::partition(std::span<const math::Box> boxes, std::vector<std::uint32_t>& order, std::uint32_t first, std::uint32_t count, int level) {
    const auto offset = static_cast<std::ptrdiff_t>(first);
    const auto length = static_cast<std::ptrdiff_t>(count);
    const auto begin = order.begin() + offset;
    const auto end = begin + length;
    math::Box centroids{centroidOf(boxes[*begin]), centroidOf(boxes[*begin])};
    for (auto position = begin + 1; position != end; ++position) {
        centroids = centroids.merged(centroidOf(boxes[*position]));
    }

    const math::Vec3 extent = centroids.getSize();
    const int axis = extent.x >= extent.y && extent.x >= extent.z ? 0 : (extent.y >= extent.z ? 1 : 2);
    const float low = along(centroids.min, axis);
    const float scale = static_cast<float>(kBins) / along(extent, axis);

    // Boxes whose centers coincide have no axis to split along, and deep levels take the median, so the depth stays bounded.
    if (!std::isfinite(scale) || level >= kMedianLevel) {
        return splitAtMedian(boxes, order, first, count, axis);
    }

    // The surface area heuristic sorts the centers into bins and picks the boundary between bins where the two sides cost the least, each side costing its area times its boxes.
    std::array<std::uint32_t, kBins> binCounts{};
    std::array<math::Box, kBins> binBoxes{};
    const auto binOf = [&](std::uint32_t box) { return std::min(kBins - 1, static_cast<std::size_t>((along(centroidOf(boxes[box]), axis) - low) * scale)); };
    for (auto position = begin; position != end; ++position) {
        const std::size_t bin = binOf(*position);
        binBoxes[bin] = binCounts[bin] == 0 ? boxes[*position] : binBoxes[bin].merged(boxes[*position]);
        ++binCounts[bin];
    }

    std::array<float, kBins - 1> leftCosts{};
    math::Box leftBox;
    std::uint32_t leftCount = 0;
    for (std::size_t bin = 0; bin + 1 < kBins; ++bin) {
        if (binCounts[bin] > 0) {
            leftBox = leftCount == 0 ? binBoxes[bin] : leftBox.merged(binBoxes[bin]);
            leftCount += binCounts[bin];
        }

        leftCosts[bin] = leftCount == 0 ? 0.0F : halfSurface(leftBox) * static_cast<float>(leftCount);
    }

    float bestCost = std::numeric_limits<float>::infinity();
    std::size_t bestBin = 0;
    math::Box rightBox;
    std::uint32_t rightCount = 0;
    for (std::size_t bin = kBins - 1; bin > 0; --bin) {
        if (binCounts[bin] > 0) {
            rightBox = rightCount == 0 ? binBoxes[bin] : rightBox.merged(binBoxes[bin]);
            rightCount += binCounts[bin];
        }

        const float cost = leftCosts[bin - 1] + halfSurface(rightBox) * static_cast<float>(rightCount);
        if (rightCount > 0 && rightCount < count && cost < bestCost) {
            bestCost = cost;
            bestBin = bin;
        }
    }

    // Boxes too large for the areas of floats leave no finite cost to compare.
    if (!std::isfinite(bestCost)) {
        return splitAtMedian(boxes, order, first, count, axis);
    }

    const auto middle = std::partition(begin, end, [&](std::uint32_t box) { return binOf(box) < bestBin; });
    return static_cast<std::uint32_t>(middle - begin);
}

} // namespace haylen::spatial3d
