#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
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

class AudioPauseTest : public MixerFixture {
  protected:
    [[nodiscard]] Mixer::BusStats statsOf(const std::string& bus) const {
        const std::vector<Mixer::BusStats> stats = mixer.getBusStats();
        return *std::ranges::find_if(stats, [&bus](const Mixer::BusStats& entry) { return entry.name == bus; });
    }
};

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

TEST_F(AudioPauseTest, BusesFollowTheEnginePauseByProcessMode) {
    EXPECT_EQ(mixer.getBusProcessMode("master"), core::ProcessMode::Inherit);
    EXPECT_EQ(mixer.getBusProcessMode("music"), core::ProcessMode::Always);
    EXPECT_EQ(mixer.getBusProcessMode("ui"), core::ProcessMode::Always);
    EXPECT_EQ(mixer.getBusProcessMode("sfx"), core::ProcessMode::Pausable);
    EXPECT_EQ(mixer.getBusProcessMode("ambience"), core::ProcessMode::Pausable);

    const Mixer::VoiceId effect = mixer.play(makeTone(toFrames(1.0F), 0.5F), {.loop = true});
    const Mixer::VoiceId music = mixer.play(makeTone(toFrames(1.0F), 0.25F), {.bus = "music", .loop = true});
    mixer.setProcessPaused(true);
    EXPECT_TRUE(mixer.isProcessPaused());
    EXPECT_NEAR(settledPeak(256), 0.25F, 0.01F);
    EXPECT_TRUE(mixer.isActive(effect));
    EXPECT_FALSE(mixer.isPaused(effect));
    EXPECT_FALSE(statsOf("sfx").processing);
    EXPECT_EQ(statsOf("sfx").paused, 1U);
    EXPECT_EQ(statsOf("music").playing, 1U);

    // A voice paused by the engine keeps its place and resumes where it stopped.
    const float cursor = mixer.getCursor(effect);
    peak(toFrames(0.1F));
    EXPECT_EQ(mixer.getCursor(effect), cursor);
    mixer.setProcessPaused(false);
    EXPECT_NEAR(settledPeak(256), 0.75F, 0.01F);

    mixer.setBusProcessMode("music", core::ProcessMode::Pausable);
    mixer.setProcessPaused(true);
    EXPECT_EQ(settledPeak(256), 0.0F);
    EXPECT_TRUE(mixer.isActive(music));
}

TEST_F(AudioPauseTest, VoicesOverrideTheModeOfTheirBus) {
    mixer.createBus("steps", "sfx");
    mixer.createBus("menu", "ui");
    const Sound tone = makeTone(toFrames(1.0F), 0.1F);
    const Mixer::VoiceId always = mixer.play(tone, {.loop = true, .processMode = core::ProcessMode::Always});
    mixer.play(tone, {.loop = true, .processMode = core::ProcessMode::WhenPaused});
    mixer.play(tone, {.loop = true, .processMode = core::ProcessMode::Disabled});
    mixer.play(tone, {.bus = "steps", .loop = true});
    mixer.play(tone, {.bus = "menu", .loop = true});
    EXPECT_EQ(mixer.getProcessMode(always), core::ProcessMode::Always);
    EXPECT_EQ(mixer.getProcessMode(9999), core::ProcessMode::Inherit);

    // Running: always, steps (pausable through sfx) and menu (always through ui).
    EXPECT_NEAR(settledPeak(256), 0.3F, 0.01F);

    // Paused: always, when paused and menu.
    mixer.setProcessPaused(true);
    EXPECT_NEAR(settledPeak(256), 0.3F, 0.01F);
    EXPECT_FALSE(statsOf("steps").processing);
    EXPECT_TRUE(statsOf("menu").processing);

    mixer.setBusProcessMode("master", core::ProcessMode::Disabled);
    mixer.setBusProcessMode("ui", core::ProcessMode::Inherit);
    EXPECT_NEAR(settledPeak(256), 0.2F, 0.01F);
    EXPECT_THROW(mixer.setBusProcessMode("missing", core::ProcessMode::Always), std::invalid_argument);
}

