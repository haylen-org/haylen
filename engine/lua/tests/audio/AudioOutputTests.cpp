#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "audio/Device.hpp"
#include "audio/MixerFixture.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/port/AudioOutput.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::audio {

// Plays through an audio output that refuses the audio until a test lets it through, the way a browser without `AudioWorklet` or an iOS audio session that another app holds refuses it, and that a test can keep waiting the way an audio service that hangs does. Its playback never reads the mix, waits for the person while the test locks it and stops working when the test fails it.
class AudioOutputTest : public haylen::audio::MixerFixture {
  protected:
    static constexpr float kFrameSeconds = 1.0F / 60.0F;
    static constexpr std::chrono::seconds kAnswerTimeout{30};

    class TestPlayback final : public haylen::port::AudioOutput::Playback {
      public:
        explicit TestPlayback(const haylen::port::AudioOutput::Request& request) noexcept : format{.sampleRate = request.sampleRate, .channels = request.channels, .periodFrames = 256, .periods = 2} {}

        ~TestPlayback() override {
            ++closedDevices;
        }

        TestPlayback(const TestPlayback&) = delete;
        TestPlayback& operator=(const TestPlayback&) = delete;

        [[nodiscard]] haylen::port::AudioOutput::Format getFormat() const noexcept override {
            return format;
        }

        void start() noexcept override {}
        void stop() noexcept override {}

        [[nodiscard]] bool isLocked() const noexcept override {
            return locked;
        }

        [[nodiscard]] std::optional<std::string> getFailure() const override {
            return failing ? std::optional<std::string>("The test refused the context after the output opened.") : std::nullopt;
        }

      private:
        haylen::port::AudioOutput::Format format;
    };

    class TestOutput final : public haylen::port::AudioOutput {
      public:
        [[nodiscard]] std::unique_ptr<Playback> open(const Request& request, Stream&) override {
            std::unique_lock lock(gateMutex);
            if (!gate.wait_for(lock, kAnswerTimeout, [] { return answering; })) {
                ADD_FAILURE() << "The test kept the opening of the output waiting longer than it allows.";
                throw std::runtime_error("The test never answered the opening of the output.");
            }

            lock.unlock();
            if (!accepting) {
                throw std::runtime_error("The test refuses the audio.");
            }

            return std::make_unique<TestPlayback>(request);
        }
    };

    void SetUp() override {
        accepting = false;
        locked = false;
        failing = false;
        answering = true;
        closedDevices = 0;
        // clang-format off
        listener = haylen::core::Log::addListener([this](haylen::core::Log::Level level, std::string_view line) {
            const std::scoped_lock lock(linesMutex);
            lines.emplace_back(level, std::string(line));
        });
        // clang-format on
    }

    void TearDown() override {
        answer();
        haylen::core::Log::removeListener(listener);
    }

    // Keeps every opening waiting until `answer`, the way an audio service that hangs keeps the system from answering.
    static void hang() {
        const std::scoped_lock lock(gateMutex);
        answering = false;
    }

    static void answer() {
        {
            const std::scoped_lock lock(gateMutex);
            answering = true;
        }
        gate.notify_all();
    }

    // Updates the mixer frame by frame over the seconds, as the engine does.
    static void play(haylen::audio::Mixer& target, float seconds) {
        for (float elapsed = 0.0F; elapsed + kFrameSeconds / 2.0F < seconds; elapsed += kFrameSeconds) {
            target.update(kFrameSeconds);
        }
    }

    // Waits for the thread that opens the device, updating the mixer without letting time pass, as the frames of an app that the system keeps waiting do.
    static bool settle(haylen::audio::Mixer& target) {
        // clang-format off
        return waitFor([&target] {
            target.update(0.0F);
            return !target.isOutputOpening();
        });
        // clang-format on
    }

