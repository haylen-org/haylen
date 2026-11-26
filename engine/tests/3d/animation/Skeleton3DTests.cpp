#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/3d/animation/PoseBatch3D.hpp"
#include "haylen/3d/animation/PoseSampler3D.hpp"
#include "haylen/3d/animation/Skeleton3D.hpp"
#include "haylen/3d/animation/Skin3D.hpp"
#include "haylen/3d/animation/SkinWeights3D.hpp"
#include "haylen/3d/animation/Skinning3D.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/core/JobSystem.hpp"
#include "haylen/io/Package.hpp"
#include "support/AllocationTracker.hpp"
#include "support/GltfFiles.hpp"

namespace haylen::animation3d {

// Builds the skeleton and the skin of the skinned column of the glTF test files, poses them with its clip and bends vertices on the CPU.
class Skeleton3DTest : public ::testing::Test {
  protected:
    [[nodiscard]] static const gltf::Document& character() {
        static const std::unique_ptr<io::Package> files = io::Package::openDirectory(test::GltfFiles::getFolder());
        static const gltf::Document document = gltf::Reader::read(*files, "character.glb");
        return document;
    }

    // A triangle bent by six joints, whose two sets of joints and weights give each vertex six influences that add up to one.
    [[nodiscard]] static gltf::Document sixInfluences() {
        test::GltfFiles::File file = test::GltfFiles::triangle();
        constexpr std::array<std::uint16_t, 12> kFirstJoints{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};
        constexpr std::array<float, 12> kFirstWeights{0.375F, 0.25F, 0.125F, 0.0625F, 0.375F, 0.25F, 0.125F, 0.0625F, 0.375F, 0.25F, 0.125F, 0.0625F};
        constexpr std::array<std::uint16_t, 12> kSecondJoints{4, 5, 0, 0, 4, 5, 0, 0, 4, 5, 0, 0};
        constexpr std::array<float, 12> kSecondWeights{0.125F, 0.0625F, 0.0F, 0.0F, 0.125F, 0.0625F, 0.0F, 0.0F, 0.125F, 0.0625F, 0.0F, 0.0F};
        core::Json& document = file.document;
        for (const auto& [offset, joints] : {std::pair{test::GltfFiles::append(file.binary, kFirstJoints), true}, std::pair{test::GltfFiles::append(file.binary, kFirstWeights), false}, std::pair{test::GltfFiles::append(file.binary, kSecondJoints), true}, std::pair{test::GltfFiles::append(file.binary, kSecondWeights), false}}) {
            document["bufferViews"].push_back({{"buffer", 0}, {"byteOffset", offset}, {"byteLength", joints ? 24 : 48}, {"target", 34962}});
            document["accessors"].push_back({{"bufferView", document["bufferViews"].size() - 1}, {"componentType", joints ? 5123 : 5126}, {"count", 3}, {"type", "VEC4"}});
        }

        document["buffers"][0]["byteLength"] = file.binary.size();
        document["meshes"][0]["primitives"][0]["attributes"].update({{"JOINTS_0", 3}, {"WEIGHTS_0", 4}, {"JOINTS_1", 5}, {"WEIGHTS_1", 6}});
        document["nodes"] = core::Json::parse(R"([{"mesh": 0, "skin": 0, "children": [1, 2, 3, 4, 5, 6]}, {"name": "J0"}, {"name": "J1"}, {"name": "J2"}, {"name": "J3"}, {"name": "J4"}, {"name": "J5"}])");
        document["skins"] = core::Json::parse(R"([{"joints": [1, 2, 3, 4, 5, 6]}])");
        return gltf::Reader::read(test::GltfFiles::glb(file), "models/fan.glb");
    }

