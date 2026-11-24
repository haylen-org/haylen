#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <limits>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "haylen/storage/Preferences.hpp"
#include "haylen/storage/ResumeState.hpp"
#include "haylen/storage/SaveSlots.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

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
    // The extension of the temporary files of writes is the storage's own, so a file named with it would vanish when the storage opens.
    EXPECT_THROW(storage.writeText("cache/data.tmp", "x"), std::invalid_argument);

    storage.flush();
    EXPECT_EQ(persisted, 1);
    EXPECT_EQ(storage.getRoot(), directory.getPath() / "saves");

    UserStorage desktop(directory.getPath() / "desktop");
    desktop.flush();
    EXPECT_EQ(persisted, 1);
}

TEST(UserStorageTest, RefusesEveryCallWhileThePlatformKeepsTheUserData) {
    const test::TemporaryDirectory directory;
    const std::string refusal = "Another tab of this site uses its user data, so this page keeps none.";
    int persisted = 0;
    UserStorage storage(directory.getPath() / "saves", [&] { ++persisted; }, refusal);
    EXPECT_FALSE(storage.isAvailable());

    std::vector<std::string> failures;
    // clang-format off
    const auto attempt = [&failures](const std::function<void()>& call) {
        try {
            call();
        } catch (const std::runtime_error& error) {
            failures.emplace_back(error.what());
        }
    };
    // clang-format on
    attempt([&] { storage.writeText("note.txt", "kept"); });
    attempt([&] { (void)storage.readText("note.txt"); });
    attempt([&] { (void)storage.exists("note.txt"); });
    attempt([&] { (void)storage.list(""); });
    attempt([&] { (void)storage.remove("note.txt"); });
    attempt([&] { storage.flush(); });
    attempt([&] { (void)storage.getRoot(); });
    EXPECT_EQ(failures, std::vector<std::string>(7, refusal));
    EXPECT_EQ(persisted, 0);
    EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "saves"));
}

TEST(UserStorageTest, RemovesStaleTemporaryFilesWhenItOpens) {
    const test::TemporaryDirectory directory;
    {
        UserStorage storage(directory.getPath());
        storage.writeText("saves/kept.json", "kept", UserStorage::Durability::Durable);
    }
    test::TestFiles::write(directory.getPath() / "saves" / "kept.json.7.tmp", "interrupted");
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
    saves.write("slot-2", haylen::core::Json::array({1, 2}));
    storage.writeText("saves/readme.txt", "not a save");
    storage.writeText("saves/bad name.json", "{}");

    EXPECT_EQ(saves.read("slot-1"), (haylen::core::Json{{"day", 3}, {"wood", 12}}));
    EXPECT_EQ(saves.read("slot-2"), haylen::core::Json::array({1, 2}));
    EXPECT_EQ(saves.read("empty"), std::nullopt);
    const std::optional<SaveSlots::Info> first = saves.getInfo("slot-1");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->savedAt, 100);
    EXPECT_EQ(first->summary, (haylen::core::Json{{"day", 3}}));
    EXPECT_EQ(saves.getInfo("empty"), std::nullopt);
    EXPECT_TRUE(saves.exists("auto_save"));

    const std::vector<SaveSlots::Info> listed = saves.list();
    ASSERT_EQ(listed.size(), 3U);
    EXPECT_EQ(listed[0].slot, "auto_save");
    EXPECT_EQ(listed[1].slot, "slot-2");
    EXPECT_EQ(listed[2].slot, "slot-1");
    EXPECT_EQ(listed[0].summary, haylen::core::Json::object());

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
        EXPECT_THROW(saves.write(name, haylen::core::Json::object()), std::invalid_argument) << name;
    }

    EXPECT_THROW(saves.write("slot", haylen::core::Json::object(), haylen::core::Json::array()), std::invalid_argument);

    storage.writeText("saves/broken.json", "{");
    storage.writeText("saves/partial.json", R"({"data": 1})");
    storage.writeText("saves/dated.json", R"({"data": 1, "summary": {}, "savedAt": "yesterday"})");
    storage.writeText("saves/summed.json", R"({"data": 1, "summary": [1], "savedAt": 5})");
    EXPECT_THROW((void)saves.read("broken"), std::runtime_error);
    EXPECT_THROW((void)saves.getInfo("partial"), std::runtime_error);
    EXPECT_THROW((void)saves.getInfo("dated"), std::runtime_error);
    EXPECT_THROW((void)saves.read("summed"), std::runtime_error);

    saves.write("real", {{"ok", true}});
    const std::optional<SaveSlots::Info> real = saves.getInfo("real");
    ASSERT_TRUE(real.has_value());
    EXPECT_GT(real->savedAt, 1'700'000'000);
}

