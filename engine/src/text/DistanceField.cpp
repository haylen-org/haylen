#include "text/DistanceField.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

#include <core/ShapeDistanceFinder.h>
#include <msdfgen.h>

#include "math/HalfFloat.hpp"
#include "text/FontFile.hpp"

namespace haylen::text {

// The functions never change once made, so every thread shares them for as long as the process runs.
hb_draw_funcs_t* DistanceField::getOutlineFuncs() {
    static hb_draw_funcs_t* const funcs = makeOutlineFuncs();
    return funcs;
}

hb_draw_funcs_t* DistanceField::makeOutlineFuncs() {
    hb_draw_funcs_t* funcs = hb_draw_funcs_create();
    hb_draw_funcs_set_move_to_func(funcs, &moveTo, nullptr, nullptr);
    hb_draw_funcs_set_line_to_func(funcs, &lineTo, nullptr, nullptr);
    hb_draw_funcs_set_quadratic_to_func(funcs, &quadraticTo, nullptr, nullptr);
    hb_draw_funcs_set_cubic_to_func(funcs, &cubicTo, nullptr, nullptr);
    hb_draw_funcs_make_immutable(funcs);
    return funcs;
}

// A move starts a contour, unless the contour before it has no edge yet, and HarfBuzz closes every contour with a line back to its start.
void DistanceField::moveTo(hb_draw_funcs_t* /*funcs*/, void* outline, hb_draw_state_t* /*state*/, float toX, float toY, void* /*user*/) {
    auto& reading = *static_cast<Outline*>(outline);
    if (reading.contour == nullptr || !reading.contour->edges.empty()) {
        reading.contour = &reading.shape.addContour();
    }
    reading.penX = toX * reading.scale;
    reading.penY = -toY * reading.scale;
}

void DistanceField::lineTo(hb_draw_funcs_t* /*funcs*/, void* outline, hb_draw_state_t* /*state*/, float toX, float toY, void* /*user*/) {
    auto& reading = *static_cast<Outline*>(outline);
    const msdfgen::Point2 pen(reading.penX, reading.penY);
    const msdfgen::Point2 end(toX * reading.scale, -toY * reading.scale);
    if (end != pen) {
        reading.contour->addEdge(msdfgen::EdgeHolder(pen, end));
    }
    reading.penX = end.x;
    reading.penY = end.y;
}

void DistanceField::quadraticTo(hb_draw_funcs_t* /*funcs*/, void* outline, hb_draw_state_t* /*state*/, float controlX, float controlY, float toX, float toY, void* /*user*/) {
    auto& reading = *static_cast<Outline*>(outline);
    const msdfgen::Point2 control(controlX * reading.scale, -controlY * reading.scale);
    const msdfgen::Point2 end(toX * reading.scale, -toY * reading.scale);
    reading.contour->addEdge(msdfgen::EdgeHolder(msdfgen::Point2(reading.penX, reading.penY), control, end));
    reading.penX = end.x;
    reading.penY = end.y;
}

void DistanceField::cubicTo(hb_draw_funcs_t* /*funcs*/, void* outline, hb_draw_state_t* /*state*/, float firstX, float firstY, float secondX, float secondY, float toX, float toY, void* /*user*/) {
    auto& reading = *static_cast<Outline*>(outline);
    const msdfgen::Point2 first(firstX * reading.scale, -firstY * reading.scale);
    const msdfgen::Point2 second(secondX * reading.scale, -secondY * reading.scale);
    const msdfgen::Point2 end(toX * reading.scale, -toY * reading.scale);
    reading.contour->addEdge(msdfgen::EdgeHolder(msdfgen::Point2(reading.penX, reading.penY), first, second, end));
    reading.penX = end.x;
    reading.penY = end.y;
}

// The outline in pixels, with y pointing down like the rows of the field. Lines, quadratic curves and cubic curves keep their exact shape.
msdfgen::Shape DistanceField::readShape(const FontFile& font, std::uint32_t glyph, float scale) {
    msdfgen::Shape shape;
    Outline outline{.shape = shape, .scale = scale};
    hb_font_draw_glyph(&font.getFont(), glyph, getOutlineFuncs(), &outline);
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

// The box of the outline rounds out to whole pixels, with y pointing down.
std::optional<DistanceField> DistanceField::measure(const FontFile& font, std::uint32_t glyph, float scale, int spread) {
    const std::optional<hb_glyph_extents_t> extents = font.getExtents(glyph);
    if (!extents) {
        return std::nullopt;
    }
    const auto left = static_cast<int>(std::floor(static_cast<float>(extents->x_bearing) * scale));
    const auto top = static_cast<int>(std::floor(static_cast<float>(-extents->y_bearing) * scale));
    const auto right = static_cast<int>(std::ceil(static_cast<float>(extents->x_bearing + extents->width) * scale));
    const auto bottom = static_cast<int>(std::ceil(static_cast<float>(-(extents->y_bearing + extents->height)) * scale));
    if (left == right || top == bottom) {
        return std::nullopt;
    }
    return DistanceField{.width = right - left + spread * 2, .height = bottom - top + spread * 2, .offsetX = left - spread, .offsetY = top - spread};
}

std::optional<DistanceField> DistanceField::build(const FontFile& font, std::uint32_t glyph, float scale, int spread, int largest) {
    std::optional<DistanceField> field = measure(font, glyph, scale, spread);
    if (!field) {
        return std::nullopt;
    }
    if (field->width > largest || field->height > largest) {
        throw std::runtime_error("The glyph is larger than the largest font atlas the device can hold.");
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

    // Each pixel samples its center, and distances across twice the spread fill the range from 0 to 1.
    std::vector<float> samples(static_cast<std::size_t>(field->width) * static_cast<std::size_t>(field->height));
    const msdfgen::BitmapSection<float, 1> output(samples.data(), field->width, field->height, msdfgen::Y_DOWNWARD);
    const msdfgen::Projection projection(msdfgen::Vector2(1.0), msdfgen::Vector2(-field->offsetX, -field->offsetY));
    msdfgen::generateSDF(output, shape, msdfgen::SDFTransformation(projection, msdfgen::Range(spread * 2.0)), msdfgen::GeneratorConfig(mayOverlap(shape)));
    field->distances.resize(samples.size());
    std::ranges::transform(samples, field->distances.begin(), [](float distance) { return math::HalfFloat::fromFloat(std::clamp(distance, 0.0F, 1.0F)); });
    return field;
}

// The distance takes the sign of the side of the outline a point lies on, so the middle lies in a hole when its sign matches the sign of a point outside the box.
float DistanceField::measureHole(const FontFile& font, std::uint32_t glyph, float scale) {
    msdfgen::Shape shape = readShape(font, glyph, scale);
    if (shape.contours.empty()) {
        return 0.0F;
    }

    shape.normalize();
    const msdfgen::Shape::Bounds bounds = shape.getBounds();
    const double outside = msdfgen::SimpleTrueShapeDistanceFinder::oneShotDistance(shape, msdfgen::Point2(bounds.l - 1.0, bounds.b - 1.0));
    const double middle = msdfgen::SimpleTrueShapeDistanceFinder::oneShotDistance(shape, msdfgen::Point2((bounds.l + bounds.r) * 0.5, (bounds.b + bounds.t) * 0.5));
    return (outside > 0.0) == (middle > 0.0) ? static_cast<float>(std::fabs(middle) * 2.0) : 0.0F;
}

} // namespace haylen::text
