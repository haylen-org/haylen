#include "haylen/2d/graphics/Path.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "2d/graphics/PathTessellation.hpp"

namespace haylen::graphics2d {

// Collects the lines of the outlines of a path as it walks its commands, where an outline starts at a move, at its first point for an arc, and otherwise at the point the command continues from.
class Path::Flattener final {
  public:
    Flattener(float limit, Outlines& target) : tolerance(limit), outlines(target) {}

    void moveTo(math::Vec2 point) {
        finish(false);
        from = point;
        begin();
    }

    void lineTo(math::Vec2 point) {
        begin();
        add(point);
    }

    // A quadratic curve of `n` equal steps strays from its lines by at most a quarter of the second difference of its points over `n` squared.
    void quadraticTo(math::Vec2 control, math::Vec2 point) {
        begin();
        const math::Vec2 origin = from;
        const int count = steps(std::sqrt(0.25F * (origin - control * 2.0F + point).getLength() / tolerance));
        for (int step = 1; step <= count; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(count);
            const float u = 1.0F - t;
            add(step == count ? point : origin * (u * u) + control * (2.0F * u * t) + point * (t * t));
        }
    }

    // A cubic curve of `n` equal steps strays from its lines by at most three quarters of the larger second difference of its points over `n` squared.
    void cubicTo(math::Vec2 first, math::Vec2 second, math::Vec2 point) {
        begin();
        const math::Vec2 origin = from;
        const float bend = std::max((origin - first * 2.0F + second).getLength(), (first - second * 2.0F + point).getLength());
        const int count = steps(std::sqrt(0.75F * bend / tolerance));
        for (int step = 1; step <= count; ++step) {
            const float t = static_cast<float>(step) / static_cast<float>(count);
            const float u = 1.0F - t;
            add(step == count ? point : origin * (u * u * u) + first * (3.0F * u * u * t) + second * (3.0F * u * t * t) + point * (t * t * t));
        }
    }

    // Every step of an arc turns by the angle whose chord strays from the circle by the tolerance.
    void arc(const Arc& part) {
        const math::Vec2 first = part.center + math::Vec2{std::cos(part.startAngle), std::sin(part.startAngle)} * part.radius;
        if (!active) {
            from = first;
        }
        begin();
        add(first);

        const float share = part.radius > 0.0F ? std::min(tolerance / part.radius, 1.0F) : 1.0F;
        const int count = steps(std::fabs(part.sweep) / (2.0F * std::acos(1.0F - share)));
        for (int step = 1; step <= count; ++step) {
            const float angle = part.startAngle + part.sweep * static_cast<float>(step) / static_cast<float>(count);
            add(part.center + math::Vec2{std::cos(angle), std::sin(angle)} * part.radius);
        }
    }

    // A closed outline drops its last point when it returns to the first, and the next line starts from the first point again.
    void close() {
        const math::Vec2 origin = outlines.points[opening];
        finish(true);
        from = origin;
    }

    // Keeps the open outline when it has a line, and drops it otherwise.
    void finish(bool closes) {
        if (!active) {
            return;
        }
        active = false;
        std::vector<math::Vec2>& line = outlines.points;
        if (closes && line.size() - opening > 2 && line.back() == line[opening]) {
            line.pop_back();
        }

        const std::size_t count = line.size() - opening;
        if (count < 2) {
            line.resize(opening);
            return;
        }
        outlines.counts.push_back(static_cast<std::uint32_t>(count));
        outlines.closed.push_back(closes ? 1U : 0U);
    }

  private:
    [[nodiscard]] static int steps(float value) noexcept {
        return value < static_cast<float>(kMaxCurveSteps) ? std::max(static_cast<int>(std::ceil(value)), 1) : kMaxCurveSteps;
    }

    void begin() {
        if (active) {
            return;
        }
        active = true;
        opening = outlines.points.size();
        outlines.points.push_back(from);
    }

    void add(math::Vec2 point) {
        if (point != outlines.points.back()) {
            outlines.points.push_back(point);
        }
        from = point;
    }