    static bool waitFor(const std::function<bool()>& condition) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!condition()) {
            if (std::chrono::steady_clock::now() > deadline) {
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        return true;
    }

    [[nodiscard]] std::vector<std::string> getLines(haylen::core::Log::Level level) {
        const std::scoped_lock lock(linesMutex);
        std::vector<std::string> found;
        for (const auto& [lineLevel, line] : lines) {
            if (lineLevel == level) {
                found.push_back(line);
            }
        }

        return found;
    }

    [[nodiscard]] std::vector<std::string> getWarnings() {
        return getLines(haylen::core::Log::Level::Warning);
    }

    static inline std::atomic<bool> accepting = false;
    static inline std::atomic<bool> locked = false;
    static inline std::atomic<bool> failing = false;
    static inline bool answering = true;
    static inline std::mutex gateMutex;
    static inline std::condition_variable gate;
    static inline std::atomic<int> closedDevices = 0;

    std::shared_ptr<haylen::port::AudioOutput> testOutput = std::make_shared<TestOutput>();

    std::mutex linesMutex;
    std::vector<std::pair<haylen::core::Log::Level, std::string>> lines;
    std::uint64_t listener = 0;
};

TEST_F(AudioOutputTest, KeepsTimeWithoutSoundWhileTheSystemRefusesTheOutput) {
    haylen::audio::Mixer refused({.device = true, .sampleRate = kRate, .channels = 2, .output = testOutput}, jobs);
    EXPECT_TRUE(refused.hasDevice());
    ASSERT_TRUE(settle(refused));
    EXPECT_FALSE(refused.isOutputAvailable());
    ASSERT_EQ(getWarnings().size(), 1U);
    EXPECT_NE(getWarnings().front().find("The test refuses the audio."), std::string::npos);
    std::vector<float> samples(64);
    EXPECT_THROW(refused.render(samples), std::logic_error);

    // Voices move on in real time and end without anyone hearing them.
    const haylen::audio::Mixer::VoiceId shot = refused.play(makeTone(toFrames(0.25F)));
    play(refused, 0.2F);
    EXPECT_TRUE(refused.isActive(shot));
    EXPECT_NEAR(refused.getCursor(shot), 0.2F, 0.01F);
    play(refused, 0.1F);
    EXPECT_FALSE(refused.isActive(shot));
    EXPECT_EQ(refused.getVoiceCount(), 0U);

    // Music crossfades, and the track it replaces goes once its fade is over.
    refused.playMusic(makeTone(toFrames(2.0F)), {.fade = 0.0F});
    const haylen::audio::Mixer::VoiceId night = refused.playMusic(makeTone(toFrames(2.0F), 0.25F), {.fade = 0.5F});
    play(refused, 0.3F);
    EXPECT_EQ(refused.getVoiceCount(), 2U);
    play(refused, 0.3F);
    EXPECT_EQ(refused.getVoiceCount(), 1U);
    EXPECT_TRUE(refused.isActive(night));

    // No time passes while the app is in the background or an interruption holds the audio, and an interruption that ends while the system still refuses the audio ends all the same.
    const float cursor = refused.getCursor(night);
    refused.suspend();
    play(refused, 0.5F);
    refused.resume();
    refused.beginInterruption();
    play(refused, 0.5F);
    EXPECT_FLOAT_EQ(refused.getCursor(night), cursor);
    refused.endInterruption();
    EXPECT_FALSE(refused.isInterrupted());
    ASSERT_TRUE(settle(refused));
    EXPECT_FALSE(refused.isOutputAvailable());
    play(refused, 0.5F);
    EXPECT_NEAR(refused.getCursor(night), cursor + 0.5F, 0.01F);
    EXPECT_EQ(getWarnings().size(), 1U);
}

