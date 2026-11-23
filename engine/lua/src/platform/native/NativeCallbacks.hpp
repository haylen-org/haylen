#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "haylen/lua/Reference.hpp"
#include "haylen/platform/native/NativeCallback.hpp"

struct lua_State;

namespace haylen::lua::platform {

// The Lua functions behind the native callbacks of one app, which run on the frame thread with the copied arguments. The app drops them when it stops, so late calls find nothing to run.
class NativeCallbacks final : public haylen::platform::NativeCallback::Receiver {
  public:
    explicit NativeCallbacks(lua_State* L);

    [[nodiscard]] std::uint64_t add(lua::Reference function);
    void remove(std::uint64_t id);

    // Runs the function of a callback with the arguments. A failure reaches the error screen like any other script error.
    void deliver(std::uint64_t id, const std::vector<haylen::platform::NativeCallback::Value>& values) override;

    // Reports a call whose arguments could not be read.
    void fail(std::uint64_t id, const std::string& message) override;

    [[nodiscard]] std::size_t size() const noexcept {
        return functions.size();
    }

  private:
    static void push(lua_State* L, const haylen::platform::NativeCallback::Value& value);

    lua_State* state = nullptr;
    std::unordered_map<std::uint64_t, lua::Reference> functions;
    std::uint64_t nextId = 1;
};

} // namespace haylen::lua::platform
