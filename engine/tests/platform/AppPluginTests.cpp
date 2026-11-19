#include <gtest/gtest.h>

#include <lua.hpp>

#include <memory>
#include <string_view>

#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

class AppPluginTest : public ::testing::Test {
  protected:
    // A plugin of an app with a Lua module of its own, which tells how many updates the plugin saw.
    class CounterPlugin final : public plugins::Plugin {
      public:
        [[nodiscard]] std::string_view getName() const noexcept override {
            return "counter";
        }
        void update(core::Engine&, float) override {
            ++updates;
        }
        void installLua(core::Engine&, lua_State* L) override {
            lua::Binding::preload(L, "game.counter", &open);
        }

      private:
        static int getUpdates(lua_State* L) {
            lua_pushinteger(L, lua::Runtime::getEngine(L).getPlugin<CounterPlugin>().updates);
            return 1;
        }

        static int open(lua_State* L) {
            const luaL_Reg functions[] = {{"updates", &getUpdates}, {nullptr, nullptr}};
            lua::Binding::newModule(L, functions);
            return 1;
        }

        int updates = 0;
    };

    // Adds the plugin before the Lua of the app runs, the way a C++ app keeps a Lua application.
    class PluginApplication final : public core::Application {
      public:
        void start(core::Engine& engine) override {
            engine.addPlugin(std::make_unique<CounterPlugin>());
            application.start(engine);
        }
        void stop(core::Engine& engine) override {
            application.stop(engine);
        }

      private:
        lua::Application application;
    };
};

TEST_F(AppPluginTest, RunsAPluginOfTheAppWithItsLuaModule) {
    test::EngineFixture fixture({{"source/main.lua", "counter = require('game.counter') before = counter.updates()"}}, std::make_unique<PluginApplication>());
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return before .. ' ' .. counter.updates()"), "0 3");
    EXPECT_NE(fixture.engine().getPlugins().find("counter"), nullptr);
}

} // namespace haylen::platform
