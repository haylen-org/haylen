#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/lua/Error.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/storage/Preferences.hpp"
#include "haylen/storage/SaveSlots.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::storage {

TEST(UserStorageTest, WritesReadsListsAndRemoves) {
    test::TemporaryDirectory directory;
    int persisted = 0;
    UserStorage storage(directory.getPath() / "saves", [&] { ++persisted; });

    storage.writeText("slots/one.json", "{\"day\": 3}");
    storage.write("raw.bin", std::vector<std::uint8_t>{1, 2, 3});
    EXPECT_TRUE(storage.exists("slots/one.json"));
    EXPECT_EQ(storage.readText("slots/one.json"), "{\"day\": 3}");
    EXPECT_EQ(storage.read("raw.bin").size(), 3U);
    EXPECT_EQ(storage.list(""), (std::vector<std::string>{"raw.bin", "slots/one.json"}));
    EXPECT_TRUE(storage.list("missing").empty());

    EXPECT_TRUE(storage.remove("raw.bin"));
    EXPECT_FALSE(storage.remove("raw.bin"));
    EXPECT_THROW((void)storage.read("raw.bin"), std::runtime_error);
    EXPECT_THROW(storage.writeText("", "x"), std::invalid_argument);
    EXPECT_THROW(storage.writeText("../escape", "x"), std::invalid_argument);

    storage.flush();
    EXPECT_EQ(persisted, 1);
    EXPECT_EQ(storage.getRoot(), directory.getPath() / "saves");

    UserStorage desktop(directory.getPath() / "desktop");
    desktop.flush();
    EXPECT_EQ(persisted, 1);
}

TEST(UserStorageTest, RemovesStaleTemporaryFilesWhenItOpens) {
    const test::TemporaryDirectory directory;
    {
        UserStorage storage(directory.getPath());
        storage.writeText("saves/kept.json", "kept", UserStorage::Durability::Durable);
    }
    std::ofstream(directory.getPath() / "saves" / "kept.json.7.tmp") << "interrupted";
    const UserStorage reopened(directory.getPath());
    EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "saves" / "kept.json.7.tmp"));
    EXPECT_EQ(reopened.readText("saves/kept.json"), "kept");
}

TEST(UserStorageTest, FailedWriteKeepsThePreviousFile) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    storage.writeText("save.json", "good");

    // A folder where the temporary file of the second write goes makes that write fail before anything replaces the file, and the cleanup that cannot remove the folder keeps the original error.
    std::filesystem::create_directories(directory.getPath() / "save.json.2.tmp" / "blocked");
    std::string error;
    try {
        storage.writeText("save.json", "lost");
    } catch (const std::runtime_error& failure) {
        error = failure.what();
    }
    EXPECT_EQ(error, "The storage file \"save.json\" could not be written.");
    EXPECT_EQ(storage.readText("save.json"), "good");
}

TEST(UserStorageTest, FailedWritesLeaveNoTemporaryFileAndNameTheRelativePath) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    storage.writeText("slot/inside.txt", "kept");
    storage.writeText("notes.txt", "kept");

    // A folder with files cannot be replaced by a file, and a file cannot hold a folder.
    for (const std::string& path : {std::string("slot"), std::string("notes.txt/deeper")}) {
        std::string error;
        try {
            storage.writeText(path, "lost");
        } catch (const std::runtime_error& failure) {
            error = failure.what();
        }
        EXPECT_EQ(error, "The storage file \"" + path + "\" could not be written.");
    }
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory.getPath()), std::filesystem::directory_iterator()), 2) << "No temporary file stays next to the folder.";
    EXPECT_EQ(storage.readText("slot/inside.txt"), "kept");
}

#if !defined(_WIN32)
TEST(UserStorageTest, RefusesToListAFolderPartly) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    storage.writeText("saves/slot1.json", "{}");
    storage.writeText("saves/locked/slot2.json", "{}");
    const std::filesystem::path locked = directory.getPath() / "saves" / "locked";
    std::filesystem::permissions(locked, std::filesystem::perms::none);

    // A user who reads every folder, such as the superuser, cannot meet the problem.
    std::error_code denied;
    const std::filesystem::directory_iterator probe(locked, denied);
    if (!denied) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
        GTEST_SKIP() << "The user reads folders without permission.";
    }

    std::string message;
    try {
        (void)storage.list("saves");
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
    EXPECT_TRUE(message.starts_with("The storage folder \"saves\" could not be listed, and the file system reported")) << message;
}
#endif

