#include <gtest/gtest.h>
#include <zip.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
#include "io/CompositePackage.hpp"
#include "io/OverlayPackage.hpp"
#include "support/AllocationTracker.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"

namespace haylen::io {

TEST(PathTest, NormalizesRelativePaths) {
    EXPECT_EQ(Path::normalize("a/./b//c"), "a/b/c");
    EXPECT_EQ(Path::normalize("a\\b\\..\\c"), "a/c");
    EXPECT_EQ(Path::normalize(""), "");
    EXPECT_THROW((void)Path::normalize("/etc/passwd"), std::invalid_argument);
    EXPECT_THROW((void)Path::normalize("C:/windows"), std::invalid_argument);
    EXPECT_THROW((void)Path::normalize("../secret"), std::invalid_argument);
    EXPECT_THROW((void)Path::normalize("a/../../b"), std::invalid_argument);
}

TEST(PathTest, BuildsAssetPathsAndParts) {
    EXPECT_EQ(Path::asset("maps/island.tmj"), "content/maps/island.tmj");
    EXPECT_THROW((void)Path::asset(""), std::invalid_argument);
    EXPECT_EQ(Path::extension("maps/island.tmj"), ".tmj");
    EXPECT_EQ(Path::extension("folder.v2/readme"), "");
    EXPECT_EQ(Path::extension("readme"), "");
    EXPECT_EQ(Path::directory("maps/tiles/a.png"), "maps/tiles");
    EXPECT_EQ(Path::directory("a.png"), "");
    EXPECT_EQ(Path::join("maps", "../tilesets/t.tsj"), "tilesets/t.tsj");
    EXPECT_EQ(Path::join("", "a/b"), "a/b");
    EXPECT_EQ(Path::plugin("firebase-analytics", Path::kPluginManifestFile), "plugins/firebase-analytics/plugin.json");
    EXPECT_EQ(Path::plugin("ads", "source/init.lua"), "plugins/ads/source/init.lua");
    EXPECT_TRUE(Path::isInside("maps/a.tmj", "maps"));
    EXPECT_TRUE(Path::isInside("maps/a.tmj", ""));
    EXPECT_FALSE(Path::isInside("mapsx/a.tmj", "maps"));
    EXPECT_FALSE(Path::isInside("maps", "maps"));
}

TEST(PathTest, MatchesPatternsOfFoldersAndNames) {
    EXPECT_TRUE(Path::matches("world/asia/**", "world/asia/maps/tokyo.tmj"));
    EXPECT_TRUE(Path::matches("world/**/tokyo.tmj", "world/tokyo.tmj"));
    EXPECT_TRUE(Path::matches("**/*.ogg", "music/theme.ogg"));
    EXPECT_TRUE(Path::matches("**/a*", "x/ab/a1"));
    EXPECT_TRUE(Path::matches("levels/level-?.json", "levels/level-3.json"));
    EXPECT_FALSE(Path::matches("levels/*.json", "levels/forest/one.json"));
    EXPECT_FALSE(Path::matches("levels/?", "levels//"));
    EXPECT_FALSE(Path::matches("world/asia/**", "world/asian/a.png"));
    EXPECT_TRUE(Path::matches("*", "readme"));
    EXPECT_FALSE(Path::matches("*", "a/b"));
}

class PackageTest : public ::testing::TestWithParam<std::string> {
  protected:
    std::unique_ptr<Package> makePackage() {
        const std::map<std::string, std::string> files = {
            {"app.json", R"({"plugins": {"ads": {"testMode": true}}})"}, {"source/main.lua", "print('hi')"}, {"content/maps/island.tmj", "{\"width\": 2}"}, {"content/audio/hit.ogg", "OggS"}, {"plugins/ads/plugin.json", R"({"id": "ads", "version": "1.0.0"})"}, {"plugins/ads/source/init.lua", "return {}"},
        };

        if (GetParam() == "directory") {
            for (const auto& [path, content] : files) {
                directory.write(path, content);
            }
            return Package::open(directory.getPath());
        }
        if (GetParam() == "zip-file") {
            const std::vector<std::uint8_t> archive = makeZip(files);
            directory.write("app.zip", std::string(archive.begin(), archive.end()));
            return Package::open(directory.getPath() / "app.zip");
        }
        if (GetParam() == "zip-memory") {
            return Package::openZip(makeZip(files), "memory.zip");
        }

        std::map<std::string, std::vector<std::uint8_t>> contents;
        for (const auto& [path, content] : files) {
            contents.emplace(path, test::TestFiles::bytes(content));
        }
        return std::make_unique<MemoryPackage>("memory", std::move(contents));
    }

