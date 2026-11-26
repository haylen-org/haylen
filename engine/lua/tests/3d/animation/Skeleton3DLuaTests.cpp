#include <gtest/gtest.h>

#include <string>

#include "support/GltfFiles.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::animation3d {

// Runs the skeletons, poses, skins, skin weights and pose batches of `haylen.animation3d` from Lua on the headless engine, with the skinned column of the glTF test files.
class Skeleton3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        fixture.runLua(R"(
            animation3d = require('haylen.animation3d')
            collections = require('haylen.collections')
            gltf = require('haylen.gltf')
            m = require('haylen.math')
            document = gltf.read('character.glb')
            skeleton = animation3d.skeletonFromGltf(document, document.skins[1])
            skin = animation3d.skinFromGltf(document, document.skins[1], skeleton)
            wave = animation3d.clipFromGltf(document, document.animations[1])
            pose = animation3d.newPose(skeleton)
        )");
        // clang-format on
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture{test::GltfFiles::getPackageFiles()};
};

// A skeleton of a glTF skin names its bones after the nodes, parents first, with their rest poses.
TEST_F(Skeleton3DLuaTest, SkeletonsListTheirBones) {
    EXPECT_EQ(lua("return skeleton.boneCount .. ' ' .. skeleton.bones[4].name .. ' ' .. skeleton.bones[4].parent .. ' ' .. tostring(skeleton.bones[5].position) .. ' ' .. skeleton:findBone('Head') .. ' ' .. tostring(skeleton:findBone('Tail'))"), "5 Spine 3 Vec3(0.0, 1.0, 0.0) 5 nil");
    EXPECT_EQ(lua("local made = animation3d.newSkeleton({{name = 'Root'}, {name = 'Arm', parent = 1, position = {1, 0, 0}}}) return made.boneCount .. ' ' .. made.bones[2].parent"), "2 1");
    EXPECT_NE(lua("animation3d.newSkeleton({{name = 'Root', size = 2}})").find("Unknown option \"size\"."), std::string::npos);
    EXPECT_NE(lua("animation3d.newSkeleton({{name = 'Root'}, {name = 'Root'}})").find("Every bone of a skeleton needs a name of its own, while the name \"Root\" of bone 1 is empty or taken."), std::string::npos);
}

// A pose samples a clip into the bones of its names, and the palette of the skin moves a vertex of the head with the head.
TEST_F(Skeleton3DLuaTest, PosesBendTheSkin) {
    EXPECT_EQ(lua("pose:computeModelMatrices() local p = collections.newFloatBuffer(36) skin:palette(pose, p) return skin.jointCount .. ' ' .. p[1] .. ' ' .. p[4] .. ' ' .. p[16]"), "3 1.0 0.0 0.0");
    EXPECT_EQ(lua("pose:sample(wave, 0.5) pose:computeModelMatrices() local head = pose:modelMatrix('Head'):apply({0, 0, 0}) return string.format('%.3f %.3f', head.x, head.y)"), "-1.250 1.000");
    EXPECT_EQ(lua("return tostring(pose:transform(5).position) .. ' ' .. tostring(pose:transform('Head').scale)"), "Vec3(0.0, 1.25, 0.0) Vec3(1.5, 1.5, 1.5)");
    EXPECT_EQ(lua("pose:setTransform('Hip', m.transform3d({0, 3, 0})) pose:computeModelMatrices() return pose:modelMatrix(5):apply({0, 0, 0}).y"), "4.0");
    EXPECT_EQ(lua("pose:setRest() pose:computeModelMatrices() local q = collections.newFloatBuffer(24) skin:dualQuatPalette(pose, q) return q[4] .. ' ' .. q[12]"), "1.0 1.0");
    EXPECT_NE(lua("pose:transform('Tail')").find("The skeleton has no bone \"Tail\"."), std::string::npos);
    EXPECT_NE(lua("skin:palette(pose, collections.newFloatBuffer(4))").find("The palette of 3 joints takes 36 values"), std::string::npos);
}

// Skin weights pack the joints and weights of a primitive and bend its positions on the CPU with matrices or dual quaternions.
TEST_F(Skeleton3DLuaTest, SkinWeightsBendVertices) {
    // clang-format off
    fixture.runLua(R"(
        weights = animation3d.skinWeightsFromGltf(document, document.meshes[1], skin, {influences = 8})
        positions = document:readAccessor(document.meshes[1].primitives[1].attributes.POSITION)
        pose:sample(wave, 0.5)
        pose:computeModelMatrices()
        bent = collections.newFloatBuffer(#positions)
        twisted = collections.newFloatBuffer(#positions)
        weights:skin(pose, skin, positions, bent)
        weights:skin(pose, skin, positions, twisted, {dualQuaternions = true})
    )");
    // clang-format on
    EXPECT_EQ(lua("return weights.influenceCount .. ' ' .. weights.vertexCount .. ' ' .. weights.jointCount"), "8 16 3");
    EXPECT_EQ(lua("local joints = collections.newFloatBuffer(128) weights:readJoints(joints) local w = collections.newFloatBuffer(128) weights:readWeights(w) return joints[1] .. ' ' .. joints[2] .. ' ' .. joints[3] .. ' ' .. w[1]"), "0.0 1.0 2.0 0.5");
    // The first vertex follows the hip by half, the spine by a quarter and the head by a quarter, which the spine turned a quarter turn around z.
    EXPECT_EQ(lua("return string.format('%.4f %.4f', positions[1], bent[1]) .. ' ' .. tostring(math.abs(twisted[1] - bent[1]) > 1e-3)"), "-0.2500 0.5625 true");
    EXPECT_NE(lua("animation3d.skinWeightsFromGltf(document, document.meshes[1], skin, {influences = 6})").find("A vertex takes 4 or 8 influences, while 6 were asked for."), std::string::npos);
}

// A batch poses a crowd on the job system and reads every palette into one buffer, while a character of it matches a pose of its own.
TEST_F(Skeleton3DLuaTest, PoseBatchesPoseCrowds) {
    // clang-format off
    fixture.runLua(R"(
        crowd = animation3d.newPoseBatch(skin)
        for index = 1, 12 do
            crowd:add(wave, {time = index * 0.05, speed = 1})
        end
        crowd:update(0.25)
        palettes = collections.newFloatBuffer(crowd.count * crowd.paletteSize)
        crowd:readPalettes(palettes)
        pose:sample(wave, crowd:time(3))
        pose:computeModelMatrices()
        own = collections.newFloatBuffer(crowd.paletteSize)
        skin:palette(pose, own)
    )");
    // clang-format on
    EXPECT_EQ(lua("return crowd.count .. ' ' .. crowd.paletteSize .. ' ' .. string.format('%.2f', crowd:time(3))"), "12 36 0.40");
    EXPECT_EQ(lua("for index = 1, 36 do if math.abs(own[index] - palettes[72 + index]) > 1e-6 then return 'differs at ' .. index end end return 'same'"), "same");
    EXPECT_EQ(lua("crowd:remove(1) crowd:setSpeed(1, 2) crowd:setTimes(collections.newFloatBuffer(11, 0.5)) return crowd.count .. ' ' .. crowd:time(11)"), "11 0.5");
    EXPECT_NE(lua("crowd:time(0)").find("characters are numbered from one"), std::string::npos);
    EXPECT_NE(lua("crowd:add(wave, {pace = 2})").find("Unknown option \"pace\"."), std::string::npos);
}

} // namespace haylen::lua::animation3d
