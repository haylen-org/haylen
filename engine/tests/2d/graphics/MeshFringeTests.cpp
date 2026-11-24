#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "2d/graphics/GpuVertex.hpp"
#include "2d/graphics/MeshFringe.hpp"

namespace haylen::graphics2d {

// Two triangles that meet tip to tip, a bowtie, meet at a corner where the normals of their four edges cancel, so the corner has no way out and its fringe stays on it.
TEST(MeshFringeTest, KeepsTheFringeOnACornerWhoseNormalsCancel) {
    const std::vector<GpuVertex> vertices{
        {{0.0F, 0.0F}, {0.0F, 0.0F}, 0xFFFFFFFFU}, {{-10.0F, -5.0F}, {0.0F, 0.0F}, 0xFFFFFFFFU}, {{-10.0F, 5.0F}, {0.0F, 0.0F}, 0xFFFFFFFFU}, {{0.0F, 0.0F}, {0.0F, 0.0F}, 0xFFFFFFFFU}, {{10.0F, 5.0F}, {0.0F, 0.0F}, 0xFFFFFFFFU}, {{10.0F, -5.0F}, {0.0F, 0.0F}, 0xFFFFFFFFU},
    };
    const std::vector<std::uint32_t> indices{0, 1, 2, 3, 4, 5};

    MeshFringe fringe;
    fringe.build(vertices, indices, 1.0F);
    ASSERT_GT(fringe.getIndices().size(), indices.size());
    for (const GpuVertex& vertex : fringe.getVertices()) {
        EXPECT_TRUE(std::isfinite(vertex.position[0]) && std::isfinite(vertex.position[1]));
    }

    bool tipStays = false;
    for (const GpuVertex& vertex : fringe.getVertices()) {
        tipStays = tipStays || (vertex.position[0] == 0.0F && vertex.position[1] == 0.0F);
    }

    EXPECT_TRUE(tipStays);
}

} // namespace haylen::graphics2d
