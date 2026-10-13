#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "haylen/input/CommandBuffer.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<input::CommandBuffer> {
    static constexpr const char* name = "haylen.CommandBuffer";
    using Storage = input::CommandBuffer;
};

} // namespace haylen::lua

namespace haylen::input {

// Installs the `CommandBuffer` class of `haylen.input` and `input.newCommandBuffer`. The user value of a buffer holds `names`, the command names in command order, so reading the kept command pushes a string that already exists.
class CommandBufferLua final {
  public:
    static void install(lua_State* L);

    // Sets `newCommandBuffer` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 5> kOptions{"buttons", "lifetime", "lifetimes", "directions", "commands"};
    static constexpr std::array<std::string_view, 6> kCommandOptions{"name", "steps", "maxTicks", "priority", "lifetime", "clearsHistory"};
    static constexpr std::array<std::string_view, 2> kDirectionOptions{"back", "forward"};
    static constexpr std::uint32_t kDefaultLifetime = 1;

    // Reads a lifetime in steps, where `false` keeps a command until it is consumed or replaced.
    [[nodiscard]] static std::optional<std::uint32_t> readLifetime(lua_State* L, int index);
    [[nodiscard]] static std::size_t readButton(lua_State* L, const CommandBuffer::Options& options, int index, const char* command);
    // Reads the buttons of a step from an action map or from an integer mask.
    template <void (CommandBuffer::*FromMask)(std::uint32_t) noexcept, void (CommandBuffer::*FromMap)(const ActionMap&) noexcept> static int feed(lua_State* L);
    [[nodiscard]] static std::size_t checkCommand(lua_State* L, const CommandBuffer& buffer, int index);

    static int newCommandBuffer(lua_State* L);
    static int peek(lua_State* L);
    static int consume(lua_State* L);
    static int held(lua_State* L);
    static int clear(lua_State* L);
    static int frozen(lua_State* L);
    static int setFrozen(lua_State* L);
    static int facing(lua_State* L);
    static int setFacing(lua_State* L);
    static int tick(lua_State* L);
};

} // namespace haylen::input
