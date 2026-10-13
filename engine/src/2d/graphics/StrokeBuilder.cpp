#include "2d/graphics/StrokeBuilder.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string>

namespace haylen::graphics2d {

void StrokeBuilder::validate(std::span<const math::Vec2> points, const Stroke& stroke) {
    if (!stroke.widths.empty() && stroke.widths.size() != points.size()) {
        throw std::invalid_argument("A stroke has one width for each of its " + std::to_string(points.size()) + " points, or none.");
    }
    if (!stroke.colors.empty() && stroke.colors.size() != points.size()) {
        throw std::invalid_argument("A stroke has one color for each of its " + std::to_string(points.size()) + " points, or none.");
    }
    const bool measured = stroke.width >= 0.0F && std::ranges::all_of(stroke.widths, [](float width) { return width >= 0.0F; }) && stroke.textureLength >= 0.0F && stroke.miterLimit >= 1.0F;
    if (!measured) {
        throw std::invalid_argument("A stroke needs widths and a texture length of zero or more and a miter limit of 1 or more.");
    }
    float period = 0.0F;
    for (const float length : stroke.dashes) {
        if (!(length >= 0.0F) || !std::isfinite(length)) {
            period = 0.0F;
            break;
        }
        period += length;
    }
    if (!stroke.dashes.empty() && !(period > 0.0F && std::isfinite(stroke.dashOffset))) {
        throw std::invalid_argument("A dash pattern needs lengths of zero or more that add up to more than zero, and a finite offset.");
    }
}

StrokeBuilder::Node StrokeBuilder::interpolate(const Node& from, const Node& to, float distance) noexcept {
    const float span = to.distance - from.distance;
    const float t = span > 0.0F ? (distance - from.distance) / span : 0.0F;
    return {math::Vec2::lerp(from.position, to.position, t), from.half + (to.half - from.half) * t, math::Color::lerp(from.color, to.color, t), distance};
}

// A curve of the radius stays within a quarter pixel of its arc.
int StrokeBuilder::arcSteps(float angle, float half) const noexcept {
    const float radius = half + pixel * 0.5F;
    const float tolerance = pixel * 0.25F;
    if (radius <= tolerance) {
        return 1;
    }
    const float step = 2.0F * std::acos(1.0F - tolerance / radius);
    return std::clamp(static_cast<int>(std::ceil(std::fabs(angle) / step)), 1, kMaxArcSteps);
}

void StrokeBuilder::build(std::span<const math::Vec2> points, const Stroke& stroke, float pixelSize, math::Vec2 textureSize, bool flippedRows) {
    validate(points, stroke);
    vertices.clear();
    indices.clear();
    join = stroke.join;
    cap = stroke.cap;
    miterLimit = stroke.miterLimit;
    pixel = std::max(pixelSize, 1e-6F);
    flipped = flippedRows;

    collect(points, stroke);
    if (nodes.size() < 2) {
        return;
    }
    const bool closed = stroke.closed && nodes.size() > 2;
    const float total = nodes.back().distance + (closed ? math::Vec2::distance(nodes.back().position, nodes.front().position) : 0.0F);
    const float tile = stroke.textureLength > 0.0F ? stroke.textureLength : (textureSize.y > 0.0F ? stroke.width * textureSize.x / textureSize.y : 0.0F);
    if (stroke.textureMode == Stroke::TextureMode::Stretch) {
        textureScale = total > 0.0F ? 1.0F / total : 0.0F;
    } else {
        textureScale = tile > 0.0F ? 1.0F / tile : 0.0F;
    }

    if (stroke.dashes.empty()) {
        buildPath(nodes, closed);
        return;
    }
    if (closed) {
        Node end = nodes.front();
        end.distance = total;
        nodes.push_back(end);
    }
    buildDashes(stroke.dashes, stroke.dashOffset);
}

// Points that repeat the point before them leave no segment and are dropped, and so is the last point of a closed stroke that returns to the first.
void StrokeBuilder::collect(std::span<const math::Vec2> points, const Stroke& stroke) {
    nodes.clear();
    for (std::size_t index = 0; index < points.size(); ++index) {
        Node node{points[index], (stroke.widths.empty() ? stroke.width : stroke.widths[index]) * 0.5F, stroke.colors.empty() ? stroke.color : stroke.colors[index], 0.0F};
        if (!nodes.empty()) {
            const float step = math::Vec2::distance(nodes.back().position, node.position);
            if (step <= 1e-6F) {
                continue;
            }
            node.distance = nodes.back().distance + step;
        }
        nodes.push_back(node);
    }
    if (stroke.closed && nodes.size() > 2 && math::Vec2::distance(nodes.front().position, nodes.back().position) <= 1e-6F) {
        nodes.pop_back();
    }
}

// Walks the pattern along the stroke and builds each drawn length as an open path of its own, whose distances keep counting along the whole stroke so a texture runs on across the gaps.
void StrokeBuilder::buildDashes(std::span<const float> pattern, float offset) {
    const std::size_t count = pattern.size() % 2 == 0 ? pattern.size() : pattern.size() * 2;
    float period = 0.0F;
    for (std::size_t entry = 0; entry < count; ++entry) {
        period += pattern[entry % pattern.size()];
    }
    const float total = nodes.back().distance;
    if (total / period * static_cast<float>(count) > static_cast<float>(kMaxDashes)) {
        throw std::invalid_argument("A dash pattern makes more than 65536 dashes along the stroke.");
    }

    float phase = std::fmod(offset, period);
    phase += phase < 0.0F ? period : 0.0F;
    std::size_t entry = 0;
    while (phase > 0.0F && phase >= pattern[entry % pattern.size()]) {
        phase -= pattern[entry % pattern.size()];
        entry = (entry + 1) % count;
    }

    std::size_t cursor = 1;
    for (float position = -phase; position < total; entry = (entry + 1) % count) {
        const float length = pattern[entry % pattern.size()];
        const float start = std::max(position, 0.0F);
        const float end = std::min(position + length, total);
        position += length;
        if (entry % 2 != 0 || end < start || (length == 0.0F && cap == Stroke::Cap::Butt)) {
            continue;
        }

        while (cursor + 1 < nodes.size() && nodes[cursor].distance < start) {
            ++cursor;
        }
        dash.clear();
        dash.push_back(interpolate(nodes[cursor - 1], nodes[cursor], start));

        // A dash of no length draws as the dot its caps make, turned along the segment it lies on.
        if (end - start <= 1e-6F) {
            Node tip = dash.front();
            tip.position += (nodes[cursor].position - nodes[cursor - 1].position).getNormalized() * (pixel * 1e-3F);
            dash.push_back(tip);
            buildPath(dash, false);
            continue;
        }
        std::size_t last = cursor;
        for (; last + 1 < nodes.size() && nodes[last].distance < end; ++last) {
            if (nodes[last].distance > start) {
                dash.push_back(nodes[last]);
            }
        }
        dash.push_back(interpolate(nodes[last - 1], nodes[last], end));
        buildPath(dash, false);
    }
}

// A closed path starts with the last section of the join at its first point and ends with that whole join, so its run closes on itself.
void StrokeBuilder::buildPath(std::span<const Node> path, bool closed) {
    continues = false;
    const std::size_t last = path.size() - 1;
    if (!closed) {
        addStartCap(path[0], (path[1].position - path[0].position).getNormalized());
        for (std::size_t index = 1; index < last; ++index) {
            addJoin(path[index - 1], path[index], path[index + 1], path[index].distance);
            emitJoint();
        }
        addEndCap(path[last], (path[last].position - path[last - 1].position).getNormalized());
        return;
    }

    addJoin(path[last], path[0], path[1], 0.0F);
    emit(joint.back());
    for (std::size_t index = 1; index < last; ++index) {
        addJoin(path[index - 1], path[index], path[index + 1], path[index].distance);
        emitJoint();
    }
    addJoin(path[last - 1], path[last], path[0], path[last].distance);
    emitJoint();
    addJoin(path[last], path[0], path[1], path[last].distance + math::Vec2::distance(path[last].position, path[0].position));
    emitJoint();
}

// The outer side of a turn takes the miter, the bevel or the arc, and the inner side meets at the corner of the offset edges, kept within the shorter segment so short segments never fold it far past their ends.
void StrokeBuilder::addJoin(const Node& previous, const Node& node, const Node& next, float distance) {
    const math::Vec2 incoming = (node.position - previous.position).getNormalized();
    const math::Vec2 outgoing = (next.position - node.position).getNormalized();
    const math::Vec2 before = incoming.getPerpendicular();
    const math::Vec2 after = outgoing.getPerpendicular();
    Section section{.center = node.position, .half = node.half, .color = node.color, .distance = distance};
    joint.clear();
    if (std::fabs(math::Vec2::cross(incoming, outgoing)) <= 1e-5F && math::Vec2::dot(incoming, outgoing) > 0.0F) {
        section.left = before;
        section.right = -before;
        joint.push_back(section);
        return;
    }

    const math::Vec2 sum = before + after;
    const float sumLength = sum.getLength();
    const math::Vec2 miterDirection = sumLength > 1e-4F ? sum / sumLength : math::Vec2{};
    const float miter = sumLength > 1e-4F ? 1.0F / std::max(math::Vec2::dot(miterDirection, before), 1e-4F) : 1e4F;
    if (join == Stroke::Join::Miter && miter <= miterLimit) {
        section.left = miterDirection * miter;
        section.right = -miterDirection * miter;
        joint.push_back(section);
        return;
    }

    const float shorter = std::min(math::Vec2::distance(previous.position, node.position), math::Vec2::distance(node.position, next.position));
    const float inner = node.half > 0.0F ? std::min(miter, std::sqrt(1.0F + (shorter / node.half) * (shorter / node.half))) : miter;
    const bool leftInner = math::Vec2::dot(outgoing, before) > 0.0F;
    const math::Vec2 innerOffset = (leftInner ? miterDirection : -miterDirection) * inner;
    const math::Vec2 from = leftInner ? -before : before;
    const math::Vec2 to = leftInner ? -after : after;
    const float angle = std::atan2(math::Vec2::cross(from, to), math::Vec2::dot(from, to));
    const int steps = join == Stroke::Join::Round ? arcSteps(angle, node.half) : 1;
    for (int step = 0; step <= steps; ++step) {
        const float share = static_cast<float>(step) / static_cast<float>(steps);
        const math::Vec2 outer = join == Stroke::Join::Round ? from.rotated(angle * share) : (step == 0 ? from : to);
        section.left = leftInner ? innerOffset : outer;
        section.right = leftInner ? outer : innerOffset;
        joint.push_back(section);
    }
}

void StrokeBuilder::emitJoint() {
    for (const Section& section : joint) {
        emit(section);
    }
}

// A cap fades out past the end, half a pixel beyond the end of a butt cap, half a width plus half a pixel beyond a square cap, and around the half circle of a round cap.
void StrokeBuilder::addStartCap(const Node& node, math::Vec2 direction) {
    const math::Vec2 normal = direction.getPerpendicular();
    const Section section{.center = node.position, .left = normal, .right = -normal, .half = node.half, .color = node.color, .distance = node.distance};
    if (cap == Stroke::Cap::Round) {
        const int steps = arcSteps(std::numbers::pi_v<float> * 0.5F, node.half);
        for (int step = 0; step <= steps; ++step) {
            const float angle = std::numbers::pi_v<float> * 0.5F * static_cast<float>(step) / static_cast<float>(steps);
            Section part = section;
            part.left = -direction * std::cos(angle) + normal * std::sin(angle);
            part.right = -direction * std::cos(angle) - normal * std::sin(angle);
            emit(part);
        }
        return;
    }

    const float reach = cap == Stroke::Cap::Square ? node.half : 0.0F;
    Section fringe = section;
    fringe.center -= direction * (reach + pixel * 0.5F);
    fringe.distance -= reach + pixel * 0.5F;
    fringe.fade = 0.0F;
    emit(fringe);
    if (reach > 0.0F) {
        Section end = section;
        end.center -= direction * reach;
        end.distance -= reach;
        emit(end);
    }
    emit(section);
}

void StrokeBuilder::addEndCap(const Node& node, math::Vec2 direction) {
    const math::Vec2 normal = direction.getPerpendicular();
    const Section section{.center = node.position, .left = normal, .right = -normal, .half = node.half, .color = node.color, .distance = node.distance};
    emit(section);
    if (cap == Stroke::Cap::Round) {
        const int steps = arcSteps(std::numbers::pi_v<float> * 0.5F, node.half);
        for (int step = steps - 1; step >= 0; --step) {
            const float angle = std::numbers::pi_v<float> * 0.5F * static_cast<float>(step) / static_cast<float>(steps);
            Section part = section;
            part.left = direction * std::cos(angle) + normal * std::sin(angle);
            part.right = direction * std::cos(angle) - normal * std::sin(angle);
            emit(part);
        }
        return;
    }

    const float reach = cap == Stroke::Cap::Square ? node.half : 0.0F;
    if (reach > 0.0F) {
        Section end = section;
        end.center += direction * reach;
        end.distance += reach;
        emit(end);
    }
    Section fringe = section;
    fringe.center += direction * (reach + pixel * 0.5F);
    fringe.distance += reach + pixel * 0.5F;
    fringe.fade = 0.0F;
    emit(fringe);
}

// The core of a section reaches half a pixel short of its edge and the outer vertices half a pixel past it, so the edge fades over one pixel, and a stroke thinner than a pixel keeps a core of nothing and fades by the share of the pixel it covers.
void StrokeBuilder::emit(const Section& section) {
    const float core = std::max(section.half - pixel * 0.5F, 0.0F);
    const float outer = core + pixel;
    math::Color full = section.color;
    full.a *= std::min(1.0F, 2.0F * section.half / pixel) * section.fade;
    math::Color edge = section.color;
    edge.a = 0.0F;
    const std::uint32_t solid = full.toRgba8();
    const std::uint32_t clear = edge.toRgba8();
    const float u = section.distance * textureScale;
    const float top = flipped ? 1.0F : 0.0F;
    const float bottom = 1.0F - top;

    const auto base = static_cast<std::uint32_t>(vertices.size());
    const math::Vec2 leftOuter = section.center + section.left * outer;
    const math::Vec2 leftCore = section.center + section.left * core;
    const math::Vec2 rightCore = section.center + section.right * core;
    const math::Vec2 rightOuter = section.center + section.right * outer;
    vertices.push_back({{leftOuter.x, leftOuter.y}, {u, top}, clear});
    vertices.push_back({{leftCore.x, leftCore.y}, {u, top}, solid});
    vertices.push_back({{rightCore.x, rightCore.y}, {u, bottom}, solid});
    vertices.push_back({{rightOuter.x, rightOuter.y}, {u, bottom}, clear});

    if (continues) {
        const std::uint32_t previous = base - 4U;
        for (std::uint32_t lane = 0; lane < 3U; ++lane) {
            indices.insert(indices.end(), {previous + lane, previous + lane + 1U, base + lane + 1U, previous + lane, base + lane + 1U, base + lane});
        }
    }
    continues = true;
}

} // namespace haylen::graphics2d
