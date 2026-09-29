#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/lua/Reference.hpp"

struct lua_State;

namespace haylen::platform {
struct Event;
}

namespace haylen::core {
class Engine;
}

namespace haylen::lua {

// The autoloads of an app: Lua modules that load before the first scene and live for the whole app. Each one is the table its module returns, which require returns again and haylen.autoloads.<name> holds. It receives start, event, fixedUpdate, update, render, renderUi and stop when it defines them, and its processMode field decides whether it runs while the game is paused.
class Autoloads final {
  public:
    // Loads the module, keeps its table under the name and calls its start. A name can only be taken once.
    void add(lua_State* L, const std::string& name, const std::string& module);

    // Names an autoload after its module, with the last part in camel case, so state.player-data becomes playerData.
    [[nodiscard]] static std::string getName(std::string_view module);

    // Pushes the table that maps names to autoload tables, which is haylen.autoloads.
    static void pushTable(lua_State* L);

    void event(core::Engine& engine, const platform::Event& event);
    void fixedUpdate(core::Engine& engine, float stepSeconds);
    void update(core::Engine& engine, float deltaSeconds);
    void render();
    void renderUi();

    // Calls stop on every autoload, the last one first, and lets go of them.
    void stop(core::Engine& engine);

  private:
    struct Entry {
        std::string name;
        Reference table;
    };

    static constexpr const char* kTable = "haylen.autoloads";

    [[nodiscard]] static bool canProcess(core::Engine& engine, const Entry& entry);
    static void call(const Entry& entry, const char* method, int arguments = 0, const std::function<void(lua_State*)>& pushArguments = {});

    std::vector<Entry> entries;
};

} // namespace haylen::lua
