#include <gtest/gtest.h>

#include <chrono>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "haylen/core/AppConfig.hpp"
#include "haylen/lua/Application.hpp"
#include "runtime/LuaConfig.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::lua {

class LuaConfigTest : public ::testing::Test {
  protected:
    // Returns the message that refuses the `lua` section, or an empty text when the section is valid.
    [[nodiscard]] static std::string refusal(const std::string& section) {
        try {
            (void)LuaConfig::fromJson(haylen::core::Json::parse(section));
        } catch (const std::invalid_argument& error) {
            return error.what();
        }

        return {};
    }
};

TEST_F(LuaConfigTest, ReadsTheLuaSectionOfTheApp) {
    const haylen::core::AppConfig app = haylen::core::AppConfig::fromJson(haylen::core::Json::parse(R"({"lua": {"autoloads": ["state.player"], "reload": "restart", "executionBudget": 2.5, "memoryBudget": 64}})"), Application::kSections);
    const LuaConfig config = LuaConfig::fromConfig(app);

    EXPECT_EQ(config.autoloads, std::vector<std::string>{"state.player"});
    EXPECT_EQ(config.reload, LuaConfig::Reload::Restart);
    EXPECT_EQ(config.executionBudget, std::chrono::milliseconds(2500));
    EXPECT_EQ(config.memoryBudget, std::size_t{64} * 1024 * 1024);
    EXPECT_EQ(LuaConfig::fromJson(config.toJson()).toJson(), config.toJson());

    const LuaConfig defaults = LuaConfig::fromConfig(haylen::core::AppConfig::fromJson(haylen::core::Json::object(), Application::kSections));
    EXPECT_TRUE(defaults.autoloads.empty());
    EXPECT_EQ(defaults.reload, LuaConfig::Reload::Module);
    EXPECT_EQ(defaults.executionBudget, std::chrono::milliseconds(5000));
    EXPECT_EQ(defaults.memoryBudget, std::size_t{1024} * 1024 * 1024);
}

TEST_F(LuaConfigTest, RefusesWrongValuesWithTheirKey) {
    EXPECT_EQ(refusal(R"({"autoload": ["state"]})"), "Unknown key \"autoload\" in the \"lua\" section of \"haylen.json\".");
    EXPECT_EQ(refusal(R"({"autoloads": "state"})"), "The value of \"lua.autoloads\" in \"haylen.json\" must be a list of module names.");
    EXPECT_EQ(refusal(R"({"autoloads": ["state.player", ""]})"), "The \"lua.autoloads\" list in \"haylen.json\" has an empty module name.");
    EXPECT_EQ(refusal(R"({"reload": "always"})"), "The \"lua.reload\" value \"always\" in \"haylen.json\" is unknown. It is \"module\" or \"restart\".");
    EXPECT_EQ(refusal(R"({"executionBudget": 0.05})"), "The \"lua.executionBudget\" value in \"haylen.json\" must be a number of seconds from 0.1 to 600.");
    EXPECT_EQ(refusal(R"({"executionBudget": "5"})"), "The \"lua.executionBudget\" value in \"haylen.json\" must be a number of seconds from 0.1 to 600.");
    EXPECT_EQ(refusal(R"({"memoryBudget": 8192})"), "The \"lua.memoryBudget\" value in \"haylen.json\" must be a number of megabytes from 16 to 4096.");
    EXPECT_EQ(refusal("[]"), "Expected the \"lua\" section of \"haylen.json\" to be a JSON object.");
}

// A Lua app reads its section before the engine starts, so a wrong value fails the app with its reason, and scripts read the section with every default filled in.
TEST_F(LuaConfigTest, TheApplicationValidatesTheSectionAndScriptsReadIt) {
    haylen::core::AppConfig wrong = haylen::core::AppConfig::fromJson(haylen::core::Json::parse(R"({"lua": {"reload": "always"}})"), Application::kSections);
    Application application;
    EXPECT_THROW(application.configure(wrong), std::invalid_argument);

    test::EngineFixture fixture({{"haylen.json", R"({"name": "Lua Config", "identifier": "com.haylen.tests", "lua": {"memoryBudget": 256}})"}, {"source/main.lua", ""}});
    EXPECT_EQ(fixture.lua("local lua = require('haylen').config.lua return lua.memoryBudget .. ' ' .. lua.reload .. ' ' .. #lua.autoloads"), "256.0 module 0");
}

} // namespace haylen::lua
