#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include "audio/MixerFixture.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/core/ScopedConnection.hpp"
#include "haylen/platform/Event.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::audio {

class AudioInterruptionTest : public MixerFixture {
  protected:
    // Counts the audio events the engine publishes.
    void listen(core::Engine& engine) {
        for (const std::string_view name : {core::LifecycleEvent::kAudioInterrupted, core::LifecycleEvent::kAudioResumed, core::LifecycleEvent::kAudioRouteChanged}) {
            connections.push_back(engine.getEvents().on(name, [this](core::EventBus::Event& event) { published.emplace_back(event.getName()); }));
        }
    }

    std::vector<std::string> published;
    std::vector<core::Connection> connections;
};

TEST_F(AudioInterruptionTest, InterruptionsStopTheOutputUntilTheyEnd) {
    const Mixer::VoiceId paused = mixer.play(makeTone(toFrames(1.0F), 0.25F), {.loop = true});
    mixer.play(makeTone(toFrames(1.0F), 0.25F), {.loop = true});
    mixer.setPaused(paused, true);
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);

    mixer.beginInterruption();
    mixer.beginInterruption();
    EXPECT_TRUE(mixer.isInterrupted());
    EXPECT_EQ(peak(256), 0.0F);
    EXPECT_EQ(mixer.getBusStats()[3].paused, 2U);

    // The output stays stopped while the app is in the background, even after the interruption ends.
    mixer.suspend();
    mixer.endInterruption();
    EXPECT_FALSE(mixer.isInterrupted());
    EXPECT_EQ(peak(256), 0.0F);
    mixer.resume();
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);
    EXPECT_TRUE(mixer.isPaused(paused));
    mixer.endInterruption();

    std::vector<Mixer::DeviceEvent> received;
    const core::ScopedConnection connection = mixer.deviceEventReceived.connect([&received](Mixer::DeviceEvent event) { received.push_back(event); });
    mixer.reportDeviceEvent(Mixer::DeviceEvent::RouteChanged);
    mixer.reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    EXPECT_TRUE(received.empty());
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(received, (std::vector<Mixer::DeviceEvent>{Mixer::DeviceEvent::RouteChanged, Mixer::DeviceEvent::InterruptionBegan}));
    EXPECT_FALSE(mixer.isInterrupted());
}

TEST_F(AudioInterruptionTest, TheEngineResumesAudioOnlyWhileTheAppIsActive) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    Mixer& audio = engine.getAudio();
    listen(engine);

    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    fixture.frames(1);
    EXPECT_TRUE(audio.isInterrupted());
    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    fixture.frames(1);
    EXPECT_EQ(published, (std::vector<std::string>{"audioInterrupted"}));

    // An interruption that ends while the app is inactive waits for the app to become active, because the system gives the audio back only to an active app.
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionEnded);
    fixture.frames(1);
    EXPECT_TRUE(audio.isInterrupted());
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_FALSE(audio.isInterrupted());
    EXPECT_EQ(published, (std::vector<std::string>{"audioInterrupted", "audioResumed"}));

    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionEnded);
    audio.reportDeviceEvent(Mixer::DeviceEvent::RouteChanged);
    fixture.frames(1);
    EXPECT_EQ(published.back(), "audioRouteChanged");
    EXPECT_EQ(published.size(), 3U);

    // Because iOS does not always report the end of an interruption, becoming active again brings the audio back anyway.
    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_FALSE(audio.isInterrupted());
    EXPECT_EQ(published, (std::vector<std::string>{"audioInterrupted", "audioResumed", "audioRouteChanged", "audioInterrupted", "audioResumed"}));
}

TEST_F(AudioInterruptionTest, PlatformInterruptionsTakeTheSamePath) {
    test::EngineFixture fixture;
    core::Engine& engine = fixture.engine();
    listen(engine);

    engine.handleEvent({.type = platform::Event::Type::InterruptionBegan});
    EXPECT_TRUE(engine.getAudio().isInterrupted());
    EXPECT_EQ(engine.getAppState(), core::Engine::AppState::Inactive);
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    EXPECT_FALSE(engine.getAudio().isInterrupted());
    EXPECT_EQ(published, (std::vector<std::string>{"audioInterrupted", "audioResumed"}));

    // The device and the platform often report the same interruption, and it still pauses, resumes and publishes once.
    published.clear();
    engine.handleEvent({.type = platform::Event::Type::InterruptionBegan});
    engine.getAudio().reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    fixture.frames(1);
    engine.getAudio().reportDeviceEvent(Mixer::DeviceEvent::InterruptionEnded);
    fixture.frames(1);
    EXPECT_TRUE(engine.getAudio().isInterrupted());
    engine.handleEvent({.type = platform::Event::Type::InterruptionEnded});
    fixture.frames(1);
    EXPECT_FALSE(engine.getAudio().isInterrupted());
    EXPECT_EQ(published, (std::vector<std::string>{"audioInterrupted", "audioResumed"}));

    engine.setPaused(true);
    EXPECT_TRUE(engine.getAudio().isProcessPaused());
    engine.setPaused(false);
    EXPECT_FALSE(engine.getAudio().isProcessPaused());
}

} // namespace haylen::audio