TEST(SaveSlotsTest, WritesReadsListsAndRemovesSlots) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    std::int64_t now = 100;
    SaveSlots saves(storage, "saves", [&now] { return now; });

    saves.write("slot-1", {{"day", 3}, {"wood", 12}}, {{"day", 3}});
    now = 250;
    saves.write("auto_save", {{"day", 5}});
    now = 250;
    saves.write("slot-2", core::Json::array({1, 2}));
    storage.writeText("saves/readme.txt", "not a save");
    storage.writeText("saves/bad name.json", "{}");

    EXPECT_EQ(saves.read("slot-1"), (core::Json{{"day", 3}, {"wood", 12}}));
    EXPECT_EQ(saves.read("slot-2"), core::Json::array({1, 2}));
    EXPECT_EQ(saves.read("empty"), std::nullopt);
    EXPECT_EQ(saves.getInfo("slot-1")->savedAt, 100);
    EXPECT_EQ(saves.getInfo("slot-1")->summary, (core::Json{{"day", 3}}));
    EXPECT_EQ(saves.getInfo("empty"), std::nullopt);
    EXPECT_TRUE(saves.exists("auto_save"));

    const std::vector<SaveSlots::Info> listed = saves.list();
    ASSERT_EQ(listed.size(), 3U);
    EXPECT_EQ(listed[0].slot, "auto_save");
    EXPECT_EQ(listed[1].slot, "slot-2");
    EXPECT_EQ(listed[2].slot, "slot-1");
    EXPECT_EQ(listed[0].summary, core::Json::object());

    EXPECT_TRUE(saves.remove("slot-2"));
    EXPECT_FALSE(saves.remove("slot-2"));
    EXPECT_FALSE(saves.exists("slot-2"));
    EXPECT_EQ(saves.list().size(), 2U);
}

TEST(SaveSlotsTest, RejectsBadNamesAndDamagedFiles) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);

    for (const std::string& name : {std::string(""), std::string("../escape"), std::string("a b"), std::string(65, 'x')}) {
        EXPECT_THROW(saves.write(name, core::Json::object()), std::invalid_argument) << name;
    }
    EXPECT_THROW(saves.write("slot", core::Json::object(), core::Json::array()), std::invalid_argument);

    storage.writeText("saves/broken.json", "{");
    storage.writeText("saves/partial.json", R"({"data": 1})");
    storage.writeText("saves/dated.json", R"({"data": 1, "summary": {}, "savedAt": "yesterday"})");
    storage.writeText("saves/summed.json", R"({"data": 1, "summary": [1], "savedAt": 5})");
    EXPECT_THROW((void)saves.read("broken"), std::runtime_error);
    EXPECT_THROW((void)saves.getInfo("partial"), std::runtime_error);
    EXPECT_THROW((void)saves.getInfo("dated"), std::runtime_error);
    EXPECT_THROW((void)saves.read("summed"), std::runtime_error);

    saves.write("real", {{"ok", true}});
    EXPECT_GT(saves.getInfo("real")->savedAt, 1'700'000'000);
}

TEST(SaveSlotsTest, MigratesOldSavesOnRead) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);
    saves.write("slot-1", {{"items", core::Json::array({"axe"})}});
    EXPECT_EQ(saves.getInfo("slot-1")->version, 1);
    EXPECT_EQ(saves.getInfo("slot-1")->revision, 1U);

    // The migrations of each version run in order, and a read never changes the stored slot.
    saves.setVersion(3);
    std::vector<int> ran;
    // clang-format off
    saves.addMigration(2, [&ran](core::Json data) {
        ran.push_back(2);
        data["stamina"] = 100;
        return data;
    });
    saves.addMigration(1, [&ran](core::Json data) {
        ran.push_back(1);
        data["inventory"] = data.at("items");
        data.erase("items");
        return data;
    });
    // clang-format on
    EXPECT_EQ(saves.read("slot-1"), (core::Json{{"inventory", core::Json::array({"axe"})}, {"stamina", 100}}));
    EXPECT_EQ(ran, (std::vector<int>{1, 2}));
    EXPECT_EQ(saves.getInfo("slot-1")->version, 1);
    saves.write("slot-2", {{"stamina", 5}});
    EXPECT_EQ(saves.getInfo("slot-2")->version, 3);

    // A missing step and a slot of a newer version fail with what went wrong.
    saves.setVersion(4);
    std::string missing;
    try {
        (void)saves.read("slot-1");
    } catch (const std::runtime_error& error) {
        missing = error.what();
    }
    EXPECT_EQ(missing, "The save slot \"slot-1\" has version 1 and no migration reaches version 4.");
    saves.setVersion(2);
    std::string newer;
    try {
        (void)saves.read("slot-2");
    } catch (const std::runtime_error& error) {
        newer = error.what();
    }
    EXPECT_EQ(newer, "The save slot \"slot-2\" has version 3, which is newer than the version 2 this app reads.");
    EXPECT_THROW(saves.setVersion(0), std::invalid_argument);
    EXPECT_THROW(saves.addMigration(0, [](core::Json data) { return data; }), std::invalid_argument);
}

