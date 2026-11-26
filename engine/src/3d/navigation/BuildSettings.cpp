#include "haylen/3d/navigation/BuildSettings.hpp"

#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace haylen::navigation3d {

int BuildSettings::getTileCells() const {
    return static_cast<int>(std::lround(tileSize / cellSize));
}

void BuildSettings::requireNonNegative(float value, std::string_view name) {
    if (!std::isfinite(value) || value < 0.0F) {
        throw std::invalid_argument("The setting \"" + std::string(name) + "\" of a navigation mesh must be a finite number of at least 0.");
    }
}

void BuildSettings::validate() const {
    if (!std::isfinite(cellSize) || cellSize < 0.01F || cellSize > 100.0F) {
        throw std::invalid_argument("The setting \"cellSize\" of a navigation mesh must be a number from 0.01 to 100 meters.");
    }
    if (!std::isfinite(cellHeight) || cellHeight < 0.01F || cellHeight > 100.0F) {
        throw std::invalid_argument("The setting \"cellHeight\" of a navigation mesh must be a number from 0.01 to 100 meters.");
    }
    if (!std::isfinite(agentHeight) || agentHeight < cellHeight * 3.0F) {
        throw std::invalid_argument("The setting \"agentHeight\" of a navigation mesh must be at least three times its \"cellHeight\".");
    }
    requireNonNegative(agentRadius, "agentRadius");
    requireNonNegative(agentMaxClimb, "agentMaxClimb");
    if (!std::isfinite(agentMaxSlope) || agentMaxSlope < 0.0F || agentMaxSlope >= std::numbers::pi_v<float> * 0.5F) {
        throw std::invalid_argument("The setting \"agentMaxSlope\" of a navigation mesh must be an angle in radians from 0 up to, but not including, half of pi.");
    }
    requireNonNegative(regionMinArea, "regionMinArea");
    requireNonNegative(regionMergeArea, "regionMergeArea");
    requireNonNegative(edgeMaxLength, "edgeMaxLength");
    requireNonNegative(edgeMaxError, "edgeMaxError");
    requireNonNegative(detailSampleDistance, "detailSampleDistance");
    requireNonNegative(detailSampleMaxError, "detailSampleMaxError");
    if (polygonMaxVertices < kMinPolygonVertices || polygonMaxVertices > kMaxPolygonVertices) {
        throw std::invalid_argument("The setting \"polygonMaxVertices\" of a navigation mesh must be a whole number from 3 to 6.");
    }
    if (!std::isfinite(tileSize) || tileSize <= 0.0F) {
        throw std::invalid_argument("The setting \"tileSize\" of a navigation mesh must be a finite number of meters above zero.");
    }

    // A tile with obstacles keeps its voxels in layers of at most 255 voxels on a side, its border included.
    const int cells = getTileCells();
    const int border = static_cast<int>(std::ceil(agentRadius / cellSize)) + 3;
    const int limit = obstacles ? 255 - border * 2 : static_cast<int>(kMaxTileCells);
    if (cells < 8 || cells > limit) {
        throw std::invalid_argument("The setting \"tileSize\" of a navigation mesh must hold from 8 to " + std::to_string(limit) + " voxels of its \"cellSize\" on a side, but it holds " + std::to_string(cells) + ".");
    }
    if (obstacles && (maxObstacles == 0 || maxObstacles > kMaxObstacles)) {
        throw std::invalid_argument("The setting \"maxObstacles\" of a navigation mesh must be a whole number from 1 to 65536.");
    }
    if (bounds && (bounds->isEmpty() || !bounds->min.isFinite() || !bounds->max.isFinite())) {
        throw std::invalid_argument("The setting \"bounds\" of a navigation mesh must be a box of finite numbers whose largest corner lies at or above its smallest corner.");
    }
}

} // namespace haylen::navigation3d