TEST(SaveSlotsTest, MigratesOldSavesOnRead) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);
    saves.write("slot-1", {{"items", haylen::core::Json::array({"axe"})}});
    const std::optional<SaveSlots::Info> written = saves.getInfo("slot-1");
    ASSERT_TRUE(written.has_value());
    EXPECT_EQ(written->version, 1);
    EXPECT_EQ(written->revision, 1U);

    // The migrations of each version run in order, and a read never changes the stored slot.
    saves.setVersion(3);
    std::vector<int> ran;
    // clang-format off
    saves.addMigration(2, [&ran](haylen::core::Json data) {
        ran.push_back(2);
        data["stamina"] = 100;
        return data;
    });
    saves.addMigration(1, [&ran](haylen::core::Json data) {
        ran.push_back(1);
        data["inventory"] = data.at("items");
        data.erase("items");
        return data;
    });
    // clang-format on
    EXPECT_EQ(saves.read("slot-1"), (haylen::core::Json{{"inventory", haylen::core::Json::array({"axe"})}, {"stamina", 100}}));
    EXPECT_EQ(ran, (std::vector<int>{1, 2}));
    const std::optional<SaveSlots::Info> unchanged = saves.getInfo("slot-1");
    ASSERT_TRUE(unchanged.has_value());
    EXPECT_EQ(unchanged->version, 1);
    saves.write("slot-2", {{"stamina", 5}});
    const std::optional<SaveSlots::Info> migrated = saves.getInfo("slot-2");
    ASSERT_TRUE(migrated.has_value());
    EXPECT_EQ(migrated->version, 3);

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
    EXPECT_THROW(saves.addMigration(0, [](haylen::core::Json data) { return data; }), std::invalid_argument);
}

