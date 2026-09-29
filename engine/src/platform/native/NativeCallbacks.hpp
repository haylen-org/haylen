#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "haylen/lua/Reference.hpp"
#include "platform/native/NativeCallback.hpp"

struct lua_State;

namespace haylen::platform {

// The Lua functions behind the native callbacks of one app, which run on the frame thread with the copied arguments. The app drops them when it stops, so late calls find nothing to run.
class NativeCallbacks final {
  public:
    explicit NativeCallbacks(lua_State* L);

    [[nodiscard]] std::uint64_t add(lua::Reference function);
    void remove(std::uint64_t id);

    // Runs the function of a callback with the arguments. A failure reaches the error screen like any other script error.
    void deliver(std::uint64_t id, const std::vector<NativeCallback::Value>& values);

    // Reports a call whose arguments could not be read.
    void fail(std::uint64_t id, const std::string& message);

    [[nodiscard]] std::size_t size() const noexcept {
        return functions.size();
    }

  private:
    static void push(lua_State* L, const NativeCallback::Value& value);

    lua_State* state = nullptr;
    std::unordered_map<std::uint64_t, lua::Reference> functions;
    std::uint64_t nextId = 1;
};

} // namespace haylen::platform
