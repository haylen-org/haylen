#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "2d/graphics/GpuVertex.hpp"
#include "haylen/2d/graphics/Stroke.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::graphics2d {

// Turns the points of a stroke into indexed triangles whose edges fade out over one pixel. Every cross section of the stroke becomes four vertices, the outer edge and the core on each side, and a section joins the one before it with three quads, so joins, caps and dashes are runs of sections around one point. The storage is kept from build to build, so building allocates nothing once it has grown.
class StrokeBuilder final {
  public:
    static constexpr std::size_t kMaxDashes = 65536;
    static constexpr int kMaxArcSteps = 64;

    // Replaces the vertices and indices with the triangles of the stroke, where `pixelSize` is the length of one pixel of the destination in the units of the points, `textureSize` the size of the texture in pixels and `flippedRows` turns its rows over for render targets that start at the bottom. Throws `std::invalid_argument` for a stroke whose values cannot draw.
    void build(std::span<const math::Vec2> points, const Stroke& stroke, float pixelSize, math::Vec2 textureSize, bool flippedRows);

    [[nodiscard]] const std::vector<GpuVertex>& getVertices() const noexcept {
        return vertices;
    }
    [[nodiscard]] const std::vector<std::uint32_t>& getIndices() const noexcept {
        return indices;
    }

  private:
    struct Node {
        math::Vec2 position{};
        float half = 0.0F;
        math::Color color{};
        float distance = 0.0F;
    };

    // A cross section: its center, the directions from the center to its left and right edges in half widths, which miters lengthen, and how much of its coverage it keeps.
    struct Section {
        math::Vec2 center{};
        math::Vec2 left{};
        math::Vec2 right{};
        float half = 0.0F;
        math::Color color{};
        float distance = 0.0F;
        float fade = 1.0F;
    };

    static void validate(std::span<const math::Vec2> points, const Stroke& stroke);
    [[nodiscard]] static Node interpolate(const Node& from, const Node& to, float distance) noexcept;
    [[nodiscard]] int arcSteps(float angle, float half) const noexcept;

    void collect(std::span<const math::Vec2> points, const Stroke& stroke);
    void buildDashes(std::span<const float> pattern, float offset);
    void buildPath(std::span<const Node> path, bool closed);
    // Fills the joint with the sections of the turn at a point, which the closed paths take apart.
    void addJoin(const Node& previous, const Node& node, const Node& next, float distance);
    void emitJoint();
    void addStartCap(const Node& node, math::Vec2 direction);
    void addEndCap(const Node& node, math::Vec2 direction);
    void emit(const Section& section);

    std::vector<Node> nodes;
    std::vector<Node> dash;
    std::vector<Section> joint;
    std::vector<GpuVertex> vertices;
    std::vector<std::uint32_t> indices;

    // The settings of the stroke that builds, and whether the next section continues a run of sections.
    Stroke::Join join = Stroke::Join::Miter;
    Stroke::Cap cap = Stroke::Cap::Butt;
    float miterLimit = 4.0F;
    float pixel = 1.0F;
    float textureScale = 0.0F;
    bool flipped = false;
    bool continues = false;
};

} // namespace haylen::graphics2d
