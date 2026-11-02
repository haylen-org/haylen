#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

class PathTessellation;

// A shape of outlines made of lines, curves and arcs, which the renderer fills and strokes with edges that fade over one pixel of the destination. Outlines may cross themselves, overlap each other and cut holes, and the fill rule decides what lies inside. A path keeps its tessellation between draws, so a path that draws again at the same scale only copies its triangles, also when its commands are cleared and given again unchanged.
class Path final {
  public:
    // The rule `NonZero` fills every area the outlines wind around, so an outline wound opposite to the one around it cuts a hole, and `EvenOdd` fills the areas inside an odd count of outlines, so every overlap cuts a hole whatever the windings.
    enum class FillRule : std::uint8_t {
        NonZero,
        EvenOdd,
    };

    // The commands of a path, which take the points that follow each other in `getPoints`: one for a move and a line, two for a quadratic curve, its control and its end, three for a cubic curve, and none for an arc, which takes the next entry of `getArcs`, and for a close.
    enum class Verb : std::uint8_t {
        Move,
        Line,
        Quadratic,
        Cubic,
        Arc,
        Close,
    };

    // A part of the circle around the center, from the start angle, measured clockwise from the x axis, turning clockwise by the sweep, or counterclockwise when the sweep is negative.
    struct Arc {
        math::Vec2 center{};
        float radius = 0.0F;
        float startAngle = 0.0F;
        float sweep = 0.0F;

        [[nodiscard]] friend constexpr bool operator==(const Arc&, const Arc&) noexcept = default;
    };

    // How a path fills: its color, its rule and an image stretched over the bounds of the path under the color, such as a gradient, white when empty.
    struct Fill {
        math::Color color = math::Color::white();
        FillRule rule = FillRule::NonZero;
        graphics::Texture texture;
    };

    // The outlines of a path as lines: the points of each outline follow each other in `points`, `counts` holds how many each outline has, and `closed` whether it closes.
    struct Outlines {
        std::vector<math::Vec2> points;
        std::vector<std::uint32_t> counts;
        std::vector<std::uint8_t> closed;
    };

    Path();
    ~Path();
    Path(const Path& other);
    Path& operator=(const Path& other);

    // Starts a new outline at the point. Lines and curves continue from the end of the command before them, and after a close from the start of the outline that closed, which starts a new outline there. Throws `std::invalid_argument` for a coordinate that is not finite.
    void moveTo(math::Vec2 point);

    // Throw `std::logic_error` when the path has no point to continue from, and `std::invalid_argument` for a coordinate that is not finite.
    void lineTo(math::Vec2 point);
    void quadraticTo(math::Vec2 control, math::Vec2 point);
    void cubicTo(math::Vec2 first, math::Vec2 second, math::Vec2 point);

    // Adds an arc, joined by a line to the end of the open outline, or starting a new outline at its first point when none is open. Throws `std::invalid_argument` for a radius that is negative or a value that is not finite.
    void arc(math::Vec2 center, float radius, float startAngle, float sweep);

    // Joins the end of the outline to its start. Throws `std::logic_error` when no outline is open.
    void close();
    void clear() noexcept;

    [[nodiscard]] bool empty() const noexcept {
        return verbs.empty();
    }
    [[nodiscard]] std::span<const Verb> getVerbs() const noexcept {
        return verbs;
    }
    [[nodiscard]] std::span<const math::Vec2> getPoints() const noexcept {
        return points;
    }
    [[nodiscard]] std::span<const Arc> getArcs() const noexcept {
        return arcs;
    }

    // Replaces the outlines with the outlines of the path, whose lines stay within the tolerance of its curves and arcs. Points that repeat the point before them are dropped, and so is an outline without a line. Throws `std::invalid_argument` for a tolerance that is not positive and finite.
    void flatten(float tolerance, Outlines& outlines) const;

    // The triangles the renderer keeps for the path between draws.
    [[nodiscard]] PathTessellation& getTessellation() const noexcept {
        return *tessellation;
    }

    [[nodiscard]] static std::optional<FillRule> fillRuleFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view fillRuleName(FillRule value) noexcept;

    static constexpr std::array<std::pair<std::string_view, FillRule>, 2> kFillRuleNames{{{"nonZero", FillRule::NonZero}, {"evenOdd", FillRule::EvenOdd}}};

    // A curve or an arc turns into at most this many lines.
    static constexpr int kMaxCurveSteps = 1024;

  private:
    class Flattener;

    static void requireFinite(math::Vec2 point);
    void requireCurrent() const;

    std::vector<Verb> verbs;
    std::vector<math::Vec2> points;
    std::vector<Arc> arcs;

    // Where the next line starts and where the open outline started, and whether there is a point to continue from.
    math::Vec2 current{};
    math::Vec2 start{};
    bool hasCurrent = false;
    bool open = false;
    std::unique_ptr<PathTessellation> tessellation;
};

} // namespace haylen::graphics2d
