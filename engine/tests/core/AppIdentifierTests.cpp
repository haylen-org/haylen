#include <gtest/gtest.h>

#include <filesystem>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "core/AppIdentifier.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/core/Log.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestApplication.hpp"
#include "support/TestFiles.hpp"
#include "support/ZipArchives.hpp"

namespace haylen::core {

class AppIdentifierTest : public ::testing::Test {
  protected:
    static constexpr const char* kWithoutIdentifier = R"({"name": "Sky Garden"})";

    [[nodiscard]] static std::string identifierOf(const std::filesystem::path& path) {
        return AppConfig::fromPackage(*io::Package::open(path)).identifier;
    }

    [[nodiscard]] static std::string resolved(const std::filesystem::path& path) {
        const std::u8string text = std::filesystem::canonical(path).u8string();
        return {reinterpret_cast<const char*>(text.data()), text.size()};
    }

    [[nodiscard]] static std::map<std::string, std::vector<std::uint8_t>> withoutIdentifier() {
        return {{"haylen.json", test::TestFiles::bytes(kWithoutIdentifier)}};
    }
};

// The tooling tests the same cases of `engine/tests/data/app-identifiers.json`, so an app has one identifier in the tooling and in every player.
TEST_F(AppIdentifierTest, ComposesTheIdentifiersThatTheToolingComposes) {
    const Json cases = Json::parse(test::TestFiles::readText(HAYLEN_TEST_APP_IDENTIFIERS));
    ASSERT_FALSE(cases.empty());
    for (const Json& entry : cases) {
        const std::string location = entry.at("location").get<std::string>();
        EXPECT_EQ(AppIdentifier::fromLocation(location), entry.at("identifier").get<std::string>()) << location;
    }
}

TEST_F(AppIdentifierTest, AFolderWithoutAnIdentifierTakesTheIdentifierOfItsResolvedPath) {
    const test::TemporaryDirectory first;
    const test::TemporaryDirectory second;
    first.write("Sky Garden/haylen.json", kWithoutIdentifier);
    second.write("Sky Garden/haylen.json", kWithoutIdentifier);
    const std::filesystem::path folder = first.getPath() / "Sky Garden";

    const std::string identifier = identifierOf(folder);
    EXPECT_TRUE(identifier.starts_with("local.skygarden")) << identifier;
    EXPECT_EQ(identifier, AppIdentifier::fromLocation(resolved(folder)));
    EXPECT_EQ(identifier, identifierOf(first.getPath() / "." / "Sky Garden"));
    EXPECT_NE(identifier, identifierOf(second.getPath() / "Sky Garden"));
}

TEST_F(AppIdentifierTest, AZipFileWithoutAnIdentifierTakesTheIdentifierOfItsResolvedPath) {
    const test::TemporaryDirectory folder;
    const std::filesystem::path file = folder.getPath() / "sky-garden.zip";
    test::ZipArchives::write(file, {{.name = "haylen.json", .bytes = test::TestFiles::bytes(kWithoutIdentifier)}});

    const std::string identifier = identifierOf(file);
    EXPECT_TRUE(identifier.starts_with("local.skygardenzip")) << identifier;
    EXPECT_EQ(identifier, AppIdentifier::fromLocation(resolved(file)));
}

// A launcher downloads a package to a file of its own and names the address that the package came from, so two apps that it downloads to the same file never share an identifier.
TEST_F(AppIdentifierTest, ALaunchedPackageTakesTheIdentifierOfTheAddressItCameFrom) {
    const test::TemporaryDirectory folder;
    const std::filesystem::path file = folder.getPath() / "download.zip";
    test::ZipArchives::write(file, {{.name = "haylen.json", .bytes = test::TestFiles::bytes(kWithoutIdentifier)}});

    const std::string garden = AppConfig::fromPackage(*io::Package::open(file, "https://example.com/apps/sky-garden.zip")).identifier;
    const std::string island = AppConfig::fromPackage(*io::Package::open(file, "https://example.com/apps/tiny-island.zip")).identifier;
    EXPECT_EQ(garden, AppIdentifier::fromLocation("https://example.com/apps/sky-garden.zip"));
    EXPECT_TRUE(garden.starts_with("local.skygardenzip")) << garden;
    EXPECT_NE(garden, island);
}

TEST_F(AppIdentifierTest, APackageInMemoryTakesTheIdentifierOfTheLocationItsCreatorNames) {
    const io::MemoryPackage project("editor", withoutIdentifier(), "https://editor.example.com/project-42");
    EXPECT_EQ(AppConfig::fromPackage(project).identifier, AppIdentifier::fromLocation("https://editor.example.com/project-42"));

    const test::TemporaryDirectory folder;
    const std::filesystem::path file = folder.getPath() / "app.zip";
    test::ZipArchives::write(file, {{.name = "haylen.json", .bytes = test::TestFiles::bytes(kWithoutIdentifier)}});
    const std::unique_ptr<io::Package> zip = io::Package::openZip(test::TestFiles::read(file), "app.zip", "https://editor.example.com/project-42");
    EXPECT_EQ(AppConfig::fromPackage(*zip).identifier, AppIdentifier::fromLocation("https://editor.example.com/project-42"));
}

// The bundle of a built app always holds the identifier that the tooling writes, so a package without one and without a location is damaged.
TEST_F(AppIdentifierTest, APackageWithoutAnIdentifierOrALocationDoesNotLoad) {
    const io::MemoryPackage bundle("app", withoutIdentifier());
    try {
        (void)AppConfig::fromPackage(bundle);
        FAIL() << "A package without an identifier or a location loaded.";
    } catch (const std::invalid_argument& error) {
        EXPECT_NE(std::string(error.what()).find("Build the app again with \"haylen app build\""), std::string::npos) << error.what();
    }
}

TEST_F(AppIdentifierTest, AnAppThatNamesItsIdentifierKeepsIt) {
    const test::TemporaryDirectory folder;
    folder.write("haylen.json", R"({"identifier": "com.example.garden"})");
    EXPECT_EQ(identifierOf(folder.getPath()), "com.example.garden");
}

// The screen of an app that did not load belongs to no app, so it keeps no files and never reaches the storage of another app.
TEST_F(AppIdentifierTest, AScreenThatBelongsToNoAppKeepsNoFiles) {
    const test::TemporaryDirectory directory;
    std::vector<std::string> lines;
    // clang-format off
    const std::uint64_t listener = Log::addListener([&lines](Log::Level level, std::string_view line) {
        if (level != Log::Level::Info && level != Log::Level::Debug) {
            lines.emplace_back(line);
        }
    });
    // clang-format on
    {
        platform::HeadlessHost host(directory.getPath() / "data");
        Engine engine(host, std::make_shared<io::MemoryPackage>("missing"), AppConfig{}, std::make_unique<test::TestApplication>(nullptr));
        engine.start();
        EXPECT_FALSE(engine.getStorage().isAvailable());
    }
    Log::removeListener(listener);

    EXPECT_TRUE(lines.empty()) << lines.front();
    EXPECT_FALSE(std::filesystem::exists(directory.getPath() / "data"));
}

} // namespace haylen::core
