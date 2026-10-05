#include "2d/graphics/NineSliceLua.hpp"

#include <algorithm>
#include <vector>

#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::graphics2d {

int NineSliceLua::getPieces(lua_State* L) {
    const NineSlice& slice = lua::Userdata::check<NineSlice>(L, 1);
    lua::Stack::push(L, std::vector<math::Rect>(slice.pieces.begin(), slice.pieces.end()));
    return 1;
}

int NineSliceLua::setPieces(lua_State* L) {
    NineSlice& slice = lua::Userdata::check<NineSlice>(L, 1);
    const std::vector<math::Rect> pieces = lua::Stack::read<std::vector<math::Rect>>(L, 3);
    luaL_argcheck(L, pieces.size() == 9, 3, "a nine-slice needs exactly nine pieces");
    std::copy(pieces.begin(), pieces.end(), slice.pieces.begin());
    return 0;
}

// Returns the border sizes as `{left, top, right, bottom}`, the form `newNineSlice` accepts.
int NineSliceLua::getBorders(lua_State* L) {
    const math::Insets borders = lua::Userdata::check<NineSlice>(L, 1).getBorders();
    lua::Stack::push(L, std::vector<float>{borders.left, borders.top, borders.right, borders.bottom});
    return 1;
}

int NineSliceLua::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<NineSlice>(L, 1).isValid());
    return 1;
}

// Returns the quads `drawNineSlice` draws for an area with `slice:layout(rect, borderScale)`, each as `{area = rect, source = rect}`.
int NineSliceLua::layout(lua_State* L) {
    const NineSlice& slice = lua::Userdata::check<NineSlice>(L, 1);
    std::vector<NineSlice::Patch> patches;
    slice.layout(lua::Stack::read<math::Rect>(L, 2), static_cast<float>(luaL_optnumber(L, 3, 1.0)), patches);
    lua_createtable(L, static_cast<int>(patches.size()), 0);
    for (std::size_t index = 0; index < patches.size(); ++index) {
        lua_createtable(L, 0, 2);
        lua::Stack::push(L, patches[index].area);
        lua_setfield(L, -2, "area");
        lua::Stack::push(L, patches[index].source);
        lua_setfield(L, -2, "source");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

void NineSliceLua::install(lua_State* L) {
    lua::ClassBuilder<NineSlice>(L).field<&NineSlice::texture>("texture").property("pieces", &getPieces, &lua::Binding::native<&setPieces>).field<&NineSlice::fill>("fill").property("borders", &getBorders).property("valid", &isValid).function("layout", &lua::Binding::native<&layout>).install();
}

} // namespace haylen::graphics2d
