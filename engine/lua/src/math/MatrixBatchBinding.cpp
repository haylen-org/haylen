#include "math/MatrixBatchBinding.hpp"

#include <string_view>
#include <vector>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/MatrixBatch.hpp"

namespace haylen::lua::math {

// Returns the list of `parent * matrix` for each matrix of the list.
int MatrixBatchBinding::multiply(lua_State* L) {
    const auto parent = lua::Stack::read<haylen::math::Mat4>(L, 1);
    std::vector<haylen::math::Mat4> matrices = lua::Stack::read<std::vector<haylen::math::Mat4>>(L, 2);
    haylen::math::MatrixBatch::multiply(parent, matrices, matrices);
    lua::Stack::push(L, matrices);
    return 1;
}

int MatrixBatchBinding::multiplyPairs(lua_State* L) {
    const std::vector<haylen::math::Mat4> lhs = lua::Stack::read<std::vector<haylen::math::Mat4>>(L, 1);
    std::vector<haylen::math::Mat4> rhs = lua::Stack::read<std::vector<haylen::math::Mat4>>(L, 2);
    if (rhs.size() != lhs.size()) {
        return luaL_error(L, "The two lists of matrices must hold as many matrices each, but they hold %d and %d.", static_cast<int>(lhs.size()), static_cast<int>(rhs.size()));
    }

    haylen::math::MatrixBatch::multiplyPairs(lhs, rhs, rhs);
    lua::Stack::push(L, rhs);
    return 1;
}

int MatrixBatchBinding::transformPoints(lua_State* L) {
    const auto matrix = lua::Stack::read<haylen::math::Mat4>(L, 1);
    std::vector<haylen::math::Vec3> points = lua::Stack::read<std::vector<haylen::math::Vec3>>(L, 2);
    haylen::math::MatrixBatch::transformPoints(matrix, points, points);
    lua::Stack::push(L, points);
    return 1;
}

void MatrixBatchBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"multiply", &lua::Binding::native<&multiply>},
        {"multiplyPairs", &lua::Binding::native<&multiplyPairs>},
        {"transformPoints", &lua::Binding::native<&transformPoints>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    const std::string_view instructions = haylen::math::MatrixBatch::getInstructions();
    lua_pushlstring(L, instructions.data(), instructions.size());
    lua_setfield(L, -2, "instructions");
    lua_setfield(L, -2, "matrixBatch");
}

} // namespace haylen::lua::math
