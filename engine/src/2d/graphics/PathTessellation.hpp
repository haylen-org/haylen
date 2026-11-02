#pragma once

#include <cstdint>
#include <vector>

#include "2d/graphics/GpuVertex.hpp"
#include "haylen/2d/graphics/Path.hpp"
#include "haylen/2d/graphics/Stroke.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

class PolygonMesh;
class StrokeBuilder;

// The triangles of the fill and the stroke of a path, kept between draws in the units of the path. Each builds again only when the commands of the path, the size of a pixel or the look of its draw differ from its build before, so a path that draws again unchanged copies them and allocates nothing, also when its fill and its stroke draw at different scales. The vertices are white with the coverage of their point in their alpha, and the fill spans the texture coordinates over the bounds of the path, so a draw only tints and places them.
class PathTessellation final {
  public:
    struct Triangles {
        std::vector<GpuVertex> vertices;
        std::vector<std::uint32_t> indices;
    };

    // Returns the fill by the rule, where `pixel` is the size of one pixel of the destination in the units of the path. The outlines resolve into simple outlines and holes by the rule first, so crossings and overlaps fill as the rule says.
    [[nodiscard]] const Triangles& getFill(const Path& path, Path::FillRule rule, float pixel, PolygonMesh& polygons);

    // Returns the stroke of every outline of the path, closed where the path closes it, built with the look of the stroke and the size of its texture in pixels. Throws `std::invalid_argument` for a stroke with widths or colors for each point, which a path does not have, or a stroke whose values cannot draw.
    [[nodiscard]] const Triangles& getStroke(const Path& path, const Stroke& stroke, float pixel, math::Vec2 textureSize, StrokeBuilder& strokes);

  private:
    // The curves of a path stay within this share of a pixel of their lines.
    static constexpr float kFlatness = 0.25F;

    // The fill resolves its outlines on a grid of this many steps per pixel, and no coordinate of the grid passes the limit.
    static constexpr double kGridSteps = 64.0;
    static constexpr double kGridLimit = 1.0e15;

    // What a fill or a stroke was built from: the revision of the commands, the size of a pixel and the outlines flattened for it.
    struct Build {
        std::uint64_t revision = 0;
        float pixel = 0.0F;
        Path::Outlines outlines;
        Triangles triangles;
    };

    // The look of the stroke the triangles were built for.
    struct StrokeLook {
        float width = 0.0F;
        Stroke::Join join = Stroke::Join::Miter;
        Stroke::Cap cap = Stroke::Cap::Butt;
        float miterLimit = 0.0F;
        Stroke::TextureMode textureMode = Stroke::TextureMode::Stretch;
        float textureLength = 0.0F;
        math::Vec2 textureSize{};
        float dashOffset = 0.0F;
        std::vector<float> dashes;
    };

    // Takes a new revision when the commands of the path differ from the commands it keeps, which makes every build of an older revision stale.
    void update(const Path& path);
    [[nodiscard]] bool isCurrent(const Build& build, float pixel) const noexcept;
    void buildFill(Path::FillRule rule, float pixel, PolygonMesh& polygons);
    [[nodiscard]] bool isLook(const Stroke& stroke, math::Vec2 textureSize) const noexcept;

    std::vector<Path::Verb> verbs;
    std::vector<math::Vec2> points;
    std::vector<Path::Arc> arcs;
    std::uint64_t revision = 0;

    Build fill;
    Path::FillRule fillRule = Path::FillRule::NonZero;
    std::vector<math::Vec2> simplePoints;
    std::vector<std::uint32_t> simpleCounts;

    Build stroke;
    StrokeLook look;
};

} // namespace haylen::graphics2d
