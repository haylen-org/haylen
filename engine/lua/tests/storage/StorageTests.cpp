#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Log.hpp"
#include "haylen/platform/Event.hpp"
#include "haylen/plugins/StoragePlugin.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::storage {

TEST(StorageLuaTest, ReadsAndWritesUserFiles) {
    test::LuaFixture fixture;
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
    test::LuaFixture fixture;
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
    test::LuaFixture fixture;
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
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Suspended});
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

// An app whose platform keeps the user data from it, such as on a web page whose user data another tab holds, starts and runs, while every call of its storage fails with the sentence of the platform and nothing reaches the folder.
TEST(StorageLuaTest, RefusesTheUserDataThatThePlatformKeepsFromTheApp) {
    const std::string refusal = "Another tab of this site uses its user data, so this page keeps no user data. Close the other tab and load this page again.";
    test::LuaFixture fixture;
    const std::filesystem::path folder = fixture.engine().getStorage().getRoot();
    std::filesystem::remove_all(folder);
    fixture.host().setUserDataRefusal(refusal);
    fixture.restart();
    fixture.runLua("storage = require('haylen.storage') preferences = require('haylen.preferences')");
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_FALSE(fixture.engine().getStorage().isAvailable());

    for (const std::string& call : {"storage.writeText('note.txt', 'kept')", "storage.readText('note.txt')", "storage.exists('note.txt')", "storage.list()", "storage.remove('note.txt')", "storage.flush()", "storage.root()", "storage.writeSlot('one', {day = 1})"}) {
        SCOPED_TRACE(call);
        const std::string answer = fixture.lua("local ok, failure = pcall(function() return " + call + " end) return tostring(ok) .. ' ' .. tostring(failure)");
        EXPECT_TRUE(answer.starts_with("false ") && answer.find(refusal) != std::string::npos) << answer;
    }

    fixture.runLua("preferences.set('volume', 0.5)");
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Suspended});
    fixture.frames(1);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_FALSE(std::filesystem::exists(folder));
}

TEST(StorageLuaTest, SavesWhatTheListenersOfTheBackgroundChange) {
    test::LuaFixture fixture;
    fixture.runLua("local preferences = require('haylen.preferences') require('haylen.events').on('appBackground', function() preferences.set('session.screen', 'map') end)");
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Suspended});
    ASSERT_TRUE(fixture.engine().getStorage().exists("preferences.json"));
    EXPECT_NE(fixture.engine().getStorage().readText("preferences.json").find("\"screen\": \"map\""), std::string::npos);
}

TEST(StorageLuaTest, KeepsTheResumeStateForALaunchAfterTheBackground) {
    test::LuaFixture fixture;
    fixture.runLua("storage = require('haylen.storage') storage.setResumeState({screen = 'shop', items = {3, 5}})");
    EXPECT_EQ(fixture.lua("return select('#', storage.resumeState())"), "0");

    // The state reaches the disk in the background and leaves it when the app comes back.
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Suspended});
    EXPECT_NE(fixture.engine().getStorage().readText("resume-state.json").find("\"shop\""), std::string::npos);
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Resumed});
    EXPECT_FALSE(fixture.engine().getStorage().exists("resume-state.json"));

    // A file the system left when it ended the app reaches the next launch, which consumes it.
    fixture.engine().getStorage().writeText("resume-state.json", R"({"savedAt": 0, "value": {"screen": "map"}})");
    fixture.restart();
    EXPECT_EQ(fixture.lua("local value, age = require('haylen.storage').resumeState() return value.screen .. ' ' .. tostring(age > 1000000)"), "map true");
    EXPECT_FALSE(fixture.engine().getStorage().exists("resume-state.json"));
    EXPECT_NE(fixture.lua("require('haylen.storage').setResumeState(function() end)").find("error: "), std::string::npos);
    EXPECT_EQ(fixture.lua("require('haylen.storage').setResumeState(nil) return 'ok'"), "ok");
}

TEST(StorageLuaTest, RefusesValuesThatJsonCannotHold) {
    test::LuaFixture fixture;
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
    test::LuaFixture fixture;
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
    test::LuaFixture fixture;
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
    EXPECT_NE(fixture.lua("storage.setSaveVersion(2^32 + 3)").find("bad argument #1 to 'setSaveVersion' (integer out of range)"), std::string::npos);
    EXPECT_EQ(fixture.lua("return storage.saveVersion()"), "2");
    EXPECT_NE(fixture.lua("storage.addMigration(0, print)").find("A save migration needs a version from 1 and a function."), std::string::npos);
}

