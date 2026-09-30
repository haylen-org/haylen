#pragma once

#include <lua.hpp>

#include <array>
#include <string_view>

namespace haylen::math {

class Random;

// Installs the `ShuffleBag` and `WeightedChoice` classes of `haylen.math`, which deal the Lua values they were made with.
class ChoiceLua final {
  public:
    static void install(lua_State* L);

    // Sets `shuffleBag` and `weightedChoice` on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 2> kBagFields{"counts", "seed"};

    // Returns the generator passed at `index`, or the one the object owns.
    [[nodiscard]] static Random& generatorOf(lua_State* L, int index, Random& own);
    static void pushItem(lua_State* L, std::size_t item);

    static int newShuffleBag(lua_State* L);
    static int bagNext(lua_State* L);
    static int bagRefill(lua_State* L);
    static int bagRemaining(lua_State* L);
    static int bagSize(lua_State* L);

    static int newWeightedChoice(lua_State* L);
    static int choicePick(lua_State* L);
    static int choiceProbability(lua_State* L);
    static int choiceSize(lua_State* L);
};

} // namespace haylen::math