TEST_F(AudioOutputTest, FallsSilentWhenTheOutputFailsAfterItOpened) {
    accepting = true;
    haylen::audio::Mixer output({.device = true, .sampleRate = kRate, .channels = 2, .output = testOutput}, jobs);
    ASSERT_TRUE(settle(output));
    output.update(kFrameSeconds);
    EXPECT_EQ(output.getOutputState(), haylen::audio::Mixer::OutputState::Running);
    EXPECT_TRUE(getWarnings().empty());

    // The platform learns later that it cannot play the output, and the app keeps time without sound, with the reason in its log once.
    failing = true;
    const haylen::audio::Mixer::VoiceId shot = output.play(makeTone(toFrames(1.0F)));
    output.update(kFrameSeconds);
    failing = false;
    EXPECT_EQ(output.getOutputState(), haylen::audio::Mixer::OutputState::Unavailable);
    EXPECT_FALSE(output.isOutputAvailable());
    ASSERT_EQ(getWarnings().size(), 1U);
    EXPECT_NE(getWarnings().front().find("The test refused the context after the output opened."), std::string::npos);
    EXPECT_EQ(closedDevices, 1);
    const float cursor = output.getCursor(shot);
    play(output, 0.3F);
    EXPECT_NEAR(output.getCursor(shot), cursor + 0.3F, 0.02F);
    EXPECT_EQ(getWarnings().size(), 1U);
}

TEST_F(AudioOutputTest, ReportsTheStateAndTheLatencyOfTheOutput) {
    accepting = true;
    locked = true;
    haylen::audio::Mixer output({.device = true, .sampleRate = kRate, .channels = 2, .output = testOutput}, jobs);
    ASSERT_TRUE(settle(output));
    EXPECT_EQ(output.getOutputState(), haylen::audio::Mixer::OutputState::Locked);
    EXPECT_NEAR(output.getOutputLatency(), 512.0F / static_cast<float>(kRate), 1e-6F);
    locked = false;
    EXPECT_EQ(output.getOutputState(), haylen::audio::Mixer::OutputState::Running);
    output.suspend();
    EXPECT_EQ(output.getOutputState(), haylen::audio::Mixer::OutputState::Suspended);
    output.resume();
    EXPECT_EQ(haylen::audio::Mixer::outputStateName(output.getOutputState()), "running");
    EXPECT_EQ(mixer.getOutputState(), haylen::audio::Mixer::OutputState::Unavailable);
    EXPECT_EQ(mixer.getOutputLatency(), 0.0F);
}

TEST_F(AudioOutputTest, PlaysInTheBackgroundOnlyForAnAppThatDeclaresIt) {
    accepting = true;
    test::LuaFixture fixture({{"haylen.json", R"({"name": "Night Radio", "identifier": "com.haylen.radio", "audio": {"iosSession": "playback"}})"}}, nullptr, {.audioOutput = testOutput});
    EXPECT_TRUE(fixture.host().isBackgroundAudio());
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return !fixture.engine().getAudio().isOutputOpening(); }));
    haylen::audio::Mixer& radio = fixture.engine().getAudio();

    // In the background the output runs only while a voice plays, so the system may suspend an app whose sound ended, and a voice that starts there brings the output back.
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Suspended});
    fixture.frames(1);
    EXPECT_EQ(radio.getOutputState(), haylen::audio::Mixer::OutputState::Suspended);
    radio.play(makeTone(toFrames(5.0F)));
    fixture.frames(1);
    EXPECT_EQ(radio.getOutputState(), haylen::audio::Mixer::OutputState::Running);

    // The app turns it off and on again while it runs, and the output follows at once.
    fixture.runLua("audio = require('haylen.audio')");
    EXPECT_EQ(fixture.lua("return audio.background()"), "true");
    fixture.runLua("audio.setBackground(false)");
    EXPECT_EQ(radio.getOutputState(), haylen::audio::Mixer::OutputState::Suspended);
    EXPECT_FALSE(fixture.host().isBackgroundAudio());
    fixture.runLua("audio.setBackground(true)");
    fixture.frames(1);
    EXPECT_EQ(radio.getOutputState(), haylen::audio::Mixer::OutputState::Running);
    EXPECT_TRUE(fixture.host().isBackgroundAudio());
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Resumed});
    fixture.frames(1);
    EXPECT_EQ(radio.getOutputState(), haylen::audio::Mixer::OutputState::Running);

    // An app that does not declare it to the system stops its output in the background whatever plays.
    const std::string_view game = R"({"name": "Tiny Island", "identifier": "com.haylen.tinyisland"})";
    fixture.package().setFile("haylen.json", std::vector<std::uint8_t>(game.begin(), game.end()));
    fixture.host().setBackgroundAudioDeclared(false);
    fixture.restart();
    EXPECT_FALSE(fixture.host().isBackgroundAudio());
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return !fixture.engine().getAudio().isOutputOpening(); }));
    haylen::audio::Mixer& island = fixture.engine().getAudio();
    island.play(makeTone(toFrames(5.0F)));
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Suspended});
    fixture.frames(1);
    EXPECT_EQ(island.getOutputState(), haylen::audio::Mixer::OutputState::Suspended);
    EXPECT_NE(fixture.lua("require('haylen.audio').setBackground(true)").find(R"(The app plays in the background only when it declares that to the system, with the mode "audio" of "UIBackgroundModes" in its "Info.plist" on Apple platforms and a service of the type "mediaPlayback" in its manifest on Android, which a plugin that plays in the background brings.)"), std::string::npos);
}