TEST(SaveSlotsTest, FallsBackToTheBackupWhenDamaged) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);
    saves.write("slot-1", {{"day", 1}});
    saves.write("slot-1", {{"day", 2}});
    EXPECT_EQ(saves.getInfo("slot-1")->revision, 2U);
    EXPECT_FALSE(saves.getInfo("slot-1")->recovered);

    // A change that the checksum does not cover, such as a bit that flipped on the disk, makes the backup take the place of the slot.
    std::string text = storage.readText("saves/slot-1.json");
    text.replace(text.find("\"day\":2"), 7, "\"day\":9");
    storage.writeText("saves/slot-1.json", text);
    EXPECT_EQ(saves.read("slot-1"), (core::Json{{"day", 1}}));
    EXPECT_TRUE(saves.getInfo("slot-1")->recovered);
    EXPECT_EQ(saves.getInfo("slot-1")->revision, 1U);
    EXPECT_EQ(saves.list().size(), 1U) << "A backup is no slot of its own.";

    // A write over a damaged slot keeps the sound backup, and a slot without a sound copy fails.
    saves.write("slot-1", {{"day", 3}});
    EXPECT_EQ(saves.getInfo("slot-1")->revision, 1U);
    storage.writeText("saves/slot-1.json", "{");
    EXPECT_EQ(saves.read("slot-1"), (core::Json{{"day", 1}}));
    storage.writeText("saves/slot-1.backup.json", "{");
    EXPECT_THROW((void)saves.read("slot-1"), std::runtime_error);
    EXPECT_TRUE(saves.remove("slot-1"));
    EXPECT_FALSE(storage.exists("saves/slot-1.backup.json"));
}

TEST(SaveSlotsTest, ImportsOnlyNewerRevisionsUnlessAskedToReplace) {
    const test::TemporaryDirectory phoneFolder;
    const test::TemporaryDirectory tabletFolder;
    UserStorage phoneStorage(phoneFolder.getPath());
    UserStorage tabletStorage(tabletFolder.getPath());
    std::int64_t now = 100;
    SaveSlots phone(phoneStorage, "saves", [&now] { return now; });
    SaveSlots tablet(tabletStorage, "saves", [&now] { return now; });

    phone.write("slot", {{"day", 1}});
    tablet.write("slot", {{"day", 4}});
    tablet.write("slot", {{"day", 5}});
    const std::string older = *phone.exportSlot("slot");
    const std::string newer = *tablet.exportSlot("slot");
    EXPECT_FALSE(tablet.importSlot("slot", older, SaveSlots::ImportPolicy::KeepNewer));
    EXPECT_EQ(tablet.read("slot"), (core::Json{{"day", 5}}));
    EXPECT_TRUE(phone.importSlot("slot", newer, SaveSlots::ImportPolicy::KeepNewer));
    EXPECT_EQ(phone.read("slot"), (core::Json{{"day", 5}}));
    EXPECT_TRUE(tablet.importSlot("slot", older, SaveSlots::ImportPolicy::Replace));
    EXPECT_EQ(tablet.getInfo("slot")->revision, 1U);
    EXPECT_TRUE(phone.importSlot("fresh", older, SaveSlots::ImportPolicy::KeepNewer));
    EXPECT_FALSE(phone.exportSlot("none").has_value());
    EXPECT_THROW((void)phone.importSlot("slot", "{}", SaveSlots::ImportPolicy::Replace), std::invalid_argument);
    EXPECT_EQ(SaveSlots::importPolicyFromName("keepNewer"), SaveSlots::ImportPolicy::KeepNewer);
    EXPECT_EQ(SaveSlots::importPolicyName(SaveSlots::ImportPolicy::Replace), "replace");
}

// A slot that is there but cannot be read fails, instead of reading as a slot that was never saved.
TEST(SaveSlotsTest, FailsForSlotsThatCannotBeRead) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);
    saves.write("locked", {{"day", 1}});

    // A file without read permission fails to read, except on platforms or for users that read it anyway.
    const std::filesystem::path file = directory.getPath() / "saves" / "locked.json";
    std::filesystem::permissions(file, std::filesystem::perms::none);
    if (std::ifstream(file).is_open()) {
        std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        GTEST_SKIP() << "This platform reads files without read permission.";
    }
    EXPECT_THROW((void)saves.read("locked"), std::runtime_error);
    std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    EXPECT_EQ(saves.read("locked"), (core::Json{{"day", 1}}));
}

