#include <gtest/gtest.h>

#include <string>

#include "support/GltfFiles.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::animation3d {

// Runs the tracks, clips and clip players of `haylen.animation3d` from Lua on the headless engine.
class Animation3DLuaTest : public ::testing::Test {
  protected:
    void SetUp() override {
        // clang-format off
        fixture.runLua(R"(
            animation3d = require('haylen.animation3d')
            collections = require('haylen.collections')
            gltf = require('haylen.gltf')
            m = require('haylen.math')
            walk = animation3d.newClip({
                name = 'walk',
                tracks = {
                    animation3d.newTrack({target = 'Hips', property = 'position', type = 'vec3', times = {0, 1}, values = {0, 1, 0, 2, 1, 0}}),
                    animation3d.newTrack({target = 'Lamp', property = 'light.intensity', times = {0, 0.5, 1}, values = {1, 3, 1}}),
                    animation3d.newTrack({target = 'Hips', property = 'mood', type = 'string', interpolation = 'step', times = {0, 0.5}, strings = {'calm', 'happy'}}),
                },
                events = {{time = 0.25, name = 'step', data = {foot = 'left'}}, {time = 0.75, name = 'step', data = {foot = 'right'}}},
                duration = 1,
            })
        )");
        // clang-format on
    }

    std::string lua(const std::string& source) {
        return fixture.lua(source);
    }

    test::LuaFixture fixture{test::GltfFiles::getPackageFiles()};
};

// A track samples into the Lua type of its values, and its keys read back as float buffers and strings.
TEST_F(Animation3DLuaTest, TracksSampleIntoTheTypesOfTheirValues) {
    EXPECT_EQ(lua("local t = walk.tracks[1] return t.target .. ' ' .. t.property .. ' ' .. t.type .. ' ' .. t.interpolation .. ' ' .. t.storage .. ' ' .. t.keyCount .. ' ' .. t.componentCount .. ' ' .. t.endTime"), "Hips position vec3 linear float 2 3 1.0");
    EXPECT_EQ(lua("return tostring(walk.tracks[1]:sample(0.5)) .. ' ' .. walk.tracks[3]:sample(0.75) .. ' ' .. #walk.tracks[1]:values() .. ' ' .. walk.tracks[3]:strings()[2]"), "Vec3(1.0, 1.0, 0.0) happy 6 happy");
    EXPECT_EQ(lua("local t = animation3d.newTrack({property = 'visible', type = 'boolean', interpolation = 'step', times = {0, 1}, values = {1, 0}}) return tostring(t:sample(0.5)) .. ' ' .. tostring(t:sample(1)) .. ' ' .. t.target"), "true false ");
    EXPECT_EQ(lua("local q = m.fromAxisAngle(m.up(), 1) local t = animation3d.newTrack({property = 'rotation', type = 'quat', times = {0}, values = {q.x, q.y, q.z, q.w}}) return tostring(t:sample(3):angleTo(q) < 1e-6)"), "true");
    EXPECT_EQ(lua("local t = animation3d.newTrack({target = 'Face', property = 'weights', type = 'weights', times = {0, 1}, values = {0, 1, 1, 0}}) local w = t:sample(0.25) return #w .. ' ' .. w[1] .. ' ' .. w[2]"), "2 0.25 0.75");
    EXPECT_EQ(lua("local t = animation3d.newTrack({property = 'tint', type = 'color', times = {0}, values = {1, 0, 0, 1}}) return t:sample(0):toHex()"), "#FFFF0000");
    EXPECT_EQ(lua("return animation3d.newTrack({property = 'height', times = collections.newFloatBuffer(2, 1), values = collections.newFloatBuffer(2)}).keyCount"), "2");
    EXPECT_EQ(lua("local t = animation3d.newTrack({property = 'height', interpolation = 'bezier', times = {0, 2}, values = {0, 10}, handles = {0, 0, 0.6, 0, -0.6, 0, 0, 0}}) return string.format('%.3f', t:sample(1))"), "5.000");
}

