#pragma once

#include <map>
#include <memory>
#include <string>

#include "haylen/core/Application.hpp"
#include "support/EngineFixture.hpp"

struct lua_State;

namespace haylen::test {

// A running engine on the headless host with an in-memory package, which runs a Lua application unless the test gives another one, with an empty `source/main.lua` unless the test gives one.
class LuaFixture final : public EngineFixture {
  public:
    explicit LuaFixture(std::map<std::string, std::string> files = {}, std::unique_ptr<core::Application> application = nullptr);
    LuaFixture(std::map<std::string, std::string> files, std::unique_ptr<core::Application> application, Options options);

    // The Lua state of the running app.
    [[nodiscard]] lua_State* lua() noexcept;

    // Runs Lua and returns its first result converted to a string, or the error message prefixed with `error: `.
    std::string lua(const std::string& source);
    void runLua(const std::string& source);

  private:
    [[nodiscard]] static std::map<std::string, std::string> withMain(std::map<std::string, std::string> files);
};

} // namespace haylen::test