TEST(PreferencesTest, StoresDottedKeysAndPersists) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    Preferences preferences(storage);
    preferences.load();
    EXPECT_FALSE(preferences.isDirty());
    EXPECT_EQ(preferences.getValues(), core::Json::object());

    preferences.set("audio.volume.music", 0.5);
    preferences.set("audio.volume.sfx", 0.8);
    preferences.set("language", "pt-BR");
    EXPECT_TRUE(preferences.isDirty());
    EXPECT_TRUE(preferences.has("audio.volume"));
    EXPECT_EQ(preferences.get("audio.volume.music"), 0.5);
    EXPECT_EQ(preferences.get("audio.volume.ui", 1.0), 1.0);
    EXPECT_EQ(preferences.get("language.code"), nullptr);
    EXPECT_THROW(preferences.set("language.code", "pt"), std::invalid_argument);
    EXPECT_THROW(preferences.set("audio..music", 1), std::invalid_argument);
    EXPECT_THROW((void)preferences.has(".audio"), std::invalid_argument);

    EXPECT_TRUE(preferences.remove("audio.volume.sfx"));
    EXPECT_FALSE(preferences.remove("audio.volume.sfx"));
    EXPECT_FALSE(preferences.remove("video.mode"));
    preferences.set("video.mode", "window");
    preferences.set("video.mode", nullptr);
    EXPECT_FALSE(preferences.has("video.mode"));
    EXPECT_EQ(preferences.get("video.mode", "full"), "full");
    preferences.set("video", nullptr);
    preferences.set("missing.key", nullptr);
    preferences.save();
    EXPECT_FALSE(preferences.isDirty());

    Preferences reloaded(storage);
    reloaded.load();
    EXPECT_EQ(reloaded.getValues(), (core::Json{{"audio", {{"volume", {{"music", 0.5}}}}}, {"language", "pt-BR"}}));
    reloaded.clear();
    EXPECT_TRUE(reloaded.isDirty());
    EXPECT_EQ(reloaded.getValues(), core::Json::object());

    storage.writeText("preferences.json", "[1, 2]");
    EXPECT_THROW(reloaded.load(), std::runtime_error);
    EXPECT_EQ(reloaded.getValues(), core::Json::object());
}

TEST(PreferencesTest, RefusesValuesThatJsonCannotHold) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    Preferences preferences(storage);
    EXPECT_THROW(preferences.set("name", std::string("\xFF")), std::invalid_argument);
    EXPECT_THROW(preferences.set("\xFF", 1), std::invalid_argument);
    EXPECT_THROW(preferences.set("list", core::Json::array({1.0, std::numeric_limits<double>::infinity()})), std::invalid_argument);
    EXPECT_THROW(preferences.set("group", core::Json{{"\xFF", 1}}), std::invalid_argument);
    EXPECT_FALSE(preferences.isDirty());

    preferences.set("volume", 0.5);
    preferences.save();
    EXPECT_EQ(storage.readText("preferences.json"), "{\n  \"volume\": 0.5\n}");
}

TEST(PreferencesTest, KeepsUnsavedChangesWhenTheFileCannotBeRead) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    storage.writeText("preferences.json", "{}");
    Preferences preferences(storage);
    preferences.set("volume", 0.5);

    // A file without read permission fails to read, except on platforms or for users that read it anyway.
    const std::filesystem::path file = directory.getPath() / "preferences.json";
    std::filesystem::permissions(file, std::filesystem::perms::none);
    if (std::ifstream(file).is_open()) {
        std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
        GTEST_SKIP() << "This platform reads files without read permission.";
    }
    EXPECT_THROW(preferences.load(), std::runtime_error);
    std::filesystem::permissions(file, std::filesystem::perms::owner_read | std::filesystem::perms::owner_write);
    EXPECT_TRUE(preferences.isDirty());
    EXPECT_EQ(preferences.get("volume"), 0.5);
}

