#include <gtest/gtest.h>

#include <stdexcept>

#include "audio/MixerFixture.hpp"
#include "haylen/2d/graphics/Camera.hpp"
#include "haylen/audio/Mixer.hpp"

namespace haylen::audio {

class SpatializationTest : public MixerFixture {};

TEST_F(SpatializationTest, AttenuationModelsFollowOpenAL) {
    using Model = Mixer::Spatialization::Model;

    Mixer::Spatialization linear{.minDistance = 100.0F, .maxDistance = 1100.0F};
    EXPECT_EQ(linear.getGain(50.0F), 1.0F);
    EXPECT_EQ(linear.getGain(100.0F), 1.0F);
    EXPECT_NEAR(linear.getGain(600.0F), 0.5F, 1e-6F);
    EXPECT_EQ(linear.getGain(1100.0F), 0.0F);
    EXPECT_EQ(linear.getGain(5000.0F), 0.0F);
    linear.rolloff = 0.5F;
    EXPECT_NEAR(linear.getGain(1100.0F), 0.5F, 1e-6F);
    linear.rolloff = 2.0F;
    EXPECT_EQ(linear.getGain(800.0F), 0.0F);

    // Inverse and exponential fade more slowly and stop fading at the maximum distance.
    const Mixer::Spatialization inverse{.model = Model::Inverse, .minDistance = 100.0F, .maxDistance = 1100.0F};
    EXPECT_EQ(inverse.getGain(50.0F), 1.0F);
    EXPECT_NEAR(inverse.getGain(200.0F), 0.5F, 1e-6F);
    EXPECT_NEAR(inverse.getGain(400.0F), 0.25F, 1e-6F);
    EXPECT_NEAR(inverse.getGain(5000.0F), 100.0F / 1100.0F, 1e-6F);

    const Mixer::Spatialization exponential{.model = Model::Exponential, .minDistance = 100.0F, .maxDistance = 1100.0F, .rolloff = 2.0F};
    EXPECT_NEAR(exponential.getGain(200.0F), 0.25F, 1e-6F);
    EXPECT_NEAR(exponential.getGain(5000.0F), 1.0F / 121.0F, 1e-6F);
}

TEST_F(SpatializationTest, PansByTheHorizontalOffset) {
    const Mixer::Spatialization settings{.panDistance = 500.0F};
    EXPECT_EQ(settings.getPan({250.0F, 999.0F}), 0.5F);
    EXPECT_EQ(settings.getPan({-1000.0F, 0.0F}), -1.0F);
    EXPECT_EQ(settings.getPan({0.0F, -300.0F}), 0.0F);
}

TEST_F(SpatializationTest, DopplerShiftsThePitchOfApproachingVoices) {
    Mixer::Spatialization settings{.doppler = 1.0F, .speedOfSound = 340.0F};
    const math::Vec2 right{100.0F, 0.0F};
    EXPECT_NEAR(settings.getDopplerPitch(right, {}, {-34.0F, 0.0F}), 340.0F / 306.0F, 1e-5F);
    EXPECT_NEAR(settings.getDopplerPitch(right, {}, {34.0F, 0.0F}), 340.0F / 374.0F, 1e-5F);
    EXPECT_NEAR(settings.getDopplerPitch(right, {34.0F, 0.0F}, {}), 374.0F / 340.0F, 1e-5F);
    EXPECT_EQ(settings.getDopplerPitch(right, {}, {0.0F, 50.0F}), 1.0F);
    EXPECT_EQ(settings.getDopplerPitch({}, {}, {-34.0F, 0.0F}), 1.0F);
    EXPECT_EQ(settings.getDopplerPitch(right, {}, {-1000.0F, 0.0F}), Mixer::Spatialization::kMaxDopplerPitch);
    EXPECT_EQ(settings.getDopplerPitch(right, {-1000.0F, 0.0F}, {}), Mixer::Spatialization::kMinDopplerPitch);
    settings.doppler = 0.0F;
    EXPECT_EQ(settings.getDopplerPitch(right, {}, {-34.0F, 0.0F}), 1.0F);
}

TEST_F(SpatializationTest, MovingVoicesPlayFasterAsTheyApproach) {
    mixer.setSpatialization({.maxDistance = 2000.0F, .doppler = 1.0F, .speedOfSound = 1000.0F});
    const Mixer::VoiceId voice = mixer.play(makeTone(toFrames(4.0F)), {.position = math::Vec2{500.0F, 0.0F}});
    mixer.setPosition(voice, {480.0F, 0.0F});
    mixer.update(0.1F);

    float start = mixer.getCursor(voice);
    peak(toFrames(0.5F));
    EXPECT_NEAR(mixer.getCursor(voice) - start, 0.625F, 0.02F);
    EXPECT_EQ(mixer.getPitch(voice), 1.0F);

    mixer.update(0.1F);
    start = mixer.getCursor(voice);
    peak(toFrames(0.5F));
    EXPECT_NEAR(mixer.getCursor(voice) - start, 0.5F, 0.02F);
}

TEST_F(SpatializationTest, TheListenerFollowsACamera) {
    graphics2d::Camera camera;
    camera.position = {300.0F, 40.0F};
    camera.offset = {10.0F, 0.0F};
    camera.addTrauma(1.0F);
    camera.update(0.1F);
    mixer.followCamera(&camera);
    EXPECT_EQ(mixer.getListener(), math::Vec2(310.0F, 40.0F));

    mixer.play(makeTone(toFrames(1.0F)), {.loop = true, .position = math::Vec2{3000.0F, 40.0F}});
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(settledPeak(256), 0.0F);
    camera.position = {2990.0F, 40.0F};
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getListener(), math::Vec2(3000.0F, 40.0F));
    EXPECT_NEAR(settledPeak(256), 0.5F, 0.01F);

    mixer.followCamera(nullptr);
    camera.position = {};
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getListener(), math::Vec2(3000.0F, 40.0F));
}

TEST_F(SpatializationTest, RejectsInvalidSettings) {
    using Model = Mixer::Spatialization::Model;
    mixer.setSpatialization({.model = Model::Inverse, .minDistance = 20.0F, .maxDistance = 400.0F, .rolloff = 2.0F});
    EXPECT_EQ(mixer.getSpatialization().model, Model::Inverse);
    EXPECT_EQ(mixer.getSpatialization().rolloff, 2.0F);

    EXPECT_THROW(mixer.setSpatialization({.model = Model::Exponential, .minDistance = 0.0F}), std::invalid_argument);
    EXPECT_THROW(mixer.setSpatialization({.rolloff = -1.0F}), std::invalid_argument);
    EXPECT_THROW(mixer.setSpatialization({.doppler = -1.0F}), std::invalid_argument);
    EXPECT_THROW(mixer.setSpatialization({.panDistance = 0.0F}), std::invalid_argument);
    EXPECT_THROW(mixer.setSpatialization({.speedOfSound = 0.0F}), std::invalid_argument);
    EXPECT_EQ(mixer.getSpatialization().maxDistance, 400.0F);
}

} // namespace haylen::audio