TEST_F(AudioOutputTest, PublishesTheUnlockOfTheOutput) {
    accepting = true;
    locked = true;
    test::LuaFixture fixture({}, nullptr, {.audioOutput = testOutput});
    fixture.runLua("audio = require('haylen.audio') unlocked = 0 require('haylen.events').on('audioUnlocked', function() unlocked = unlocked + 1 end)");
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return !fixture.engine().getAudio().isOutputOpening(); }));
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return audio.outputState() .. ' ' .. unlocked"), "locked 0");
    locked = false;
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return audio.outputState() .. ' ' .. unlocked .. ' ' .. tostring(audio.outputLatency() > 0)"), "running 1 true");
}

TEST_F(AudioOutputTest, OpensTheOutputOnceTheSystemOffersIt) {
    haylen::audio::Mixer recovering({.device = true, .sampleRate = kRate, .channels = 2, .output = testOutput}, jobs);
    const haylen::audio::Mixer::VoiceId voice = recovering.play(makeTone(toFrames(0.25F)));
    ASSERT_TRUE(settle(recovering));
    play(recovering, 0.1F);

    // Ending an interruption, as the engine does whenever the app becomes active, opens the output, which then plays the voices.
    accepting = true;
    recovering.endInterruption();
    EXPECT_TRUE(recovering.isOutputOpening());
    ASSERT_TRUE(settle(recovering));
    EXPECT_TRUE(recovering.isOutputAvailable());
    play(recovering, 0.5F);
    EXPECT_TRUE(recovering.isActive(voice));
    EXPECT_NEAR(recovering.getCursor(voice), 0.1F, 0.01F);

    // An output that played and is refused at the end of an interruption warns again, and the voices go on without it.
    recovering.beginInterruption();
    accepting = false;
    recovering.endInterruption();
    EXPECT_FALSE(recovering.isInterrupted());
    ASSERT_TRUE(settle(recovering));
    EXPECT_FALSE(recovering.isOutputAvailable());
    EXPECT_EQ(getWarnings().size(), 2U);
    EXPECT_EQ(closedDevices, 1);
    play(recovering, 0.2F);
    EXPECT_FALSE(recovering.isActive(voice));
}

