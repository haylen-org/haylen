#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#include "debug/PackageEditTest.hpp"
#include "haylen/io/PackageWatcher.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::debug {

class PackageWatcherTest : public PackageEditTest {
  protected:
    // The writer of a test stops at this bound even when the test never tells it to, so a test that fails early never hangs on its thread.
    static constexpr std::chrono::seconds kWriterBound{10};
};

TEST_F(PackageWatcherTest, ReportsAddedChangedAndRemovedFiles) {
    const test::TemporaryDirectory directory;
    directory.write("haylen.json", "{}");
    directory.write("source/main.lua", "print(1)");
    directory.write("content/a.json", "{}");
    directory.write("native/android/App.kt", "");
    directory.write("plugins/ads/plugin.json", "{}");
    directory.write("plugins/ads/source/init.lua", "return {}");
    directory.write("plugins/ads/android/build.gradle.kts", "");
    io::PackageWatcher watcher(directory.getPath());
    EXPECT_TRUE(watcher.scan().empty());

    // Only `haylen.json`, `source`, `content` and the manifests and Lua modules of plugins belong to the package, so files next to them and the native parts of plugins never count.
    directory.write("content/b.json", "[]");
    directory.write("README.md", "# Notes");
    directory.write("native/web/app.js", "");
    directory.write("plugins/README.md", "# Plugins");
    directory.write("plugins/ads/android/src/main/AndroidManifest.xml", "");
    directory.write("plugins/ads/source/banner.lua", "return {}");
    touch(directory.getPath() / "source/main.lua", 5);
    touch(directory.getPath() / "haylen.json", 5);
    touch(directory.getPath() / "plugins/ads/plugin.json", 5);
    ASSERT_TRUE(std::filesystem::remove(directory.getPath() / "content/a.json"));
    EXPECT_EQ(watcher.scan(), (std::vector<std::string>{"content/a.json", "content/b.json", "haylen.json", "plugins/ads/plugin.json", "plugins/ads/source/banner.lua", "source/main.lua"}));
    EXPECT_TRUE(watcher.scan().empty());
}

TEST_F(PackageWatcherTest, IgnoresHiddenBackupAndNonLuaSourceFiles) {
    const test::TemporaryDirectory directory;
    directory.write("haylen.json", "{}");
    directory.write("source/main.lua", "");
    directory.write("plugins/ads/plugin.json", "{}");
    io::PackageWatcher watcher(directory.getPath());

    // Editors write swap, backup and lock files next to the file they save, which must never restart the app.
    directory.write("source/.level.lua.swp", "swap");
    directory.write("source/level.lua~", "backup");
    directory.write("source/#level.lua#", "autosave");
    directory.write("source/level.lua.orig", "merge");
    directory.write("source/notes.txt", "notes");
    directory.write("source/.cache/level.lua", "cache");
    directory.write("content/.cache", "cache");
    directory.write("content/hero.png.tmp", "half");
    directory.write("plugins/ads/source/notes.md", "notes");
    directory.write("source/level.lua", "return {}");
    directory.write("content/hero.png", "png");
    directory.write("plugins/ads/source/banner.lua", "return {}");
    EXPECT_EQ(watcher.scan(), (std::vector<std::string>{"content/hero.png", "plugins/ads/source/banner.lua", "source/level.lua"}));

    EXPECT_TRUE(io::PackageWatcher::isWatched("haylen.json"));
    EXPECT_TRUE(io::PackageWatcher::isWatched("plugins/ads/plugin.json"));
    EXPECT_FALSE(io::PackageWatcher::isWatched("plugins/ads/android/build.gradle.kts"));
    EXPECT_FALSE(io::PackageWatcher::isWatched("plugins/README.md"));
    EXPECT_FALSE(io::PackageWatcher::isWatched("native/web/app.js"));
    EXPECT_FALSE(io::PackageWatcher::isWatched("source/.level.lua.swp"));
}

TEST_F(PackageWatcherTest, WaitsForFilesToSettle) {
    const test::TemporaryDirectory directory;
    directory.write("content/level.json", "{}");
    io::PackageWatcher watcher(directory.getPath());

    // A file that grows while the scan looks at it is still being written, so the scan leaves it for later, whether it changes or is new. The writer appends to the files, since a file truncated and written again can make the file system flush it when it closes and hold the writer up for longer than the scan waits.
    std::atomic<bool> writing = true;
    // clang-format off
    std::thread writer([&directory, &writing] {
        const auto deadline = std::chrono::steady_clock::now() + kWriterBound;
        while (writing && std::chrono::steady_clock::now() < deadline) {
            for (const char* file : {"content/level.json", "content/added.json"}) {
                std::ofstream stream(directory.getPath() / file, std::ios::app);
                stream << ' ';
                if (!stream) {
                    ADD_FAILURE() << "The writer could not append to \"" << file << "\".";
                    return;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    // clang-format on
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const std::vector<std::string> busy = watcher.scan();
    writing = false;
    writer.join();
    EXPECT_TRUE(busy.empty());
    EXPECT_EQ(watcher.scan(), (std::vector<std::string>{"content/added.json", "content/level.json"}));
    EXPECT_TRUE(watcher.scan().empty());
}

} // namespace haylen::debug
