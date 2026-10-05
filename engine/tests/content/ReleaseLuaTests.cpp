#include <gtest/gtest.h>
#include <lua.hpp>

#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "content/Error.hpp"
#include "content/LuaCompiler.hpp"
#include "content/ReleasePackage.hpp"
#include "core/ErrorScreen.hpp"
#include "haylen/core/AppConfig.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Application.hpp"
#include "platform/headless/HeadlessHost.hpp"
#include "support/EngineFixture.hpp"
#include "support/ReleaseFixture.hpp"
#include "support/TemporaryDirectory.hpp"

namespace haylen::content {

class ReleaseLuaTest : public ::testing::Test {
  protected:
    // Builds the release of the files and starts it on the headless host the way a shipped app starts, then runs one frame.
    void start(const std::map<std::string, std::string>& files) {
        release.build(files);
        package = release.open();
        core::AppConfig config = core::AppConfig::fromPackage(*package);
        auto application = std::make_unique<lua::Application>();
        application->configure(config);
        engine = std::make_unique<core::Engine>(host, package, std::move(config), std::move(application));
        engine->start();
        engine->frame(1.0 / 60.0);
    }

    [[nodiscard]] std::string readGlobal(const char* name) const {
        lua_State* L = engine->getLuaState();
        lua_getglobal(L, name);
        std::string value = lua_isstring(L, -1) != 0 ? lua_tostring(L, -1) : "";
        lua_pop(L, 1);
        return value;
    }

    test::ReleaseFixture release;
    test::TemporaryDirectory data;
    platform::HeadlessHost host{data.getPath()};
    std::shared_ptr<io::Package> package;
    std::unique_ptr<core::Engine> engine;
};

TEST_F(ReleaseLuaTest, RunsModulesPluginsAndAutoloadsFromBytecode) {
    start({
        {"app.json", R"({"name": "Release Lua", "identifier": "dev.haylen.tests", "autoload": ["state.player"], "plugins": {"ads": {}}})"},
        {"source/main.lua", "local menu = require('scenes.menu')\nlocal ads = require('ads')\nloaded = menu.name .. ' ' .. tostring(ads.ready) .. ' ' .. require('state.player').name"},
        {"source/scenes/menu.lua", "return {name = 'menu'}"},
        {"source/state/player.lua", "return {name = 'player'}"},
        {"plugins/ads/plugin.json", R"({"id": "ads", "version": "1.0.0"})"},
        {"plugins/ads/source/init.lua", "return {ready = true}"},
    });
    ASSERT_EQ(engine->getError(), nullptr) << engine->getError()->getMessage();
    EXPECT_EQ(readGlobal("loaded"), "menu true player");
    for (const std::string& path : {"source/main.lua", "source/scenes/menu.lua", "source/state/player.lua", "plugins/ads/source/init.lua"}) {
        EXPECT_TRUE(package->isLuaBytecode(path)) << path;
    }
    EXPECT_FALSE(package->isLuaBytecode("app.json"));
    EXPECT_FALSE(package->isLuaBytecode("plugins/ads/plugin.json"));
}

TEST_F(ReleaseLuaTest, ReportsErrorsWithTheModuleTheLineAndTheLocals) {
    start({
        {"app.json", R"({"name": "Release Lua", "identifier": "dev.haylen.tests"})"},
        {"source/main.lua", "require('scenes.menu').open(nil)"},
        {"source/scenes/menu.lua", "local menu = {}\n\nfunction menu.open(value)\n    return value.title\nend\n\nreturn menu"},
    });
    const lua::Error* error = engine->getError();
    ASSERT_NE(error, nullptr);
    EXPECT_EQ(error->getFile(), "source/scenes/menu.lua");
    EXPECT_EQ(error->getLine(), 4);
    EXPECT_NE(error->getMessage().find("(local 'value')"), std::string::npos) << error->getMessage();
    EXPECT_TRUE(core::ErrorScreen(*engine, *error, true).getExcerpt().empty()) << "A release keeps no source text to show.";
}

TEST_F(ReleaseLuaTest, LoadsBytecodeOnlyFromAnAuthenticatedRelease) {
    // Bytecode in any other package stays a file that loads only as text, so it never runs.
    const std::vector<std::uint8_t> chunk = LuaCompiler::compile("loaded = 'bytecode'", "source/main.lua");
    test::EngineFixture fixture({{"source/main.lua", std::string(chunk.begin(), chunk.end())}});
    ASSERT_NE(fixture.engine().getError(), nullptr);
    EXPECT_NE(fixture.engine().getError()->getMessage().find("binary chunk"), std::string::npos) << fixture.engine().getError()->getMessage();
    EXPECT_FALSE(fixture.package().isLuaBytecode("source/main.lua"));
}

TEST_F(ReleaseLuaTest, RefusesBytecodeOfAnotherLuaAbi) {
    release.build({{"app.json", R"({"name": "Release Lua", "identifier": "dev.haylen.tests"})"}, {"source/main.lua", "loaded = true"}});
    Compatibility other = release.getCompatibility();
    other.luaAbi = "lua-5.4-f0-i4-x4-l8-n8-le";
    const VerifyingKey trusted = release.getSigningKey().getVerifyingKey();
    try {
        (void)ReleasePackage::open(release.getFiles(), release.getKeys(), std::span(&trusted, 1), other);
        FAIL() << "A release of another Lua ABI must not open.";
    } catch (const Error& error) {
        EXPECT_EQ(error.getCode(), Error::Code::LuaBytecodeIncompatible) << error.what();
    }
}

} // namespace haylen::content
