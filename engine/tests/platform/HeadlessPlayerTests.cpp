#include <gtest/gtest.h>

#include <string>

#include "platform/headless/HeadlessPlayer.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

class HeadlessPlayerTest : public ::testing::Test {
  protected:
    HeadlessPlayerTest() {
        app.write("app.json", R"({"name": "Player Test", "identifier": "dev.haylen.tests"})");
    }

    // Plays the app with the source of its main module and returns the exit status of the player.
    int play(const std::string& main, long long frames = 60) {
        app.write("source/main.lua", main);
        return HeadlessPlayer::run({.package = app.getPath(), .frames = frames});
    }

    test::TemporaryDirectory app;
};

TEST_F(HeadlessPlayerTest, ExitsWithTheStatusTheAppQuitWith) {
    EXPECT_EQ(play("require('haylen').quit()"), 0);
    EXPECT_EQ(play("require('haylen').quit({status = 3})"), 3);
    EXPECT_EQ(play("require('haylen.timer').after(0.05, function() require('haylen').quit({status = 255}) end)"), 255);
}

TEST_F(HeadlessPlayerTest, ExitsWithOneWhenAnErrorWasReportedOrTheAppDidNotQuit) {
    EXPECT_EQ(play("error('Broken on purpose.')"), 1);
    EXPECT_EQ(play("local haylen = require('haylen') haylen.setRecoverable(true) haylen.reportError('Broken on purpose.') haylen.recover() haylen.quit()"), 1);
    EXPECT_EQ(play("", 3), 1);
}

} // namespace haylen::platform
