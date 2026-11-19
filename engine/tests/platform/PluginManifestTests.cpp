#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/io/MemoryPackage.hpp"
#include "platform/PluginManifest.hpp"
#include "support/TestFiles.hpp"

namespace haylen::platform {

TEST(PluginManifestTest, ReadsWhatTheRuntimeNeeds) {
    io::MemoryPackage package("manifest");
    package.setFile("plugins/camera/plugin.json", test::TestFiles::bytes(R"({
        "interface": 1, "id": "camera", "name": "Camera", "version": "1.2.0", "description": "Scans codes.",
        "implementations": {"ios": "native", "catalyst": "native", "tvos": "lua", "android": "native"},
        "dependencies": {"permissions-kit": "1.0", "cloud": "2"},
        "permissions": ["camera"],
        "parameters": {"usage": {"type": "string", "default": "Scans codes.", "description": "The usage."}, "key": {"type": "string", "required": true, "description": "The key."}},
        "contributions": [
            {"type": "appleClass", "platforms": ["ios"], "name": "CameraPlugin"},
            {"type": "appleClass", "platforms": ["catalyst"], "name": "CameraDesktopPlugin"},
            {"type": "framework", "platforms": ["ios", "catalyst"], "name": "AVFoundation.framework"},
            {"type": "gradleModule", "platforms": ["android"], "path": "android"}
        ]
    })"));

    const PluginManifest manifest = PluginManifest::read(package, "camera");
    EXPECT_EQ(manifest.getVersion(), "1.2.0");
    EXPECT_EQ(manifest.getDependencies(), (std::vector<std::string>{"cloud", "permissions-kit"}));
    EXPECT_EQ(manifest.getDefaults(), core::Json::parse(R"({"usage": "Scans codes."})"));
    EXPECT_TRUE(manifest.isNative("android"));
    EXPECT_FALSE(manifest.isNative("tvos"));
    EXPECT_FALSE(manifest.isNative("web"));

    // Each Apple platform takes the class of the contribution that lists it, and a platform where the plugin works with its Lua alone takes none.
    EXPECT_EQ(manifest.findAppleClass("ios"), std::optional<std::string>("CameraPlugin"));
    EXPECT_EQ(manifest.findAppleClass("catalyst"), std::optional<std::string>("CameraDesktopPlugin"));
    EXPECT_EQ(manifest.findAppleClass("tvos"), std::nullopt);
    EXPECT_EQ(manifest.findAppleClass("macos"), std::nullopt);
}

TEST(PluginManifestTest, RefusesWhatTheSchemaRefuses) {
    io::MemoryPackage package("broken");
    // clang-format off
    const auto failure = [&package](const std::string& manifest) {
        package.setFile("plugins/kit/plugin.json", test::TestFiles::bytes(manifest));
        try {
            static_cast<void>(PluginManifest::read(package, "kit"));
        } catch (const std::runtime_error& error) {
            return std::string(error.what());
        }
        return std::string();
    };
    // clang-format on

    EXPECT_EQ(failure("["), "The file \"plugins/kit/plugin.json\" of the package is not a JSON object.");
    EXPECT_EQ(failure(R"({"implementations": {"ios": "lua"}})"), "The file \"plugins/kit/plugin.json\" of the package has no version.");
    EXPECT_EQ(failure(R"({"version": "1"})"), "The file \"plugins/kit/plugin.json\" of the package has no \"implementations\", the object that says whether the plugin works with its native part or with its Lua alone on each platform, such as {\"ios\": \"native\", \"web\": \"lua\"}.");
    EXPECT_EQ(failure(R"({"version": "1", "implementations": {"ios": "swift"}})"), "The implementation of \"plugins/kit/plugin.json\" in the package for \"ios\" must be \"native\" or \"lua\", not \"swift\".");
    EXPECT_EQ(failure(R"({"version": "1", "implementations": {"ios": "lua"}, "dependencies": ["cloud"]})"), "The \"dependencies\" of \"plugins/kit/plugin.json\" in the package is not an object of plugin ids with the oldest version that the plugin works with, such as {\"cloud\": \"1.2\"}.");
    EXPECT_EQ(failure(R"({"version": "1", "implementations": {"ios": "lua"}, "dependencies": {"cloud": 1}})"), "The dependency \"cloud\" of \"plugins/kit/plugin.json\" in the package has no version, such as \"1.2\".");
    EXPECT_EQ(failure(R"({"version": "1", "implementations": {"ios": "lua"}, "parameters": {"usage": "text"}})"), "The parameter \"usage\" of \"plugins/kit/plugin.json\" in the package is not an object with its \"type\" and \"description\".");
    EXPECT_EQ(failure(R"({"version": "1", "implementations": {"ios": "native"}, "contributions": [{"type": "appleClass", "platforms": ["ios"]}]})"), "The \"appleClass\" contribution of \"plugins/kit/plugin.json\" in the package needs the \"name\" of the Objective-C class of the plugin.");
    EXPECT_EQ(failure(R"({"version": "1", "implementations": {"ios": "native"}, "contributions": [{"type": "appleClass", "name": "KitPlugin"}]})"), "A contribution of \"plugins/kit/plugin.json\" in the package needs \"platforms\", the list of the platforms it applies to, such as [\"ios\"].");
    EXPECT_EQ(failure(R"({"version": "1", "implementations": {"ios": "native"}, "contributions": [3]})"), "A contribution of \"plugins/kit/plugin.json\" in the package is not an object with its \"type\" and \"platforms\".");
}

} // namespace haylen::platform
