#include <gtest/gtest.h>

#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "haylen/assets/Manager.hpp"
#include "support/GltfFiles.hpp"
#include "support/LuaFixture.hpp"
#include "support/TestFiles.hpp"

namespace haylen::lua::gltf {

class GltfLuaTest : public ::testing::Test {
  protected:
    // The test files of the reader as the files of a package.
    [[nodiscard]] static std::map<std::string, std::string> files() {
        std::map<std::string, std::string> package;
        const std::filesystem::path folder = test::GltfFiles::getFolder();
        for (const std::filesystem::directory_entry& entry : std::filesystem::recursive_directory_iterator(folder / "content")) {
            if (entry.is_regular_file()) {
                package[std::filesystem::relative(entry.path(), folder).generic_string()] = test::TestFiles::toText(test::TestFiles::read(entry.path()));
            }
        }

        return package;
    }
};

// A document crosses into Lua as one graph of tables, whose references are the tables themselves and whose objects name their place in the file from zero.
TEST_F(GltfLuaTest, ReadsADocumentAsAGraphOfTables) {
    test::LuaFixture fixture(files());
    fixture.runLua("gltf = require('haylen.gltf') document = gltf.read('cube.glb')");
    EXPECT_EQ(fixture.lua("return document.file .. ' ' .. document.asset.version .. ' ' .. document.asset.copyright"), "cube.glb 2.0 Made for the tests of Haylen.");
    EXPECT_EQ(fixture.lua("return tostring(document.scene == document.scenes[1]) .. ' ' .. document.scene.nodes[1].name .. ' ' .. tostring(document.nodes[1].mesh == document.meshes[1])"), "true Cube true");
    EXPECT_EQ(fixture.lua("local primitive = document.meshes[1].primitives[1] return primitive.material.name .. ' ' .. primitive.mode .. ' ' .. primitive.indices.count .. ' ' .. primitive.indices.componentType .. ' ' .. primitive.attributes.POSITION.type"), "Checker triangles 36 unsignedShort vec3");
    EXPECT_EQ(fixture.lua("local texture = document.materials[1].baseColorTexture return texture.texCoord .. ' ' .. texture.texture.source.mimeType .. ' ' .. texture.texture.sampler.minFilter .. ' ' .. texture.texture.sampler.wrapS"), "0 image/png linearMipmapLinear repeat");
    EXPECT_EQ(fixture.lua("return document.nodes[1].index .. ' ' .. document.accessors[5].index .. ' ' .. tostring(document.nodes[1].extras.spin) .. ' ' .. document.buffers[1].storage .. ' ' .. document.materials[1].alphaMode"), "0 4 true glb opaque");
    EXPECT_EQ(fixture.lua("return tostring(document.nodes[1].translation) .. ' ' .. tostring(document.nodes[1].matrix)"), "Vec3(0.0, 0.0, 0.0) nil");
    EXPECT_NE(fixture.lua("document.nodes = {}").find("has no writable property \"nodes\""), std::string::npos);
}

// The accessors of a document read into float buffers and sequences of integers, and its images decode on the frame thread and on the task pool.
TEST_F(GltfLuaTest, ReadsAccessorsAndDecodesImages) {
    test::LuaFixture fixture(files());
    fixture.runLua("gltf = require('haylen.gltf') async = require('async') document = gltf.read('cube.glb') primitive = document.meshes[1].primitives[1]");
    EXPECT_EQ(fixture.lua("local positions = document:readAccessor(primitive.attributes.POSITION) return #positions .. ' ' .. positions[1]"), "72 0.5");
    EXPECT_EQ(fixture.lua("local indices = document:readUnsigned(primitive.indices) return #indices .. ' ' .. indices[1] .. ' ' .. indices[3]"), "36 0 2");
    EXPECT_EQ(fixture.lua("local image = document:decodeImage(document.images[1], {maxSize = 64}) return image.width .. ' ' .. image:pixel(0, 0):toHex()"), "16 #FFE67828");
    // clang-format off
    fixture.runLua(R"(
        async.spawn(function()
            done = document:decodeImageAsync(document.images[1]):await().height
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(done)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return done"), "16");
    EXPECT_NE(fixture.lua("document:readUnsigned(primitive.attributes.POSITION)").find("unsigned integers that are not normalized"), std::string::npos);
    EXPECT_NE(fixture.lua("document:readAccessor({index = 0})").find("The argument 2 is not a table of the list \"accessors\" of this document."), std::string::npos);
    EXPECT_NE(fixture.lua("document:readAccessor(gltf.read('cube.glb').accessors[1])").find("is not a table of the list \"accessors\""), std::string::npos);
    EXPECT_NE(fixture.lua("document:decodeImage(document.images[1], {maxSize = 8})").find("image 0 of cube.glb"), std::string::npos);
}

// Skins, animations, morph targets and cameras cross with their references, their enums as names and their optional values as nil.
TEST_F(GltfLuaTest, ReadsSkinsAnimationsMorphsAndCameras) {
    test::LuaFixture fixture(files());
    fixture.runLua("gltf = require('haylen.gltf') character = gltf.read('character.glb') morph = gltf.read('morph.glb') cameras = gltf.read('camera-light.gltf')");
    EXPECT_EQ(fixture.lua("local skin = character.skins[1] return #skin.joints .. ' ' .. skin.joints[1].name .. ' ' .. skin.skeleton.name .. ' ' .. skin.inverseBindMatrices.type .. ' ' .. skin.joints[1].parent.name"), "3 Hip Skeleton mat4 Skeleton");
    EXPECT_EQ(fixture.lua("local animation = character.animations[1] local channel = animation.channels[3] return animation.name .. ' ' .. animation.duration .. ' ' .. channel.path .. ' ' .. channel.node.name .. ' ' .. channel.sampler.interpolation .. ' ' .. tostring(channel.sampler == animation.samplers[3])"), "Wave 1.0 scale Head cubicSpline true");
    EXPECT_EQ(fixture.lua("local mesh = morph.meshes[1] return #mesh.primitives[1].targets .. ' ' .. mesh.weights[2] .. ' ' .. morph.nodes[1].weights[2] .. ' ' .. tostring(mesh.primitives[1].targets[1].POSITION.sparse.count)"), "2 0.5 0.75 1");
    EXPECT_EQ(fixture.lua("return cameras.cameras[1].type .. ' ' .. cameras.cameras[1].perspective.zfar .. ' ' .. tostring(cameras.cameras[2].perspective.zfar) .. ' ' .. cameras.cameras[3].orthographic.xmag .. ' ' .. cameras.extensionsUsed[1] .. ' ' .. #cameras.extensions.KHR_lights_punctual.lights"), "perspective 100.0 nil 4.0 KHR_lights_punctual 1");
}

// A file that breaks a rule reaches Lua with the sentence that names the file, and a synchronous read never waits for a file that is not on the device.
TEST_F(GltfLuaTest, RaisesTheSentencesOfTheReader) {
    test::LuaFixture fixture(files());
    fixture.runLua("gltf = require('haylen.gltf')");
    EXPECT_NE(fixture.lua("gltf.read('invalid/uri-network.gltf')").find("The glTF file \"invalid/uri-network.gltf\" names the URI \"https://example.com/cube.bin\" of buffer 0, which is an address and not a file of the package."), std::string::npos);
    EXPECT_NE(fixture.lua("gltf.read('missing.glb')").find("The glTF file \"missing.glb\" is not in the content folder of the package."), std::string::npos);
    EXPECT_NE(fixture.lua("gltf.read('../cube.glb')").find("must stay inside its root folder"), std::string::npos);

    haylen::assets::Manager& assets = fixture.engine().getAssets();
    // clang-format off
    assets.setAvailability({
        .isLocal = [](std::string_view path) { return path != "cube/cube.bin"; },
        .acquire = [](std::vector<std::string>, haylen::assets::Manager::GroupProgress, std::function<void(std::string)> done) { done("The download failed."); },
    });
    // clang-format on
    EXPECT_NE(fixture.lua("gltf.read('cube/cube.gltf')").find("The asset \"cube/cube.bin\" is not on the device yet"), std::string::npos);
}

// An asynchronous read brings the file and then the files beside it onto the device before it reads them, and a failure rejects its promise with its sentence.
TEST_F(GltfLuaTest, ReadsAsynchronouslyAfterTheFilesComeOntoTheDevice) {
    test::LuaFixture fixture(files());
    haylen::assets::Manager& assets = fixture.engine().getAssets();
    std::vector<std::string> delivered;
    std::vector<std::vector<std::string>> requests;
    // clang-format off
    assets.setAvailability({
        .isLocal = [&delivered](std::string_view path) { return std::ranges::find(delivered, path) != delivered.end(); },
        .acquire = [&delivered, &requests](std::vector<std::string> paths, haylen::assets::Manager::GroupProgress, std::function<void(std::string)> done) {
            requests.push_back(paths);
            delivered.insert(delivered.end(), paths.begin(), paths.end());
            done("");
        },
    });
    fixture.runLua(R"(
        gltf = require('haylen.gltf')
        async = require('async')
        async.spawn(function()
            local document = gltf.readAsync('cube/cube.gltf'):await()
            done = document.images[1].path .. ' ' .. document.buffers[1].uri
        end)
        async.spawn(function()
            local _, failure = gltf.readAsync('invalid/accessor-outside-buffer.gltf'):await()
            refused = failure
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return tostring(done ~= nil and refused ~= nil)") == "true" || fixture.engine().getError() != nullptr; }));
    ASSERT_EQ(fixture.engine().getError(), nullptr) << fixture.engine().getError()->what();
    EXPECT_EQ(fixture.lua("return done"), "textures/checker-board.png cube.bin");
    EXPECT_NE(fixture.lua("return refused").find("accessor 0 reads past the end of buffer view 1"), std::string::npos);
    ASSERT_EQ(requests.size(), 3U);
    EXPECT_EQ(requests[0], (std::vector<std::string>{"cube/cube.gltf"}));
    EXPECT_EQ(requests[2], (std::vector<std::string>{"cube/cube.bin", "textures/checker-board.png"}));
}

} // namespace haylen::lua::gltf
