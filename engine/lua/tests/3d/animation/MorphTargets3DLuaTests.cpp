#include <gtest/gtest.h>

#include <string>

#include "support/GltfFiles.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::animation3d {

// Runs the morph targets of `haylen.animation3d` from Lua on the headless engine, with the quad of two targets of the glTF test files.
class MorphTargets3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        fixture.runLua(R"(
            animation3d = require('haylen.animation3d')
            collections = require('haylen.collections')
            gltf = require('haylen.gltf')
            document = gltf.read('morph.glb')
            targets = animation3d.morphTargetsFromGltf(document, document.meshes[1])
            positions = document:readAccessor(document.meshes[1].primitives[1].attributes.POSITION)
            normals = document:readAccessor(document.meshes[1].primitives[1].attributes.NORMAL)
        )");
        // clang-format on
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture{test::GltfFiles::getPackageFiles()};
};

// The targets of a glTF mesh blend by weights from a list or a float buffer into the positions and normals of float buffers.
TEST_F(MorphTargets3DLuaTest, BlendsTargetsByWeight) {
    EXPECT_EQ(lua("return targets.targetCount .. ' ' .. targets.vertexCount .. ' ' .. #targets.names .. ' ' .. tostring(targets.memorySize > 0)"), "2 4 2 true");
    EXPECT_EQ(lua("local out = collections.newFloatBuffer(12) targets:apply({1, 0.5}, positions, out) return out[7] .. ' ' .. out[8] .. ' ' .. out[9] .. ' ' .. out[4]"), "1.25 1.0 0.5 1.25");
    EXPECT_EQ(lua("local w = collections.newFloatBuffer(2, 1) local out = collections.newFloatBuffer(12) local n = collections.newFloatBuffer(12) targets:apply(w, positions, out, {normals = normals, morphedNormals = n}) return n[9] .. ' ' .. out[7]"), "1.0 1.5");
    EXPECT_NE(lua("targets:apply({1}, positions, collections.newFloatBuffer(12))").find("The morph targets take 2 finite weights, one for each target, while 1 weights were given."), std::string::npos);
    EXPECT_NE(lua("targets:apply({1, 0}, positions, collections.newFloatBuffer(12), {normals = normals})").find("The options \"normals\" and \"morphedNormals\" come together"), std::string::npos);
}

// Targets made in Lua name their vertices from 1, are found by name, and the names of the face expressions are those that face tracking reports.
TEST_F(MorphTargets3DLuaTest, FindsTargetsByNameAndFaceExpression) {
    // clang-format off
    fixture.runLua(R"(
        face = animation3d.newMorphTargets(4, {
            {name = 'jawOpen', vertices = {2, 3}, positions = {0, -0.1, 0, 0, -0.2, 0}},
            {name = 'eyeBlinkLeft', vertices = {1}, positions = collections.newFloatBuffer(3, 0.01)},
        })
    )");
    // clang-format on
    EXPECT_EQ(lua("return face:find('jawOpen') .. ' ' .. face:find(animation3d.faceExpressions[1]) .. ' ' .. tostring(face:find('cheekPuff')) .. ' ' .. #animation3d.faceExpressions .. ' ' .. animation3d.faceExpressions[18]"), "1 2 nil 52 jawOpen");
    EXPECT_NE(lua("animation3d.newMorphTargets(4, {{name = 'smile', vertices = {5}, positions = {0, 0, 0}}})").find("The morph target \"smile\" names vertex 4 at place 0"), std::string::npos);
    EXPECT_NE(lua("animation3d.newMorphTargets(4, {{name = 'smile', vertices = {0.5}}})").find("The vertices of morph target 1 are whole numbers counted from 1, while one of them is 0.5."), std::string::npos);
    EXPECT_NE(lua("animation3d.newMorphTargets(4, {{name = 'smile', colors = {}}})").find("Unknown option \"colors\"."), std::string::npos);
}

} // namespace haylen::lua::animation3d