    // Returns the bytes of a zip archive with the files, built by libzip in the temporary directory.
    std::vector<std::uint8_t> makeZip(const std::map<std::string, std::string>& files) {
        const std::filesystem::path path = directory.getPath() / "archive.zip";
        int error = 0;
        zip_t* archive = zip_open(path.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
        if (archive == nullptr) {
            throw std::runtime_error("Could not create a test zip archive.");
        }
        for (const auto& [name, content] : files) {
            zip_source_t* source = zip_source_buffer(archive, content.data(), content.size(), 0);
            zip_file_add(archive, name.c_str(), source, ZIP_FL_ENC_UTF_8);
        }
        zip_close(archive);

        std::ifstream stream(path, std::ios::binary);
        std::vector<std::uint8_t> data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
        stream.close();
        std::filesystem::remove(path);
        return data;
    }

    [[nodiscard]] static std::string getError(const std::function<void()>& action) {
        try {
            action();
        } catch (const std::exception& error) {
            return error.what();
        }
        return {};
    }

    test::TemporaryDirectory directory;
};

TEST_P(PackageTest, ReadsFilesAndAssets) {
    const std::unique_ptr<Package> package = makePackage();
    EXPECT_FALSE(package->getName().empty());
    EXPECT_TRUE(package->exists("source/main.lua"));
    EXPECT_TRUE(package->exists("./source/main.lua"));
    EXPECT_FALSE(package->exists("source/missing.lua"));
    EXPECT_EQ(package->readText("source/main.lua"), "print('hi')");
    EXPECT_TRUE(package->assetExists("maps/island.tmj"));
    EXPECT_EQ(package->readAssetText("maps/island.tmj"), "{\"width\": 2}");
    EXPECT_EQ(package->readAsset("audio/hit.ogg").size(), 4U);
    EXPECT_THROW((void)package->read("missing.lua"), std::runtime_error);
    EXPECT_THROW((void)package->read("../outside"), std::invalid_argument);
}

TEST_P(PackageTest, ReadsFilesInRanges) {
    const std::unique_ptr<Package> package = makePackage();
    EXPECT_EQ(package->getFileSize("source/main.lua"), 11U);
    EXPECT_EQ(package->getAssetSize("maps/island.tmj"), 12U);
    EXPECT_THROW((void)package->getFileSize("source/missing.lua"), std::runtime_error);
    EXPECT_EQ(package->readRange("source/main.lua", 6, 4), test::TestFiles::bytes("'hi'"));
    EXPECT_EQ(package->readRange("source/main.lua", 6, 100), test::TestFiles::bytes("'hi')"));
    EXPECT_TRUE(package->readRange("source/main.lua", 50, 4).empty());
    EXPECT_EQ(package->readAssetRange("maps/island.tmj", 2, 5), test::TestFiles::bytes("width"));

    // A reader moves back and forth through its file.
    const std::unique_ptr<PackageReader> reader = package->openReader("content/maps/island.tmj");
    EXPECT_EQ(reader->getSize(), 12U);
    std::vector<std::uint8_t> bytes(3);
    reader->readExactly(9, bytes);
    EXPECT_EQ(bytes, test::TestFiles::bytes(" 2}"));
    reader->readExactly(1, bytes);
    EXPECT_EQ(bytes, test::TestFiles::bytes("\"wi"));
    EXPECT_EQ(reader->read(12, bytes), 0U);
    EXPECT_THROW(reader->readExactly(11, bytes), std::runtime_error);
    EXPECT_THROW((void)package->openReader("missing.lua"), std::runtime_error);

    // Every package names a missing file by its path inside the package, never by a path of the host.
    const std::string missing = "The package file \"" + std::string(package->getName()) + "/source/missing.lua\" was not found.";
    EXPECT_EQ(getError([&] { (void)package->getFileSize("source/missing.lua"); }), missing);
    EXPECT_EQ(getError([&] { (void)package->openReader("./source/missing.lua"); }), missing);
}

TEST_P(PackageTest, CarriesThePluginsOfTheApp) {
    const std::unique_ptr<Package> package = makePackage();
    EXPECT_EQ(core::AppConfig::fromPackage(*package).plugins.at("ads").at("testMode"), true);
    EXPECT_EQ(package->list(Path::kPluginsDirectory), (std::vector<std::string>{"plugins/ads/plugin.json", "plugins/ads/source/init.lua"}));
    EXPECT_EQ(package->readText(Path::plugin("ads", "source/init.lua")), "return {}");
    EXPECT_EQ(package->listAssets("").size(), 2U) << "The files of plugins are no assets.";
}

TEST_P(PackageTest, ListsFilesRecursively) {
    const std::unique_ptr<Package> package = makePackage();
    EXPECT_EQ(package->list("content"), (std::vector<std::string>{"content/audio/hit.ogg", "content/maps/island.tmj"}));
    EXPECT_EQ(package->listAssets(""), (std::vector<std::string>{"audio/hit.ogg", "maps/island.tmj"}));
    EXPECT_EQ(package->listAssets("maps"), (std::vector<std::string>{"maps/island.tmj"}));
    EXPECT_TRUE(package->list("nothing").empty());

    // A folder that climbs out of `content` is rejected like any asset path, instead of listing the whole package.
    EXPECT_THROW((void)package->listAssets(".."), std::invalid_argument);
    EXPECT_THROW((void)package->listAssets("maps/../.."), std::invalid_argument);
}

TEST_P(PackageTest, NamesItsFolderOnlyWhenItReadsAFolder) {
    const std::unique_ptr<Package> package = makePackage();
    if (GetParam() == "directory") {
        EXPECT_EQ(package->getDirectory(), std::filesystem::canonical(directory.getPath()));
        return;
    }
    EXPECT_FALSE(package->getDirectory().has_value());
}

TEST(PackageLuaTest, ListingAssetsNeverLeavesTheContentFolder) {
    test::EngineFixture fixture({{"content/maps/island.tmj", "{}"}});
    EXPECT_NE(fixture.lua("return require('haylen.assets').list('..')").find("The path \"..\" must stay inside its root folder."), std::string::npos);
}

INSTANTIATE_TEST_SUITE_P(Sources, PackageTest, ::testing::Values("directory", "zip-file", "zip-memory", "memory"));

TEST(PackageOpenTest, RejectsMissingAndInvalidPackages) {
    test::TemporaryDirectory directory;
    EXPECT_THROW((void)Package::openDirectory(directory.getPath() / "missing"), std::runtime_error);
    EXPECT_THROW((void)Package::open(directory.getPath() / "missing.zip"), std::runtime_error);
    EXPECT_THROW((void)Package::openZip(test::TestFiles::bytes("not a zip"), "broken.zip"), std::runtime_error);

    directory.write("broken.zip", "not a zip");
    try {
        (void)Package::open(directory.getPath() / "broken.zip");
        ADD_FAILURE() << "A file that is no zip archive must be rejected.";
    } catch (const std::runtime_error& error) {
        EXPECT_TRUE(std::string(error.what()).starts_with("The package archive \"broken.zip\" is not a valid zip file, and the zip reader reported \"")) << error.what();
    }
}

TEST(PackageOpenTest, ReportsDamagedZipEntries) {
    test::TemporaryDirectory directory;
    const std::filesystem::path file = directory.getPath() / "app.zip";
    const std::string level(64 * 1024, 'a');
    int error = 0;
    zip_t* archive = zip_open(file.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    ASSERT_NE(archive, nullptr);
    const zip_int64_t entry = zip_file_add(archive, "content/level.txt", zip_source_buffer(archive, level.data(), level.size(), 0), 0);
    zip_set_file_compression(archive, static_cast<zip_uint64_t>(entry), ZIP_CM_DEFLATE, 0);
    zip_close(archive);

    // The compressed bytes of the only entry follow its local header of 30 bytes, its name and its extra field, and a first byte with every bit set starts a deflate block of a type that does not exist.
    std::ifstream stream(file, std::ios::binary);
    std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    ASSERT_EQ(bytes[8], ZIP_CM_DEFLATE);
    const std::size_t nameLength = std::size_t{bytes[26]} | std::size_t{bytes[27]} << 8;
    const std::size_t extraLength = std::size_t{bytes[28]} | std::size_t{bytes[29]} << 8;
    bytes[30 + nameLength + extraLength] = 0xFF;
    const std::unique_ptr<Package> package = Package::openZip(std::move(bytes), "broken.zip");

    // Reading from the start and skipping forward to a later offset both stop at the damage instead of returning bytes the entry does not hold.
    const std::string message = "The package file \"broken.zip/content/level.txt\" could not be read.";
    for (const std::uint64_t offset : {std::uint64_t{0}, std::uint64_t{1000}}) {
        try {
            (void)package->readRange("content/level.txt", offset, 100);
            ADD_FAILURE() << "A damaged entry must fail at " << offset << ".";
        } catch (const std::runtime_error& failure) {
            EXPECT_EQ(std::string(failure.what()), message);
        }
    }
}

TEST(PackageOpenTest, ReportsAFolderFileThatShrinksWhileItIsRead) {
    test::TemporaryDirectory directory;
    directory.write("content/level.bin", std::string(1000, 'a'));
    const std::unique_ptr<Package> package = Package::open(directory.getPath());
    const std::unique_ptr<PackageReader> reader = package->openReader("content/level.bin");

    // A reader keeps the size its file had when it opened, so a file that loses bytes fails the read instead of handing out less.
    std::filesystem::resize_file(directory.getPath() / "content" / "level.bin", 10);
    std::vector<std::uint8_t> bytes(100);
    try {
        (void)reader->read(0, bytes);
        ADD_FAILURE() << "A file that shrank must fail the read.";
    } catch (const std::runtime_error& error) {
        EXPECT_EQ(std::string(error.what()), "The package file \"" + std::string(package->getName()) + "/content/level.bin\" could not be read.");
    }
}

TEST(PackageOpenTest, OpensZipFilesWithoutReadingThemWhole) {
    test::TemporaryDirectory directory;
    const std::vector<std::uint8_t> large = test::TestFiles::randomBytes(8 * 1024 * 1024, 1);
    int error = 0;
    zip_t* archive = zip_open((directory.getPath() / "app.zip").string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    ASSERT_NE(archive, nullptr);
    zip_file_add(archive, "content/large.bin", zip_source_buffer(archive, large.data(), large.size(), 0), 0);
    const std::string small = "{}";
    zip_file_add(archive, "content/small.json", zip_source_buffer(archive, small.data(), small.size(), 0), 0);
    zip_close(archive);

    std::unique_ptr<Package> package;
    {
        const test::AllocationTracker tracker;
        package = Package::open(directory.getPath() / "app.zip");
        EXPECT_EQ(package->readAssetText("small.json"), "{}");
        EXPECT_LT(tracker.getLargest(), std::size_t{1} << 20) << "Opening an archive and reading a small entry never holds the whole archive.";
    }

    // A compressed entry reads ranges forward and backward.
    const std::vector<std::uint8_t> late = package->readAssetRange("large.bin", 6 * 1024 * 1024, 1000);
    const std::vector<std::uint8_t> early = package->readAssetRange("large.bin", 1000, 1000);
    EXPECT_TRUE(std::equal(late.begin(), late.end(), large.begin() + 6 * 1024 * 1024));
    EXPECT_TRUE(std::equal(early.begin(), early.end(), large.begin() + 1000));
    EXPECT_EQ(package->readAsset("large.bin"), large);
}

TEST(PackageOpenTest, KeepsAZipReaderSafeWhenItsArchiveShrinks) {
    test::TemporaryDirectory directory;
    const std::filesystem::path file = directory.getPath() / "app.zip";
    const std::vector<std::uint8_t> large = test::TestFiles::randomBytes(4 * 1024 * 1024, 2);
    int error = 0;
    zip_t* archive = zip_open(file.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    ASSERT_NE(archive, nullptr);
    zip_file_add(archive, "content/large.bin", zip_source_buffer(archive, large.data(), large.size(), 0), 0);
    zip_close(archive);

    const std::unique_ptr<Package> package = Package::open(file);
    std::unique_ptr<PackageReader> reader = package->openReader("content/large.bin");
    std::vector<std::uint8_t> bytes(1000);
    reader->readExactly(2 * 1024 * 1024, bytes);

    // Going back in a compressed entry opens it again, which fails once the archive lost its bytes, and the reader still reads and closes safely.
    std::filesystem::resize_file(file, 0);
    EXPECT_THROW(reader->readExactly(0, bytes), std::runtime_error);
    EXPECT_THROW(reader->readExactly(0, bytes), std::runtime_error);
    reader.reset();
}

#if !defined(_WIN32)
TEST(PackageOpenTest, RefusesToListAFolderPartly) {
    test::TemporaryDirectory directory;
    directory.write("content/maps/island.tmj", "{}");
    directory.write("content/locked/secret.json", "{}");
    const std::filesystem::path locked = directory.getPath() / "content" / "locked";
    std::filesystem::permissions(locked, std::filesystem::perms::none);

    // A user who reads every folder, such as the superuser, cannot meet the problem.
    std::error_code denied;
    const std::filesystem::directory_iterator probe(locked, denied);
    if (!denied) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
        GTEST_SKIP() << "The user reads folders without permission.";
    }

    const std::unique_ptr<Package> package = Package::open(directory.getPath());
    std::string message;
    try {
        (void)package->listAssets("");
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
    EXPECT_TRUE(message.starts_with("The package folder \"" + std::string(package->getName()) + "/content\" could not be listed")) << message;
    EXPECT_EQ(message.find(directory.getPath().generic_string()), std::string::npos) << "The message names the folder inside the package.";
}

TEST(PackageOpenTest, ReportsAFolderFileItCannotOpen) {
    test::TemporaryDirectory directory;
    directory.write("content/secret.json", "{}");
    const std::filesystem::path locked = directory.getPath() / "content" / "secret.json";
    std::filesystem::permissions(locked, std::filesystem::perms::none);
    if (std::ifstream(locked).is_open()) {
        std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
        GTEST_SKIP() << "The user reads files without permission.";
    }

    const std::unique_ptr<Package> package = Package::open(directory.getPath());
    std::string message;
    try {
        (void)package->readAsset("secret.json");
    } catch (const std::runtime_error& error) {
        message = error.what();
    }
    std::filesystem::permissions(locked, std::filesystem::perms::owner_all);
    EXPECT_EQ(message, "The package file \"" + std::string(package->getName()) + "/content/secret.json\" could not be opened.");
}
#endif

TEST(CompositePackageTest, ShowsLaterLayersOverEarlierOnes) {
    const auto base = std::make_shared<MemoryPackage>("base", std::map<std::string, std::vector<std::uint8_t>>{{"content/a.txt", test::TestFiles::bytes("base a")}, {"content/b.txt", test::TestFiles::bytes("base b")}});
    const auto patch = std::make_shared<MemoryPackage>("patch", std::map<std::string, std::vector<std::uint8_t>>{{"content/b.txt", test::TestFiles::bytes("patched b")}, {"content/c.txt", test::TestFiles::bytes("patch c")}});
    const CompositePackage package("app", {base, patch});

    EXPECT_EQ(package.getName(), "app");
    EXPECT_EQ(package.readAssetText("a.txt"), "base a");
    EXPECT_EQ(package.readAssetText("b.txt"), "patched b");
    EXPECT_EQ(package.getAssetSize("b.txt"), 9U);
    EXPECT_EQ(package.readAssetText("c.txt"), "patch c");
    EXPECT_EQ(package.listAssets(""), (std::vector<std::string>{"a.txt", "b.txt", "c.txt"}));
    EXPECT_FALSE(package.exists("content/d.txt"));
    EXPECT_THROW((void)package.read("content/d.txt"), std::runtime_error);
}

TEST(PackageLuaTest, ReadsAssetsInRanges) {
    test::EngineFixture fixture({{"content/levels/cave.bin", "0123456789"}});
    EXPECT_EQ(fixture.lua("return require('haylen.assets').fileSize('levels/cave.bin')"), "10");
    EXPECT_EQ(fixture.lua("return require('haylen.assets').bytes('levels/cave.bin', 3, 4)"), "3456");
    EXPECT_EQ(fixture.lua("return require('haylen.assets').bytes('levels/cave.bin', 8, 100)"), "89");
    EXPECT_EQ(fixture.lua("return #require('haylen.assets').bytes('levels/cave.bin', 20, 4)"), "0");
    EXPECT_NE(fixture.lua("return require('haylen.assets').bytes('levels/cave.bin', -1, 4)").find("bad argument #2"), std::string::npos);
    EXPECT_NE(fixture.lua("return require('haylen.assets').fileSize('levels/missing.bin')").find("was not found."), std::string::npos);
}

TEST(MemoryPackageTest, ReplacesAndRemovesFiles) {
    MemoryPackage package("editor");
    package.setFile("main.lua", test::TestFiles::bytes("a"));
    EXPECT_EQ(package.readText("main.lua"), "a");
    const std::unique_ptr<PackageReader> reader = package.openReader("main.lua");
    package.setFile("./main.lua", test::TestFiles::bytes("b"));
    EXPECT_EQ(package.readText("main.lua"), "b");
    std::vector<std::uint8_t> opened(1);
    reader->readExactly(0, opened);
    EXPECT_EQ(opened, test::TestFiles::bytes("a")) << "A reader keeps the bytes its file had when it opened.";
    EXPECT_TRUE(package.removeFile("main.lua"));
    EXPECT_FALSE(package.removeFile("main.lua"));
    EXPECT_FALSE(package.exists("main.lua"));
}

TEST(OverlayPackageTest, ReadsReplacedFilesOverTheBaseAndHidesRemovedOnes) {
    const auto base = std::make_shared<MemoryPackage>("base", std::map<std::string, std::vector<std::uint8_t>>{{"source/a.lua", test::TestFiles::bytes("a")}, {"source/b.lua", test::TestFiles::bytes("b")}, {"content/c.json", test::TestFiles::bytes("{}")}});
    EXPECT_THROW((void)OverlayPackage(nullptr), std::invalid_argument);
    OverlayPackage overlay(base);
    overlay.setFile("source/a.lua", test::TestFiles::bytes("edited"));
    EXPECT_TRUE(overlay.removeFile("source/b.lua"));
    EXPECT_FALSE(overlay.removeFile("source/missing.lua"));
    overlay.setFile("./source//d.lua", test::TestFiles::bytes("added"));
    overlay.setFile("content/e.json", test::TestFiles::bytes("[]"));

    EXPECT_EQ(overlay.readText("source/a.lua"), "edited");
    EXPECT_EQ(overlay.getFileSize("source/a.lua"), 6U);
    EXPECT_FALSE(overlay.exists("source/b.lua"));
    EXPECT_THROW((void)overlay.read("source/b.lua"), std::runtime_error);
    EXPECT_EQ(overlay.readText("source/d.lua"), "added");
    EXPECT_EQ(overlay.readAssetText("c.json"), "{}");
    EXPECT_EQ(overlay.list("source"), (std::vector<std::string>{"source/a.lua", "source/d.lua"}));
    EXPECT_EQ(overlay.list("content"), (std::vector<std::string>{"content/c.json", "content/e.json"}));
    EXPECT_EQ(overlay.getName(), "base");
    EXPECT_FALSE(overlay.isLuaBytecode("source/a.lua"));
    EXPECT_EQ(base->readText("source/a.lua"), "a");

    // An overlay names the folder of the package it covers, when that package reads a folder.
    EXPECT_FALSE(overlay.getDirectory().has_value());
    const test::TemporaryDirectory folder;
    EXPECT_EQ(OverlayPackage(Package::openDirectory(folder.getPath())).getDirectory(), std::filesystem::canonical(folder.getPath()));

    overlay.setFile("source/b.lua", test::TestFiles::bytes("back"));
    EXPECT_EQ(overlay.readText("source/b.lua"), "back");
    EXPECT_THROW(overlay.setFile("../secret.lua", {}), std::invalid_argument);
    EXPECT_THROW((void)overlay.exists("/etc/passwd"), std::invalid_argument);
}

} // namespace haylen::io
