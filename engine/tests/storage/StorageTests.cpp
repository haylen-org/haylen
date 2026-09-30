#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "haylen/core/Engine.hpp"
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
    // clang-format off
    std::thread writer([&] {
        for (int round = 0; round < 20; ++round) {
            storage.writeText("shared.bin", first);
        }
    });
    // clang-format on
    for (int round = 0; round < 20; ++round) {
        storage.writeText("shared.bin", second);
        const std::string stored = storage.readText("shared.bin");
        EXPECT_TRUE(stored == first || stored == second);
    }
    writer.join();
    EXPECT_EQ(storage.list(""), (std::vector<std::string>{"shared.bin"}));
}

} // namespace haylen::storage