    float tolerance = 1.0F;
    Outlines& outlines;
    math::Vec2 from{};
    std::size_t opening = 0;
    bool active = false;
};

Path::Path() : tessellation(std::make_unique<PathTessellation>()) {}

Path::~Path() = default;

Path::Path(const Path& other) : verbs(other.verbs), points(other.points), arcs(other.arcs), current(other.current), start(other.start), hasCurrent(other.hasCurrent), open(other.open), tessellation(std::make_unique<PathTessellation>()) {}

// The tessellation stays with the path it belongs to, and builds again on its next draw only if the commands it takes differ.
Path& Path::operator=(const Path& other) {
    verbs = other.verbs;
    points = other.points;
    arcs = other.arcs;
    current = other.current;
    start = other.start;
    hasCurrent = other.hasCurrent;
    open = other.open;
    return *this;
}

std::optional<Path::FillRule> Path::fillRuleFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kFillRuleNames, name, &std::pair<std::string_view, FillRule>::first);
    return found != kFillRuleNames.end() ? std::optional<FillRule>(found->second) : std::nullopt;
}

std::string_view Path::fillRuleName(FillRule value) noexcept {
    const auto found = std::ranges::find(kFillRuleNames, value, &std::pair<std::string_view, FillRule>::second);
    return found != kFillRuleNames.end() ? found->first : std::string_view{};
}

void Path::requireFinite(math::Vec2 point) {
    if (!std::isfinite(point.x) || !std::isfinite(point.y)) {
        throw std::invalid_argument("A path takes finite coordinates.");
    }
}

void Path::requireCurrent() const {
    if (!hasCurrent) {
        throw std::logic_error("A path has no point to continue from, so \"moveTo\" comes before its first line or curve.");
    }
}

void Path::moveTo(math::Vec2 point) {
    requireFinite(point);
    verbs.push_back(Verb::Move);
    points.push_back(point);
    current = point;
    start = point;
    hasCurrent = true;
    open = true;
}

void Path::lineTo(math::Vec2 point) {
    requireFinite(point);
    requireCurrent();
    verbs.push_back(Verb::Line);
    points.push_back(point);
    current = point;
    open = true;
}

void Path::quadraticTo(math::Vec2 control, math::Vec2 point) {
    requireFinite(control);
    requireFinite(point);
    requireCurrent();
    verbs.push_back(Verb::Quadratic);
    points.insert(points.end(), {control, point});
    current = point;
    open = true;
}

void Path::cubicTo(math::Vec2 first, math::Vec2 second, math::Vec2 point) {
    requireFinite(first);
    requireFinite(second);
    requireFinite(point);
    requireCurrent();
    verbs.push_back(Verb::Cubic);
    points.insert(points.end(), {first, second, point});
    current = point;
    open = true;
}

void Path::arc(math::Vec2 center, float radius, float startAngle, float sweep) {
    requireFinite(center);
    if (!(radius >= 0.0F) || !std::isfinite(radius) || !std::isfinite(startAngle) || !std::isfinite(sweep)) {
        throw std::invalid_argument("An arc of a path takes a finite radius of zero or more and finite angles.");
    }

    verbs.push_back(Verb::Arc);
    arcs.push_back({.center = center, .radius = radius, .startAngle = startAngle, .sweep = sweep});
    if (!open) {
        start = center + math::Vec2{std::cos(startAngle), std::sin(startAngle)} * radius;
    }
    const float end = startAngle + sweep;
    current = center + math::Vec2{std::cos(end), std::sin(end)} * radius;
    hasCurrent = true;
    open = true;
}

void Path::close() {
    if (!open) {
        throw std::logic_error("A path closes only an outline it has open, so \"close\" follows a move, a line, a curve or an arc.");
    }
    verbs.push_back(Verb::Close);
    current = start;
    open = false;
}

void Path::clear() noexcept {
    verbs.clear();
    points.clear();
    arcs.clear();
    current = {};
    start = {};
    hasCurrent = false;
    open = false;
}

void Path::flatten(float tolerance, Outlines& outlines) const {
    if (!(tolerance > 0.0F) || !std::isfinite(tolerance)) {
        throw std::invalid_argument("A path flattens within a positive and finite tolerance.");
    }
    outlines.points.clear();
    outlines.counts.clear();
    outlines.closed.clear();

    Flattener flattener(tolerance, outlines);
    std::size_t point = 0;
    std::size_t arc = 0;
    for (const Verb verb : verbs) {
        switch (verb) {
        case Verb::Move:
            flattener.moveTo(points[point++]);
            break;
        case Verb::Line:
            flattener.lineTo(points[point++]);
            break;
        case Verb::Quadratic:
            flattener.quadraticTo(points[point], points[point + 1]);
            point += 2;
            break;
        case Verb::Cubic:
            flattener.cubicTo(points[point], points[point + 1], points[point + 2]);
            point += 3;
            break;
        case Verb::Arc:
            flattener.arc(arcs[arc++]);
            break;
        case Verb::Close:
            flattener.close();
            break;
        }
    }
    flattener.finish(false);
}

} // namespace haylen::graphics2d