    // A root and a bone that twists half a turn around the x axis, as a forearm twists at the wrist.
    [[nodiscard]] static std::shared_ptr<const Skeleton3D> twist() {
        return std::make_shared<const Skeleton3D>(std::vector<Skeleton3D::Bone>{{.name = "Root"}, {.name = "Wrist", .parent = 0}});
    }
};

// The skeleton of a skin holds its joints and every node above them, parents first, with the rest pose and the names of the nodes.
TEST_F(Skeleton3DTest, MakesTheSkeletonOfAGltfSkin) {
    const Skeleton3D skeleton = Skeleton3D::fromGltf(character(), 0);
    ASSERT_EQ(skeleton.getBoneCount(), 5U);
    const std::span<const Skeleton3D::Bone> bones = skeleton.getBones();
    EXPECT_EQ(bones[0].name, "Character");
    EXPECT_FALSE(bones[0].parent.has_value());
    EXPECT_EQ(bones[1].name, "Skeleton");
    EXPECT_EQ(bones[2].name, "Hip");
    EXPECT_EQ(bones[3].name, "Spine");
    EXPECT_EQ(bones[3].parent, 2U);
    EXPECT_EQ(bones[4].name, "Head");
    EXPECT_EQ(bones[4].rest.position, (math::Vec3{0.0F, 1.0F, 0.0F}));
    EXPECT_EQ(skeleton.findBone("Spine"), 3U);
    EXPECT_FALSE(skeleton.findBone("Tail").has_value());
    EXPECT_THROW((void)Skeleton3D::fromGltf(character(), 1), std::out_of_range);
}

TEST_F(Skeleton3DTest, RefusesBonesThatBreakTheRules) {
    EXPECT_THROW(Skeleton3D({}), std::invalid_argument);
    EXPECT_THROW(Skeleton3D({{.name = "Root"}, {.name = "Root"}}), std::invalid_argument);
    EXPECT_THROW(Skeleton3D({{.name = "Root", .parent = 1}, {.name = "Arm"}}), std::invalid_argument);
    EXPECT_THROW(Skeleton3D({{.name = "Root", .rest = {.position = {std::nanf(""), 0.0F, 0.0F}, .rotation = math::Quat::identity(), .scale = {1.0F, 1.0F, 1.0F}}}}), std::invalid_argument);

    gltf::Document cycle = character();
    cycle.nodes[2].parent = 5;
    EXPECT_THROW((void)Skeleton3D::fromGltf(cycle, 0), std::invalid_argument);
}

// A clip of the file poses the bones of its names, and the palette of the skin is the identity at rest, since the inverse bind matrices undo the rest pose.
TEST_F(Skeleton3DTest, PosesAndBuildsThePaletteOfTheSkin) {
    auto skeleton = std::make_shared<const Skeleton3D>(Skeleton3D::fromGltf(character(), 0));
    const Skin3D skin = Skin3D::fromGltf(character(), 0, skeleton);
    ASSERT_EQ(skin.getJointCount(), 3U);
    EXPECT_EQ(skin.getJoints()[2].bone, 4U);

    Pose3D pose(skeleton);
    std::vector<math::Mat4> palette(skin.getJointCount());
    skin.computePalette(pose, palette);
    for (const math::Mat4& matrix : palette) {
        EXPECT_TRUE(math::Mat4::approximately(matrix, math::Mat4::identity()));
    }

    const PoseSampler3D sampler(std::make_shared<const Clip3D>(Clip3D::fromGltf(character(), 0)), skeleton);
    EXPECT_EQ(sampler.getBoundTrackCount(), 3U);
    std::vector<std::size_t> cursors(sampler.getBoundTrackCount());
    sampler.sample(0.5F, pose.getLocalTransforms(), cursors);
    pose.computeModelMatrices();
    EXPECT_TRUE(math::Vec3::approximately(pose.getModelMatrices()[4].getTranslation(), {-1.25F, 1.0F, 0.0F}));

    // The head turns a quarter turn around z with the spine, so a vertex of the head lands where the head went.
    skin.computePalette(pose, palette);
    EXPECT_TRUE(math::Vec3::approximately(palette[2].apply({0.0F, 2.0F, 0.0F}), {-1.25F, 1.0F, 0.0F}));
    EXPECT_THROW(skin.computePalette(Pose3D(std::make_shared<const Skeleton3D>(Skeleton3D::fromGltf(character(), 0))), palette), std::invalid_argument);

    std::vector<float> rows(skin.getJointCount() * Skin3D::kRowFloats);
    Skin3D::writeRows(palette, rows);
    EXPECT_EQ(rows[3], palette[0].get(0, 3));
    EXPECT_EQ(rows[12 + 7], palette[1].get(1, 3));
}

// Every vertex keeps its largest weights in their order, the lower joint first between equal weights, and they add up to one again.
TEST_F(Skeleton3DTest, PacksFourOrEightInfluencesPerVertex) {
    const gltf::Document document = sixInfluences();
    const SkinWeights3D four = SkinWeights3D::fromGltf(document, 0, 0, 6, 4);
    EXPECT_EQ(four.getVertexCount(), 3U);
    EXPECT_EQ(std::vector<std::uint16_t>(four.getJoints().begin(), four.getJoints().begin() + 4), (std::vector<std::uint16_t>{0, 1, 2, 4}));
    EXPECT_FLOAT_EQ(four.getWeights()[0], 0.375F / 0.875F);
    EXPECT_FLOAT_EQ(four.getWeights()[3], 0.125F / 0.875F);

    const SkinWeights3D eight = SkinWeights3D::fromGltf(document, 0, 0, 6, 8);
    EXPECT_EQ(std::vector<std::uint16_t>(eight.getJoints().begin(), eight.getJoints().begin() + 8), (std::vector<std::uint16_t>{0, 1, 2, 4, 3, 5, 0, 0}));
    EXPECT_EQ(std::vector<float>(eight.getWeights().begin(), eight.getWeights().begin() + 8), (std::vector<float>{0.375F, 0.25F, 0.125F, 0.125F, 0.0625F, 0.0625F, 0.0F, 0.0F}));

    const SkinWeights3D column = SkinWeights3D::fromGltf(character(), 0, 0, 3, 4);
    EXPECT_EQ(std::vector<std::uint16_t>(column.getJoints().begin(), column.getJoints().begin() + 4), (std::vector<std::uint16_t>{0, 1, 2, 0}));

    EXPECT_THROW((void)SkinWeights3D::fromGltf(document, 0, 0, 6, 5), std::invalid_argument);
    EXPECT_THROW((void)SkinWeights3D::fromGltf(document, 0, 0, 4, 4), std::invalid_argument);
}

// Joints and weights that break the rules are refused with sentences that name the vertex.
TEST_F(Skeleton3DTest, RefusesHostileWeights) {
    // clang-format off
    const auto refusal = [](std::vector<std::uint16_t> joints, std::vector<float> weights) -> std::string {
        try {
            const SkinWeights3D packed(4, std::move(joints), std::move(weights), 2);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return "";
    };
    // clang-format on

    EXPECT_EQ(refusal({0, 2, 0, 0}, {0.5F, 0.5F, 0.0F, 0.0F}), "Vertex 0 of the skin weights names joint 2 of a skin with 2 joints.");
    EXPECT_EQ(refusal({0, 1, 0, 0}, {0.5F, 0.25F, 0.0F, 0.0F}), "The weights of vertex 0 of the skin weights add up to 0.75 and not to 1.");
    EXPECT_EQ(refusal({0, 1, 0, 0}, {0.5F, -0.5F, 1.0F, 0.0F}), "Vertex 0 of the skin weights has the weight -0.5, while weights are finite and zero or more.");
    EXPECT_EQ(refusal({1, 1, 0, 0}, {0.5F, 0.5F, 0.0F, 0.0F}), "Vertex 0 of the skin weights names joint 1 twice.");
    EXPECT_EQ(refusal({0, 1, 0}, {1.0F, 0.0F, 0.0F}), "The skin weights hold 3 joints and 3 weights, while each of their vertices takes 4 of both.");
}

// A vertex halfway between a bone and a bone twisted half a turn collapses onto the axis with blended matrices and keeps its distance with dual quaternions, while a vertex of one joint lands the same with both.
TEST_F(Skeleton3DTest, SkinsWithMatricesAndDualQuaternions) {
    const std::shared_ptr<const Skeleton3D> skeleton = twist();
    const Skin3D skin(skeleton, {{.bone = 0}, {.bone = 1}});
    Pose3D pose(skeleton);
    pose.getLocalTransforms()[1].rotation = math::Quat::fromAxisAngle({1.0F, 0.0F, 0.0F}, 3.14159265F);
    pose.computeModelMatrices();
    std::vector<math::Mat4> matrices(2);
    std::vector<math::DualQuat> dualQuats(2);
    skin.computePalette(pose, matrices);
    skin.computeDualQuatPalette(pose, dualQuats);

    const SkinWeights3D weights(4, {0, 1, 0, 0, 1, 0, 0, 0}, {0.5F, 0.5F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F}, 2);
    const std::vector<math::Vec3> positions{{0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    const std::vector<math::Vec3> normals{{0.0F, 1.0F, 0.0F}, {0.0F, 1.0F, 0.0F}};
    std::vector<math::Vec3> linear(2);
    std::vector<math::Vec3> linearNormals(2);
    std::vector<math::Vec3> dual(2);
    std::vector<math::Vec3> dualNormals(2);
    Skinning3D::applyLinear(weights, matrices, {.positions = positions, .normals = normals, .skinnedPositions = linear, .skinnedNormals = linearNormals});
    Skinning3D::applyDualQuat(weights, dualQuats, {.positions = positions, .normals = normals, .skinnedPositions = dual, .skinnedNormals = dualNormals});

    EXPECT_NEAR(linear[0].getLength(), 0.0F, 1e-5F);
    EXPECT_NEAR(dual[0].getLength(), 1.0F, 1e-5F);
    EXPECT_NEAR(dualNormals[0].getLength(), 1.0F, 1e-5F);
    EXPECT_TRUE(math::Vec3::approximately(linear[1], {0.0F, -1.0F, 0.0F}));
    EXPECT_TRUE(math::Vec3::approximately(dual[1], {0.0F, -1.0F, 0.0F}));
    EXPECT_THROW(Skinning3D::applyLinear(weights, std::span(matrices).first(1), {.positions = positions, .skinnedPositions = linear}), std::invalid_argument);

    // A joint scaled to zero still turns its dual quaternion by the rotations of its bones.
    pose.getLocalTransforms()[1].scale = {0.0F, 0.0F, 0.0F};
    pose.computeModelMatrices();
    skin.computeDualQuatPalette(pose, dualQuats);
    EXPECT_TRUE(math::Quat::approximately(dualQuats[1].real, pose.getLocalTransforms()[1].rotation));
}

// A batch poses every character on the job system the same as one at a time, writes every palette into one buffer and allocates nothing without a job system.
TEST_F(Skeleton3DTest, PosesCrowdsInOneBuffer) {
    auto skeleton = std::make_shared<const Skeleton3D>(Skeleton3D::fromGltf(character(), 0));
    auto skin = std::make_shared<const Skin3D>(Skin3D::fromGltf(character(), 0, skeleton));
    auto clip = std::make_shared<const Clip3D>(Clip3D::fromGltf(character(), 0));
    PoseBatch3D serial(skin);
    PoseBatch3D parallel(skin);
    for (int index = 0; index < 40; ++index) {
        const PoseBatch3D::Options options{.time = static_cast<float>(index) * 0.025F, .speed = index % 2 == 0 ? 1.0F : -0.5F};
        EXPECT_EQ(serial.add(clip, options), static_cast<std::size_t>(index));
        (void)parallel.add(clip, options);
    }

    core::JobSystem jobs([](const std::string& error) { ADD_FAILURE() << error; });
    serial.update(0.3F);
    parallel.update(0.3F, &jobs);
    ASSERT_EQ(serial.getPalettes().size(), 40U * 3U * Skin3D::kRowFloats);
    EXPECT_TRUE(std::equal(serial.getPalettes().begin(), serial.getPalettes().end(), parallel.getPalettes().begin()));
    EXPECT_FLOAT_EQ(serial.getTime(1), 0.025F - 0.15F + 1.0F);

    // The palette of a character matches a pose of its own at the same time.
    Pose3D pose(skeleton);
    const PoseSampler3D sampler(clip, skeleton);
    std::vector<std::size_t> cursors(sampler.getBoundTrackCount());
    sampler.sample(serial.getTime(3), pose.getLocalTransforms(), cursors);
    pose.computeModelMatrices();
    std::vector<math::Mat4> palette(3);
    skin->computePalette(pose, palette);
    std::vector<float> rows(3 * Skin3D::kRowFloats);
    Skin3D::writeRows(palette, rows);
    EXPECT_TRUE(std::equal(rows.begin(), rows.end(), serial.getPalettes().begin() + 3 * 3 * Skin3D::kRowFloats));

    serial.remove(0);
    EXPECT_EQ(serial.getCount(), 39U);
    EXPECT_THROW(serial.setTime(39, 0.0F), std::out_of_range);
    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 10; ++frame) {
        serial.update(1.0F / 60.0F);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
}

} // namespace haylen::animation3d
