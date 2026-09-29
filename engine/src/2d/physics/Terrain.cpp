#include "haylen/2d/physics/Terrain.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

#include "haylen/2d/physics/World.hpp"
#include "haylen/math/Geometry.hpp"
#include "haylen/math/MarchingSquares.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Polygon.hpp"

namespace haylen::physics2d {

Terrain::Terrain(World& owner, const Options& settings) : world(owner), options(settings) {
    if (settings.columns < 2 || settings.rows < 2 || settings.cellSize <= 0.0F || settings.chunkSize < 1 || settings.simplifyTolerance < 0.0F) {
        throw std::invalid_argument("A terrain needs at least 2 by 2 samples, a positive cell size and chunk size, and a tolerance of at least zero.");
    }
    samples.assign(static_cast<std::size_t>(settings.columns) * static_cast<std::size_t>(settings.rows), 0);
    chunkColumns = (settings.columns - 2) / settings.chunkSize + 1;
    chunkRows = (settings.rows - 2) / settings.chunkSize + 1;
    chunks.resize(static_cast<std::size_t>(chunkColumns) * static_cast<std::size_t>(chunkRows));
}

Terrain::~Terrain() {
    for (Chunk& chunk : chunks) {
        chunk.body.destroy();
    }
}

math::Rect Terrain::getBounds() const noexcept {
    return {options.origin.x, options.origin.y, static_cast<float>(options.columns - 1) * options.cellSize, static_cast<float>(options.rows - 1) * options.cellSize};
}

void Terrain::setSamples(std::span<const std::uint8_t> values) {
    if (values.size() != samples.size()) {
        throw std::invalid_argument("The terrain needs one value per sample.");
    }
    std::copy(values.begin(), values.end(), samples.begin());
    markDirty({0, 0, options.columns - 1, options.rows - 1});
}

std::uint8_t Terrain::getSample(int column, int row) const {
    if (column < 0 || row < 0 || column >= options.columns || row >= options.rows) {
        throw std::out_of_range("The sample is outside the terrain.");
    }
    return samples[static_cast<std::size_t>(row) * static_cast<std::size_t>(options.columns) + static_cast<std::size_t>(column)];
}

bool Terrain::isSolid(math::Vec2 point) const noexcept {
    const math::Vec2 local = (point - options.origin) / options.cellSize;
    if (local.x < 0.0F || local.y < 0.0F || local.x > static_cast<float>(options.columns - 1) || local.y > static_cast<float>(options.rows - 1)) {
        return false;
    }

    const int column = std::min(static_cast<int>(local.x), options.columns - 2);
    const int row = std::min(static_cast<int>(local.y), options.rows - 2);
    const float across = local.x - static_cast<float>(column);
    const float down = local.y - static_cast<float>(row);
    const auto at = [this](int x, int y) { return static_cast<float>(samples[static_cast<std::size_t>(y) * static_cast<std::size_t>(options.columns) + static_cast<std::size_t>(x)]); };
    const float top = math::Math::lerp(at(column, row), at(column + 1, row), across);
    const float bottom = math::Math::lerp(at(column, row + 1), at(column + 1, row + 1), across);
    return math::Math::lerp(top, bottom, down) >= 127.5F;
}

Terrain::Span Terrain::spanAround(const math::Rect& area) const noexcept {
    const math::Rect grown = area.expanded(options.cellSize);
    // clang-format off
    const auto sample = [this](float position, float origin, int count, bool upward) {
        const float at = (position - origin) / options.cellSize;
        return std::clamp(static_cast<int>(upward ? std::ceil(at) : std::floor(at)), 0, count - 1);
    };
    // clang-format on
    return {sample(grown.getLeft(), options.origin.x, options.columns, false), sample(grown.getTop(), options.origin.y, options.rows, false), sample(grown.getRight(), options.origin.x, options.columns, true), sample(grown.getBottom(), options.origin.y, options.rows, true)};
}

// A sample lies on the edge shared by two chunks when it sits on a multiple of the chunk size, so both chunks change.
void Terrain::markDirty(const Span& span) noexcept {
    const int size = options.chunkSize;
    const int firstChunkColumn = std::max(0, (std::max(0, span.firstColumn - 1)) / size);
    const int lastChunkColumn = std::min(chunkColumns - 1, span.lastColumn / size);
    const int firstChunkRow = std::max(0, (std::max(0, span.firstRow - 1)) / size);
    const int lastChunkRow = std::min(chunkRows - 1, span.lastRow / size);
    for (int chunkRow = firstChunkRow; chunkRow <= lastChunkRow; ++chunkRow) {
        for (int chunkColumn = firstChunkColumn; chunkColumn <= lastChunkColumn; ++chunkColumn) {
            chunks[static_cast<std::size_t>(chunkRow) * static_cast<std::size_t>(chunkColumns) + static_cast<std::size_t>(chunkColumn)].dirty = true;
        }
    }
}

// The coverage of a sample runs from 1 half a cell inside the shape to 0 half a cell outside, so the outline lands on the shape edge.
template <typename Distance> void Terrain::stamp(const math::Rect& area, bool adding, Distance&& signedDistance) {
    const Span span = spanAround(area);
    for (int row = span.firstRow; row <= span.lastRow; ++row) {
        for (int column = span.firstColumn; column <= span.lastColumn; ++column) {
            const math::Vec2 point = options.origin + math::Vec2{static_cast<float>(column), static_cast<float>(row)} * options.cellSize;
            const float coverage = math::Math::saturate(0.5F - signedDistance(point) / options.cellSize);
            const auto value = static_cast<std::uint8_t>(std::lround(coverage * 255.0F));
            std::uint8_t& sample = samples[static_cast<std::size_t>(row) * static_cast<std::size_t>(options.columns) + static_cast<std::size_t>(column)];
            sample = adding ? std::max(sample, value) : std::min(sample, static_cast<std::uint8_t>(255 - value));
        }
    }
    markDirty(span);
}

float Terrain::distanceToPolygon(std::span<const math::Vec2> polygon, math::Vec2 point) noexcept {
    float nearest = std::numeric_limits<float>::max();
    for (std::size_t index = 0; index < polygon.size(); ++index) {
        nearest = std::min(nearest, math::Geometry::distanceToSegment({polygon[index], polygon[(index + 1) % polygon.size()]}, point));
    }
    return math::Geometry::contains(polygon, point) ? -nearest : nearest;
}

void Terrain::fill(const math::Circle& circle) {
    stamp(circle.getBounds(), true, [&circle](math::Vec2 point) { return math::Vec2::distance(point, circle.center) - circle.radius; });
}

void Terrain::fill(std::span<const math::Vec2> polygon) {
    stamp(math::Geometry::bounds(polygon), true, [polygon](math::Vec2 point) { return distanceToPolygon(polygon, point); });
}

void Terrain::carve(const math::Circle& circle) {
    stamp(circle.getBounds(), false, [&circle](math::Vec2 point) { return math::Vec2::distance(point, circle.center) - circle.radius; });
}

void Terrain::carve(std::span<const math::Vec2> polygon) {
    stamp(math::Geometry::bounds(polygon), false, [polygon](math::Vec2 point) { return distanceToPolygon(polygon, point); });
}

std::vector<Explosion::Hit> Terrain::explode(const math::Circle& crater, const Explosion::Options& blast) {
    carve(crater);
    return Explosion::apply(world, blast);
}

std::vector<std::vector<math::Vec2>> Terrain::getOutlines() const {
    std::vector<std::vector<math::Vec2>> outlines;
    for (const Chunk& chunk : chunks) {
        outlines.insert(outlines.end(), chunk.outlines.begin(), chunk.outlines.end());
    }
    return outlines;
}

std::vector<Body> Terrain::getBodies() const {
    std::vector<Body> bodies;
    for (const Chunk& chunk : chunks) {
        if (chunk.body.isValid()) {
            bodies.push_back(chunk.body);
        }
    }
    return bodies;
}

std::size_t Terrain::getDirtyChunkCount() const noexcept {
    return static_cast<std::size_t>(std::count_if(chunks.begin(), chunks.end(), [](const Chunk& chunk) { return chunk.dirty; }));
}

std::size_t Terrain::update() {
    std::size_t rebuilt = 0;
    for (std::size_t chunk = 0; chunk < chunks.size(); ++chunk) {
        if (chunks[chunk].dirty) {
            rebuild(chunk);
            ++rebuilt;
        }
    }
    return rebuilt;
}

Terrain::Span Terrain::spanOf(std::size_t index) const noexcept {
    const int column = static_cast<int>(index % static_cast<std::size_t>(chunkColumns)) * options.chunkSize;
    const int row = static_cast<int>(index / static_cast<std::size_t>(chunkColumns)) * options.chunkSize;
    return {column, row, std::min(column + options.chunkSize, options.columns - 1), std::min(row + options.chunkSize, options.rows - 1)};
}

// Chunks share their edge samples, and outlines close along them, so neighboring chunks meet exactly at their common edge.
void Terrain::rebuild(std::size_t index) {
    Chunk& chunk = chunks[index];
    const Span span = spanOf(index);
    const int width = span.lastColumn - span.firstColumn + 1;
    const int height = span.lastRow - span.firstRow + 1;
    std::vector<float> field;
    field.reserve(static_cast<std::size_t>(width) * static_cast<std::size_t>(height));
    for (int row = span.firstRow; row <= span.lastRow; ++row) {
        for (int column = span.firstColumn; column <= span.lastColumn; ++column) {
            field.push_back(static_cast<float>(samples[static_cast<std::size_t>(row) * static_cast<std::size_t>(options.columns) + static_cast<std::size_t>(column)]) / 255.0F);
        }
    }

    const math::Vec2 corner = options.origin + math::Vec2{static_cast<float>(span.firstColumn), static_cast<float>(span.firstRow)} * options.cellSize;
    const math::Rect area{corner.x, corner.y, static_cast<float>(width - 1) * options.cellSize, static_cast<float>(height - 1) * options.cellSize};
    chunk.outlines.clear();
    for (const std::vector<math::Vec2>& traced : math::MarchingSquares::trace(field, width, height, {.threshold = 0.5F, .spacing = options.cellSize, .origin = corner})) {
        std::vector<math::Vec2> outline = simplify(traced, area);
        // Specks smaller than a quarter cell carry no useful collision.
        if (outline.size() < 3 || std::fabs(math::Geometry::signedArea(outline)) < options.cellSize * options.cellSize * 0.25F) {
            continue;
        }
        // Chain loops need four points, so a triangle gains the middle of its longest edge.
        if (outline.size() == 3) {
            std::size_t longest = 0;
            for (std::size_t edge = 1; edge < 3; ++edge) {
                if (math::Vec2::distanceSquared(outline[edge], outline[(edge + 1) % 3]) > math::Vec2::distanceSquared(outline[longest], outline[(longest + 1) % 3])) {
                    longest = edge;
                }
            }
            outline.insert(outline.begin() + static_cast<std::ptrdiff_t>(longest) + 1, (outline[longest] + outline[(longest + 1) % 3]) * 0.5F);
        }
        chunk.outlines.push_back(std::move(outline));
    }

    chunk.body.destroy();
    chunk.body = {};
    if (!chunk.outlines.empty()) {
        chunk.body = world.createBody({.type = Body::Type::Static});
        for (const std::vector<math::Vec2>& outline : chunk.outlines) {
            chunk.body.addChain(outline, true, options.shape);
        }
    }
    chunk.dirty = false;
}

std::vector<math::Vec2> Terrain::simplify(const std::vector<math::Vec2>& outline, const math::Rect& area) const {
    const float epsilon = options.cellSize * 1e-3F;
    // clang-format off
    const auto edges = [&](math::Vec2 point) {
        return (std::fabs(point.x - area.getLeft()) < epsilon ? 1 : 0) + (std::fabs(point.x - area.getRight()) < epsilon ? 1 : 0) + (std::fabs(point.y - area.getTop()) < epsilon ? 1 : 0) + (std::fabs(point.y - area.getBottom()) < epsilon ? 1 : 0);
    };
    // clang-format on

    // Points where the outline enters or leaves the chunk edge, and chunk corners, stay where they are.
    std::vector<std::size_t> anchors;
    const std::size_t count = outline.size();
    for (std::size_t index = 0; index < count; ++index) {
        const int here = edges(outline[index]);
        const bool entering = here > 0 && (edges(outline[(index + count - 1) % count]) == 0 || edges(outline[(index + 1) % count]) == 0);
        if (entering || here > 1) {
            anchors.push_back(index);
        }
    }
    if (anchors.empty()) {
        return math::Polygon::simplify(outline, options.simplifyTolerance, true);
    }

    std::vector<math::Vec2> result;
    for (std::size_t anchor = 0; anchor < anchors.size(); ++anchor) {
        const std::size_t from = anchors[anchor];
        const std::size_t to = anchor + 1 < anchors.size() ? anchors[anchor + 1] : anchors.front() + count;
        std::vector<math::Vec2> run;
        for (std::size_t index = from; index <= to; ++index) {
            run.push_back(outline[index % count]);
        }
        const std::vector<math::Vec2> simplified = math::Polygon::simplify(run, options.simplifyTolerance, false);
        result.insert(result.end(), simplified.begin(), simplified.end() - 1);
    }
    return result;
}

} // namespace haylen::physics2d
