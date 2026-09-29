#include "text/DistanceField.hpp"

#include <algorithm>
#include <limits>
#include <utility>

#include <core/ShapeDistanceFinder.h>
#include <msdfgen.h>

namespace haylen::text {

// The outline in pixels, with y pointing down like the rows of the field. Lines, quadratic curves and cubic curves keep their exact shape. A malformed CFF outline may draw before its first move, which starts a contour at the origin.
msdfgen::Shape DistanceField::readShape(const stbtt_fontinfo& font, int glyph, float scale) {
    stbtt_vertex* vertices = nullptr;
    const int count = stbtt_GetGlyphShape(&font, glyph, &vertices);
    msdfgen::Shape shape;
    msdfgen::Contour* contour = nullptr;
    msdfgen::Point2 pen;
    for (int index = 0; index < count; ++index) {
        const stbtt_vertex& vertex = vertices[index];
        const msdfgen::Point2 end(vertex.x * scale, -vertex.y * scale);
        const msdfgen::Point2 control(vertex.cx * scale, -vertex.cy * scale);
        if (contour == nullptr || (vertex.type == STBTT_vmove && !contour->edges.empty())) {
            contour = &shape.addContour();
        }
        if (vertex.type == STBTT_vline && end != pen) {
            contour->addEdge(msdfgen::EdgeHolder(pen, end));
        } else if (vertex.type == STBTT_vcurve) {
            contour->addEdge(msdfgen::EdgeHolder(pen, control, end));
        } else if (vertex.type == STBTT_vcubic) {
            contour->addEdge(msdfgen::EdgeHolder(pen, control, msdfgen::Point2(vertex.cx1 * scale, -vertex.cy1 * scale), end));
        }
        pen = end;
    }
    stbtt_FreeShape(&font, vertices);
    if (!shape.contours.empty() && shape.contours.back().edges.empty()) {
        shape.contours.pop_back();
    }
    return shape;
}

// Contours that wind the same way and share part of their boxes may overlap, which only the slower way of combining contours keeps whole inside, while holes and separate parts never need it.
bool DistanceField::mayOverlap(const msdfgen::Shape& shape) {
    constexpr double kFar = std::numeric_limits<double>::max();
    std::vector<std::pair<msdfgen::Shape::Bounds, int>> contours;
    for (const msdfgen::Contour& contour : shape.contours) {
        msdfgen::Shape::Bounds box{kFar, kFar, -kFar, -kFar};
        contour.bound(box.l, box.b, box.r, box.t);
        contours.emplace_back(box, contour.winding());
    }
    for (std::size_t first = 0; first < contours.size(); ++first) {
        for (std::size_t second = first + 1; second < contours.size(); ++second) {
            const auto& [a, aWinding] = contours[first];
            const auto& [b, bWinding] = contours[second];
            if (aWinding == bWinding && a.l < b.r && b.l < a.r && a.b < b.t && b.b < a.t) {
                return true;
            }
        }
    }
    return false;
}

std::optional<DistanceField> DistanceField::build(const stbtt_fontinfo& font, int glyph, float scale, int spread) {
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    stbtt_GetGlyphBitmapBox(&font, glyph, scale, scale, &left, &top, &right, &bottom);
    if (left == right || top == bottom) {
        return std::nullopt;
    }
    msdfgen::Shape shape = readShape(font, glyph, scale);
    if (shape.contours.empty()) {
        return std::nullopt;
    }
    shape.normalize();
    shape.setYAxisOrientation(msdfgen::Y_DOWNWARD);

    // TrueType and CFF outlines wind their contours in opposite directions, so a point outside the glyph tells which way this one fills.
    const msdfgen::Shape::Bounds bounds = shape.getBounds();
    if (msdfgen::SimpleTrueShapeDistanceFinder::oneShotDistance(shape, msdfgen::Point2(bounds.l - 1.0, bounds.b - 1.0)) > 0.0) {
        for (msdfgen::Contour& outline : shape.contours) {
            outline.reverse();
        }
    }

    // Each pixel samples its center, and distances across twice the spread fill the range of a byte.
    DistanceField field{.width = right - left + spread * 2, .height = bottom - top + spread * 2, .offsetX = left - spread, .offsetY = top - spread};
    std::vector<float> distances(static_cast<std::size_t>(field.width) * static_cast<std::size_t>(field.height));
    const msdfgen::BitmapSection<float, 1> output(distances.data(), field.width, field.height, msdfgen::Y_DOWNWARD);
    const msdfgen::Projection projection(msdfgen::Vector2(1.0), msdfgen::Vector2(-field.offsetX, -field.offsetY));
    msdfgen::generateSDF(output, shape, msdfgen::SDFTransformation(projection, msdfgen::Range(spread * 2.0)), msdfgen::GeneratorConfig(mayOverlap(shape)));
    field.pixels.resize(distances.size());
    std::ranges::transform(distances, field.pixels.begin(), [](float distance) { return msdfgen::pixelFloatToByte(distance); });
    return field;
}

} // namespace haylen::text
