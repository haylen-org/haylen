#include <gtest/gtest.h>

#include "haylen/core/Json.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::lua::platform {

TEST(CrashLuaTest, HandsTheReportOfALastCrashToTheNextStartAndKeepsBreadcrumbs) {
    test::LuaFixture fixture;
    haylen::storage::UserStorage& files = fixture.engine().getStorage();
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
    const haylen::core::Json context = haylen::core::Json::parse(fixture.host().getCrashContext());
    ASSERT_FALSE(context.at("breadcrumbs").empty());
    EXPECT_EQ(context.at("breadcrumbs").back().at("message"), "Opened the shop.");
    EXPECT_EQ(context.at("app").at("identifier"), fixture.engine().getConfig().identifier);
    EXPECT_EQ(context.at("app").at("build"), fixture.engine().getConfig().build);
    EXPECT_TRUE(context.at("log").is_array());

    fixture.runLua("for index = 1, 150 do debugging.addBreadcrumb('Step ' .. index) end");
    EXPECT_EQ(fixture.lua("local kept = debugging.breadcrumbs() return #kept .. ' ' .. kept[1].message .. ' ' .. kept[#kept].message"), "100 Step 51 Step 150");
    fixture.frames(1);
    EXPECT_EQ(haylen::core::Json::parse(fixture.host().getCrashContext()).at("breadcrumbs").size(), 30U);

    files.writeText("crash-report.json", R"({"reason":"segmentationFault","context":{"breadcrumbs":[{"message":"Bought a sword."}]}})");
    fixture.restart();
    EXPECT_EQ(fixture.lua("local crash = require('haylen.debug').lastCrash() return crash.reason .. ' ' .. crash.context.breadcrumbs[1].message"), "segmentationFault Bought a sword.");
    EXPECT_FALSE(fixture.engine().getStorage().exists("crash-report.json"));
    fixture.restart();
    EXPECT_EQ(fixture.lua("return tostring(require('haylen.debug').lastCrash())"), "nil");
}

} // namespace haylen::lua::platform
