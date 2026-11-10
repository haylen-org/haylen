#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "platform/headless/HeadlessPlayer.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

class HeadlessPlayerTest : public ::testing::Test {
  protected:
    HeadlessPlayerTest() {
        app.write("app.json", R"({"name": "Player Test", "identifier": "com.haylen.tests"})");
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

// The player has no audio device, and its voices still play out in real time, so an app that waits for a sound to end goes on.
TEST_F(HeadlessPlayerTest, PlaysSoundsOutWithoutADevice) {
    // clang-format off
    EXPECT_EQ(play(R"(
        local audio = require('haylen.audio')
        local haylen = require('haylen')
        local beep = audio.newSound(string.rep(string.pack('<h', 1000), 2205), {format = 'int16', sampleRate = 22050, channels = 1})
        local voice = audio.play(beep)
        require('haylen.scene').push({update = function()
            if not audio.active(voice) then haylen.quit() end
        end})
    )", 60), 0);
    // clang-format on
}

// A launched app plays until it quits, and the app that launched it starts again from its package, which the shared preferences of the two apps tell apart.
TEST_F(HeadlessPlayerTest, StartsTheLauncherAgainWhenTheAppItLaunchedQuits) {
    test::TemporaryDirectory guest;
    guest.write("app.json", R"({"name": "Guest", "identifier": "com.haylen.tests"})");
    guest.write("source/main.lua", "local haylen = require('haylen') local preferences = require('haylen.preferences') preferences.set('guest', haylen.launched() and haylen.canQuit()) preferences.save() haylen.quit({status = 9})");
    const std::string path = guest.getPath().generic_string();
    // clang-format off
    EXPECT_EQ(play(R"(
        local haylen = require('haylen')
        local preferences = require('haylen.preferences')
        if preferences.get('guest', false) then
            preferences.remove('guest')
            preferences.save()
            haylen.quit({status = haylen.launched() and 1 or 5})
        else
            haylen.launch(')" + path + R"(')
        end
    )", 120), 5);
    // clang-format on
}

// A launched app whose engine cannot start, since a file stands where its storage folder goes, fails the run, and the app that launched it starts again.
TEST_F(HeadlessPlayerTest, StartsTheLauncherAgainWhenTheAppItLaunchedCannotStart) {
    test::TemporaryDirectory guest;
    guest.write("app.json", R"({"name": "Guest", "identifier": "com.haylen.brokenguest"})");
    guest.write("source/main.lua", "require('haylen').quit()");
    const std::filesystem::path storage = std::filesystem::temp_directory_path() / "haylen-headless" / "com.haylen.tests" / "com.haylen.brokenguest";
    std::filesystem::create_directories(storage.parent_path());
    std::filesystem::remove_all(storage);
    std::ofstream(storage) << "A file where the storage folder of the guest goes.";
    test::TemporaryDirectory records;
    const std::filesystem::path restarted = records.getPath() / "restarted";
    // clang-format off
    EXPECT_EQ(play(R"(
        local haylen = require('haylen')
        local preferences = require('haylen.preferences')
        if preferences.get('brokenGuest', false) then
            preferences.remove('brokenGuest')
            preferences.save()
            local record = io.open(')" + restarted.generic_string() + R"(', 'w')
            record:write(tostring(haylen.launched()))
            record:close()
            haylen.quit()
        else
            preferences.set('brokenGuest', true)
            preferences.save()
            haylen.launch(')" + guest.getPath().generic_string() + R"(')
        end
    )", 120), 1);
    // clang-format on
    std::filesystem::remove(storage);

    std::ifstream record(restarted);
    const std::string launched((std::istreambuf_iterator<char>(record)), std::istreambuf_iterator<char>());
    EXPECT_EQ(launched, "false");
}

// An app launched without native access has none of the modules and functions that reach native code and other programs but keeps every module of the engine, an app it launches with native access has none either, and the app that launched it keeps them.
TEST_F(HeadlessPlayerTest, LaunchesAnAppWithoutNativeAccess) {
    // The two apps share preferences of their own, which no other test that runs at the same time writes.
    app.write("app.json", R"({"name": "Player Test", "identifier": "com.haylen.nativeaccess"})");
    test::TemporaryDirectory guest;
    guest.write("app.json", R"({"name": "Guest", "identifier": "com.haylen.nativeaccess"})");
    const std::string path = guest.getPath().generic_string();
    // clang-format off
    guest.write("source/main.lua", R"(
        local haylen = require('haylen')
        local preferences = require('haylen.preferences')
        local withheld = {ffi = true, process = true, ['haylen.native'] = true}
        local function lacksNativeAccess()
            local loaded, message = pcall(require, 'ffi')
            local refused = not loaded and message:find('The app that launched this app gave it no native access, so the module "ffi" is not available.', 1, true) ~= nil
            return refused and not pcall(require, 'process') and not pcall(require, 'haylen.native') and ffi == nil and process == nil and io.popen == nil and os.execute == nil
        end
        local function keepsEngineModules()
            for name in pairs(package.preload) do
                if not withheld[name] and not pcall(require, name) then
                    return false
                end
            end
            return pcall(require, 'http') and pcall(require, 'fs')
        end
        local stage = preferences.get('stage')
        if stage == 'guest' then
            preferences.set('guest', lacksNativeAccess() and keepsEngineModules())
            preferences.set('stage', 'nested')
            preferences.save()
            haylen.launch(')" + path + R"(', {nativeAccess = true})
        elseif stage == 'nested' then
            preferences.set('nested', lacksNativeAccess())
            preferences.set('stage', 'done')
            preferences.save()
            haylen.quit()
        else
            haylen.quit()
        end
    )");
    EXPECT_EQ(play(R"(
        local haylen = require('haylen')
        local preferences = require('haylen.preferences')
        if preferences.get('stage') == 'done' then
            local passed = preferences.get('guest', false) and preferences.get('nested', false) and pcall(require, 'ffi') and ffi ~= nil and io.popen ~= nil
            preferences.clear()
            preferences.save()
            haylen.quit({status = passed and 5 or 1})
        else
            preferences.set('stage', 'guest')
            preferences.save()
            haylen.launch(')" + path + R"(', {nativeAccess = false})
        end
    )", 240), 5);
    // clang-format on
}

} // namespace haylen::platform