TEST(StorageLuaTest, ReadsAndWritesUserFiles) {
    test::EngineFixture fixture;
    fixture.runLua("storage = require('haylen.storage')");

    fixture.runLua("storage.writeText('notes.txt', 'hello') storage.writeJson('saves/slot.json', {day = 4, inventory = {'wood', 'stone'}})");
    EXPECT_EQ(fixture.lua("return storage.readText('notes.txt')"), "hello");
    EXPECT_EQ(fixture.lua("local save = storage.readJson('saves/slot.json') return save.day .. save.inventory[2]"), "4stone");
    EXPECT_EQ(fixture.lua("return table.concat(storage.list(), ',')"), "notes.txt,saves/slot.json");
    EXPECT_EQ(fixture.lua("return table.concat(storage.list('saves'), ',')"), "saves/slot.json");
    EXPECT_EQ(fixture.lua("return storage.exists('notes.txt')"), "true");
    EXPECT_EQ(fixture.lua("return storage.remove('notes.txt')"), "true");
    EXPECT_EQ(fixture.lua("return storage.exists('notes.txt')"), "false");

    const int before = fixture.host().getPersistCount();
    fixture.runLua("storage.flush()");
    EXPECT_EQ(fixture.host().getPersistCount(), before + 1);
    EXPECT_NE(fixture.lua("return storage.readText('missing.txt')").find("The storage file \"missing.txt\" was not found."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.writeText('../escape.txt', 'x')").find("error: "), std::string::npos);
    EXPECT_NE(fixture.lua("storage.writeJson('bad.json', {callback = print})").find("cannot be converted to JSON"), std::string::npos);
}

TEST(StorageLuaTest, SharesTheFolderWithVarnFs) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        storage = require('haylen.storage')
        fs = require('fs')
        async = require('async')
        storage.writeText('notes/today.txt', 'from storage')
        async.spawn(function()
            local root = storage.root()
            local text = fs.readFile(root .. '/notes/today.txt'):await()
            fs.mkdir(root .. '/cache/maps'):await()
            fs.writeFile(root .. '/cache/maps/island.json', '{"size": 3}'):await()
            local names = fs.readdir(root .. '/notes'):await()
            fs.removeRecursive(root .. '/notes'):await()
            result = text .. ' ' .. names[1]
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return result") != "nil"; }));
    EXPECT_EQ(fixture.lua("return result"), "from storage today.txt");
    EXPECT_EQ(fixture.lua("return storage.readJson('cache/maps/island.json').size .. ' ' .. tostring(storage.exists('notes/today.txt')) .. ' ' .. table.concat(storage.list(), ',')"), "3 false cache/maps/island.json");
    EXPECT_EQ(fixture.lua("return storage.root()"), fixture.engine().getStorage().getRoot().generic_string());
}

TEST(StorageLuaTest, SavesSlotsPreferencesAndEngineState) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        storage = require('haylen.storage')
        preferences = require('haylen.preferences')
        audio = require('haylen.audio')
        input = require('haylen.input')
        window = require('haylen.window')
    )");
    // clang-format on

    EXPECT_EQ(fixture.lua("storage.writeSlot('island', {day = 4, trees = {1, 2}}, {day = 4}) local data = storage.readSlot('island') return data.day .. ' ' .. #data.trees"), "4 2");
    EXPECT_EQ(fixture.lua("local info = storage.slotInfo('island') return info.slot .. ' ' .. info.summary.day .. ' ' .. tostring(info.savedAt > 0)"), "island 4 true");
    EXPECT_EQ(fixture.lua("storage.writeSlot('second', {}) return #storage.listSlots() .. ' ' .. tostring(storage.slotExists('second'))"), "2 true");
    EXPECT_EQ(fixture.lua("return tostring(storage.removeSlot('second')) .. tostring(storage.readSlot('second')) .. tostring(storage.slotInfo('second'))"), "truenilnil");
    EXPECT_NE(fixture.lua("storage.writeSlot('bad name', {})").find("save slot name"), std::string::npos);

    EXPECT_EQ(fixture.lua("preferences.set('game.difficulty', 'hard') return preferences.get('game.difficulty') .. ' ' .. preferences.get('game.speed', 2) .. ' ' .. tostring(preferences.dirty())"), "hard 2 true");
    EXPECT_EQ(fixture.lua("preferences.save() return tostring(preferences.dirty()) .. ' ' .. tostring(preferences.has('game')) .. ' ' .. preferences.values().game.difficulty"), "false true hard");

    // Suspending writes pending changes, so they survive a load from disk.
    fixture.runLua("preferences.set('game.difficulty', 'easy')");
    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    EXPECT_EQ(fixture.lua("preferences.set('game.difficulty', 'lost') preferences.load() return preferences.get('game.difficulty')"), "easy");
    EXPECT_EQ(fixture.lua("return tostring(preferences.remove('game.difficulty')) .. tostring(preferences.has('game.difficulty'))"), "truefalse");
    EXPECT_EQ(fixture.lua("preferences.set('game.speed', 3) preferences.set('game.speed', nil) return tostring(preferences.has('game.speed')) .. ' ' .. preferences.get('game.speed', 5)"), "false 5");
    EXPECT_EQ(fixture.lua("preferences.clear() return next(preferences.values()) == nil"), "true");

    // clang-format off
    fixture.runLua(R"(
        input.loadActions({actions = {{name = 'jump', type = 'button', bindings = {'key:space'}}}})
        audio.setBusVolume('music', 0.6)
        audio.setBusMuted('sfx', true)
        window.setFullscreen(true)
        preferences.capture()
        preferences.save()
        audio.setBusVolume('music', 1)
        audio.setBusMuted('sfx', false)
        window.setFullscreen(false)
        input.loadActions({actions = {{name = 'left', type = 'button', bindings = {'key:a'}}, {name = 'right', type = 'button', bindings = {'key:d'}}}})
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return preferences.get('audio.volume.music') .. ' ' .. tostring(preferences.get('audio.muted.sfx')) .. ' ' .. tostring(preferences.get('window.fullscreen'))"), "0.6 true true");
    const std::string saved = fixture.engine().getStorage().readText("preferences.json");
    EXPECT_NE(saved.find("\"music\": 0.6"), std::string::npos);
    EXPECT_EQ(saved.find("0.60000"), std::string::npos) << "Captured floats keep their shortest decimal form.";
    EXPECT_EQ(fixture.lua("preferences.apply() return string.format('%.6f', audio.busVolume('music')) .. ' ' .. tostring(audio.busMuted('sfx')) .. ' ' .. tostring(window.fullscreen()) .. ' ' .. table.concat(input.actionNames(), ',')"), "0.600000 true true jump");
    EXPECT_NE(fixture.lua("preferences.set('window.fullscreen', 'yes') preferences.apply()").find("The preference \"window.fullscreen\" has a value of the wrong type."), std::string::npos);

    fixture.engine().getStorage().writeText("preferences.json", "{");
    EXPECT_NE(fixture.lua("preferences.load()").find("The preferences file \"preferences.json\" is damaged."), std::string::npos);
}

TEST(StorageLuaTest, SavesWhatTheListenersOfTheBackgroundChange) {
    test::EngineFixture fixture;
    fixture.runLua("local preferences = require('haylen.preferences') require('haylen.events').on('appBackground', function() preferences.set('session.screen', 'map') end)");
    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    ASSERT_TRUE(fixture.engine().getStorage().exists("preferences.json"));
    EXPECT_NE(fixture.engine().getStorage().readText("preferences.json").find("\"screen\": \"map\""), std::string::npos);
}

TEST(StorageLuaTest, RefusesValuesThatJsonCannotHold) {
    test::EngineFixture fixture;
    fixture.runLua("storage = require('haylen.storage') preferences = require('haylen.preferences') invalid = string.char(255)");
    EXPECT_EQ(fixture.lua("preferences.set('name', invalid)"), "error: test:1: A string that is not valid UTF-8 cannot be converted to JSON.");
    EXPECT_EQ(fixture.lua("preferences.set('ratio', 0 / 0)"), "error: test:1: A number that is not finite cannot be converted to JSON.");
    EXPECT_EQ(fixture.lua("preferences.set('volume', 0.5) preferences.save() return tostring(preferences.has('ratio')) .. ' ' .. preferences.get('volume')"), "false 0.5");
    EXPECT_NE(fixture.engine().getStorage().readText("preferences.json").find("\"volume\": 0.5"), std::string::npos);

    EXPECT_EQ(fixture.lua("storage.writeJson('bad.json', {name = invalid})"), "error: test:1: A string that is not valid UTF-8 cannot be converted to JSON.");
    EXPECT_EQ(fixture.lua("storage.writeJson('bad.json', {[invalid] = true})"), "error: test:1: A key that is not valid UTF-8 cannot be converted to JSON.");
    EXPECT_EQ(fixture.lua("storage.writeJsonAsync('bad.json', {math.huge})"), "error: test:1: A number that is not finite cannot be converted to JSON.");
    EXPECT_EQ(fixture.lua("storage.writeSlot('slot', {x = 0 / 0})"), "error: test:1: A number that is not finite cannot be converted to JSON.");
    EXPECT_EQ(fixture.lua("storage.writeSlotAsync('slot', {}, {title = invalid})"), "error: test:1: A string that is not valid UTF-8 cannot be converted to JSON.");
    EXPECT_FALSE(fixture.engine().getStorage().exists("bad.json"));
    EXPECT_FALSE(fixture.engine().getStorage().exists("saves/slot.json"));
}

TEST(StorageLuaTest, KeepsBusesWhoseNamesHoldDotsApart) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        preferences = require('haylen.preferences')
        audio = require('haylen.audio')
        audio.createBus('voice')
        audio.createBus('voice.lines')
        audio.setBusVolume('voice', 0.5)
        audio.setBusVolume('voice.lines', 0.25)
        audio.setBusMuted('voice.lines', true)
        preferences.capture()
        audio.setBusVolume('voice', 1)
        audio.setBusVolume('voice.lines', 1)
        audio.setBusMuted('voice.lines', false)
        preferences.apply()
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("return audio.busVolume('voice') .. ' ' .. audio.busVolume('voice.lines') .. ' ' .. tostring(audio.busMuted('voice.lines'))"), "0.5 0.25 true");
    EXPECT_EQ(fixture.lua("return preferences.get('audio.volume')['voice.lines']"), "0.25");
    EXPECT_NE(fixture.lua("preferences.set('audio.muted', {voice = 'yes'}) preferences.apply()").find("The preference \"audio.muted\" has a value of the wrong type for the bus \"voice\"."), std::string::npos);
}

