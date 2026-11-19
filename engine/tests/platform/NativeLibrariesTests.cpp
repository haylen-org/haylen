#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/platform/NativeLibraries.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

class NativeLibrariesTest : public ::testing::Test {
  protected:
    static int linkedTwice(int value) {
        return value * 2;
    }
};

TEST_F(NativeLibrariesTest, NamesTheFilesOfALibraryOnThePlatform) {
    EXPECT_EQ(NativeLibraries::getFileNames("game_platform.dll"), (std::vector<std::string>{"game_platform.dll"}));
    EXPECT_EQ(NativeLibraries::getFileNames("libgame_platform.so.1"), (std::vector<std::string>{"libgame_platform.so.1"}));
#if defined(_WIN32)
    EXPECT_EQ(NativeLibraries::getFileNames("native-test"), (std::vector<std::string>{"native-test.dll"}));
#elif defined(__APPLE__)
    EXPECT_EQ(NativeLibraries::getFileNames("native-test"), (std::vector<std::string>{"libnative-test.dylib", "native-test.framework/native-test"}));
#else
    EXPECT_EQ(NativeLibraries::getFileNames("native-test"), (std::vector<std::string>{"libnative-test.so"}));
#endif
    EXPECT_TRUE(NativeLibraries::isAvailable());
}

TEST_F(NativeLibrariesTest, LoadsByPathAndByNameNextToTheExecutable) {
    const NativeLibraries::Library byPath = NativeLibraries::open(HAYLEN_NATIVE_TEST_LIBRARY);
    ASSERT_NE(byPath.handle, nullptr);
    EXPECT_FALSE(byPath.linked);
    auto* add = reinterpret_cast<std::int32_t (*)(std::int32_t, std::int32_t)>(NativeLibraries::findSymbol(byPath, "native_test_add"));
    ASSERT_NE(add, nullptr);
    EXPECT_EQ(add(20, 22), 42);
    EXPECT_EQ(NativeLibraries::findSymbol(byPath, "native_test_missing"), nullptr);

    // The tests run next to the library, which is where a packaged app keeps its libraries on desktop platforms.
    const NativeLibraries::Library byName = NativeLibraries::open("native-test");
    EXPECT_EQ(byName.handle, byPath.handle);
    EXPECT_EQ(std::filesystem::path(byName.path).filename(), std::filesystem::path(HAYLEN_NATIVE_TEST_LIBRARY).filename());
    EXPECT_EQ(NativeLibraries::findSymbol("native_test_add"), reinterpret_cast<void*>(add));
    EXPECT_EQ(NativeLibraries::findSymbol("native_test_nowhere"), nullptr);
}

#if defined(HAYLEN_PLAYER_FOLDER)
// The tests of native libraries in the test project load the test library by name in both players, which look for it next to themselves.
TEST_F(NativeLibrariesTest, SitsNextToBothPlayers) {
    const std::filesystem::path library = std::filesystem::path(HAYLEN_NATIVE_TEST_LIBRARY).filename();
    for (const std::filesystem::path folder : {HAYLEN_PLAYER_FOLDER, HAYLEN_HEADLESS_PLAYER_FOLDER}) {
        EXPECT_TRUE(std::filesystem::is_regular_file(folder / library)) << folder;
    }
}
#endif

TEST_F(NativeLibrariesTest, SearchesAddedFoldersBeforeThePlatformFolders) {
    const test::TemporaryDirectory folder;
    const std::filesystem::path library(HAYLEN_NATIVE_TEST_LIBRARY);
    const std::vector<std::string> names = NativeLibraries::getFileNames("native_copy");
    ASSERT_FALSE(names.empty());
    const std::string copy = names.front();
    std::filesystem::copy_file(library, folder.getPath() / copy);

    EXPECT_THROW((void)NativeLibraries::open("native_copy"), std::runtime_error);
    NativeLibraries::addSearchFolder(folder.getPath());
    const NativeLibraries::Library loaded = NativeLibraries::open("native_copy");
    EXPECT_NE(loaded.handle, nullptr);
    EXPECT_NE(NativeLibraries::findSymbol(loaded, "native_test_checksum"), nullptr);
}

TEST_F(NativeLibrariesTest, ListsEveryPlaceItSearched) {
    try {
        (void)NativeLibraries::open("native_nowhere");
        FAIL() << "A missing library loads.";
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        EXPECT_NE(message.find("The native library \"native_nowhere\" could not be loaded. These are the places it searched:"), std::string::npos);
        EXPECT_NE(message.find(std::filesystem::path(HAYLEN_NATIVE_TEST_LIBRARY).make_preferred().parent_path().string()), std::string::npos);
        EXPECT_NE(message.find("not found"), std::string::npos);
        EXPECT_NE(message.find("The libraries linked into the app: not registered"), std::string::npos);
    }

    try {
        (void)NativeLibraries::open("/nowhere/libnative_nowhere.so");
        FAIL() << "A missing path loads.";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string(error.what()).find("/nowhere/libnative_nowhere.so: not found"), std::string::npos);
        EXPECT_EQ(std::string(error.what()).find("linked into the app"), std::string::npos);
    }

    EXPECT_THROW((void)NativeLibraries::open(""), std::invalid_argument);
}

TEST_F(NativeLibrariesTest, FindsTheSymbolsOfLinkedLibraries) {
    NativeLibraries::registerLinked("native_linked", {{"native_linked_twice", reinterpret_cast<void*>(&linkedTwice)}});
    const NativeLibraries::Library linked = NativeLibraries::open("native_linked");
    EXPECT_TRUE(linked.linked);
    EXPECT_EQ(linked.handle, nullptr);
    EXPECT_EQ(NativeLibraries::findSymbol(linked, "native_linked_twice"), reinterpret_cast<void*>(&linkedTwice));
    EXPECT_EQ(NativeLibraries::findSymbol(linked, "native_linked_other"), nullptr);
    EXPECT_EQ(NativeLibraries::findSymbol("native_linked_twice"), reinterpret_cast<void*>(&linkedTwice));

    NativeLibraries::registerLinked("native_unknown", {});
    EXPECT_EQ(NativeLibraries::findSymbol(NativeLibraries::open("native_unknown"), "native_linked_twice"), nullptr);
}

} // namespace haylen::platform
