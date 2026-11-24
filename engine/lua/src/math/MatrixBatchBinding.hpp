#pragma once

#include <lua.hpp>

namespace haylen::lua::math {

// Installs the `matrixBatch` table of `haylen.math`, which multiplies and applies lists of matrices in one call.
class MatrixBatchBinding final {
  public:
    // Sets `matrixBatch` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static int multiply(lua_State* L);
    static int multiplyPairs(lua_State* L);
    static int transformPoints(lua_State* L);
};

} // namespace haylen::lua::math