TEST(StorageLuaTest, MigratesSlotsAndMovesThemBetweenDevices) {
    test::EngineFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        storage = require('haylen.storage')
        written = {}
        require('haylen.events').on('saveSlotWritten', function(info) written[#written + 1] = info.slot .. ':' .. info.revision .. ':' .. info.version end)
        storage.writeSlot('slot-1', {items = {'axe'}})
        storage.setSaveVersion(3)
        storage.addMigration(1, function(data) data.inventory = data.items data.items = nil return data end)
        storage.addMigration(2, function(data) data.stamina = 100 return data end)
    )");
    // clang-format on
    EXPECT_EQ(fixture.lua("local data = storage.readSlot('slot-1') return data.inventory[1] .. ' ' .. data.stamina .. ' ' .. tostring(data.items) .. ' ' .. storage.saveVersion()"), "axe 100 nil 3");
    EXPECT_EQ(fixture.lua("local info = storage.slotInfo('slot-1') return info.version .. ' ' .. info.revision .. ' ' .. tostring(info.recovered) .. ' ' .. table.concat(written, ',')"), "1 1 false slot-1:1:1");

    // clang-format off
    fixture.runLua(R"(
        require('async').spawn(function()
            asyncStamina = storage.readSlotAsync('slot-1'):await().stamina
            storage.writeSlotAsync('slot-1', {stamina = 7}):await()
            done = true
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(done)") == "true"; }));
    EXPECT_EQ(fixture.lua("return asyncStamina .. ' ' .. table.concat(written, ',')"), "100 slot-1:1:1,slot-1:2:3");

    EXPECT_EQ(fixture.lua("local bytes = storage.exportSlot('slot-1') return tostring(storage.importSlot('copy', bytes)) .. ' ' .. tostring(storage.importSlot('copy', bytes)) .. ' ' .. tostring(storage.importSlot('copy', bytes, {policy = 'replace'})) .. ' ' .. storage.readSlot('copy').stamina"), "true false true 7");
    EXPECT_EQ(fixture.lua("return storage.exportSlot('none')"), "nil");
    EXPECT_NE(fixture.lua("storage.importSlot('copy', 'nonsense')").find("The imported save slot \"copy\" is damaged."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.importSlot('copy', storage.exportSlot('slot-1'), {policy = 'newest'})").find("The policy of an imported save slot must be \"replace\" or \"keepNewer\"."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.importSlot('copy', storage.exportSlot('slot-1'), {mode = 1})").find("Unknown option \"mode\"."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.setSaveVersion(4) return storage.readSlot('slot-1')").find("The save slot \"slot-1\" has version 3 and no migration reaches version 4."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.setSaveVersion(2) return storage.readSlot('slot-1')").find("has version 3, which is newer than the version 2 this app reads."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.setSaveVersion(0)").find("A save version is a whole number from 1."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.addMigration(0, print)").find("A save migration needs a version from 1 and a function."), std::string::npos);
}

TEST(StorageLuaTest, RunsAsynchronousOperationsInOrderOnTheIoPool) {
    test::EngineFixture fixture;
    const int persisted = fixture.host().getPersistCount();
    // clang-format off
    fixture.runLua(R"(
        storage = require('haylen.storage')
        results = {}
        storage.writeTextAsync('notes.txt', 'first')
        storage.writeTextAsync('notes.txt', 'second')
        require('async').spawn(function()
            results[#results + 1] = storage.readTextAsync('notes.txt'):await()
            results[#results + 1] = tostring(storage.writeJsonAsync('cache/world.json', {seed = 42}):await())
            results[#results + 1] = storage.readJsonAsync('cache/world.json'):await().seed
            results[#results + 1] = table.concat(storage.listAsync():await(), ',')
            results[#results + 1] = tostring(storage.removeAsync('notes.txt'):await())
            results[#results + 1] = select(2, storage.readTextAsync('notes.txt'):await())
            results[#results + 1] = select(2, storage.readTextAsync('../outside.txt'):await())
            results[#results + 1] = tostring(storage.writeSlotAsync('slot-1', {day = 3}, {day = 3}):await())
            results[#results + 1] = storage.readSlotAsync('slot-1'):await().day
            results[#results + 1] = storage.slotInfoAsync('slot-1'):await().summary.day
            results[#results + 1] = tostring(storage.readSlotAsync('none'):await()) .. ' ' .. tostring(storage.slotInfoAsync('none'):await())
            results[#results + 1] = #storage.listSlotsAsync():await()
            results[#results + 1] = select(2, storage.writeSlotAsync('bad name', {}):await())
            results[#results + 1] = tostring(storage.removeSlotAsync('slot-1'):await()) .. ' ' .. tostring(storage.removeSlotAsync('slot-1'):await())
            done = true
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(done)") == "true"; }));
    // clang-format off
    EXPECT_EQ(fixture.lua("return table.concat(results, ' | ')"), "second | true | 42 | cache/world.json,notes.txt | true | The storage file \"notes.txt\" was not found. | The path \"../outside.txt\" must stay inside its root folder. | "
        "true | 3 | 3 | nil nil | 1 | The save slot name \"bad name\" must use 1 to 64 letters, digits, dashes or underscores. | true false");
    // clang-format on
    EXPECT_EQ(fixture.host().getPersistCount(), persisted + 3) << "The slot write and both removals flush before they settle.";

    // Values become JSON on the frame thread, so a value that cannot raises at once.
    EXPECT_NE(fixture.lua("storage.writeJsonAsync('bad.json', {callback = print})").find("A function cannot be converted to JSON."), std::string::npos);
    EXPECT_NE(fixture.lua("storage.writeSlotAsync('slot', {}, {print})").find("cannot be converted to JSON"), std::string::npos);
    EXPECT_NE(fixture.lua("storage.readTextAsync()").find("error: "), std::string::npos);
}

TEST(StorageLuaTest, FinishesQueuedOperationsWhenTheAppStops) {
    test::EngineFixture fixture;
    fixture.runLua("local storage = require('haylen.storage') for index = 1, 50 do storage.writeSlotAsync('slot-' .. index, {index = index}) end");
    plugins::StoragePlugin& plugin = fixture.engine().getPlugin<plugins::StoragePlugin>();
    plugin.stop(fixture.engine());
    EXPECT_EQ(plugin.getSaveSlots().list().size(), 50U);
    EXPECT_EQ(plugin.getSaveSlots().read("slot-50"), (core::Json{{"index", 50}}));
}

TEST(StoragePluginTest, KeepsRunningOperationsAfterOneFails) {
    test::EngineFixture fixture;
    plugins::StoragePlugin& plugin = fixture.engine().getPlugin<plugins::StoragePlugin>();
    std::atomic<bool> ran = false;
    plugin.queueOperation([] { throw std::runtime_error("The operation failed on purpose."); });
    plugin.queueOperation([&ran] { ran = true; });
    ASSERT_TRUE(fixture.frameUntil([&] { return ran && fixture.engine().getError() != nullptr; }));
    EXPECT_NE(fixture.engine().getError()->getMessage().find("The operation failed on purpose."), std::string::npos);

    std::atomic<bool> later = false;
    plugin.queueOperation([&later] { later = true; });
    EXPECT_TRUE(fixture.frameUntil([&] { return later.load(); }));
}

TEST(StoragePluginTest, StartsWithDefaultPreferencesWhenTheFileIsDamaged) {
    test::EngineFixture fixture;
    fixture.engine().getStorage().writeText("preferences.json", "{");
    std::vector<std::string> warnings;
    std::mutex logMutex;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&](core::Log::Level level, std::string_view line) {
        const std::scoped_lock lock(logMutex);
        if (level == core::Log::Level::Warning) {
            warnings.emplace_back(line);
        }
    });
    // clang-format on
    fixture.restart();
    core::Log::removeListener(listener);

    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getPlugin<plugins::StoragePlugin>().getPreferences().getValues(), core::Json::object());
    EXPECT_TRUE(std::ranges::any_of(warnings, [](const std::string& line) { return line.find("The preferences file \"preferences.json\" is damaged. The app starts with default preferences, and the next save replaces the file.") != std::string::npos; }));
    fixture.runLua("local preferences = require('haylen.preferences') preferences.set('volume', 0.5) preferences.save()");
    EXPECT_EQ(fixture.engine().getStorage().readText("preferences.json"), "{\n  \"volume\": 0.5\n}");
}

// Saving on suspension never stops the app, so a file that cannot be written leaves the changes waiting for the next save.
TEST(StoragePluginTest, LogsPreferencesThatCannotBeSaved) {
    test::EngineFixture fixture;
    std::filesystem::create_directories(fixture.engine().getStorage().getRoot() / "preferences.json" / "blocked");
    std::vector<std::string> errors;
    std::mutex logMutex;
    // clang-format off
    const std::uint64_t listener = core::Log::addListener([&](core::Log::Level level, std::string_view line) {
        const std::scoped_lock lock(logMutex);
        if (level == core::Log::Level::Error) {
            errors.emplace_back(line);
        }
    });
    // clang-format on
    fixture.runLua("require('haylen.preferences').set('volume', 0.5)");
    fixture.engine().handleEvent({.type = platform::Event::Type::Suspended});
    core::Log::removeListener(listener);

    EXPECT_TRUE(std::ranges::any_of(errors, [](const std::string& line) { return line.find("The preferences could not be saved: The storage file \"preferences.json\" could not be written.") != std::string::npos; }));
    EXPECT_TRUE(fixture.engine().getPlugin<plugins::StoragePlugin>().getPreferences().isDirty());
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(StoragePluginTest, RequiresAStartedEngine) {
    plugins::StoragePlugin plugin;
    EXPECT_THROW((void)plugin.getSaveSlots(), std::logic_error);
    EXPECT_THROW((void)plugin.getPreferences(), std::logic_error);
    EXPECT_THROW(plugin.queueOperation([] {}), std::logic_error);
}

TEST(UserStorageTest, ReadsAndWritesOfTheSameFileOnTwoThreadsNeverMix) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    const std::string first(1U << 20U, 'a');
    const std::string second(1U << 20U, 'b');
    std::string failure;
    // clang-format off
    std::thread writer([&] {
        try {
            for (int round = 0; round < 20; ++round) {
                storage.writeText("shared.bin", first);
            }
        } catch (const std::exception& error) {
            failure = error.what();
        }
    });
    // clang-format on
    for (int round = 0; round < 20; ++round) {
        storage.writeText("shared.bin", second);
        const std::string stored = storage.readText("shared.bin");
        EXPECT_TRUE(stored == first || stored == second);
    }
    writer.join();
    EXPECT_EQ(failure, "");
    EXPECT_EQ(storage.list(""), (std::vector<std::string>{"shared.bin"}));
}

// Windows refuses to replace a file that another program holds open, as a virus scanner does for a moment after a file changes, so a write waits for the program to let go.
TEST(UserStorageTest, ReplacesAFileThatAnotherProgramHoldsForAMoment) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    storage.writeText("save.json", "{}");
    std::atomic<bool> holding = false;
    // clang-format off
    std::thread scanner([&] {
        const std::ifstream held(directory.getPath() / "save.json", std::ios::binary);
        holding = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
    // clang-format on
    while (!holding) {
        std::this_thread::yield();
    }
    storage.writeText("save.json", R"({"level": 2})");
    scanner.join();
    EXPECT_EQ(storage.readText("save.json"), R"({"level": 2})");
}

} // namespace haylen::storage
