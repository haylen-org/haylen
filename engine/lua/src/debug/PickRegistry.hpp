#pragma once

#include <lua.hpp>

#include <cstdint>

#include "haylen/debug/Picking.hpp"

namespace haylen::lua::debug {

// Turns the tags of `debug::Picking` into the Lua values of the app, for the root `selected` of the inspector. While a pick looks for its hit, every binding that draws a thing that can be picked records the Lua value of the owner of its tag, with a resolver for the items inside it, such as the nodes of a tree. Nothing here knows a dimension, so the drawers of 2D, of 3D and of plugins record their values the same way. The values stay weak, so a pick never keeps a value of the app alive.
class PickRegistry final {
  public:
    // Records the value at index as the owner of the tags of `owner`, with the C function that takes the owner and the item of a tag and returns the thing they name, or null for an owner that is the thing itself.
    static void remember(lua_State* L, int index, std::uint64_t owner, lua_CFunction resolver);

    // Makes the thing of the tag the selection and returns `true`, or clears the selection and returns `false` when no value of the app answers to the tag. Either way the values that the pick recorded go.
    static bool select(lua_State* L, const haylen::debug::Picking::Tag& tag);
    static void clear(lua_State* L);

    // Pushes the selected thing, or `nil` while nothing is selected or it is gone.
    static void pushSelected(lua_State* L);

    // The owner of the tags of an object, which its address names while it lives.
    [[nodiscard]] static std::uint64_t ownerOf(const void* address) noexcept;

  private:
    static constexpr const char* kValuesKey = "haylen.debug.pickValues";
    static constexpr const char* kResolversKey = "haylen.debug.pickResolvers";
    static constexpr const char* kSelectionKey = "haylen.debug.selection";

    static void forget(lua_State* L);
};

} // namespace haylen::lua::debug
