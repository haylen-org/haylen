#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>

#include "haylen/ai/ResponseCurve.hpp"
#include "haylen/ai/UtilitySelector.hpp"

namespace haylen::ai {

// Installs the UtilitySelector class of haylen.ai, whose considerations read Lua functions of a context value.
class UtilityLua final {
  public:
    // A selector created from Lua. Its inputs run on the Lua thread that asks for a choice, with the selector at stack index 1 and the context at index 2.
    struct Scripted {
        UtilitySelector selector;
        lua_State* caller = nullptr;
    };

    static void install(lua_State* L);

    // Sets newUtilitySelector on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    class CallerScope;

    static constexpr std::array<std::string_view, 3> kOptionFields{"name", "weight", "considerations"};
    static constexpr std::array<std::string_view, 5> kConsiderationFields{"name", "input", "minimum", "maximum", "curve"};
    static constexpr std::array<std::string_view, 5> kCurveFields{"shape", "slope", "exponent", "shift", "offset"};

    [[nodiscard]] static ResponseCurve readCurve(lua_State* L, int index);
    [[nodiscard]] static float callInput(Scripted& self, lua_Integer input);
    // Finds an option by its 1-based index or its name.
    [[nodiscard]] static std::size_t readOption(lua_State* L, int index, const UtilitySelector& selector);

    static int newSelector(lua_State* L);
    static int choose(lua_State* L);
    static int score(lua_State* L);
    static int options(lua_State* L);
};

} // namespace haylen::ai