// Tracks and clips refuse unknown options, unknown names of enums and keys that break the rules, with sentences.
TEST_F(Animation3DLuaTest, RefusesWrongDescriptions) {
    EXPECT_NE(lua("animation3d.newTrack({property = 'x', times = {0}, values = {0}, speed = 2})").find("Unknown option \"speed\"."), std::string::npos);
    EXPECT_NE(lua("animation3d.newTrack({property = 'x', type = 'matrix', times = {0}, values = {0}})").find("matrix"), std::string::npos);
    EXPECT_NE(lua("animation3d.newTrack({target = 'Box', property = 'x', times = {1, 0}, values = {0, 1}})").find("The \"x\" track of \"Box\" has key 1 at 0 seconds, before the key before it, while the times of a track never go back."), std::string::npos);
    EXPECT_NE(lua("animation3d.newTrack({property = 'x', times = {'soon'}, values = {0}})").find("The times are a list of numbers, and entry 1 is not a number."), std::string::npos);
    EXPECT_NE(lua("animation3d.newClip({tracks = {walk}})").find("The item 1 of the option \"tracks\" is not a track."), std::string::npos);
    EXPECT_NE(lua("animation3d.newClip({events = {{time = 1, name = 'hit', when = 2}}})").find("Unknown option \"when\"."), std::string::npos);
    EXPECT_NE(lua("animation3d.newClipPlayer(walk, {loop = 'forever'})").find("forever"), std::string::npos);
}

// A clip samples every track into one float buffer at the offsets it names, and lists its tracks and events.
TEST_F(Animation3DLuaTest, ClipsSampleIntoOneBuffer) {
    EXPECT_EQ(lua("return walk.name .. ' ' .. walk.duration .. ' ' .. walk.valueCount .. ' ' .. #walk.tracks .. ' ' .. walk.events[2].data.foot .. ' ' .. walk:offset('Lamp', 'light.intensity') .. ' ' .. tostring(walk:offset('Lamp', 'position'))"), "walk 1.0 5 3 right 4 nil");
    EXPECT_EQ(lua("local values = collections.newFloatBuffer(6) walk:sample(0.5, values, 2) return values[1] .. ' ' .. values[2] .. ' ' .. values[5] .. ' ' .. values[6]"), "0.0 1.0 3.0 1.0");
    EXPECT_EQ(lua("return walk:track('Lamp', 'light.intensity').keyCount .. ' ' .. tostring(walk:track('Lamp', 'scale'))"), "3 nil");
    EXPECT_NE(lua("walk:sample(0, collections.newFloatBuffer(2))").find("A sample of the clip \"walk\" takes 5 values"), std::string::npos);
}

// Edits return new clips: trims, joins, resamples, optimizations, compression, additive clips, names and morph sequences.
TEST_F(Animation3DLuaTest, ClipsEditIntoNewClips) {
    EXPECT_EQ(lua("local part = walk:trimmed(0.5, 1) return part.duration .. ' ' .. #part.events .. ' ' .. walk.duration"), "0.5 1 1.0");
    EXPECT_EQ(lua("local twice = walk:joined(walk) return twice.duration .. ' ' .. #twice.events .. ' ' .. twice.tracks[1].keyCount"), "2.0 4 4");
    EXPECT_EQ(lua("local even = walk:resampled(10) return even.tracks[1].keyCount .. ' ' .. even:optimized(0.001).tracks[1].keyCount"), "11 2");
    EXPECT_EQ(lua("local small = walk:resampled(30):compressed(0.01) return small.tracks[1].storage .. ' ' .. tostring(small.memorySize < walk:resampled(30).memorySize)"), "quantized true");
    EXPECT_EQ(lua("local additive = walk:toAdditive(walk, {time = 1}) return tostring(additive.tracks[1]:sample(0)) .. ' ' .. walk:renamed('stroll').name"), "Vec3(-2.0, 0.0, 0.0) stroll");
    EXPECT_EQ(lua("local talk = animation3d.clipFromMorphSequence('Face', 4, {name = 'talk', framesPerSecond = 4, loop = false}) local w = talk.tracks[1]:sample(0.5) return talk.name .. ' ' .. talk.duration .. ' ' .. w[3]"), "talk 0.75 1.0");
    EXPECT_NE(lua("walk:toAdditive(walk, {at = 1})").find("Unknown option \"at\"."), std::string::npos);
}

