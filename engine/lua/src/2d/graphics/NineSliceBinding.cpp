#include "2d/graphics/NineSliceBinding.hpp"

#include <algorithm>
#include <vector>

#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua::graphics2d {

int NineSliceBinding::getPieces(lua_State* L) {
    const haylen::graphics2d::NineSlice& slice = lua::Userdata::check<haylen::graphics2d::NineSlice>(L, 1);
    lua::Stack::push(L, std::vector<haylen::math::Rect>(slice.pieces.begin(), slice.pieces.end()));
    return 1;
}

int NineSliceBinding::setPieces(lua_State* L) {
    haylen::graphics2d::NineSlice& slice = lua::Userdata::check<haylen::graphics2d::NineSlice>(L, 1);
    const std::vector<haylen::math::Rect> pieces = lua::Stack::read<std::vector<haylen::math::Rect>>(L, 3);
    luaL_argcheck(L, pieces.size() == 9, 3, "a nine-slice needs exactly nine pieces");
    std::copy(pieces.begin(), pieces.end(), slice.pieces.begin());
    return 0;
}

// Returns the border sizes as `{left, top, right, bottom}`, the form `newNineSlice` accepts.
int NineSliceBinding::getBorders(lua_State* L) {
    const haylen::math::Insets borders = lua::Userdata::check<haylen::graphics2d::NineSlice>(L, 1).getBorders();
    lua::Stack::push(L, std::vector<float>{borders.left, borders.top, borders.right, borders.bottom});
    return 1;
}

int NineSliceBinding::isValid(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<haylen::graphics2d::NineSlice>(L, 1).isValid());
    return 1;
}

// Returns the quads `drawNineSlice` draws for an area with `slice:layout(rect, borderScale)`, each as `{area = rect, source = rect}`.
int NineSliceBinding::layout(lua_State* L) {
    const haylen::graphics2d::NineSlice& slice = lua::Userdata::check<haylen::graphics2d::NineSlice>(L, 1);
    std::vector<haylen::graphics2d::NineSlice::Patch> patches;
    slice.layout(lua::Stack::read<haylen::math::Rect>(L, 2), lua::Converter<float>::readOr(L, 3, 1.0), patches);
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

void NineSliceBinding::install(lua_State* L) {
    lua::ClassBuilder<haylen::graphics2d::NineSlice>(L).field<&haylen::graphics2d::NineSlice::texture>("texture").property("pieces", &getPieces, &lua::Binding::native<&setPieces>).field<&haylen::graphics2d::NineSlice::fill>("fill").property("borders", &getBorders).property("valid", &isValid).function("layout", &lua::Binding::native<&layout>).install();
}

} // namespace haylen::lua::graphics2d
