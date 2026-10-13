#pragma once

#include <cstddef>
#include <vector>

#include "haylen/core/Json.hpp"

struct lua_State;

namespace haylen::net {

// Adds the MessagePack functions to `haylen.net`: `encodeMessagePack` and `decodeMessagePack` on the frame thread, and `decodeMessagePackAsync`, which decodes on the task pool and settles a promise with the value, so a large message never holds a frame up.
class MessagePackLua final {
  public:
    // Sets the functions on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    // Turns every reference of a `haylen.Bytes` value into a binary value.
    static void embedBytes(core::Json& value, std::vector<std::vector<std::byte>>& buffers);

    static int encodeMessagePack(lua_State* L);
    static int decodeMessagePack(lua_State* L);
    static int decodeMessagePackAsync(lua_State* L);
};

} // namespace haylen::net
