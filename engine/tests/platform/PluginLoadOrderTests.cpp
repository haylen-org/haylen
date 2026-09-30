#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/io/MemoryPackage.hpp"
#include "platform/PluginLoadOrder.hpp"
#include "support/TestFiles.hpp"

namespace haylen::platform {

TEST(PluginLoadOrderTest, KeepsTheOrderOfAppJsonWithRequiredPluginsFirst) {
    // The order of `app.json` is not alphabetical, and "analytics" requires "firebase", which `app.json` lists after it.
    io::MemoryPackage package("order");
    package.setFile("app.json", test::TestFiles::bytes(R"({"name": "Order", "plugins": {"zoo-ads": {}, "analytics": {}, "firebase": {}, "camera": {}}})"));
    package.setFile("plugins/zoo-ads/plugin.json", test::TestFiles::bytes(R"({"id": "zoo-ads", "version": "1"})"));
    package.setFile("plugins/analytics/plugin.json", test::TestFiles::bytes(R"({"id": "analytics", "version": "1", "requires": ["firebase", "absent"]})"));
    package.setFile("plugins/firebase/plugin.json", test::TestFiles::bytes(R"({"id": "firebase", "version": "1"})"));
    package.setFile("plugins/camera/plugin.json", test::TestFiles::bytes(R"({"id": "camera", "version": "1", "requires": ["zoo-ads"]})"));

    // A required plugin that `app.json` does not list loads nowhere, so it takes no place in the order.
    EXPECT_EQ(PluginLoadOrder::read(package), (std::vector<std::string>{"zoo-ads", "firebase", "analytics", "camera"}));
}

TEST(PluginLoadOrderTest, ReadsNoPluginsWithoutThePluginsOfAppJson) {
    io::MemoryPackage package("empty");
    package.setFile("app.json", test::TestFiles::bytes(R"({"name": "Empty"})"));
    EXPECT_TRUE(PluginLoadOrder::read(package).empty());
}

TEST(PluginLoadOrderTest, RejectsFilesThatAreNotJsonObjects) {
    io::MemoryPackage package("broken");
    // clang-format off
    const auto rejects = [&package](const std::string& reason) {
        try {
            static_cast<void>(PluginLoadOrder::read(package));
            ADD_FAILURE() << "The package was read: " << reason;
        } catch (const std::runtime_error& error) {
            EXPECT_EQ(error.what(), reason);
        }
    };
    // clang-format on

    package.setFile("app.json", test::TestFiles::bytes("["));
    rejects("The file \"app.json\" of the package is not a JSON object.");
    package.setFile("app.json", test::TestFiles::bytes(R"({"plugins": {"broken": {}}})"));
    package.setFile("plugins/broken/plugin.json", test::TestFiles::bytes("{"));
    rejects("The file \"plugins/broken/plugin.json\" of the package is not a JSON object.");
}

} // namespace haylen::platform
