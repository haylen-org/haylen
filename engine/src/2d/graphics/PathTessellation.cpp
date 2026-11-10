#include "2d/graphics/PathTessellation.hpp"

#include <algorithm>
#include <clipper2/clipper.h>
#include <cmath>
#include <span>
#include <stdexcept>

#include "2d/graphics/PolygonMesh.hpp"
#include "2d/graphics/StrokeBuilder.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Geometry.hpp"

namespace haylen::graphics2d {

void PathTessellation::update(const Path& path) {
    if (revision != 0 && std::ranges::equal(path.getVerbs(), verbs) && std::ranges::equal(path.getPoints(), points) && std::ranges::equal(path.getArcs(), arcs)) {
        return;
    }
    verbs.assign(path.getVerbs().begin(), path.getVerbs().end());
    points.assign(path.getPoints().begin(), path.getPoints().end());
    arcs.assign(path.getArcs().begin(), path.getArcs().end());
    ++revision;
}

bool PathTessellation::isCurrent(const Build& build, float pixel) const noexcept {
    return build.revision == revision && build.pixel == pixel;
}

// A draw that matches no build makes the build drawn longest ago again, where a build never made counts as the oldest.
const PathTessellation::Triangles& PathTessellation::getFill(const Path& path, Path::FillRule rule, float pixel, PolygonMesh& polygons) {
    update(path);
    ++draws;
    FillBuild* replaced = &fillBuilds.front();
    for (FillBuild& kept : fillBuilds) {
        if (isCurrent(kept.build, pixel) && kept.rule == rule) {
            kept.build.used = draws;
            return kept.build.triangles;
        }
        if (kept.build.used < replaced->build.used) {
            replaced = &kept;
        }
    }

    Build& build = replaced->build;
    build.revision = 0;
    path.flatten(pixel * kFlatness, build.outlines);
    replaced->rule = rule;
    buildFill(*replaced, pixel, polygons);
    build.revision = revision;
    build.pixel = pixel;
    build.used = draws;
    return build.triangles;
}

// Every outline fills as if it closes. The union of the outlines by the rule gives simple outlines and holes that touch at most at corners, whose edges all fade, on a grid fine enough that its steps never show.
void PathTessellation::buildFill(FillBuild& fill, float pixel, PolygonMesh& polygons) {
    Triangles& triangles = fill.build.triangles;
    const Path::Outlines& outlines = fill.build.outlines;
    triangles.vertices.clear();
    triangles.indices.clear();
    if (outlines.counts.empty()) {
        return;
    }

    const math::Rect bounds = math::Geometry::bounds(outlines.points);
    const double extent = std::max({std::fabs(static_cast<double>(bounds.getLeft())), std::fabs(static_cast<double>(bounds.getRight())), std::fabs(static_cast<double>(bounds.getTop())), std::fabs(static_cast<double>(bounds.getBottom())), 1.0});
    const double scale = std::min(kGridSteps / static_cast<double>(pixel), kGridLimit / extent);
    Clipper2Lib::Paths64 subjects;
    subjects.reserve(outlines.counts.size());
    std::size_t first = 0;
    for (const std::uint32_t count : outlines.counts) {
        Clipper2Lib::Path64& subject = subjects.emplace_back();
        subject.reserve(count);
        for (std::size_t index = first; index < first + count; ++index) {
            const math::Vec2 point = outlines.points[index];
            subject.emplace_back(std::llround(static_cast<double>(point.x) * scale), std::llround(static_cast<double>(point.y) * scale));
        }
        first += count;
    }

    Clipper2Lib::Clipper64 clipper;
    clipper.PreserveCollinear(false);
    clipper.AddSubject(subjects);
    Clipper2Lib::Paths64 solution;
    if (!clipper.Execute(Clipper2Lib::ClipType::Union, fill.rule == Path::FillRule::EvenOdd ? Clipper2Lib::FillRule::EvenOdd : Clipper2Lib::FillRule::NonZero, solution)) {
        throw std::runtime_error("The outlines of the path could not be resolved into a fill.");
    }

    simplePoints.clear();
    simpleCounts.clear();
    for (const Clipper2Lib::Path64& outline : solution) {
        for (const Clipper2Lib::Point64& point : outline) {
            simplePoints.push_back({static_cast<float>(static_cast<double>(point.x) / scale), static_cast<float>(static_cast<double>(point.y) / scale)});
        }
        simpleCounts.push_back(static_cast<std::uint32_t>(outline.size()));
    }
    polygons.build(simplePoints, simpleCounts, math::Color::white().toRgba8(), pixel);
    triangles.vertices.assign(polygons.getVertices().begin(), polygons.getVertices().end());
    triangles.indices.assign(polygons.getIndices().begin(), polygons.getIndices().end());

    const math::Vec2 size{std::max(bounds.width, 1.0e-6F), std::max(bounds.height, 1.0e-6F)};
    for (GpuVertex& vertex : triangles.vertices) {
        vertex.uv[0] = (vertex.position[0] - bounds.x) / size.x;
        vertex.uv[1] = (vertex.position[1] - bounds.y) / size.y;
    }
}

bool PathTessellation::isLook(const StrokeLook& look, const Stroke& value, math::Vec2 textureSize) noexcept {
    return value.width == look.width && value.join == look.join && value.cap == look.cap && value.miterLimit == look.miterLimit && value.textureMode == look.textureMode && value.textureLength == look.textureLength && textureSize == look.textureSize && value.dashOffset == look.dashOffset && std::ranges::equal(value.dashes, look.dashes);
}

const PathTessellation::Triangles& PathTessellation::getStroke(const Path& path, const Stroke& value, float pixel, math::Vec2 textureSize, StrokeBuilder& strokes) {
    if (!value.widths.empty() || !value.colors.empty()) {
        throw std::invalid_argument("A stroke of a path takes one width and one color, without widths or colors for each point.");
    }
    update(path);
    ++draws;
    StrokeBuild* replaced = &strokeBuilds.front();
    for (StrokeBuild& kept : strokeBuilds) {
        if (isCurrent(kept.build, pixel) && isLook(kept.look, value, textureSize)) {
            kept.build.used = draws;
            return kept.build.triangles;
        }
        if (kept.build.used < replaced->build.used) {
            replaced = &kept;
        }
    }

    Build& build = replaced->build;
    build.revision = 0;
    path.flatten(pixel * kFlatness, build.outlines);
    buildStroke(*replaced, value, pixel, textureSize, strokes);
    build.revision = revision;
    build.pixel = pixel;
    build.used = draws;
    return build.triangles;
}

// Every outline strokes on its own, white and with rows that start at the top, which the draw tints and flips for its texture.
void PathTessellation::buildStroke(StrokeBuild& stroke, const Stroke& value, float pixel, math::Vec2 textureSize, StrokeBuilder& strokes) {
    const Path::Outlines& outlines = stroke.build.outlines;
    Triangles& triangles = stroke.build.triangles;
    triangles.vertices.clear();
    triangles.indices.clear();
    Stroke white = value;
    white.color = math::Color::white();
    white.texture = {};
    std::size_t first = 0;
    for (std::size_t outline = 0; outline < outlines.counts.size(); ++outline) {
        const std::uint32_t count = outlines.counts[outline];
        white.closed = outlines.closed[outline] != 0;
        strokes.build(std::span(outlines.points).subspan(first, count), white, pixel, textureSize, false);
        first += count;

        const auto base = static_cast<std::uint32_t>(triangles.vertices.size());
        triangles.vertices.insert(triangles.vertices.end(), strokes.getVertices().begin(), strokes.getVertices().end());
        for (const std::uint32_t index : strokes.getIndices()) {
            triangles.indices.push_back(base + index);
        }
    }

    StrokeLook& look = stroke.look;
    look.width = value.width;
    look.join = value.join;
    look.cap = value.cap;
    look.miterLimit = value.miterLimit;
    look.textureMode = value.textureMode;
    look.textureLength = value.textureLength;
    look.textureSize = textureSize;
    look.dashOffset = value.dashOffset;
    look.dashes.assign(value.dashes.begin(), value.dashes.end());
}

} // namespace haylen::graphics2d
