#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/3d/animation/Clip3D.hpp"
#include "haylen/3d/gltf/Reader.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/math/Quat.hpp"
#include "support/AllocationTracker.hpp"
#include "support/GltfFiles.hpp"

namespace haylen::animation3d {

// Builds clips from tracks, events and glTF files, samples them in one buffer and edits them.
class Clip3DTest : public ::testing::Test {
  protected:
    [[nodiscard]] static gltf::Document read(std::string_view path) {
        static const std::unique_ptr<io::Package> files = io::Package::openDirectory(test::GltfFiles::getFolder());
        return gltf::Reader::read(*files, path);
    }

    [[nodiscard]] static Track3D line(std::string target, std::string property, std::vector<float> times, std::vector<float> values) {
        return Track3D({.target = std::move(target), .property = std::move(property), .times = std::move(times), .values = std::move(values)});
    }

    [[nodiscard]] static Clip3D walk() {
        std::vector<Track3D> tracks;
        tracks.push_back(Track3D({.target = "Hips", .property = "position", .type = Track3D::Type::Vec3, .times = {0.0F, 1.0F}, .values = {0.0F, 1.0F, 0.0F, 2.0F, 1.0F, 0.0F}}));
        tracks.push_back(line("Lamp", "light.intensity", {0.0F, 0.5F, 1.0F}, {1.0F, 3.0F, 1.0F}));
        tracks.push_back(Track3D({.target = "Hips", .property = "mood", .type = Track3D::Type::String, .interpolation = Track3D::Interpolation::Step, .times = {0.0F, 0.5F}, .strings = {"calm", "happy"}}));
        return Clip3D({.name = "walk", .tracks = std::move(tracks), .events = {{.time = 0.75F, .name = "step", .data = {{"foot", "right"}}}, {.time = 0.25F, .name = "step", .data = {{"foot", "left"}}}}});
    }

    [[nodiscard]] static std::vector<float> sample(const Clip3D& clip, float time) {
        std::vector<float> values(clip.getValueCount());
        std::vector<std::size_t> cursors(clip.getTracks().size());
        clip.sample(time, values, cursors);
        return values;
    }

    // The values of the walk without its string track, whose keys count differently once edits add keys.
    [[nodiscard]] static std::vector<float> numbers(const Clip3D& clip, float time) {
        std::vector<float> values = sample(clip, time);
        values.resize(4);
        return values;
    }
};

// A clip lays the values of its tracks one after another, keeps its events in the order of their times and lasts until its last key or event.
TEST_F(Clip3DTest, SamplesEveryTrackIntoOneBuffer) {
    const Clip3D clip = walk();
    EXPECT_EQ(clip.getDuration(), 1.0F);
    EXPECT_EQ(clip.getValueCount(), 5U);
    EXPECT_EQ(clip.getValueOffset(1), 3U);
    EXPECT_EQ(clip.findTrack("Lamp", "light.intensity"), 1U);
    EXPECT_FALSE(clip.findTrack("Lamp", "position").has_value());
    EXPECT_EQ(clip.getEvents()[0].data.at("foot"), "left");
    EXPECT_EQ(sample(clip, 0.5F), (std::vector<float>{1.0F, 1.0F, 0.0F, 3.0F, 1.0F}));
    EXPECT_THROW((void)clip.getValueOffset(3), std::out_of_range);

    std::vector<float> values(clip.getValueCount());
    std::vector<std::size_t> cursors(clip.getTracks().size());
    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 120; ++frame) {
        clip.sample(static_cast<float>(frame) / 120.0F, values, cursors);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
}

TEST_F(Clip3DTest, RefusesTracksAndEventsThatBreakTheRules) {
    std::vector<Track3D> twice;
    twice.push_back(line("Lamp", "light.intensity", {0.0F}, {1.0F}));
    twice.push_back(line("Lamp", "light.intensity", {0.0F}, {2.0F}));
    EXPECT_THROW(Clip3D({.name = "glow", .tracks = twice}), std::invalid_argument);
    try {
        const Clip3D clip({.name = "glow", .tracks = std::move(twice)});
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The clip \"glow\" holds the \"light.intensity\" track of \"Lamp\" twice, while a clip animates each property of a target once.");
    }