TEST(StorageLuaTest, RejectsAnAsynchronousReadWhoseVersionOrMigrationFails) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        storage = require('haylen.storage')
        storage.writeSlot('old', {gold = 5})
        storage.setSaveVersion(3)
        storage.writeSlot('newer', {gold = 9})
        storage.setSaveVersion(2)
        storage.addMigration(1, function(data) error('The gold of the old save cannot be read.', 0) end)
        results = {}
        require('async').spawn(function()
            local data, message = storage.readSlotAsync('old'):await()
            results[#results + 1] = tostring(data) .. ' ' .. message
            data, message = storage.readSlotAsync('newer'):await()
            results[#results + 1] = tostring(data) .. ' ' .. message
            done = true
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(done)") == "true"; })) << "The coroutine that awaits the failed reads goes on.";
    EXPECT_EQ(fixture.lua("return results[1]"), "nil The gold of the old save cannot be read.");
    EXPECT_NE(fixture.lua("return results[2]").find("nil The save slot \"newer\" has version 3, which is newer than the version 2 this app reads."), std::string::npos);
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(StorageLuaTest, RunsAsynchronousOperationsInOrderOnTheIoPool) {
    test::LuaFixture fixture;
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

// The event tells what the write stored, even when an operation queued after it changed the slot before the frame thread heard of it.
TEST(StorageLuaTest, ReportsTheRevisionThatAnAsynchronousWriteStored) {
    test::LuaFixture fixture;
    // clang-format off
    fixture.runLua(R"(
        storage = require('haylen.storage')
        require('haylen.events').on('saveSlotWritten', function(info) written = info.slot .. ':' .. info.revision .. ':' .. info.version end)
        require('async').spawn(function()
            local wrote = storage.writeSlotAsync('a', {day = 1})
            local removed = storage.removeSlotAsync('a')
            results = tostring(wrote:await()) .. ' ' .. tostring(removed:await())
        end)
    )");
    // clang-format on
    ASSERT_TRUE(fixture.frameUntil([&] { return fixture.lua("return tostring(results)") != "nil"; }));
    EXPECT_EQ(fixture.lua("return results .. ' ' .. written"), "true true a:1:1");
}

TEST(StorageLuaTest, FinishesQueuedOperationsWhenTheAppStops) {
    test::LuaFixture fixture;
    fixture.runLua("local storage = require('haylen.storage') for index = 1, 50 do storage.writeSlotAsync('slot-' .. index, {index = index}) end");
    haylen::plugins::StoragePlugin& plugin = fixture.engine().getPlugin<haylen::plugins::StoragePlugin>();
    plugin.stop(fixture.engine());
    EXPECT_EQ(plugin.getSaveSlots().list().size(), 50U);
    EXPECT_EQ(plugin.getSaveSlots().read("slot-50"), (haylen::core::Json{{"index", 50}}));
}

TEST(StoragePluginTest, KeepsRunningOperationsAfterOneFails) {
    test::LuaFixture fixture;
    haylen::plugins::StoragePlugin& plugin = fixture.engine().getPlugin<haylen::plugins::StoragePlugin>();
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
    test::LuaFixture fixture;
    fixture.engine().getStorage().writeText("preferences.json", "{");
    std::vector<std::string> warnings;
    std::mutex logMutex;
    // clang-format off
    const std::uint64_t listener = haylen::core::Log::addListener([&](haylen::core::Log::Level level, std::string_view line) {
        const std::scoped_lock lock(logMutex);
        if (level == haylen::core::Log::Level::Warning) {
            warnings.emplace_back(line);
        }
    });
    // clang-format on
    fixture.restart();
    haylen::core::Log::removeListener(listener);

    EXPECT_EQ(fixture.engine().getError(), nullptr);
    EXPECT_EQ(fixture.engine().getPlugin<haylen::plugins::StoragePlugin>().getPreferences().getValues(), haylen::core::Json::object());
    EXPECT_TRUE(std::ranges::any_of(warnings, [](const std::string& line) { return line.find("The preferences file \"preferences.json\" is damaged. The app starts with default preferences, and the next save replaces the file.") != std::string::npos; }));
    fixture.runLua("local preferences = require('haylen.preferences') preferences.set('volume', 0.5) preferences.save()");
    EXPECT_EQ(fixture.engine().getStorage().readText("preferences.json"), "{\n  \"volume\": 0.5\n}");
}

// Saving on suspension never stops the app, so a file that cannot be written leaves the changes waiting for the next save.
TEST(StoragePluginTest, LogsPreferencesThatCannotBeSaved) {
    test::LuaFixture fixture;
    std::filesystem::create_directories(fixture.engine().getStorage().getRoot() / "preferences.json" / "blocked");
    std::vector<std::string> errors;
    std::mutex logMutex;
    // clang-format off
    const std::uint64_t listener = haylen::core::Log::addListener([&](haylen::core::Log::Level level, std::string_view line) {
        const std::scoped_lock lock(logMutex);
        if (level == haylen::core::Log::Level::Error) {
            errors.emplace_back(line);
        }
    });
    // clang-format on
    fixture.runLua("require('haylen.preferences').set('volume', 0.5)");
    fixture.engine().handleEvent({.type = haylen::platform::Event::Type::Suspended});
    haylen::core::Log::removeListener(listener);

    EXPECT_TRUE(std::ranges::any_of(errors, [](const std::string& line) { return line.find("The preferences could not be saved: The storage file \"preferences.json\" could not be written.") != std::string::npos; }));
    EXPECT_TRUE(fixture.engine().getPlugin<haylen::plugins::StoragePlugin>().getPreferences().isDirty());
    EXPECT_EQ(fixture.engine().getError(), nullptr);
}

TEST(StoragePluginTest, RequiresAStartedEngine) {
    haylen::plugins::StoragePlugin plugin;
    EXPECT_THROW((void)plugin.getSaveSlots(), std::logic_error);
    EXPECT_THROW((void)plugin.getPreferences(), std::logic_error);
    EXPECT_THROW(plugin.queueOperation([] {}), std::logic_error);
}

} // namespace haylen::lua::storage
