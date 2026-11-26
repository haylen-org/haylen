#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/3d/animation/MorphTargets3D.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/io/Package.hpp"
#include "support/AllocationTracker.hpp"
#include "support/GltfFiles.hpp"

namespace haylen::animation3d {

// Reads the morph targets of the glTF test files, blends them by weights on the CPU and checks the rules and limits of targets made in code.
class MorphTargets3DTest : public ::testing::Test {
  protected:
    [[nodiscard]] static gltf::Document quad() {
        static const std::unique_ptr<io::Package> files = io::Package::openDirectory(test::GltfFiles::getFolder());
        return gltf::Reader::read(*files, "morph.glb");
    }

    [[nodiscard]] static std::string refusal(std::size_t vertexCount, std::vector<MorphTargets3D::Target> targets) {
        try {
            const MorphTargets3D morph(vertexCount, std::move(targets));
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return "";
    }

    const std::vector<math::Vec3> positions{{0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, {1.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    const std::vector<math::Vec3> normals = std::vector<math::Vec3>(4, math::Vec3{0.0F, 0.0F, 1.0F});
};

// Each target keeps only the vertices it moves, and a blend adds each target times its weight over the base shape.
TEST_F(MorphTargets3DTest, BlendsTheSparseTargetsOfAGltfMesh) {
    const MorphTargets3D targets = MorphTargets3D::fromGltf(quad(), 0, 0);
    ASSERT_EQ(targets.getTargetCount(), 2U);
    EXPECT_EQ(targets.getVertexCount(), 4U);
    EXPECT_EQ(targets.getTargets()[0].vertices, (std::vector<std::uint32_t>{2}));
    EXPECT_EQ(targets.getTargets()[1].vertices, (std::vector<std::uint32_t>{1, 2}));
    EXPECT_TRUE(targets.getTargets()[1].normals.empty());

    std::vector<math::Vec3> morphed(4);
    std::vector<math::Vec3> morphedNormals(4);
    const std::vector<float> weights{1.0F, 0.5F};
    targets.apply(weights, {.positions = positions, .normals = normals, .morphedPositions = morphed, .morphedNormals = morphedNormals});
    EXPECT_EQ(morphed[0], (math::Vec3{0.0F, 0.0F, 0.0F}));
    EXPECT_EQ(morphed[1], (math::Vec3{1.25F, 0.0F, 0.0F}));
    EXPECT_EQ(morphed[2], (math::Vec3{1.25F, 1.0F, 0.5F}));
    EXPECT_EQ(morphedNormals[2], (math::Vec3{0.0F, 0.0F, 1.0F}));

    // Weights below zero and above one reach past the targets, as glTF allows.
    const std::vector<float> beyond{-1.0F, 2.0F};
    targets.apply(beyond, {.positions = positions, .morphedPositions = morphed});
    EXPECT_EQ(morphed[2], (math::Vec3{2.0F, 1.0F, -0.5F}));
}

// A blend checks its weights and vertices, and targets made in code obey the rules of sparse targets.
TEST_F(MorphTargets3DTest, RefusesWeightsAndTargetsPastTheirLimits) {
    const MorphTargets3D targets = MorphTargets3D::fromGltf(quad(), 0, 0);
    std::vector<math::Vec3> morphed(4);
    EXPECT_THROW(targets.apply(std::vector<float>{1.0F}, {.positions = positions, .morphedPositions = morphed}), std::invalid_argument);
    EXPECT_THROW(targets.apply(std::vector<float>{1.0F, std::nanf("")}, {.positions = positions, .morphedPositions = morphed}), std::invalid_argument);
    EXPECT_THROW(targets.apply(std::vector<float>{1.0F, 0.0F}, {.positions = std::span(positions).first(3), .morphedPositions = morphed}), std::invalid_argument);

    EXPECT_EQ(refusal(4, {{.name = "smile", .vertices = {1, 4}, .positions = {{}, {}}}}), "The morph target \"smile\" names vertex 4 at place 1, while the vertices of a target grow and stay below the 4 vertices of the mesh.");
    EXPECT_EQ(refusal(4, {{.vertices = {2, 1}, .positions = {{}, {}}}}), "Morph target 0 names vertex 1 at place 1, while the vertices of a target grow and stay below the 4 vertices of the mesh.");
    EXPECT_EQ(refusal(4, {{.name = "smile", .vertices = {1}, .positions = {{}, {}}}}), "The morph target \"smile\" holds 2 differences of an attribute for its 1 vertices.");
    EXPECT_EQ(refusal(4, {{.name = "smile", .vertices = {1}, .normals = {{std::nanf(""), 0.0F, 0.0F}}}}), "The morph target \"smile\" holds a difference that is not finite.");
    EXPECT_EQ(refusal(4, {{.name = "smile"}, {.name = "smile"}}), "The morph target \"smile\" has the name of another target, while each target needs a name of its own.");
}

// Many targets that each move a few vertices of a large mesh take the memory of those vertices alone, and a blend of them allocates nothing.
TEST_F(MorphTargets3DTest, KeepsManySparseTargetsSmall) {
    constexpr std::size_t kVertices = 10000;
    std::vector<MorphTargets3D::Target> list;
    for (std::uint32_t target = 0; target < 200; ++target) {
        list.push_back({.name = "shape" + std::to_string(target), .vertices = {target * 40, target * 40 + 1}, .positions = {{0.0F, 0.01F, 0.0F}, {0.0F, 0.02F, 0.0F}}});
    }

    const MorphTargets3D targets(kVertices, std::move(list));
    EXPECT_LT(targets.getMemorySize(), 200U * 2U * 16U + 1U);
    EXPECT_EQ(targets.findTarget("shape120"), 120U);

    const std::vector<math::Vec3> base(kVertices);
    std::vector<math::Vec3> morphed(kVertices);
    std::vector<float> weights(200, 0.0F);
    weights[120] = 1.0F;
    const test::AllocationTracker tracker;
    targets.apply(weights, {.positions = base, .morphedPositions = morphed});
    EXPECT_EQ(tracker.getTotal(), 0U);
    EXPECT_EQ(morphed[4801], (math::Vec3{0.0F, 0.02F, 0.0F}));
}

// The targets of a face named after the face expressions are found by the expressions, which hold the 52 names that face tracking reports.
TEST_F(MorphTargets3DTest, FindsTheTargetsOfFaceExpressions) {
    EXPECT_EQ(core::EnumNames<FaceExpression3D>::kNames.size(), 52U);
    EXPECT_EQ(core::EnumNames<FaceExpression3D>::name(FaceExpression3D::JawOpen), "jawOpen");
    EXPECT_EQ(core::EnumNames<FaceExpression3D>::fromName("tongueOut"), FaceExpression3D::TongueOut);

    std::vector<MorphTargets3D::Target> face{{.name = "eyeBlinkLeft", .vertices = {0}, .positions = {{0.0F, -0.01F, 0.0F}}}, {.name = "jawOpen", .vertices = {1}, .positions = {{0.0F, -0.05F, 0.0F}}}};
    const MorphTargets3D targets(4, std::move(face));
    EXPECT_EQ(targets.findTarget(FaceExpression3D::JawOpen), 1U);
    EXPECT_FALSE(targets.findTarget(FaceExpression3D::CheekPuff).has_value());
}

} // namespace haylen::animation3d
