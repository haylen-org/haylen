#include <gtest/gtest.h>
#include <zip.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>

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
    OverlayPackage overlay(base);
    overlay.setFile("source/a.lua", test::TestFiles::bytes("edited"));
    EXPECT_TRUE(overlay.removeFile("source/b.lua"));
    EXPECT_FALSE(overlay.removeFile("source/missing.lua"));
    overlay.setFile("./source//d.lua", test::TestFiles::bytes("added"));

    EXPECT_EQ(overlay.readText("source/a.lua"), "edited");
    EXPECT_EQ(overlay.getFileSize("source/a.lua"), 6U);
    EXPECT_FALSE(overlay.exists("source/b.lua"));
    EXPECT_THROW((void)overlay.read("source/b.lua"), std::runtime_error);
    EXPECT_EQ(overlay.readText("source/d.lua"), "added");
    EXPECT_EQ(overlay.readAssetText("c.json"), "{}");
    EXPECT_EQ(overlay.list("source"), (std::vector<std::string>{"source/a.lua", "source/d.lua"}));
    EXPECT_EQ(overlay.getName(), "base");
    EXPECT_FALSE(overlay.isLuaBytecode("source/a.lua"));
    EXPECT_EQ(base->readText("source/a.lua"), "a");

    overlay.setFile("source/b.lua", test::TestFiles::bytes("back"));
    EXPECT_EQ(overlay.readText("source/b.lua"), "back");
    EXPECT_THROW(overlay.setFile("../secret.lua", {}), std::invalid_argument);
    EXPECT_THROW((void)overlay.exists("/etc/passwd"), std::invalid_argument);
}

} // namespace haylen::io