TEST(SaveSlotsTest, FallsBackToTheBackupWhenDamaged) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);
    saves.write("slot-1", {{"day", 1}});
    saves.write("slot-1", {{"day", 2}});
    const std::optional<SaveSlots::Info> sound = saves.getInfo("slot-1");
    ASSERT_TRUE(sound.has_value());
    EXPECT_EQ(sound->revision, 2U);
    EXPECT_FALSE(sound->recovered);

    // A change that the checksum does not cover, such as a bit that flipped on the disk, makes the backup take the place of the slot.
    std::string text = storage.readText("saves/slot-1.json");
    text.replace(text.find("\"day\":2"), 7, "\"day\":9");
    storage.writeText("saves/slot-1.json", text);
    EXPECT_EQ(saves.read("slot-1"), (haylen::core::Json{{"day", 1}}));
    const std::optional<SaveSlots::Info> recovered = saves.getInfo("slot-1");
    ASSERT_TRUE(recovered.has_value());
    EXPECT_TRUE(recovered->recovered);
    EXPECT_EQ(recovered->revision, 1U);
    EXPECT_EQ(saves.list().size(), 1U) << "A backup is no slot of its own.";

    // A write over a damaged slot keeps the sound backup, and a slot without a sound copy fails.
    saves.write("slot-1", {{"day", 3}});
    const std::optional<SaveSlots::Info> rewritten = saves.getInfo("slot-1");
    ASSERT_TRUE(rewritten.has_value());
    EXPECT_EQ(rewritten->revision, 1U);
    storage.writeText("saves/slot-1.json", "{");
    EXPECT_EQ(saves.read("slot-1"), (haylen::core::Json{{"day", 1}}));
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
    const std::optional<std::string> older = phone.exportSlot("slot");
    const std::optional<std::string> newer = tablet.exportSlot("slot");
    ASSERT_TRUE(older.has_value());
    ASSERT_TRUE(newer.has_value());
    EXPECT_FALSE(tablet.importSlot("slot", *older, SaveSlots::ImportPolicy::KeepNewer));
    EXPECT_EQ(tablet.read("slot"), (haylen::core::Json{{"day", 5}}));
    EXPECT_TRUE(phone.importSlot("slot", *newer, SaveSlots::ImportPolicy::KeepNewer));
    EXPECT_EQ(phone.read("slot"), (haylen::core::Json{{"day", 5}}));
    EXPECT_TRUE(tablet.importSlot("slot", *older, SaveSlots::ImportPolicy::Replace));
    const std::optional<SaveSlots::Info> imported = tablet.getInfo("slot");
    ASSERT_TRUE(imported.has_value());
    EXPECT_EQ(imported->revision, 1U);
    EXPECT_TRUE(phone.importSlot("fresh", *older, SaveSlots::ImportPolicy::KeepNewer));
    EXPECT_FALSE(phone.exportSlot("none").has_value());
    EXPECT_THROW((void)phone.importSlot("slot", "{}", SaveSlots::ImportPolicy::Replace), std::invalid_argument);
    EXPECT_EQ(SaveSlots::importPolicyFromName("keepNewer"), SaveSlots::ImportPolicy::KeepNewer);
    EXPECT_EQ(SaveSlots::importPolicyName(SaveSlots::ImportPolicy::Replace), "replace");
}

// The checksum of a slot finds damage, not intent, so anyone can sign bytes that hold any field.
class SaveSlotsForgeryTest : public ::testing::Test {
  protected:
    // Returns the bytes of a slot whose checksum holds, computed like the checksum that a write stores.
    [[nodiscard]] static std::string sign(haylen::core::Json saved) {
        std::uint64_t hash = 14695981039346656037ULL;
        for (const char character : saved.dump()) {
            hash = (hash ^ static_cast<unsigned char>(character)) * 1099511628211ULL;
        }

        saved["checksum"] = std::format("{:016x}", hash);
        return saved.dump();
    }
};

TEST_F(SaveSlotsForgeryTest, RefusesRevisionsAndVersionsThatNoWriteStores) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);
    const haylen::core::Json sound{{"data", 1}, {"summary", haylen::core::Json::object()}, {"savedAt", 5}, {"version", 1}, {"revision", 1}};
    EXPECT_TRUE(saves.importSlot("sound", sign(sound), SaveSlots::ImportPolicy::Replace));

    haylen::core::Json negative = sound;
    negative["revision"] = -1;
    haylen::core::Json huge = sound;
    huge["version"] = std::int64_t{1} << 40;
    for (const haylen::core::Json& forged : {negative, huge}) {
        try {
            (void)saves.importSlot("slot", sign(forged), SaveSlots::ImportPolicy::Replace);
            ADD_FAILURE() << "A slot of " << forged.dump() << " must be refused.";
        } catch (const std::invalid_argument& error) {
            EXPECT_STREQ(error.what(), "The imported save slot \"slot\" is damaged.");
        }
    }
}