    EXPECT_THROW(Clip3D({.name = "glow", .events = {{.time = -1.0F, .name = "flash"}}}), std::invalid_argument);
    EXPECT_THROW(Clip3D({.name = "glow", .events = {{.time = 1.0F}}}), std::invalid_argument);
    EXPECT_THROW(Clip3D({.name = "glow", .events = {{.time = 2.0F, .name = "flash"}}, .duration = 1.0F}), std::invalid_argument);
}

// The clip of a glTF animation names its nodes by their names and moves their transforms with the interpolation of each sampler.
TEST_F(Clip3DTest, ReadsTheAnimationsOfAGltfFile) {
    const gltf::Document document = read("character.glb");
    const Clip3D clip = Clip3D::fromGltf(document, 0);
    EXPECT_EQ(clip.getName(), "Wave");
    EXPECT_EQ(clip.getDuration(), 1.0F);
    ASSERT_EQ(clip.getTracks().size(), 3U);
    const Track3D& rotation = clip.getTracks()[0];
    EXPECT_EQ(rotation.getTarget(), "Spine");
    EXPECT_EQ(rotation.getProperty(), "rotation");
    EXPECT_EQ(rotation.getType(), Track3D::Type::Quat);
    EXPECT_EQ(clip.getTracks()[1].getProperty(), "position");
    EXPECT_EQ(clip.getTracks()[1].getInterpolation(), Track3D::Interpolation::Step);
    EXPECT_EQ(clip.getTracks()[2].getProperty(), "scale");
    EXPECT_EQ(clip.getTracks()[2].getInterpolation(), Track3D::Interpolation::CubicSpline);

    const std::vector<float> values = sample(clip, 0.25F);
    EXPECT_TRUE(math::Quat::approximately({values[0], values[1], values[2], values[3]}, math::Quat::fromAxisAngle({0.0F, 0.0F, 1.0F}, 0.7853982F)));
    EXPECT_EQ(values[5], 1.0F);
    EXPECT_EQ(sample(clip, 0.5F)[5], 1.25F);
    EXPECT_FLOAT_EQ(sample(clip, 0.5F)[7], 1.5F);

    const Clip3D morph = Clip3D::fromGltf(read("morph.glb"), 0);
    ASSERT_EQ(morph.getTracks().size(), 1U);
    EXPECT_EQ(morph.getTracks()[0].getTarget(), "Quad");
    EXPECT_EQ(morph.getTracks()[0].getComponentCount(), 2U);
    EXPECT_EQ(sample(morph, 0.0F), (std::vector<float>{0.0F, 1.0F}));
    EXPECT_THROW((void)Clip3D::fromGltf(document, 1), std::out_of_range);
}

// A document made in code is checked again, so a hostile channel is refused with a sentence that names the file and the animation.
TEST_F(Clip3DTest, RefusesHostileGltfAnimations) {
    gltf::Document document = read("character.glb");
    document.animations[0].channels[0].sampler = 9;
    EXPECT_THROW((void)Clip3D::fromGltf(document, 0), std::invalid_argument);
    try {
        (void)Clip3D::fromGltf(document, 0);
    } catch (const std::invalid_argument& error) {
        EXPECT_STREQ(error.what(), "The animation 0 of the glTF file \"character.glb\" does not make a clip, since channel 0 names sampler 9, which the animation lacks. Export the model again from the tool that made it, so it follows glTF 2.0.");
    }

    gltf::Document weights = read("character.glb");
    weights.animations[0].channels[0].path = gltf::Animation::Path::Weights;
    EXPECT_THROW((void)Clip3D::fromGltf(weights, 0), std::invalid_argument);

    gltf::Document unordered = read("character.glb");
    unordered.animations[0].samplers[0].input = unordered.animations[0].samplers[0].output;
    EXPECT_THROW((void)Clip3D::fromGltf(unordered, 0), std::invalid_argument);

    gltf::Document duplicated = read("character.glb");
    duplicated.nodes[5].name = "Spine";
    const Clip3D clip = Clip3D::fromGltf(duplicated, 0);
    EXPECT_EQ(clip.getTracks()[0].getTarget(), "/nodes/3");
    EXPECT_EQ(clip.getTracks()[1].getTarget(), "/nodes/5");
}

