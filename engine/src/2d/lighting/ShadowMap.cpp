#include "2d/lighting/ShadowMap.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

#include "haylen/math/Geometry.hpp"
#include "haylen/math/Math.hpp"
#include "haylen/math/Segment.hpp"

namespace haylen::lighting2d {

void ShadowMap::appendSegments(const Occluder& occluder, std::vector<Segment>& segments) {
    const std::vector<math::Vec2> points = occluder.getWorldPoints();
    const std::size_t count = occluder.closed ? points.size() : points.size() - 1;
    for (std::size_t index = 0; index < count; ++index) {
        segments.push_back({.from = points[index], .to = points[(index + 1) % points.size()], .mask = occluder.mask, .cull = occluder.cull});
    }
}

ShadowMap::Axis ShadowMap::cast(const Light& light, std::span<const Segment> segments, const math::Rect& bounds, std::span<float> row) {
    std::fill(row.begin(), row.end(), kClear);
    if (light.type == Light::Type::Directional) {
        return castParallel(light, segments, bounds, row);
    }
    castRadial(light, segments, row);
    return {};
}

float ShadowMap::getBias(const Light& light, const Axis& axis) noexcept {
    return kBiasUnits / (light.type == Light::Type::Directional ? axis.alongSpan : getRange(light));
}

float ShadowMap::getRange(const Light& light) noexcept {
    return light.radius * std::max(light.scale.x, light.scale.y);
}

bool ShadowMap::blocks(const Light& light, std::span<const Segment> segments, math::Vec2 point) noexcept {
    // A directional light is blocked when a segment crosses the ray from the point back toward the light, and a point light when one crosses the path from the light to the point.
    const bool directional = light.type == Light::Type::Directional;
    const math::Vec2 origin = directional ? point : light.position;
    const math::Vec2 path = directional ? -math::Vec2::fromAngle(light.rotation) : point - light.position;
    const float length = path.getLength();
    if (length <= 0.0F) {
        return false;
    }

    const math::Vec2 unit = path / length;
    const float nearest = directional ? kBiasUnits : 0.0F;
    const float farthest = directional ? std::numeric_limits<float>::infinity() : length - kBiasUnits;
    for (const Segment& segment : segments) {
        const math::Vec2 edge = segment.to - segment.from;
        const float denominator = math::Vec2::cross(unit, edge);
        if (!casts(light, segment) || std::fabs(denominator) < 1.0e-6F) {
            continue;
        }
        const float along = math::Vec2::cross(segment.from - origin, edge) / denominator;
        const float across = math::Vec2::cross(segment.from - origin, unit) / denominator;
        if (across >= 0.0F && across <= 1.0F && along >= nearest && along <= farthest) {
            return true;
        }
    }
    return false;
}

bool ShadowMap::casts(const Light& light, const Segment& segment) noexcept {
    if ((segment.mask & light.shadowMask) == 0) {
        return false;
    }
    if (segment.cull == Occluder::Cull::Disabled) {
        return true;
    }

    // With y pointing down, a positive cross product means the light sees the edge turn clockwise on screen.
    const math::Vec2 toward = light.type == Light::Type::Directional ? -math::Vec2::fromAngle(light.rotation) : light.position - segment.from;
    const float winding = math::Vec2::cross(segment.to - segment.from, toward);
    return segment.cull == Occluder::Cull::Clockwise ? winding <= 0.0F : winding >= 0.0F;
}

void ShadowMap::castRadial(const Light& light, std::span<const Segment> segments, std::span<float> row) {
    const auto width = static_cast<int>(row.size());
    const float texels = static_cast<float>(width);
    const float range = getRange(light);

    for (const Segment& segment : segments) {
        const math::Vec2 from = segment.from - light.position;
        const math::Vec2 to = segment.to - light.position;
        if (!casts(light, segment) || math::Geometry::distanceToSegment(math::Segment{from, to}, math::Vec2{}) > range) {
            continue;
        }

        // A segment that does not pass through the light covers less than half a turn, from the angle of one end to the other the short way. Every texel it touches takes it, at the direction inside the texel closest to its center, so the silhouettes of overlapping occluders leave no texel open.
        const float fromAngle = std::atan2(from.y, from.x);
        const float sweep = math::Math::wrapAngle(std::atan2(to.y, to.x) - fromAngle);
        const float start = sweep >= 0.0F ? fromAngle : fromAngle + sweep;
        const float first = (start / math::Math::kTau + 0.5F) * texels;
        const float last = first + std::fabs(sweep) / math::Math::kTau * texels;

        const math::Vec2 edge = to - from;
        for (auto index = static_cast<int>(std::floor(first)); static_cast<float>(index) < last; ++index) {
            const float coordinate = std::clamp(static_cast<float>(index) + 0.5F, first, last);
            const math::Vec2 direction = math::Vec2::fromAngle(coordinate / texels * math::Math::kTau - math::Math::kPi);
            const float denominator = math::Vec2::cross(direction, edge);
            if (std::fabs(denominator) < 1.0e-6F) {
                continue;
            }
            const float distance = math::Vec2::cross(from, edge) / denominator;
            if (distance < 0.0F) {
                continue;
            }
            float& texel = row[static_cast<std::size_t>(((index % width) + width) % width)];
            texel = std::min(texel, distance / range);
        }
    }
}

ShadowMap::Axis ShadowMap::castParallel(const Light& light, std::span<const Segment> segments, const math::Rect& bounds, std::span<float> row) {
    const math::Vec2 direction = math::Vec2::fromAngle(light.rotation);
    const math::Vec2 across = direction.getPerpendicular();
    const std::array<math::Vec2, 4> corners{bounds.getMin(), math::Vec2{bounds.getRight(), bounds.getTop()}, bounds.getMax(), math::Vec2{bounds.getLeft(), bounds.getBottom()}};

    float acrossMin = std::numeric_limits<float>::max();
    float acrossMax = std::numeric_limits<float>::lowest();
    float alongMin = std::numeric_limits<float>::max();
    float alongMax = std::numeric_limits<float>::lowest();
    for (const math::Vec2 corner : corners) {
        acrossMin = std::min(acrossMin, math::Vec2::dot(corner, across));
        acrossMax = std::max(acrossMax, math::Vec2::dot(corner, across));
        alongMin = std::min(alongMin, math::Vec2::dot(corner, direction));
        alongMax = std::max(alongMax, math::Vec2::dot(corner, direction));
    }
    const Axis axis{.acrossStart = acrossMin, .acrossSpan = std::max(acrossMax - acrossMin, 1.0F), .alongStart = alongMin, .alongSpan = std::max(alongMax - alongMin, 1.0F)};

    const auto width = static_cast<int>(row.size());
    const float texels = static_cast<float>(width);
    for (const Segment& segment : segments) {
        const float fromAcross = math::Vec2::dot(segment.from, across);
        const float toAcross = math::Vec2::dot(segment.to, across);
        if (!casts(light, segment) || fromAcross == toAcross) {
            continue;
        }

        // Every texel the segment touches takes it, at the point inside the texel closest to its center.
        const float fromAlong = math::Vec2::dot(segment.from, direction);
        const float toAlong = math::Vec2::dot(segment.to, direction);
        const float first = (std::min(fromAcross, toAcross) - axis.acrossStart) / axis.acrossSpan * texels;
        const float last = (std::max(fromAcross, toAcross) - axis.acrossStart) / axis.acrossSpan * texels;
        const int begin = std::max(0, static_cast<int>(std::floor(first)));
        const int end = std::min(width - 1, static_cast<int>(std::ceil(last)) - 1);
        for (int index = begin; index <= end; ++index) {
            const float coordinate = std::clamp(static_cast<float>(index) + 0.5F, first, last);
            const float position = axis.acrossStart + coordinate / texels * axis.acrossSpan;
            const float along = math::Math::lerp(fromAlong, toAlong, (position - fromAcross) / (toAcross - fromAcross));
            float& texel = row[static_cast<std::size_t>(index)];
            texel = std::min(texel, (along - axis.alongStart) / axis.alongSpan);
        }
    }
    return axis;
}

} // namespace haylen::lighting2d
