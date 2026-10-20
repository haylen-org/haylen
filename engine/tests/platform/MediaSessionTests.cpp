#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/EventBus.hpp"
#include "haylen/core/LifecycleEvent.hpp"
#include "haylen/platform/MediaSession.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

TEST(MediaSessionTest, ShowsWhatPlaysAndPublishesTheCommandsOfThePlatform) {
    test::EngineFixture fixture;
    std::vector<std::string> log;
    fixture.engine().getEvents().on(core::LifecycleEvent::kMediaCommand, [&log](core::EventBus::Event& event) { log.push_back(event.getData().dump()); });

    MediaSession& session = fixture.engine().getMediaSession();
    session.setNowPlaying({.title = "Harbor", .artist = "Band", .duration = 120.0, .position = 3.5, .playing = true, .commands = MediaSession::bit(MediaSession::Command::TogglePlayPause)});
    ASSERT_TRUE(fixture.host().getNowPlaying().has_value());
    EXPECT_EQ(fixture.host().getNowPlaying()->title, "Harbor");
    EXPECT_EQ(fixture.host().getNowPlaying()->commands, MediaSession::bit(MediaSession::Command::TogglePlayPause));

    // Commands arrive from any thread and reach the app at the start of the next frame.
    // clang-format off
    std::thread([] {
        MediaSession::receive(MediaSession::Command::TogglePlayPause, 0.0);
        MediaSession::receive(MediaSession::Command::Seek, 42.5);
    }).join();
    // clang-format on
    EXPECT_TRUE(log.empty());
    fixture.frames(1);
    EXPECT_EQ(log, (std::vector<std::string>{R"({"command":"togglePlayPause"})", R"({"command":"seek","position":42.5})"}));

    session.clear();
    EXPECT_FALSE(fixture.host().getNowPlaying().has_value());
    EXPECT_THROW(session.setNowPlaying({.duration = -1.0}), std::invalid_argument);
}

// An engine that stops leaves the controls, and the commands of the platform that arrive meanwhile reach no app.
TEST(MediaSessionTest, LeavesTheControlsWhenTheEngineStops) {
    test::EngineFixture fixture;
    fixture.engine().getMediaSession().setNowPlaying({.title = "Menu"});
    fixture.restart();
    EXPECT_FALSE(fixture.host().getNowPlaying().has_value());
    std::vector<std::string> log;
    fixture.engine().getEvents().on(core::LifecycleEvent::kMediaCommand, [&log](core::EventBus::Event& event) { log.push_back(event.getData().dump()); });
    MediaSession::receive(MediaSession::Command::Next, 0.0);
    fixture.frames(1);
    EXPECT_EQ(log.size(), 1U);
}

TEST(MediaSessionTest, TakesWhatPlaysFromLua) {
    test::EngineFixture fixture;
    fixture.runLua("system = require('haylen.system') events = require('haylen.events') heard = {} events.on('mediaCommand', function(event) heard[#heard + 1] = event.command .. (event.position and (' ' .. event.position) or '') end)");
    fixture.runLua("system.setNowPlaying({title = 'Tides', artist = 'Band', album = 'Sea', duration = 60, position = 2, playing = true, artwork = '\\137PNG', commands = {'play', 'pause', 'seek'}})");
    const auto& shown = fixture.host().getNowPlaying();
    ASSERT_TRUE(shown.has_value());
    EXPECT_EQ(shown->album, "Sea");
    EXPECT_EQ(shown->artwork.size(), 4U);
    EXPECT_EQ(shown->commands, MediaSession::bit(MediaSession::Command::Play) | MediaSession::bit(MediaSession::Command::Pause) | MediaSession::bit(MediaSession::Command::Seek));

    MediaSession::receive(MediaSession::Command::Pause, 0.0);
    MediaSession::receive(MediaSession::Command::Seek, 12.0);
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return table.concat(heard, ', ')"), "pause, seek 12.0");

    EXPECT_NE(fixture.lua("system.setNowPlaying({title = 'x', speed = 2})").find("Unknown option \"speed\"."), std::string::npos);
    EXPECT_NE(fixture.lua("system.setNowPlaying({commands = {'rewind'}})").find("The option \"commands\" names \"rewind\", which is not \"play\", \"pause\", \"togglePlayPause\", \"stop\", \"next\", \"previous\" or \"seek\"."), std::string::npos);
    EXPECT_NE(fixture.lua("system.setNowPlaying({commands = 'play'})").find("The option \"commands\" must be a list of command names."), std::string::npos);
    EXPECT_NE(fixture.lua("system.setNowPlaying({artwork = 3})").find("The option \"artwork\" must be the bytes of a PNG or JPEG image."), std::string::npos);
    EXPECT_NE(fixture.lua("system.setNowPlaying({position = -1})").find("The duration and the position of what plays cannot be negative."), std::string::npos);
    fixture.runLua("system.clearNowPlaying()");
    EXPECT_FALSE(fixture.host().getNowPlaying().has_value());
}

// The track that Android and the web receive carries any title an app gives it, where bytes that are no UTF-8 become replacement characters.
TEST(MediaSessionTest, WritesATrackWhoseTextsAreNotUtf8) {
    const core::Json track = core::Json::parse(MediaSession::writeNowPlaying({.title = "Ol\xE1", .artist = "Band", .duration = 90.0, .playing = true}));
    EXPECT_EQ(track.at("title"), "Ol\xEF\xBF\xBD");
    EXPECT_EQ(track.at("artist"), "Band");
    EXPECT_EQ(track.at("playing"), true);
    EXPECT_EQ(track.at("commands"), MediaSession::kAllCommands);
}

} // namespace haylen::platform
