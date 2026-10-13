#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/animation/AtlasBuilder.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<animation2d::AtlasBuilder> {
    static constexpr const char* name = "haylen.AtlasBuilder";
    using Storage = std::shared_ptr<animation2d::AtlasBuilder>;
};

} // namespace haylen::lua

namespace haylen::animation2d {

// Installs the `AtlasBuilder` class of `haylen.animation2d`.
class AtlasBuilderLua final {
  public:
    static void install(lua_State* L);

    // Creates a builder with `animation2d.newAtlasBuilder()`.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 4> kOptionFields{"padding", "extrude", "maxSize", "trim"};

    [[nodiscard]] static AtlasBuilder::Options readOptions(lua_State* L, int index);
    static int add(lua_State* L);
    static int addAtlas(lua_State* L);
    static int build(lua_State* L);
    static int buildAsync(lua_State* L);
    static int count(lua_State* L);
};

} // namespace haylen::animation2d
