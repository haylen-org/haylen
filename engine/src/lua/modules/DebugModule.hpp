#pragma once

#include <memory>
#include <string>
#include <string_view>

#include "haylen/core/Connection.hpp"
#include "haylen/lua/Module.hpp"

namespace haylen::debug {
class ScriptInspector;
}

namespace haylen::lua::modules {

// Installs `haylen.debug` and gives the debug plugin what it shows of the Lua state: the memory of the state, its named signals with their listeners and its values for the inspector, and in development the console command `lua`, which runs Lua code in the app. It takes them back when the app stops.
class DebugModule final : public Module {
  public:
    DebugModule();
    ~DebugModule() override;

    DebugModule(const DebugModule&) = delete;
    DebugModule& operator=(const DebugModule&) = delete;

    [[nodiscard]] std::string_view getName() const noexcept override {
        return "haylen.debug";
    }
    void install(core::Engine& engine, lua_State* L) override;
    void stop(core::Engine& engine) override;

  private:
    [[nodiscard]] static std::string run(lua_State* L, const std::string& chunk);

    std::unique_ptr<debug::ScriptInspector> inspector;
    core::Connection command;
};

} // namespace haylen::lua::modules
