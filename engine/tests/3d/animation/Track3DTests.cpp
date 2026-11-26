#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/3d/animation/Track3D.hpp"
#include "haylen/math/Quat.hpp"
#include "support/AllocationTracker.hpp"

namespace haylen::animation3d {

// Samples tracks of every type and interpolation and checks their edits against the curves they keep.
class Track3DTest : public ::testing::Test {
  protected:
    [[nodiscard]] static Track3D numbers(Track3D::Interpolation interpolation, std::vector<float> times, std::vector<float> values, std::vector<float> handles = {}) {
        return Track3D({.target = "Box", .property = "height", .type = Track3D::Type::Number, .interpolation = interpolation, .times = std::move(times), .values = std::move(values), .handles = std::move(handles)});
    }

    [[nodiscard]] static float sample(const Track3D& track, float time) {
        std::array<float, 1> value{};
        std::size_t cursor = 0;
        track.sample(time, value, cursor);
        return value[0];
    }

    [[nodiscard]] static std::vector<float> sampleAll(const Track3D& track, float time) {
        std::vector<float> value(track.getComponentCount());
        std::size_t cursor = 0;
        track.sample(time, value, cursor);
        return value;
    }

    [[nodiscard]] static std::string refusal(const Track3D::Description& description) {
        try {
            const Track3D track(description);
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return "";
    }
};

// A track of steps holds the value of the last key at or before each time, and of two keys at one time the later one shows from that time on, so repeated keys change nothing between them.
TEST_F(Track3DTest, StepsHoldTheLastKeyWithRepeatedKeys) {
    const Track3D track = numbers(Track3D::Interpolation::Step, {0.0F, 1.0F, 1.0F, 2.0F, 3.0F}, {4.0F, 5.0F, 6.0F, 6.0F, 9.0F});
    EXPECT_EQ(sample(track, -1.0F), 4.0F);
    EXPECT_EQ(sample(track, 0.999F), 4.0F);
    EXPECT_EQ(sample(track, 1.0F), 6.0F);
    EXPECT_EQ(sample(track, 2.5F), 6.0F);
    EXPECT_EQ(sample(track, 3.0F), 9.0F);
    EXPECT_EQ(sample(track, 50.0F), 9.0F);

    const Track3D fewer = track.optimized(0.0F);
    EXPECT_EQ(fewer.getKeyCount(), 3U);
    for (const float time : {0.0F, 0.5F, 1.0F, 2.2F, 3.0F, 4.0F}) {
        EXPECT_EQ(sample(fewer, time), sample(track, time)) << time;
    }
}

// A linear track gives the values of its keys exactly at their times and at both ends, holds them outside the keys, and two keys at one time jump there.
TEST_F(Track3DTest, LinesFindTheirKeysWithExactEnds) {
    const Track3D track = numbers(Track3D::Interpolation::Linear, {0.25F, 0.5F, 0.5F, 1.75F}, {0.1F, 0.7F, 3.0F, -2.3F});
    EXPECT_EQ(sample(track, 0.0F), 0.1F);
    EXPECT_EQ(sample(track, 0.25F), 0.1F);
    EXPECT_FLOAT_EQ(sample(track, 0.375F), 0.4F);
    EXPECT_EQ(sample(track, 0.5F), 3.0F);
    EXPECT_EQ(sample(track, 1.75F), -2.3F);
    EXPECT_EQ(sample(track, 2.0F), -2.3F);
    EXPECT_FLOAT_EQ(sample(track, 1.125F), 0.35F);
}

// The cursor only speeds the search up: samples in any order give what a fresh cursor gives.
TEST_F(Track3DTest, AnyCursorFindsTheSameKey) {
    std::vector<float> times;
    std::vector<float> values;
    for (int key = 0; key < 200; ++key) {
        times.push_back(static_cast<float>(key) * 0.1F);
        values.push_back(std::sin(static_cast<float>(key)));
    }

    const Track3D track = numbers(Track3D::Interpolation::Linear, times, values);
    std::size_t cursor = 0;
    for (const float time : {0.05F, 0.12F, 19.9F, 7.31F, 7.29F, 7.3F, 0.0F, 25.0F, 3.33F, 3.34F, 3.2F}) {
        std::array<float, 1> warm{};
        track.sample(time, warm, cursor);
        EXPECT_EQ(warm[0], sample(track, time)) << time;
    }

    EXPECT_THROW((void)track.findKey(std::nanf(""), cursor), std::invalid_argument);
}

// A cubic spline follows the formula of glTF with its tangents in units per second, reaches its keys exactly, and moves on smoothly as time grows.
TEST_F(Track3DTest, CubicSplinesFollowTheTangentsOfGltf) {
    // In tangent, value and out tangent of each key, for a curve of a constant speed of two units per second.
    const Track3D track = numbers(Track3D::Interpolation::CubicSpline, {0.0F, 1.0F, 3.0F}, {0.0F, 0.0F, 2.0F, 2.0F, 2.0F, 2.0F, 2.0F, 6.0F, 2.0F});
    EXPECT_EQ(sample(track, 0.0F), 0.0F);
    EXPECT_EQ(sample(track, 1.0F), 2.0F);
    EXPECT_EQ(sample(track, 3.0F), 6.0F);
    EXPECT_NEAR(sample(track, 0.5F), 1.0F, 1e-6F);
    EXPECT_NEAR(sample(track, 2.0F), 4.0F, 1e-6F);

    // A curve that rises with growing time never steps back between samples.
    const Track3D rising = numbers(Track3D::Interpolation::CubicSpline, {0.0F, 1.0F, 2.0F}, {0.0F, 0.0F, 0.5F, 1.0F, 1.0F, 1.0F, 0.5F, 3.0F, 0.0F});
    float previous = sample(rising, 0.0F);
    for (int step = 1; step <= 400; ++step) {
        const float value = sample(rising, static_cast<float>(step) / 200.0F);
        EXPECT_GE(value, previous) << step;
        previous = value;
    }

    EXPECT_EQ(previous, 3.0F);
}

// A linear rotation turns at a constant speed along the shorter arc, even when the next key holds the opposite form of a quaternion.
TEST_F(Track3DTest, RotationsTakeTheShorterArc) {
    const math::Quat quarter = math::Quat::fromAxisAngle(math::Vec3::up(), 1.5707964F);
    const Track3D track({.target = "Hips", .property = "rotation", .type = Track3D::Type::Quat, .times = {0.0F, 1.0F}, .values = {0.0F, 0.0F, 0.0F, 1.0F, -quarter.x, -quarter.y, -quarter.z, -quarter.w}});
    const std::vector<float> half = sampleAll(track, 0.5F);
    const math::Quat sampled{half[0], half[1], half[2], half[3]};
    EXPECT_TRUE(math::Quat::approximately(sampled, math::Quat::fromAxisAngle(math::Vec3::up(), 0.7853982F)));
    EXPECT_NEAR(sampled.getLength(), 1.0F, 1e-6F);
}

// A Bezier track runs through the handles of its keys, whose times shape the speed: a curve with flat handles eases in and out, and a curve whose handles lie on the line between its keys is that line.
TEST_F(Track3DTest, BezierCurvesFollowTheHandlesOfTheirKeys) {
    const Track3D eased = numbers(Track3D::Interpolation::Bezier, {0.0F, 2.0F}, {0.0F, 10.0F}, {0.0F, 0.0F, 0.6F, 0.0F, -0.6F, 0.0F, 0.0F, 0.0F});
    EXPECT_EQ(sample(eased, 0.0F), 0.0F);
    EXPECT_NEAR(sample(eased, 1.0F), 5.0F, 1e-4F);
    EXPECT_LT(sample(eased, 0.2F), 1.0F);
    EXPECT_EQ(sample(eased, 2.0F), 10.0F);

    const Track3D straight = numbers(Track3D::Interpolation::Bezier, {0.0F, 3.0F}, {0.0F, 6.0F}, {0.0F, 0.0F, 1.0F, 2.0F, -1.0F, -2.0F, 0.0F, 0.0F});
    for (const float time : {0.3F, 1.1F, 2.9F}) {
        EXPECT_NEAR(sample(straight, time), time * 2.0F, 1e-4F) << time;
    }
}

// Every value of a track is the value its keys give, so hostile keys are refused with a sentence that names the track and the rule.
TEST_F(Track3DTest, RefusesKeysThatBreakTheRules) {
    EXPECT_EQ(refusal({.target = "Box", .property = "height", .times = {}}), "The \"height\" track of \"Box\" has no keys, while a track needs at least one key.");
    EXPECT_EQ(refusal({.target = "Box", .property = "height", .times = {0.0F, -1.0F}, .values = {0.0F, 1.0F}}), "The \"height\" track of \"Box\" has the time -1 at key 1, while the times of a track are finite and zero or more seconds.");
    EXPECT_EQ(refusal({.target = "Box", .property = "height", .times = {1.0F, 0.5F}, .values = {0.0F, 1.0F}}), "The \"height\" track of \"Box\" has key 1 at 0.5 seconds, before the key before it, while the times of a track never go back.");
    EXPECT_EQ(refusal({.target = "Box", .property = "position", .type = Track3D::Type::Vec3, .times = {0.0F, 1.0F}, .values = {0.0F, 1.0F, 2.0F}}), "The \"position\" track of \"Box\" holds 3 values, while its 2 keys of 3 components need 6.");
    EXPECT_EQ(refusal({.target = "Box", .property = "height", .times = {0.0F}, .values = {std::nanf("")}}), "The \"height\" track of \"Box\" holds the value nan at place 0 of its values, while values are finite numbers.");
    EXPECT_EQ(refusal({.target = "Box", .property = "rotation", .type = Track3D::Type::Quat, .times = {0.0F}, .values = {0.0F, 0.0F, 0.0F, 2.0F}}), "The \"rotation\" track of \"Box\" holds a quaternion of length 2 at key 0, while rotations are unit quaternions.");
    EXPECT_EQ(refusal({.target = "Box", .property = "visible", .type = Track3D::Type::Boolean, .times = {0.0F, 1.0F}, .values = {0.0F, 1.0F}}), "The \"visible\" track of \"Box\" holds boolean values, which change only by steps, so its interpolation is \"step\".");
    EXPECT_EQ(refusal({.target = "Box", .property = "visible", .type = Track3D::Type::Boolean, .interpolation = Track3D::Interpolation::Step, .times = {0.0F}, .values = {0.5F}}), "The \"visible\" track of \"Box\" holds the value 0.5 at key 0, while a boolean track holds 0 or 1.");
    EXPECT_EQ(refusal({.target = "Box", .property = "state", .type = Track3D::Type::String, .interpolation = Track3D::Interpolation::Step, .times = {0.0F, 1.0F}, .strings = {"idle"}}), "The \"state\" track of \"Box\" has 2 keys and 1 strings, while each key of a string track holds one string.");
    EXPECT_EQ(refusal({.target = "Box", .property = "height", .interpolation = Track3D::Interpolation::Bezier, .times = {0.0F, 1.0F}, .values = {0.0F, 1.0F}, .handles = {0.0F, 0.0F, 2.0F, 0.0F, -0.5F, 0.0F, 0.0F, 0.0F}}), "The \"height\" track of \"Box\" has a handle of key 0 that reaches past the key next to it, so the curve would go back in time.");
    EXPECT_EQ(refusal({.target = "Box", .property = "height", .times = {0.0F}, .values = {1.0F}, .handles = {0.0F, 0.0F, 0.0F, 0.0F}}), "The \"height\" track of \"Box\" holds Bezier handles, while only a Bezier track takes them.");
    EXPECT_EQ(refusal({.target = "Face", .property = "weights", .type = Track3D::Type::Weights, .times = {0.0F, 1.0F}, .values = {0.0F, 1.0F, 0.5F}}), "The \"weights\" track of \"Face\" holds 3 values, which do not give the same number of weights to each of its 2 keys.");
    EXPECT_EQ(refusal({.target = "Box", .property = ""}), "A track needs the property it animates, such as \"position\".");
}

// Sampling every track of many keys allocates nothing.
TEST_F(Track3DTest, SamplesWithoutAllocating) {
    const Track3D spline = numbers(Track3D::Interpolation::CubicSpline, {0.0F, 1.0F, 3.0F}, {0.0F, 0.0F, 2.0F, 2.0F, 2.0F, 2.0F, 2.0F, 6.0F, 2.0F});
    const Track3D rotation({.target = "Hips", .property = "rotation", .type = Track3D::Type::Quat, .times = {0.0F, 1.0F}, .values = {0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.7071068F, 0.0F, 0.7071068F}});
    const Track3D eased = numbers(Track3D::Interpolation::Bezier, {0.0F, 2.0F}, {0.0F, 10.0F}, {0.0F, 0.0F, 0.6F, 0.0F, -0.6F, 0.0F, 0.0F, 0.0F});
    const Track3D steps = spline.quantized();
    std::array<float, 4> value{};
    std::array<std::size_t, 4> cursors{};
    const test::AllocationTracker tracker;
    for (int frame = 0; frame < 300; ++frame) {
        const float time = static_cast<float>(frame) / 100.0F;
        spline.sample(time, std::span(value).first(1), cursors[0]);
        rotation.sample(time, value, cursors[1]);
        eased.sample(time, std::span(value).first(1), cursors[2]);
        steps.sample(time, std::span(value).first(1), cursors[3]);
    }

    EXPECT_EQ(tracker.getTotal(), 0U);
}

// A trim keeps the curve between its ends exactly, moved to start at zero, with keys at both ends.
TEST_F(Track3DTest, TrimsKeepTheCurveBetweenTheirEnds) {
    const Track3D line = numbers(Track3D::Interpolation::Linear, {0.0F, 1.0F, 2.0F}, {0.0F, 4.0F, 0.0F});
    const Track3D spline = numbers(Track3D::Interpolation::CubicSpline, {0.0F, 1.0F, 3.0F}, {0.0F, 0.0F, 1.0F, -3.0F, 2.0F, 5.0F, 1.0F, 6.0F, 0.0F});
    const Track3D eased = numbers(Track3D::Interpolation::Bezier, {0.0F, 2.0F}, {0.0F, 10.0F}, {0.0F, 0.0F, 0.6F, 3.0F, -0.6F, -1.0F, 0.0F, 0.0F});
    for (const Track3D* track : {&line, &spline, &eased}) {
        const Track3D part = track->trimmed(0.4F, 1.7F);
        EXPECT_EQ(part.getStartTime(), 0.0F);
        EXPECT_FLOAT_EQ(part.getEndTime(), 1.3F);
        for (int step = 0; step <= 26; ++step) {
            const float time = static_cast<float>(step) * 0.05F;
            EXPECT_NEAR(sample(part, time), sample(*track, time + 0.4F), 1e-4F) << time;
        }
    }

    const Track3D steps = numbers(Track3D::Interpolation::Step, {0.0F, 1.0F, 2.0F}, {1.0F, 2.0F, 3.0F});
    const Track3D part = steps.trimmed(0.5F, 1.0F);
    EXPECT_EQ(sample(part, 0.0F), 1.0F);
    EXPECT_EQ(sample(part, 0.5F), 2.0F);
    EXPECT_THROW((void)steps.trimmed(1.0F, 0.5F), std::invalid_argument);
}

// Resampling writes keys at a fixed rate that end exactly at the end, and optimizing takes away the keys that a line between the kept ones gives back within the tolerance.
TEST_F(Track3DTest, ResamplesAndOptimizesWithinTheTolerance) {
    const Track3D spline = numbers(Track3D::Interpolation::CubicSpline, {0.0F, 1.0F}, {0.0F, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F});
    const Track3D keys = spline.resampled(30.0F, 0.0F, 1.0F);
    EXPECT_EQ(keys.getInterpolation(), Track3D::Interpolation::Linear);
    EXPECT_EQ(keys.getKeyCount(), 31U);
    EXPECT_EQ(keys.getEndTime(), 1.0F);
    EXPECT_THROW((void)spline.resampled(1e30F, 0.0F, 1.0F), std::invalid_argument);

    const Track3D fewer = keys.optimized(0.01F);
    EXPECT_LT(fewer.getKeyCount(), keys.getKeyCount());
    for (int step = 0; step <= 100; ++step) {
        const float time = static_cast<float>(step) * 0.01F;
        EXPECT_NEAR(sample(fewer, time), sample(keys, time), 0.0101F) << time;
    }

    const Track3D flat = numbers(Track3D::Interpolation::Linear, {0.0F, 1.0F, 2.0F}, {3.0F, 3.0F, 3.0F});
    EXPECT_EQ(flat.optimized(0.0F).getKeyCount(), 1U);
}

// Quantized values take half the memory and move by at most half a step of the range of their column.
TEST_F(Track3DTest, QuantizesWithinHalfAStep) {
    std::vector<float> times;
    std::vector<float> values;
    for (int key = 0; key < 64; ++key) {
        times.push_back(static_cast<float>(key) / 30.0F);
        values.insert(values.end(), {std::sin(static_cast<float>(key) * 0.2F) * 3.0F, static_cast<float>(key) * 0.5F, -1.0F});
    }

    const Track3D track({.target = "Hand", .property = "position", .type = Track3D::Type::Vec3, .times = times, .values = values});
    const Track3D steps = track.quantized();
    EXPECT_EQ(steps.getStorage(), Track3D::Storage::Quantized);
    EXPECT_LT(steps.getMemorySize(), track.getMemorySize());
    EXPECT_NEAR(steps.getQuantizationError(), 31.5F / 65535.0F / 2.0F, 1e-7F);
    const std::vector<float> decoded = steps.getValues();
    for (std::size_t index = 0; index < values.size(); ++index) {
        EXPECT_LE(std::abs(decoded[index] - values[index]), steps.getQuantizationError() * 1.001F + 1e-6F) << index;
    }

    EXPECT_THROW((void)Track3D({.property = "visible", .type = Track3D::Type::Boolean, .interpolation = Track3D::Interpolation::Step, .times = {0.0F}, .values = {1.0F}}).quantized(), std::invalid_argument);
}

// A track relative to a reference subtracts positions, divides scales and turns rotations back, so adding it over the reference gives the track again.
TEST_F(Track3DTest, BecomesRelativeToAReference) {
    const Track3D position({.target = "Hips", .property = "position", .type = Track3D::Type::Vec3, .times = {0.0F, 1.0F}, .values = {1.0F, 2.0F, 3.0F, 2.0F, 2.0F, 3.0F}});
    EXPECT_EQ(sampleAll(position.relativeTo(std::vector<float>{1.0F, 2.0F, 3.0F}), 1.0F), (std::vector<float>{1.0F, 0.0F, 0.0F}));
    const Track3D scale({.target = "Hips", .property = "scale", .type = Track3D::Type::Vec3, .times = {0.0F}, .values = {2.0F, 4.0F, 1.0F}});
    EXPECT_EQ(sampleAll(scale.relativeTo(std::vector<float>{2.0F, 2.0F, 1.0F}), 0.0F), (std::vector<float>{1.0F, 2.0F, 1.0F}));
    EXPECT_THROW((void)scale.relativeTo(std::vector<float>{0.0F, 1.0F, 1.0F}), std::invalid_argument);

    const math::Quat base = math::Quat::fromAxisAngle(math::Vec3::up(), 0.4F);
    const math::Quat turned = math::Quat::fromAxisAngle(math::Vec3::up(), 1.0F);
    const Track3D rotation({.target = "Hips", .property = "rotation", .type = Track3D::Type::Quat, .times = {0.0F}, .values = {turned.x, turned.y, turned.z, turned.w}});
    const std::vector<float> delta = sampleAll(rotation.relativeTo(std::vector<float>{base.x, base.y, base.z, base.w}), 0.0F);
    EXPECT_TRUE(math::Quat::approximately(base * math::Quat{delta[0], delta[1], delta[2], delta[3]}, turned));
}

} // namespace haylen::animation3d
