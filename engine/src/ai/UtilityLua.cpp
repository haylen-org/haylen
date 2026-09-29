#include "ai/UtilityLua.hpp"

#include <optional>
#include <string>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/EnumNames.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/math/Random.hpp"

namespace haylen::lua {

template <> struct Type<ai::UtilityLua::Scripted> {
    static constexpr const char* name = "haylen.UtilitySelector";
    using Storage = ai::UtilityLua::Scripted;
};

template <> struct EnumNames<ai::ResponseCurve::Shape> {
    static std::optional<ai::ResponseCurve::Shape> fromName(std::string_view name) {
        return ai::ResponseCurve::shapeFromName(name);
    }
    static std::string_view name(ai::ResponseCurve::Shape value) {
        return ai::ResponseCurve::shapeName(value);
    }
};

} // namespace haylen::lua

namespace haylen::ai {

// Lends the calling thread to the inputs for the length of one call.
class UtilityLua::CallerScope final {
  public:
    CallerScope(Scripted& selector, lua_State* L) : owner(selector) {
        owner.caller = L;
    }
    ~CallerScope() {
        owner.caller = nullptr;
    }

    CallerScope(const CallerScope&) = delete;
    CallerScope& operator=(const CallerScope&) = delete;

  private:
    Scripted& owner;
};

ResponseCurve UtilityLua::readCurve(lua_State* L, int index) {
    ResponseCurve curve;
    if (lua_isnoneornil(L, index)) {
        return curve;
    }
    luaL_checktype(L, index, LUA_TTABLE);
    lua::Table::checkFields(L, index, {kCurveFields});
    lua::Table::readField(L, index, "shape", curve.shape);
    lua::Table::readField(L, index, "slope", curve.slope);
    lua::Table::readField(L, index, "exponent", curve.exponent);
    lua::Table::readField(L, index, "shift", curve.shift);
    lua::Table::readField(L, index, "offset", curve.offset);
    return curve;
}

float UtilityLua::callInput(Scripted& self, lua_Integer input) {
    lua_State* L = self.caller;
    lua::Userdata::pushField(L, 1, "inputs");
    lua_rawgeti(L, -1, input);
    lua_remove(L, -2);
    lua_pushvalue(L, 2);
    lua_call(L, 1, 1);
    const auto value = static_cast<float>(luaL_checknumber(L, -1));
    lua_pop(L, 1);
    return value;
}

std::size_t UtilityLua::readOption(lua_State* L, int index, const UtilitySelector& selector) {
    const std::vector<UtilitySelector::Option>& options = selector.getOptions();
    if (lua_type(L, index) == LUA_TSTRING) {
        const std::string_view name = lua::Stack::read<std::string_view>(L, index);
        for (std::size_t option = 0; option < options.size(); ++option) {
            if (options[option].name == name) {
                return option;
            }
        }
        luaL_argerror(L, index, "unknown option");
    }
    const auto option = lua::Stack::read<std::size_t>(L, index);
    luaL_argcheck(L, option >= 1 && option <= options.size(), index, "no option with this number");
    return option - 1;
}

// Builds a selector with newUtilitySelector({{name, weight, considerations = {{name, input = function(context) end, minimum, maximum, curve}}}}).
int UtilityLua::newSelector(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    Scripted& self = lua::Userdata::emplace<Scripted>(L);
    const int owner = lua_gettop(L);
    lua_newtable(L);
    const int inputs = lua_gettop(L);
    lua::Userdata::setField(L, owner, "inputs", inputs);
    Scripted* scripted = &self;

    const lua_Integer optionCount = luaL_len(L, 1);
    for (lua_Integer entry = 1; entry <= optionCount; ++entry) {
        lua_rawgeti(L, 1, entry);
        const int table = lua_gettop(L);
        luaL_checktype(L, table, LUA_TTABLE);
        lua::Table::checkFields(L, table, {kOptionFields});
        UtilitySelector::Option option;
        lua::Table::readField(L, table, "name", option.name);
        lua::Table::readField(L, table, "weight", option.weight);

        lua_getfield(L, table, "considerations");
        luaL_checktype(L, -1, LUA_TTABLE);
        const lua_Integer considerationCount = luaL_len(L, -1);
        for (lua_Integer factor = 1; factor <= considerationCount; ++factor) {
            lua_rawgeti(L, -1, factor);
            const int description = lua_gettop(L);
            luaL_checktype(L, description, LUA_TTABLE);
            lua::Table::checkFields(L, description, {kConsiderationFields});
            UtilitySelector::Consideration& consideration = option.considerations.emplace_back();
            lua::Table::readField(L, description, "name", consideration.name);
            lua::Table::readField(L, description, "minimum", consideration.minimum);
            lua::Table::readField(L, description, "maximum", consideration.maximum);
            lua_getfield(L, description, "curve");
            consideration.curve = readCurve(L, -1);
            lua_pop(L, 1);

            lua_getfield(L, description, "input");
            luaL_checktype(L, -1, LUA_TFUNCTION);
            const lua_Integer input = luaL_len(L, inputs) + 1;
            lua_rawseti(L, inputs, input);
            consideration.input = [scripted, input] { return callInput(*scripted, input); };
            lua_pop(L, 1);
        }
        lua_settop(L, inputs);
        self.selector.add(std::move(option));
    }
    lua_settop(L, owner);
    return 1;
}

// Chooses with choose(context), or at random among the options scoring at least tolerance times the best with choose(context, random, tolerance), and returns the name and score of the choice or nil.
int UtilityLua::choose(lua_State* L) {
    Scripted& self = lua::Userdata::check<Scripted>(L, 1);
    lua_settop(L, 4);
    const CallerScope caller(self, L);

    const std::optional<UtilitySelector::Choice> choice = lua_isnil(L, 3) ? self.selector.choose() : self.selector.choose(lua::Userdata::check<math::Random>(L, 3), static_cast<float>(luaL_optnumber(L, 4, 0.9)));
    if (!choice) {
        lua_pushnil(L);
        return 1;
    }
    lua::Stack::push(L, self.selector.getOptions()[choice->index].name);
    lua::Stack::push(L, choice->score);
    return 2;
}

// Scores one option with score(option, context), where the option is its name or number.
int UtilityLua::score(lua_State* L) {
    Scripted& self = lua::Userdata::check<Scripted>(L, 1);
    const std::size_t option = readOption(L, 2, self.selector);
    lua_settop(L, 3);
    lua_remove(L, 2);
    const CallerScope caller(self, L);
    lua::Stack::push(L, self.selector.score(option));
    return 1;
}

int UtilityLua::options(lua_State* L) {
    const Scripted& self = lua::Userdata::check<Scripted>(L, 1);
    lua_createtable(L, static_cast<int>(self.selector.size()), 0);
    for (std::size_t option = 0; option < self.selector.size(); ++option) {
        lua::Stack::push(L, self.selector.getOptions()[option].name);
        lua_rawseti(L, -2, static_cast<lua_Integer>(option + 1));
    }
    return 1;
}

void UtilityLua::install(lua_State* L) {
    lua::ClassBuilder<Scripted>(L).function("choose", &lua::Binding::native<&choose>).function("score", &lua::Binding::native<&score>).property("options", &options).install();
}

void UtilityLua::addFunctions(lua_State* L) {
    lua_pushcfunction(L, &lua::Binding::native<&newSelector>);
    lua_setfield(L, -2, "newUtilitySelector");
}

} // namespace haylen::ai