// The animations of a glTF document become clips whose tracks name the nodes of the file.
TEST_F(Animation3DLuaTest, ClipsComeFromGltfDocuments) {
    fixture.runLua("document = gltf.read('character.glb')");
    EXPECT_EQ(lua("local clip = animation3d.clipFromGltf(document, document.animations[1]) return clip.name .. ' ' .. clip.duration .. ' ' .. clip.tracks[1].target .. ' ' .. clip.tracks[1].property .. ' ' .. clip.tracks[3].interpolation"), "Wave 1.0 Spine rotation cubicSpline");
    EXPECT_EQ(lua("return #animation3d.clipsFromGltf(document)"), "1");
    EXPECT_NE(lua("animation3d.clipFromGltf(document, document.nodes[1])").find("The argument 2 is not a table of the list \"animations\" of this document."), std::string::npos);
}

// A player moves through its clip, reads its values, and reports events and the end of a clip that plays once to listeners that end with their owners.
TEST_F(Animation3DLuaTest, PlayersReportEventsToOwnedListeners) {
    // clang-format off
    fixture.runLua(R"(
        heard = {}
        owner = {}
        player = animation3d.newClipPlayer(walk, {loop = 'once'})
        player:on('event', function(event) heard[#heard + 1] = event.name .. ':' .. event.data.foot .. '@' .. event.time end, {owner = owner})
        player:on('complete', function() heard[#heard + 1] = 'complete' end, {owner = owner})
        player:update(0.5)
    )");
    // clang-format on
    EXPECT_EQ(lua("return table.concat(heard, ' ') .. ' ' .. player.time .. ' ' .. tostring(player:value('Hips', 'position')) .. ' ' .. player:value('Hips', 'mood')"), "step:left@0.25 0.5 Vec3(1.0, 1.0, 0.0) happy");
    EXPECT_EQ(lua("player:update(1) return table.concat(heard, ' ') .. ' ' .. tostring(player.finished)"), "step:left@0.25 step:right@0.75 complete true");
    EXPECT_EQ(lua("local values = collections.newFloatBuffer(5) player:readValues(values) return values[4]"), "1.0");

    EXPECT_EQ(lua("heard = {} player.loop = 'loop' player.speed = -2 player.time = 1 player:update(0.25) return table.concat(heard, ' ') .. ' ' .. player.time .. ' ' .. player.loop"), "step:right@0.75 0.5 loop");
    fixture.runLua("owner = nil collectgarbage() collectgarbage()");
    fixture.frames(1);
    EXPECT_EQ(lua("heard = {} player:update(0.25) return #heard"), "0");
    EXPECT_NE(lua("player:on('loop', function() end)").find("Unknown clip player event \"loop\". Clip players report \"event\" and \"complete\"."), std::string::npos);
    EXPECT_NE(lua("player.time = 4").find("The time 4 lies outside the clip \"walk\", which lasts 1 seconds."), std::string::npos);
}

// An error of a listener reaches the caller of the update that ran it.
TEST_F(Animation3DLuaTest, ListenerErrorsReachTheUpdate) {
    fixture.runLua("failing = animation3d.newClipPlayer(walk) failing:on('event', function() error('Listener failed.') end, {owner = walk})");
    EXPECT_NE(lua("failing:update(0.5)").find("Listener failed."), std::string::npos);
    EXPECT_EQ(lua("return failing.time"), "0.5");
}

} // namespace haylen::lua::animation3d
