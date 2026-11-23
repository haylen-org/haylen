#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/io/MemoryPackage.hpp"
#include "platform/PluginLoadOrder.hpp"
#include "support/TestFiles.hpp"

namespace haylen::platform {

TEST(PluginLoadOrderTest, KeepsTheOrderOfHaylenJsonWithDependenciesFirst) {
    // The order of `haylen.json` is not alphabetical, and "analytics" depends on "cloud", which `haylen.json` lists after it.
    io::MemoryPackage package("order");
    package.setFile("haylen.json", test::TestFiles::bytes(R"({"name": "Order", "plugins": {"zoo-ads": {}, "analytics": {}, "cloud": {}, "camera": {}}})"));
    package.setFile("plugins/zoo-ads/plugin.json", test::TestFiles::bytes(R"({"id": "zoo-ads", "version": "1", "implementations": {"ios": "lua"}})"));
    package.setFile("plugins/analytics/plugin.json", test::TestFiles::bytes(R"({"id": "analytics", "version": "1", "implementations": {"ios": "lua"}, "dependencies": {"cloud": "1.0", "absent": "2"}})"));
    package.setFile("plugins/cloud/plugin.json", test::TestFiles::bytes(R"({"id": "cloud", "version": "1", "implementations": {"ios": "lua"}})"));
    package.setFile("plugins/camera/plugin.json", test::TestFiles::bytes(R"({"id": "camera", "version": "1", "implementations": {"ios": "lua"}, "dependencies": {"zoo-ads": "1"}})"));

    // A dependency that `haylen.json` does not list loads nowhere, so it takes no place in the order.
    EXPECT_EQ(PluginLoadOrder::read(package), (std::vector<std::string>{"zoo-ads", "cloud", "analytics", "camera"}));
}

TEST(PluginLoadOrderTest, ReadsNoPluginsWithoutThePluginsOfHaylenJson) {
    io::MemoryPackage package("empty");
    package.setFile("haylen.json", test::TestFiles::bytes(R"({"name": "Empty"})"));
    EXPECT_TRUE(PluginLoadOrder::read(package).empty());
}

TEST(PluginLoadOrderTest, RejectsFilesThatAreNotJsonObjects) {
    io::MemoryPackage package("broken");
    // clang-format off
    const auto rejects = [&package](const std::string& reason) {
        try {
            static_cast<void>(PluginLoadOrder::read(package));
            ADD_FAILURE() << "The package was read: " << reason;
        } catch (const std::exception& error) {
            EXPECT_EQ(error.what(), reason);
        }
    };
    // clang-format on

    package.setFile("haylen.json", test::TestFiles::bytes("["));
    rejects("The file \"haylen.json\" of the package is not a JSON object.");
    package.setFile("haylen.json", test::TestFiles::bytes(R"({"plugins": {"broken": {}}})"));
    package.setFile("plugins/broken/plugin.json", test::TestFiles::bytes("{"));
    rejects("The file \"plugins/broken/plugin.json\" is not valid JSON, since it ends before its last value is complete.");
}

} // namespace haylen::platform
