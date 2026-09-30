#include <gtest/gtest.h>
#include <zip.h>

#include <fstream>
#include <iterator>
#include <stdexcept>

#include "haylen/core/AppConfig.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/io/Path.hpp"
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

TEST(MemoryPackageTest, ReplacesAndRemovesFiles) {
    MemoryPackage package("editor");
    package.setFile("main.lua", test::TestFiles::bytes("a"));
    EXPECT_EQ(package.readText("main.lua"), "a");
    package.setFile("./main.lua", test::TestFiles::bytes("b"));
    EXPECT_EQ(package.readText("main.lua"), "b");
    EXPECT_TRUE(package.removeFile("main.lua"));
    EXPECT_FALSE(package.removeFile("main.lua"));
    EXPECT_FALSE(package.exists("main.lua"));
}

} // namespace haylen::io
