#include <gtest/gtest.h>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include "audio/MixerFixture.hpp"

namespace haylen::audio {

class AudioPauseTest : public MixerFixture {
  protected:
    [[nodiscard]] Mixer::BusStats statsOf(const std::string& bus) const {
        std::vector<Mixer::BusStats> stats;
        mixer.readBusStats(stats);
        const auto found = std::ranges::find_if(stats, [&bus](const Mixer::BusStats& entry) { return entry.name == bus; });
        if (found == stats.end()) {
            throw std::runtime_error("The mixer has no bus \"" + bus + "\".");
        }

        return *found;
    }
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

    // Running: always, `steps` (pausable through `sfx`) and `menu` (always through `ui`).
    EXPECT_NEAR(settledPeak(256), 0.3F, 0.01F);

    // Paused: always, when paused and `menu`.
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
    std::vector<Mixer::BusStats> buses;
    mixer.readBusStats(buses);
    EXPECT_EQ(buses.size(), 5U);

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

} // namespace haylen::audio
