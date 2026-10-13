#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/graphics/TileLayer.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::lua {

template <> struct Type<graphics2d::TileLayer> {
    static constexpr const char* name = "haylen.TileLayer";
    using Storage = std::shared_ptr<graphics2d::TileLayer>;
};

} // namespace haylen::lua

namespace haylen::graphics2d {

// Installs the `TileLayer` class of `haylen.graphics2d`, whose textures count from one.
class TileLayerLua final {
  public:
    static void install(lua_State* L);

    // Creates a layer with `graphics2d.newTileLayer(textures, {cellWidth, cellHeight, chunkSize})`, from one texture or a list of them.
    static int create(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 3> kOptionFields{"cellWidth", "cellHeight", "chunkSize"};
    static constexpr std::array<std::string_view, 6> kFrameFields{"texture", "source", "offsetX", "offsetY", "width", "height"};

    [[nodiscard]] static TileLayer& check(lua_State* L);
    static int defineFrame(lua_State* L);
    static int setCell(lua_State* L);
    static int setCells(lua_State* L);
    static int cell(lua_State* L);
    static int clear(lua_State* L);
    static int draw(lua_State* L);
    static int chunkCount(lua_State* L);
    static int batchCount(lua_State* L);
};

} // namespace haylen::graphics2d
