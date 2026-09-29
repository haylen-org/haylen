#include "math/ChoiceLua.hpp"

#include <cstdint>
#include <optional>
#include <vector>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Random.hpp"
#include "haylen/math/ShuffleBag.hpp"
#include "haylen/math/WeightedChoice.hpp"

namespace haylen::lua {

template <> struct Type<math::ShuffleBag> {
    static constexpr const char* name = "haylen.ShuffleBag";
    using Storage = math::ShuffleBag;
};

template <> struct Type<math::WeightedChoice> {
    static constexpr const char* name = "haylen.WeightedChoice";
    using Storage = math::WeightedChoice;
};

} // namespace haylen::lua

namespace haylen::math {

// Each object keeps its items and its own generator in its user value, and a generator passed to a draw takes the place of its own.
Random& ChoiceLua::generatorOf(lua_State* L, int index, Random& own) {
    return lua_isnoneornil(L, index) ? own : lua::Userdata::check<Random>(L, index);
}

void ChoiceLua::pushItem(lua_State* L, std::size_t item) {
    lua::Userdata::pushField(L, 1, "items");
    lua_rawgeti(L, -1, static_cast<lua_Integer>(item + 1));
    lua_remove(L, -2);
}

// Creates a bag with shuffleBag(items, {counts = {...}, seed = n}), where item i goes into the bag counts[i] times.
int ChoiceLua::newShuffleBag(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const auto size = static_cast<std::size_t>(luaL_len(L, 1));
    std::vector<std::uint32_t> counts(size, 1);
    std::optional<lua_Integer> seed;
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
        lua::Table::checkFields(L, 2, {kBagFields});
        lua::Table::readField(L, 2, "counts", counts);
        lua::Table::readField(L, 2, "seed", seed);
    }
    if (counts.size() != size) {
        return luaL_argerror(L, 2, "expected one count per item");
    }

    lua::Userdata::emplace<ShuffleBag>(L, counts);
    const int bag = lua_gettop(L);
    lua::Userdata::setField(L, bag, "items", 1);
    if (seed) {
        lua::Userdata::emplace<Random>(L, static_cast<std::uint64_t>(*seed));
    } else {
        lua::Userdata::emplace<Random>(L);
    }
    lua::Userdata::setField(L, bag, "random", -1);
    lua_settop(L, bag);
    return 1;
}

int ChoiceLua::bagNext(lua_State* L) {
    ShuffleBag& bag = lua::Userdata::check<ShuffleBag>(L, 1);
    lua::Userdata::pushField(L, 1, "random");
    Random& own = lua::Userdata::check<Random>(L, -1);
    pushItem(L, bag.next(generatorOf(L, 2, own)));
    return 1;
}

int ChoiceLua::bagRefill(lua_State* L) {
    lua::Userdata::check<ShuffleBag>(L, 1).refill();
    return 0;
}

int ChoiceLua::bagRemaining(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ShuffleBag>(L, 1).getRemaining());
    return 1;
}

int ChoiceLua::bagSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ShuffleBag>(L, 1).size());
    return 1;
}

// Creates a choice with weightedChoice(items, weights, seed), where the seed is optional.
int ChoiceLua::newWeightedChoice(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    const auto weights = lua::Stack::read<std::vector<float>>(L, 2);
    if (weights.size() != static_cast<std::size_t>(luaL_len(L, 1))) {
        return luaL_argerror(L, 2, "expected one weight per item");
    }

    lua::Userdata::emplace<WeightedChoice>(L, weights);
    const int choice = lua_gettop(L);
    lua::Userdata::setField(L, choice, "items", 1);
    if (lua_isnoneornil(L, 3)) {
        lua::Userdata::emplace<Random>(L);
    } else {
        lua::Userdata::emplace<Random>(L, static_cast<std::uint64_t>(luaL_checkinteger(L, 3)));
    }
    lua::Userdata::setField(L, choice, "random", -1);
    lua_settop(L, choice);
    return 1;
}

int ChoiceLua::choicePick(lua_State* L) {
    const WeightedChoice& choice = lua::Userdata::check<WeightedChoice>(L, 1);
    lua::Userdata::pushField(L, 1, "random");
    Random& own = lua::Userdata::check<Random>(L, -1);
    pushItem(L, choice.pick(generatorOf(L, 2, own)));
    return 1;
}

int ChoiceLua::choiceProbability(lua_State* L) {
    const auto item = lua::Stack::read<std::size_t>(L, 2);
    luaL_argcheck(L, item >= 1, 2, "expected an item number from 1");
    lua::Stack::push(L, lua::Userdata::check<WeightedChoice>(L, 1).getProbability(item - 1));
    return 1;
}

int ChoiceLua::choiceSize(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<WeightedChoice>(L, 1).size());
    return 1;
}

void ChoiceLua::install(lua_State* L) {
    lua::ClassBuilder<ShuffleBag>(L).function("next", &lua::Binding::native<&bagNext>).function("refill", &lua::Binding::native<&bagRefill>).property("remaining", &bagRemaining).property("size", &bagSize).install();
    lua::ClassBuilder<WeightedChoice>(L).function("pick", &lua::Binding::native<&choicePick>).function("probability", &lua::Binding::native<&choiceProbability>).property("size", &choiceSize).install();
}

void ChoiceLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newShuffleBag>);
    lua_setfield(L, -2, "shuffleBag");
    lua_pushcfunction(L, &lua::Binding::native<&newWeightedChoice>);
    lua_setfield(L, -2, "weightedChoice");
}

} // namespace haylen::math
