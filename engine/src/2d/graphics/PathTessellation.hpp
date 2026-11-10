#pragma once

#include <array>
#include <cstddef>
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

// The triangles of the fill and the stroke of a path, kept between draws in the units of the path. It keeps a few builds of each, one for every size of a pixel, fill rule or look of a stroke that the path draws with, and builds again only the least recently drawn one when a draw matches none. So a path that draws again unchanged copies its triangles and allocates nothing, also when it draws at several scales, by both rules or with several strokes in every frame. The vertices are white with the coverage of their point in their alpha, and the fill spans the texture coordinates over the bounds of the path, so a draw only tints and places them.
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

    // The builds of the fill and of the stroke that a path keeps.
    static constexpr std::size_t kBuildsKept = 4;

    // The fill resolves its outlines on a grid of this many steps per pixel, and no coordinate of the grid passes the limit.
    static constexpr double kGridSteps = 64.0;
    static constexpr double kGridLimit = 1.0e15;

    // What a fill or a stroke was built from: the revision of the commands, the size of a pixel and the outlines flattened for it, with the draw that used it last.
    struct Build {
        std::uint64_t revision = 0;
        float pixel = 0.0F;
        std::uint64_t used = 0;
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

    struct FillBuild {
        Build build;
        Path::FillRule rule = Path::FillRule::NonZero;
    };

    struct StrokeBuild {
        Build build;
        StrokeLook look;
    };

    // Takes a new revision when the commands of the path differ from the commands it keeps, which makes every build of an older revision stale.
    void update(const Path& path);
    [[nodiscard]] bool isCurrent(const Build& build, float pixel) const noexcept;
    void buildFill(FillBuild& fill, float pixel, PolygonMesh& polygons);
    void buildStroke(StrokeBuild& stroke, const Stroke& value, float pixel, math::Vec2 textureSize, StrokeBuilder& strokes);
    [[nodiscard]] static bool isLook(const StrokeLook& look, const Stroke& stroke, math::Vec2 textureSize) noexcept;

    std::vector<Path::Verb> verbs;
    std::vector<math::Vec2> points;
    std::vector<Path::Arc> arcs;
    std::uint64_t revision = 0;
    std::uint64_t draws = 0;

    std::array<FillBuild, kBuildsKept> fillBuilds;
    std::vector<math::Vec2> simplePoints;
    std::vector<std::uint32_t> simpleCounts;

    std::array<StrokeBuild, kBuildsKept> strokeBuilds;
};

} // namespace haylen::graphics2d
