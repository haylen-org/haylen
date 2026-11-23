#include <gtest/gtest.h>

#include <lua.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Application.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/lua/Application.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Module.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/ScriptPlugin.hpp"
#include "haylen/plugins/Plugin.hpp"
#include "support/LuaFixture.hpp"

namespace haylen::platform {

class AppPluginTest : public ::testing::Test {
  protected:
    // A plugin of an app, which counts the updates it saw.
    class CounterPlugin final : public plugins::Plugin {
      public:
        [[nodiscard]] std::string_view getName() const noexcept override {
            return "counter";
        }
        void update(core::Engine&, float) override {
            ++updates;
        }

        int updates = 0;
    };

    // The Lua module of the plugin, which tells Lua how many updates the plugin saw.
    class CounterModule final : public lua::Module {
      public:
        [[nodiscard]] std::string_view getName() const noexcept override {
            return "game.counter";
        }
        void install(core::Engine&, lua_State* L) override {
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
    };

    // A module that records the order in which the modules installed.
    class OrderModule final : public lua::Module {
      public:
        OrderModule(std::string moduleName, std::vector<std::string>& installed) : name(std::move(moduleName)), order(installed) {}

        [[nodiscard]] std::string_view getName() const noexcept override {
            return name;
        }
        void install(core::Engine&, lua_State*) override {
            order.push_back(name);
        }

      private:
        std::string name;
        std::vector<std::string>& order;
    };

    // Adds the plugin and its module before the Lua of the app runs, the way a C++ app keeps a Lua application. The script plugin runs already, so the module installs at once.
    class PluginApplication final : public core::Application {
      public:
        void addPlugins(core::Engine& engine) override {
            application.addPlugins(engine);
        }
        void start(core::Engine& engine) override {
            engine.addPlugin(std::make_unique<CounterPlugin>());
            engine.getPlugin<lua::ScriptPlugin>().addModule(std::make_unique<CounterModule>());
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
    test::LuaFixture fixture({{"source/main.lua", "counter = require('game.counter') before = counter.updates()"}}, std::make_unique<PluginApplication>());
    fixture.frames(3);
    EXPECT_EQ(fixture.lua("return before .. ' ' .. counter.updates()"), "0 3");
    EXPECT_NE(fixture.engine().getPlugins().find("counter"), nullptr);
}

// The modules of the engine install in order when the script plugin starts, and a module added later installs at once, after them.
TEST_F(AppPluginTest, InstallsModulesInOrderAndAModuleAddedAfterTheStartAtOnce) {
    std::vector<std::string> installed;
    test::LuaFixture fixture;
    lua::ScriptPlugin& script = fixture.engine().getPlugin<lua::ScriptPlugin>();
    script.addModule(std::make_unique<OrderModule>("game.first", installed));
    script.addModule(std::make_unique<OrderModule>("game.second", installed));
    EXPECT_EQ(installed, (std::vector<std::string>{"game.first", "game.second"}));
    EXPECT_THROW(script.addModule(std::make_unique<OrderModule>("game.first", installed)), std::invalid_argument);

    script.addModule(std::make_unique<CounterModule>());
    fixture.engine().addPlugin(std::make_unique<CounterPlugin>());
    fixture.frames(2);
    EXPECT_EQ(fixture.lua("return require('game.counter').updates()"), "2");
}

} // namespace haylen::platform
