#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include "haylen/core/Engine.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "platform/CrashGuard.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::platform {

class CrashGuardTest : public ::testing::Test {
  protected:
    void TearDown() override {
        CrashGuard::install({});
    }

    [[nodiscard]] static core::Json readReport(const std::filesystem::path& file) {
        std::ifstream stream(file, std::ios::binary);
        return core::Json::parse(std::string(std::istreambuf_iterator<char>(stream), {}));
    }

    test::TemporaryDirectory directory;
};

TEST_F(CrashGuardTest, WritesTheReasonAndTheLastPublishedContext) {
    const std::filesystem::path report = directory.getPath() / "crash-report.json";
    CrashGuard::writeReport("segmentationFault");
    EXPECT_FALSE(std::filesystem::exists(report)) << "Nothing is written before the guard knows its file.";

    CrashGuard::install(report);
    CrashGuard::setContext(R"({"breadcrumbs":[{"message":"Opened the shop."}]})");
    CrashGuard::setContext(R"({"breadcrumbs":[{"message":"Bought a sword."}]})");
    CrashGuard::writeReport("segmentationFault");
    const core::Json written = readReport(report);
    EXPECT_EQ(written.at("reason"), "segmentationFault");
    EXPECT_EQ(written.at("context").at("breadcrumbs").at(0).at("message"), "Bought a sword.");

    CrashGuard::setContext(std::string(CrashGuard::kContextSize + 1, ' '));
    CrashGuard::writeReport("abort");
    EXPECT_EQ(readReport(report), (core::Json{{"reason", "abort"}, {"context", core::Json::object()}}));
}

TEST(CrashLuaTest, HandsTheReportOfALastCrashToTheNextStartAndKeepsBreadcrumbs) {
    test::EngineFixture fixture;
    storage::UserStorage& files = fixture.engine().getStorage();
    EXPECT_EQ(fixture.host().getCrashReport(), files.getRoot() / "crash-report.json");
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.debug').lastCrash())"), "nil");

    // clang-format off
    fixture.runLua(R"(
        debugging = require('haylen.debug')
        heard = {}
        require('haylen.events').on('breadcrumbAdded', function(crumb) heard[#heard + 1] = crumb.message .. ' ' .. crumb.data.items end)
        debugging.addBreadcrumb('Opened the shop.', {items = 12})
    )");
    // clang-format on
    fixture.frames(1);
    EXPECT_EQ(fixture.lua("return heard[1]"), "Opened the shop. 12");
    const core::Json context = core::Json::parse(fixture.host().getCrashContext());
    EXPECT_EQ(context.at("breadcrumbs").back().at("message"), "Opened the shop.");
    EXPECT_EQ(context.at("app").at("identifier"), fixture.engine().getConfig().identifier);
    EXPECT_TRUE(context.at("log").is_array());

    fixture.runLua("for index = 1, 150 do debugging.addBreadcrumb('Step ' .. index) end");
    EXPECT_EQ(fixture.lua("local kept = debugging.breadcrumbs() return #kept .. ' ' .. kept[1].message .. ' ' .. kept[#kept].message"), "100 Step 51 Step 150");
    fixture.frames(1);
    EXPECT_EQ(core::Json::parse(fixture.host().getCrashContext()).at("breadcrumbs").size(), 30U);

    files.writeText("crash-report.json", R"({"reason":"segmentationFault","context":{"breadcrumbs":[{"message":"Bought a sword."}]}})");
    fixture.restart();
    EXPECT_EQ(fixture.lua("local crash = require('haylen.debug').lastCrash() return crash.reason .. ' ' .. crash.context.breadcrumbs[1].message"), "segmentationFault Bought a sword.");
    EXPECT_FALSE(fixture.engine().getStorage().exists("crash-report.json"));
    fixture.restart();
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.debug').lastCrash())"), "nil");
}

} // namespace haylen::platform
