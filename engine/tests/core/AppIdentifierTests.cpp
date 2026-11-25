#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "core/AppIdentifier.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/io/MemoryPackage.hpp"
#include "haylen/io/Package.hpp"
#include "support/TemporaryDirectory.hpp"
#include "support/TestFiles.hpp"
#include "support/ZipArchives.hpp"

namespace haylen::core {

class AppIdentifierTest : public ::testing::Test {
  protected:
    static constexpr const char* kWithoutIdentifier = R"({"name": "Sky Garden"})";

    [[nodiscard]] static std::string identifierOf(const std::filesystem::path& location) {
        return AppConfig::fromPackage(*io::Package::open(location)).identifier;
    }
};

// The tooling tests the same cases of `engine/tests/data/app-identifiers.json`, so a folder has one identifier in the tooling and in every player.
TEST_F(AppIdentifierTest, ComposesTheIdentifiersThatTheToolingComposes) {
    const Json cases = Json::parse(test::TestFiles::readText(HAYLEN_TEST_APP_IDENTIFIERS));
    ASSERT_FALSE(cases.empty());
    for (const Json& entry : cases) {
        const std::string name = entry.at("name").get<std::string>();
        const std::string path = entry.at("path").get<std::string>();
        EXPECT_EQ(AppIdentifier::compose(name, path), entry.at("identifier").get<std::string>()) << path;
    }
}

TEST_F(AppIdentifierTest, AFolderWithoutAnIdentifierTakesTheIdentifierOfItsFolder) {
    const test::TemporaryDirectory first;
    const test::TemporaryDirectory second;
    first.write("Sky Garden/haylen.json", kWithoutIdentifier);
    second.write("Sky Garden/haylen.json", kWithoutIdentifier);
    const std::filesystem::path folder = first.getPath() / "Sky Garden";

    const std::string identifier = identifierOf(folder);
    EXPECT_TRUE(identifier.starts_with("local.skygarden")) << identifier;
    EXPECT_EQ(identifier, AppIdentifier::generate(folder));
    EXPECT_EQ(identifier, identifierOf(first.getPath() / "." / "Sky Garden"));
    EXPECT_NE(identifier, identifierOf(second.getPath() / "Sky Garden"));
}

TEST_F(AppIdentifierTest, AZipFileWithoutAnIdentifierTakesTheIdentifierOfItsFile) {
    const test::TemporaryDirectory folder;
    const std::filesystem::path file = folder.getPath() / "sky-garden.zip";
    test::ZipArchives::write(file, {{.name = "haylen.json", .bytes = test::TestFiles::bytes(kWithoutIdentifier)}});

    const std::string identifier = identifierOf(file);
    EXPECT_TRUE(identifier.starts_with("local.skygardenzip")) << identifier;
    EXPECT_EQ(identifier, AppIdentifier::generate(file));
}

TEST_F(AppIdentifierTest, AnAppThatNamesItsIdentifierKeepsIt) {
    const test::TemporaryDirectory folder;
    folder.write("haylen.json", R"({"identifier": "com.example.garden"})");
    EXPECT_EQ(identifierOf(folder.getPath()), "com.example.garden");

    const io::MemoryPackage memory("app", {{"haylen.json", test::TestFiles::bytes(kWithoutIdentifier)}});
    EXPECT_EQ(AppConfig::fromPackage(memory).identifier, AppConfig{}.identifier);
}

} // namespace haylen::core
