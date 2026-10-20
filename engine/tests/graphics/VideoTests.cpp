#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "haylen/audio/Mixer.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/graphics/Video.hpp"
#include "haylen/plugins/VideoPlugin.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/FakeVideoDecoder.hpp"

namespace haylen::graphics {

class VideoTest : public ::testing::Test {
  protected:
    VideoTest() : fixture({{"content/videos/intro.mp4", "not a real video"}, {"content/videos/broken.mp4", "broken"}}) {
        useDecoder({});
        fixture.runLua("video = require('haylen.video') events = {}");
    }

    // Opens every video with the fake decoder of the settings, and the file `videos/broken.mp4` with an error.
    void useDecoder(test::FakeVideoDecoder::Settings settings) {
        // clang-format off
        fixture.host().setVideoOpener([settings, log = log](std::shared_ptr<io::PackageReader> file, std::string_view path) -> std::unique_ptr<platform::VideoDecoder> {
            if (path == "videos/broken.mp4") {
                throw std::runtime_error("The file is no video this platform decodes.");
            }
            EXPECT_EQ(file->getSize(), 16U);
            return std::make_unique<test::FakeVideoDecoder>(settings, log);
        });
        // clang-format on
    }

    // Loads the intro with the options and waits until it is ready.
    void loadIntro(const std::string& options = "{}") {
        fixture.runLua("intro = video.load('videos/intro.mp4', " + options + ") intro:on('ready', function() events[#events + 1] = 'ready' end) intro:on('ended', function() events[#events + 1] = 'ended' end)");
        ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return intro.state") != "loading"; }));
    }

    ~VideoTest() override {
        hold(false);
    }

    // Holds every read of a frame of the decoder while the value is set, as a decoder that falls behind the playback does.
    void hold(bool value) {
        {
            const std::scoped_lock lock(log->mutex);
            log->held = value;
        }
        log->released.notify_all();
    }

    // Plays frames until the time of the video reaches the seconds.
    void playUntil(double seconds) {
        ASSERT_TRUE(fixture.frameUntil([this, seconds] { return std::stod(fixture.lua("return intro.time")) >= seconds; }));
    }