TEST_F(AudioPauseTest, PausingEveryVoiceKeepsTheVoicesTheAppPaused) {
    const Sound tone = makeTone(toFrames(1.0F), 0.2F);
    const Mixer::VoiceId held = mixer.play(tone, {.loop = true});
    const Mixer::VoiceId free = mixer.play(tone, {.loop = true});
    mixer.setPaused(held, true);
    EXPECT_TRUE(mixer.isPaused(held));
    EXPECT_NEAR(settledPeak(256), 0.2F, 0.01F);

    mixer.pauseAll();
    EXPECT_EQ(settledPeak(256), 0.0F);
    const Mixer::VoiceId later = mixer.play(tone, {.loop = true});
    EXPECT_NEAR(settledPeak(256), 0.2F, 0.01F);
    EXPECT_EQ(mixer.getBusStats().size(), 5U);

    // Each reason holds on its own, so ending one pause leaves what another reason or the app paused.
    mixer.pauseAll(Mixer::PauseReason::Interruption);
    mixer.resumeAll();
    EXPECT_EQ(settledPeak(256), 0.0F);
    mixer.resumeAll(Mixer::PauseReason::Interruption);
    EXPECT_NEAR(settledPeak(256), 0.4F, 0.01F);
    EXPECT_TRUE(mixer.isPaused(held));
    EXPECT_FALSE(mixer.isPaused(free));
    EXPECT_FALSE(mixer.isPaused(later));

    mixer.setPaused(held, false);
    EXPECT_NEAR(settledPeak(256), 0.6F, 0.01F);
    EXPECT_FALSE(mixer.isPaused(9999));
}

TEST_F(AudioPauseTest, StoppedAndFinishedVoicesStayDone) {
    const Mixer::VoiceId stopped = mixer.play(makeTone(toFrames(1.0F)), {.loop = true});
    mixer.setProcessPaused(true);
    mixer.stop(stopped, 0.5F);
    mixer.setProcessPaused(false);
    mixer.update(1.0F / 60.0F);
    EXPECT_FALSE(mixer.isActive(stopped));

    // A short voice that ends while the app holds it does not start over when the app lets go.
    const Mixer::VoiceId shortVoice = mixer.play(makeTone(64));
    peak(toFrames(0.01F));
    mixer.setPaused(shortVoice, true);
    mixer.setPaused(shortVoice, false);
    EXPECT_EQ(settledPeak(256), 0.0F);
    mixer.update(1.0F / 60.0F);
    EXPECT_EQ(mixer.getVoiceCount(), 0U);
}

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
    EXPECT_EQ(published, (std::vector<std::string>{"audio_interrupted"}));

    // An interruption that ends while the app is inactive waits for the app to become active, because the system gives the audio back only to an active app.
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionEnded);
    fixture.frames(1);
    EXPECT_TRUE(audio.isInterrupted());
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_FALSE(audio.isInterrupted());
    EXPECT_EQ(published, (std::vector<std::string>{"audio_interrupted", "audio_resumed"}));

    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionEnded);
    audio.reportDeviceEvent(Mixer::DeviceEvent::RouteChanged);
    fixture.frames(1);
    EXPECT_EQ(published.back(), "audio_route_changed");
    EXPECT_EQ(published.size(), 3U);

    // Because iOS does not always report the end of an interruption, becoming active again brings the audio back anyway.
    audio.reportDeviceEvent(Mixer::DeviceEvent::InterruptionBegan);
    fixture.frames(1);
    engine.handleEvent({.type = platform::Event::Type::FocusLost});
    engine.handleEvent({.type = platform::Event::Type::FocusGained});
    EXPECT_FALSE(audio.isInterrupted());
    EXPECT_EQ(published, (std::vector<std::string>{"audio_interrupted", "audio_resumed", "audio_route_changed", "audio_interrupted", "audio_resumed"}));
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
    EXPECT_EQ(published, (std::vector<std::string>{"audio_interrupted", "audio_resumed"}));

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
    EXPECT_EQ(published, (std::vector<std::string>{"audio_interrupted", "audio_resumed"}));

    engine.setPaused(true);
    EXPECT_TRUE(engine.getAudio().isProcessPaused());
    engine.setPaused(false);
    EXPECT_FALSE(engine.getAudio().isProcessPaused());
}

} // namespace haylen::audio