// Each write reads the revision that the write before it stored, so writes of one slot on two threads lose none.
TEST(SaveSlotsTest, CountsEveryWriteOfOneSlotFromTwoThreads) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    SaveSlots saves(storage);
    constexpr int kWrites = 25;
    std::string failure;
    // clang-format off
    std::thread other([&] {
        try {
            for (int round = 0; round < kWrites; ++round) {
                saves.write("shared", {{"thread", "other"}});
            }
        } catch (const std::exception& error) {
            failure = error.what();
        }
    });
    // clang-format on
    for (int round = 0; round < kWrites; ++round) {
        saves.write("shared", {{"thread", "main"}});
    }

    other.join();
    EXPECT_EQ(failure, "");
    const std::optional<SaveSlots::Info> shared = saves.getInfo("shared");
    ASSERT_TRUE(shared.has_value());
    EXPECT_EQ(shared->revision, static_cast<std::uint64_t>(kWrites * 2));
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
    EXPECT_EQ(saves.read("locked"), (haylen::core::Json{{"day", 1}}));
}

TEST(PreferencesTest, StoresDottedKeysAndPersists) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    Preferences preferences(storage);
    preferences.load();
    EXPECT_FALSE(preferences.isDirty());
    EXPECT_EQ(preferences.getValues(), haylen::core::Json::object());

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
    EXPECT_EQ(reloaded.getValues(), (haylen::core::Json{{"audio", {{"volume", {{"music", 0.5}}}}}, {"language", "pt-BR"}}));
    reloaded.clear();
    EXPECT_TRUE(reloaded.isDirty());
    EXPECT_EQ(reloaded.getValues(), haylen::core::Json::object());

    storage.writeText("preferences.json", "[1, 2]");
    EXPECT_THROW(reloaded.load(), std::runtime_error);
    EXPECT_EQ(reloaded.getValues(), haylen::core::Json::object());
}

TEST(PreferencesTest, RefusesValuesThatJsonCannotHold) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    Preferences preferences(storage);
    EXPECT_THROW(preferences.set("name", std::string("\xFF")), std::invalid_argument);
    EXPECT_THROW(preferences.set("\xFF", 1), std::invalid_argument);
    EXPECT_THROW(preferences.set("list", haylen::core::Json::array({1.0, std::numeric_limits<double>::infinity()})), std::invalid_argument);
    EXPECT_THROW(preferences.set("group", haylen::core::Json{{"\xFF", 1}}), std::invalid_argument);
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

TEST(ResumeStateTest, ReachesOnlyTheLaunchAfterTheBackground) {
    const test::TemporaryDirectory directory;
    UserStorage storage(directory.getPath());
    ResumeState state(storage);
    state.load();
    EXPECT_FALSE(state.getRestored());

    state.set(haylen::core::Json{{"screen", "map"}, {"day", 4}});
    state.save();
    ASSERT_TRUE(storage.exists("resume-state.json"));
    state.discard();
    EXPECT_FALSE(storage.exists("resume-state.json"));

    state.save();
    ResumeState next(storage);
    next.load();
    ASSERT_TRUE(next.getRestored());
    EXPECT_EQ(next.getRestored()->value, (haylen::core::Json{{"screen", "map"}, {"day", 4}}));
    EXPECT_GE(next.getRestored()->age, 0.0);
    EXPECT_LT(next.getRestored()->age, 60.0);
    EXPECT_FALSE(storage.exists("resume-state.json"));

    state.set(nullptr);
    state.save();
    EXPECT_FALSE(storage.exists("resume-state.json"));
    EXPECT_THROW(state.set(std::string(1, '\xFF')), std::invalid_argument);

    storage.writeText("resume-state.json", "{\"value\": 1}");
    EXPECT_THROW(next.load(), std::runtime_error);
    EXPECT_FALSE(next.getRestored());
    EXPECT_FALSE(storage.exists("resume-state.json"));
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
    std::atomic<bool> held = false;
    // clang-format off
    std::thread scanner([&] {
        const std::ifstream file(directory.getPath() / "save.json", std::ios::binary);
        held = file.is_open();
        holding = true;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    });
    // clang-format on
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (!holding && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::yield();
    }

    if (!held) {
        scanner.join();
        FAIL() << "The other program could not open the file in time.";
    }

    storage.writeText("save.json", R"({"level": 2})");
    scanner.join();
    EXPECT_EQ(storage.readText("save.json"), R"({"level": 2})");
}

} // namespace haylen::storage