    std::shared_ptr<test::FakeVideoDecoder::Log> log = std::make_shared<test::FakeVideoDecoder::Log>();
    test::EngineFixture fixture;
};

TEST_F(VideoTest, LoadsInTheBackgroundAndShowsTheFirstFrame) {
    fixture.runLua("intro = video.load('videos/intro.mp4')");
    EXPECT_EQ(fixture.lua("return intro.state .. ' ' .. intro.width .. ' ' .. intro.duration .. ' ' .. intro.texture.width"), "loading 0 0.0 1");
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return intro.frameCount") == "1"; }));
    EXPECT_EQ(fixture.lua("return intro.state .. ' ' .. intro.width .. 'x' .. intro.height .. ' ' .. intro.duration .. ' ' .. intro.texture.width .. 'x' .. intro.texture.height"), "ready 4x2 1.0 4x2");
    EXPECT_EQ(fixture.lua("return tostring(intro.playing) .. ' ' .. tostring(intro.ended) .. ' ' .. intro.time .. ' ' .. tostring(intro.error)"), "false false 0.0 nil");

    // A paused video keeps its first frame and decodes only the few frames it keeps ahead.
    fixture.frames(10);
    EXPECT_EQ(fixture.lua("return intro.frameCount"), "1");
    const std::scoped_lock lock(log->mutex);
    EXPECT_LE(log->framesRead, 6);
}

TEST_F(VideoTest, PlaysToItsEndOnceAndPlaysAgainFromTheStart) {
    loadIntro("{autoplay = true}");
    EXPECT_EQ(fixture.lua("return table.concat(events, ' ') .. ' ' .. tostring(intro.playing)"), "ready true");
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return tostring(intro.ended)") == "true"; }));
    EXPECT_EQ(fixture.lua("return table.concat(events, ' ') .. ' ' .. tostring(intro.playing)"), "ready ended false");
    fixture.frames(10);
    EXPECT_EQ(fixture.lua("return table.concat(events, ' ')"), "ready ended");
    EXPECT_GE(std::stoi(fixture.lua("return intro.frameCount")), 5);

    fixture.runLua("intro:play()");
    EXPECT_EQ(fixture.lua("return intro.time .. ' ' .. tostring(intro.ended)"), "0.0 false");
    playUntil(0.3);
    const std::scoped_lock lock(log->mutex);
    EXPECT_EQ(log->seeks.front(), 0.0);
}

TEST_F(VideoTest, LoopsWithoutEnding) {
    loadIntro("{autoplay = true, loop = true}");
    fixture.frames(150, 1.0 / 60.0);
    EXPECT_EQ(fixture.lua("return table.concat(events, ' ') .. ' ' .. tostring(intro.playing) .. ' ' .. tostring(intro.time < 1)"), "ready true true");
    const std::scoped_lock lock(log->mutex);
    EXPECT_GE(log->seeks.size(), 1U);
}

TEST_F(VideoTest, SeeksPausesAndPlaysAtItsRate) {
    loadIntro();
    fixture.runLua("intro.time = 0.55");
    // clang-format off
    ASSERT_TRUE(fixture.frameUntil([this] {
        const std::scoped_lock lock(log->mutex);
        return !log->seeks.empty();
    }));
    // clang-format on
    EXPECT_EQ(fixture.lua("return intro.time"), "0.55");
    {
        const std::scoped_lock lock(log->mutex);
        EXPECT_DOUBLE_EQ(log->seeks.back(), 0.55);
    }

    // The decoder falls behind while the video plays at twice its speed, so the clock stops at the end and waits for the last frame there.
    hold(true);
    fixture.runLua("intro.rate = 2 intro:play()");
    fixture.frames(6, 0.05);
    EXPECT_EQ(fixture.lua("return intro.time .. ' ' .. tostring(intro.playing) .. ' ' .. tostring(intro.ended)"), "1.0 true false");
    hold(false);
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return tostring(intro.ended)") == "true"; }));
    EXPECT_EQ(fixture.lua("return intro.time .. ' ' .. tostring(intro.playing)"), "1.0 false");
    fixture.runLua("intro:seek(0.1) intro:pause()");
    fixture.frames(5, 0.05);
    EXPECT_EQ(fixture.lua("return intro.time .. ' ' .. tostring(intro.playing)"), "0.1 false");
    EXPECT_NE(fixture.lua("intro.rate = 8").find("The rate of a video must be from 0.25 to 4."), std::string::npos);
    EXPECT_NE(fixture.lua("intro:seek(0 / 0)").find("A video seeks to a finite number of seconds."), std::string::npos);
    EXPECT_NE(fixture.lua("intro.volume = -1").find("The volume of a video must be zero or positive."), std::string::npos);
}

TEST_F(VideoTest, HoldsWithThePauseOfTheGameByItsProcessMode) {
    loadIntro("{autoplay = true}");
    fixture.runLua("always = video.load('videos/intro.mp4', {autoplay = true, processMode = 'always'})");
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return always.state") == "ready"; }));
    fixture.engine().setPaused(true);
    const std::string held = fixture.lua("return intro.time");
    fixture.frames(5, 0.05);
    EXPECT_EQ(fixture.lua("return intro.time"), held);
    EXPECT_GT(std::stod(fixture.lua("return always.time")), 0.2);
}

TEST_F(VideoTest, PlaysItsSoundThroughABusOfTheMixer) {
    audio::Mixer& mixer = fixture.engine().getAudio();
    const std::size_t before = mixer.getVoiceCount();
    loadIntro("{bus = 'music', volume = 0.5}");
    EXPECT_EQ(mixer.getVoiceCount(), before);
    fixture.runLua("intro:play()");
    fixture.frames(1);
    EXPECT_EQ(mixer.getVoiceCount(), before + 1);
    fixture.runLua("intro.muted = true intro.volume = 0.25");
    EXPECT_EQ(fixture.lua("return tostring(intro.muted) .. ' ' .. intro.volume"), "true 0.25");

    // A seek starts the sound of the new time on a voice of its own, and the release stops it.
    fixture.runLua("intro:seek(0.5)");
    fixture.frames(1);
    EXPECT_EQ(mixer.getVoiceCount(), before + 1);
    fixture.runLua("intro:release()");
    fixture.frames(1);
    EXPECT_EQ(mixer.getVoiceCount(), before);
    EXPECT_EQ(fixture.engine().getPlugin<plugins::VideoPlugin>().getVideoCount(), 0U);
}

// A seek while the decoder reads the end of the sound for the time it leaves plays the sound of the new time.
TEST_F(VideoTest, PlaysTheSoundOfASeekThatLandsWhileTheEndOfTheSoundDecodes) {
    loadIntro();
    fixture.frames(5);
    hold(true);
    fixture.runLua("intro:seek(0.9)");
    // clang-format off
    ASSERT_TRUE(fixture.frameUntil([this] {
        const std::scoped_lock lock(log->mutex);
        return !log->seeks.empty() && log->seeks.back() == 0.9;
    }));
    // clang-format on

    // The read of the frame at 0.9 waits while the video seeks back to the start, and then decodes the sound up to the end.
    fixture.runLua("intro:seek(0)");
    hold(false);
    // clang-format off
    ASSERT_TRUE(fixture.frameUntil([this] {
        const std::scoped_lock lock(log->mutex);
        return log->seeks.back() == 0.0 && log->soundTime > 0.0 && log->soundTime < 0.9;
    }));
    // clang-format on
}

TEST_F(VideoTest, ReportsFilesThatFailAndPlatformsWithoutADecoder) {
    fixture.runLua("broken = video.load('videos/broken.mp4') broken:on('error', function(error) events[#events + 1] = error.code .. ': ' .. error.message end)");
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return broken.state") == "failed"; }));
    EXPECT_EQ(fixture.lua("return table.concat(events, ' ')"), "failed: The file is no video this platform decodes.");
    EXPECT_EQ(fixture.lua("return broken.error"), "The file is no video this platform decodes.");
    EXPECT_NE(fixture.lua("video.load('videos/missing.mp4')").find("The video \"videos/missing.mp4\" does not exist."), std::string::npos);
    EXPECT_NE(fixture.lua("video.load('videos/intro.mp4', {speed = 2})").find("Unknown option \"speed\"."), std::string::npos);
    EXPECT_NE(fixture.lua("broken:on('paused', function() end)").find("Unknown video event \"paused\"."), std::string::npos);

    // A decoder that breaks while the video plays fails it with its message.
    useDecoder({.failAt = 3});
    loadIntro("{autoplay = true}");
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return intro.state") == "failed"; }));
    EXPECT_EQ(fixture.lua("return intro.error"), "The fake video broke at its frame 3.");

    fixture.host().setVideoOpener({});
    fixture.runLua("events = {} none = video.load('videos/intro.mp4') none:on('error', function(error) events[#events + 1] = error.code end)");
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return none.state") == "failed"; }));
    EXPECT_EQ(fixture.lua("return table.concat(events, ' ') .. ' ' .. none.error"), "unsupported This platform has no decoder of video files.");
}

TEST_F(VideoTest, ResolvesAnAsynchronousLoadOnceReadyAndEndsWithItsOwner) {
    // clang-format off
    fixture.runLua(R"(
        async = require('async')
        owner = {}
        async.spawn(function()
            local loaded = video.loadAsync('videos/intro.mp4', {owner = owner}):await()
            events[#events + 1] = loaded.state .. ' ' .. loaded.width
        end)
        async.spawn(function()
            local loaded, failure = video.loadAsync('videos/broken.mp4'):await()
            events[#events + 1] = tostring(loaded) .. ' ' .. failure
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return #events") == "2"; }));
    EXPECT_EQ(fixture.lua("table.sort(events) return table.concat(events, ', ')"), "nil The file is no video this platform decodes., ready 4");
    plugins::VideoPlugin& plugin = fixture.engine().getPlugin<plugins::VideoPlugin>();
    EXPECT_EQ(plugin.getVideoCount(), 2U);
    fixture.runLua("owner = nil collectgarbage() collectgarbage()");
    fixture.frames(2);
    EXPECT_EQ(plugin.getVideoCount(), 1U);
}

TEST_F(VideoTest, FollowsADecoderWithAClockOfItsOwn) {
    useDecoder({.info = {.duration = 2.0, .width = 8, .height = 4, .sampleRate = 8000, .channels = 1, .clocked = true}});

    // The video waits for the size that the decoder learns after it opens, and fails with the error a read reports meanwhile.
    {
        const std::scoped_lock lock(log->mutex);
        log->sized = false;
    }
    fixture.runLua("intro = video.load('videos/intro.mp4')");
    fixture.frames(5);
    EXPECT_EQ(fixture.lua("return intro.state"), "loading");
    {
        const std::scoped_lock lock(log->mutex);
        log->error = "The browser could not play the video.";
    }
    ASSERT_TRUE(fixture.frameUntil([this] { return fixture.lua("return intro.state") == "failed"; }));
    EXPECT_EQ(fixture.lua("return intro.error"), "The browser could not play the video.");
    {
        const std::scoped_lock lock(log->mutex);
        log->sized = true;
        log->error.clear();
    }
    loadIntro();
    fixture.runLua("intro.rate = 1.5 intro:play()");
    fixture.frames(1);
    {
        const std::scoped_lock lock(log->mutex);
        EXPECT_EQ(log->controls.back(), "play 1.5");
        log->clock = 0.75;
        log->frameShown = true;
    }
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return intro.time .. ' ' .. intro.frameCount .. ' ' .. intro.texture.width"), "0.75 1 8");
    fixture.runLua("intro:seek(1.25) intro:pause()");
    fixture.frames(1);
    const std::scoped_lock lock(log->mutex);
    EXPECT_DOUBLE_EQ(log->seeks.back(), 1.25);
    EXPECT_EQ(log->controls.back(), "pause 1.5");

    // The frame thread alone reads a decoder with a clock of its own.
    EXPECT_EQ(log->readingThreads, std::set{std::this_thread::get_id()});
}

} // namespace haylen::graphics