// A trim keeps the part between its ends with its events, a join plays one clip after the other, and a resample and an optimization keep the values within their tolerance.
TEST_F(Clip3DTest, EditsReturnNewClips) {
    const Clip3D clip = walk();
    const Clip3D part = clip.trimmed(0.5F, 1.0F);
    EXPECT_EQ(part.getDuration(), 0.5F);
    ASSERT_EQ(part.getEvents().size(), 1U);
    EXPECT_EQ(part.getEvents()[0].time, 0.25F);
    EXPECT_EQ(numbers(part, 0.0F), numbers(clip, 0.5F));
    EXPECT_THROW((void)clip.trimmed(0.5F, 2.0F), std::invalid_argument);

    const Clip3D twice = clip.joined(clip.renamed("again"));
    EXPECT_EQ(twice.getName(), "walk");
    EXPECT_EQ(twice.getDuration(), 2.0F);
    EXPECT_EQ(twice.getEvents().size(), 4U);
    EXPECT_EQ(numbers(twice, 1.5F), numbers(clip, 0.5F));

    const Clip3D even = clip.resampled(10.0F);
    EXPECT_EQ(even.getTracks()[0].getKeyCount(), 11U);
    EXPECT_EQ(even.getTracks()[2].getKeyCount(), 2U);
    const Clip3D fewer = even.optimized(0.001F);
    EXPECT_EQ(fewer.getTracks()[0].getKeyCount(), 2U);
    EXPECT_EQ(fewer.getTracks()[1].getKeyCount(), 3U);

    const Clip3D small = even.compressed(0.01F);
    EXPECT_LT(small.getMemorySize(), even.getMemorySize());
    for (int step = 0; step <= 20; ++step) {
        const float time = static_cast<float>(step) / 20.0F;
        const std::vector<float> exact = sample(even, time);
        const std::vector<float> approximate = sample(small, time);
        for (std::size_t index = 0; index < exact.size(); ++index) {
            EXPECT_NEAR(approximate[index], exact[index], 0.01F) << time;
        }
    }
}

// An additive clip holds the difference to a reference pose, and a sequence of morph targets plays each target fully at its frame.
TEST_F(Clip3DTest, MakesAdditiveClipsAndMorphSequences) {
    const Clip3D clip = walk();
    const Clip3D additive = clip.toAdditive(clip, 0.0F);
    EXPECT_EQ(sample(additive, 0.0F), (std::vector<float>{0.0F, 0.0F, 0.0F, 0.0F, 0.0F}));
    EXPECT_EQ(sample(additive, 0.5F), (std::vector<float>{1.0F, 0.0F, 0.0F, 2.0F, 1.0F}));
    EXPECT_THROW((void)clip.toAdditive(Clip3D({.name = "empty"}), 0.0F), std::invalid_argument);

    const Clip3D sequence = Clip3D::fromMorphSequence({.name = "talk", .target = "Face", .targetCount = 3, .framesPerSecond = 2.0F});
    EXPECT_EQ(sequence.getDuration(), 1.5F);
    EXPECT_EQ(sample(sequence, 0.5F), (std::vector<float>{0.0F, 1.0F, 0.0F}));
    EXPECT_EQ(sample(sequence, 1.25F), (std::vector<float>{0.5F, 0.0F, 0.5F}));
    EXPECT_EQ(sample(sequence, 1.5F), (std::vector<float>{1.0F, 0.0F, 0.0F}));
    EXPECT_THROW((void)Clip3D::fromMorphSequence({.name = "talk", .target = "Face", .targetCount = 0}), std::invalid_argument);
}

} // namespace haylen::animation3d