TEST_F(AudioOutputTest, KeepsPlayingWhileTheOutputDoesNotAnswer) {
    accepting = true;
    hang();
    const auto before = std::chrono::steady_clock::now();
    haylen::audio::Mixer waiting({.device = true, .sampleRate = kRate, .channels = 2, .output = testOutput}, jobs);
    EXPECT_LT(std::chrono::steady_clock::now() - before, std::chrono::seconds(1));
    EXPECT_TRUE(waiting.isOutputOpening());
    EXPECT_FALSE(waiting.isOutputAvailable());

    // The frames go on, and the voices move on without sound while the system keeps the opening waiting.
    const haylen::audio::Mixer::VoiceId voice = waiting.play(makeTone(toFrames(8.0F)));
    play(waiting, haylen::audio::Device::kAnswerSeconds - 0.5F);
    EXPECT_TRUE(getWarnings().empty());
    play(waiting, 1.0F);
    EXPECT_NEAR(waiting.getCursor(voice), haylen::audio::Device::kAnswerSeconds + 0.5F, 0.02F);
    ASSERT_EQ(getWarnings().size(), 1U);
    EXPECT_NE(getWarnings().front().find("does not answer"), std::string::npos);

    // An interruption that ends meanwhile waits for the same opening, and the warning stays the only one.
    waiting.beginInterruption();
    waiting.endInterruption();
    play(waiting, 1.0F);
    EXPECT_TRUE(waiting.isOutputOpening());
    EXPECT_EQ(getWarnings().size(), 1U);

    // Once the system answers, the next update takes the device, which plays from then on.
    answer();
    ASSERT_TRUE(settle(waiting));
    EXPECT_TRUE(waiting.isOutputAvailable());
    EXPECT_EQ(getLines(haylen::core::Log::Level::Info).size(), 1U);
    EXPECT_EQ(getWarnings().size(), 1U);
}

TEST_F(AudioOutputTest, ClosesAnOpeningThatOutlivesItsMixer) {
    accepting = true;
    hang();
    {
        haylen::audio::Mixer gone({.device = true, .sampleRate = kRate, .channels = 2, .output = testOutput}, jobs);
        EXPECT_TRUE(gone.isOutputOpening());
    }

    // The thread that opened the device closes it once the system answers, since no mixer takes it anymore.
    answer();
    EXPECT_TRUE(waitFor([] { return closedDevices == 1; }));
}

TEST_F(AudioOutputTest, RunsAnAppWhoseOutputDoesNotAnswer) {
    accepting = true;
    hang();
    test::LuaFixture fixture({{"source/main.lua", "frames = 0 require('haylen.scene').push({update = function() frames = frames + 1 end})"}}, nullptr, {.audioOutput = testOutput});
    EXPECT_EQ(fixture.lua("return require('haylen.audio').hasDevice()"), "true");
    EXPECT_EQ(fixture.lua("return require('haylen.audio').outputOpening()"), "true");

    // The app updates and draws its frames while the system keeps the device waiting, and plays sound once it answers.
    fixture.frames(240);
    EXPECT_EQ(fixture.lua("return frames"), "240");
    EXPECT_EQ(fixture.lua("return require('haylen.audio').outputAvailable()"), "false");
    EXPECT_EQ(getWarnings().size(), 1U);
    answer();
    EXPECT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return require('haylen.audio').outputAvailable()") == "true"; }));
    EXPECT_EQ(fixture.lua("return require('haylen.audio').outputOpening()"), "false");
}

TEST_F(AudioOutputTest, OpensTheOutputAgainWhenTheSystemResetsItsServices) {
    accepting = true;
    test::LuaFixture fixture({}, nullptr, {.audioOutput = testOutput});
    ASSERT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return require('haylen.audio').outputAvailable()") == "true"; }));
    const int closed = closedDevices;
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::AudioServicesReset});
    EXPECT_EQ(closedDevices, closed + 1);
    EXPECT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return require('haylen.audio').outputAvailable()") == "true"; }));

    fixture.runLua("require('haylen.audio').restartOutput()");
    EXPECT_EQ(closedDevices, closed + 2);
    EXPECT_TRUE(fixture.frameUntil([&fixture] { return fixture.lua("return require('haylen.audio').outputAvailable()") == "true"; }));
}

} // namespace haylen::lua::audio
