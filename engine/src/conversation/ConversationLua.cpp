#include "conversation/ConversationLua.hpp"

#include <lua.hpp>

#include <cstdint>
#include <memory>
#include <random>
#include <string>
#include <utility>

#include "haylen/core/Connection.hpp"
#include "haylen/core/Engine.hpp"
#include "haylen/localization/Catalog.hpp"
#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/JsonConverter.hpp"
#include "haylen/lua/Runtime.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/TypeConverter.hpp"
#include "haylen/lua/Userdata.hpp"
#include "haylen/plugins/LocalizationPlugin.hpp"
#include "lua/Owners.hpp"

namespace haylen::conversation {

void ConversationLua::pushConversation(lua_State* L, std::shared_ptr<Conversation> conversation) {
    lua::Userdata::emplace<Conversation>(L, std::move(conversation));
}

void ConversationLua::pushState(lua_State* L, std::shared_ptr<ConversationState> state) {
    lua::Userdata::emplace<ConversationState>(L, std::move(state));
}

// A key of the catalog becomes its text with the variables as arguments, and anything else is the text itself.
void ConversationLua::pushText(lua_State* L, const std::string& key, const core::Json& variables) {
    const localization::Catalog& catalog = lua::Runtime::getEngine(L).getPlugin<plugins::LocalizationPlugin>().getCatalog();
    lua::Stack::push(L, catalog.has(key) ? catalog.getText(key, variables) : key);
}

// Listeners run after the step that ran the commands returned, so a listener may advance the state.
void ConversationLua::deliver(ConversationState& state) {
    state.deliverCommands();
}

// Reads a conversation with `conversation.new(document)` from a table in the format of `.conversation` files.
int ConversationLua::create(lua_State* L) {
    pushConversation(L, std::make_shared<Conversation>(Conversation::parse(lua::JsonConverter::read(L, 1))));
    return 1;
}

// Starts a walk with `conversation:start(node, {variables, seed})`, whose variants repeat for the same seed.
int ConversationLua::start(lua_State* L) {
    const std::shared_ptr<Conversation>& graph = lua::Userdata::checkShared<Conversation>(L, 1);
    const std::string node = lua::Stack::read<std::string>(L, 2);
    core::Json values = core::Json::object();
    std::uint64_t seed = std::random_device{}();
    if (!lua_isnoneornil(L, 3)) {
        luaL_checktype(L, 3, LUA_TTABLE);
        lua::Table::checkFields(L, 3, {kStartFields});
        if (lua_getfield(L, 3, "variables") != LUA_TNIL) {
            values = lua::JsonConverter::read(L, -1);
        }
        if (lua_getfield(L, 3, "seed") != LUA_TNIL) {
            seed = static_cast<std::uint64_t>(luaL_checkinteger(L, -1));
        }
    }
    auto state = std::make_shared<ConversationState>(graph, values, seed);
    state->start(node);
    pushState(L, std::move(state));
    return 1;
}

// Resumes a walk with `conversation:restore(saved)` from what `state:save()` returned.
int ConversationLua::restore(lua_State* L) {
    const std::shared_ptr<Conversation>& graph = lua::Userdata::checkShared<Conversation>(L, 1);
    pushState(L, ConversationState::restore(graph, lua::JsonConverter::read(L, 2)));
    return 1;
}

int ConversationLua::texts(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Conversation>(L, 1).getTexts());
    return 1;
}

int ConversationLua::nodes(lua_State* L) {
    const std::vector<Conversation::Node>& all = lua::Userdata::check<Conversation>(L, 1).getNodes();
    lua_createtable(L, static_cast<int>(all.size()), 0);
    for (std::size_t index = 0; index < all.size(); ++index) {
        lua::Stack::push(L, all[index].name);
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

// Returns the line as `{speaker, text, key, tags}`, or nil without one.
int ConversationLua::line(lua_State* L) {
    const ConversationState& state = lua::Userdata::check<ConversationState>(L, 1);
    if (!state.getLine()) {
        lua_pushnil(L);
        return 1;
    }
    const ConversationState::Line& shown = *state.getLine();
    lua_createtable(L, 0, 4);
    lua::Stack::push(L, shown.speaker);
    lua_setfield(L, -2, "speaker");
    pushText(L, shown.text, state.getVariables());
    lua_setfield(L, -2, "text");
    lua::Stack::push(L, shown.text);
    lua_setfield(L, -2, "key");
    lua::Stack::push(L, shown.tags);
    lua_setfield(L, -2, "tags");
    return 1;
}

// Returns the choices that their conditions allow as `{text, key}`, empty while none waits.
int ConversationLua::choices(lua_State* L) {
    const ConversationState& state = lua::Userdata::check<ConversationState>(L, 1);
    const std::vector<ConversationState::Choice>& offered = state.getChoices();
    lua_createtable(L, static_cast<int>(offered.size()), 0);
    for (std::size_t index = 0; index < offered.size(); ++index) {
        lua_createtable(L, 0, 2);
        pushText(L, offered[index].text, state.getVariables());
        lua_setfield(L, -2, "text");
        lua::Stack::push(L, offered[index].text);
        lua_setfield(L, -2, "key");
        lua_rawseti(L, -2, static_cast<lua_Integer>(index + 1));
    }
    return 1;
}

int ConversationLua::finished(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ConversationState>(L, 1).isFinished());
    return 1;
}

int ConversationLua::variables(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<ConversationState>(L, 1).getVariables());
    return 1;
}

int ConversationLua::node(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ConversationState>(L, 1).getNodeName());
    return 1;
}

int ConversationLua::advance(lua_State* L) {
    ConversationState& state = lua::Userdata::check<ConversationState>(L, 1);
    state.advance();
    deliver(state);
    return 0;
}

// Takes a choice with `state:choose(index)`, counted from 1 among `state.choices`.
int ConversationLua::choose(lua_State* L) {
    ConversationState& state = lua::Userdata::check<ConversationState>(L, 1);
    const lua_Integer index = luaL_checkinteger(L, 2);
    if (index < 1) {
        return luaL_argerror(L, 2, "expected a choice from 1");
    }
    state.choose(static_cast<std::size_t>(index - 1));
    deliver(state);
    return 0;
}

int ConversationLua::setVariable(lua_State* L) {
    lua::Userdata::check<ConversationState>(L, 1).setVariable(lua::Stack::read<std::string>(L, 2), lua_isnoneornil(L, 3) ? core::Json() : lua::JsonConverter::read(L, 3));
    return 0;
}

int ConversationLua::visits(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<ConversationState>(L, 1).getVisits(lua::Stack::read<std::string_view>(L, 2)));
    return 1;
}

int ConversationLua::save(lua_State* L) {
    lua::JsonConverter::push(L, lua::Userdata::check<ConversationState>(L, 1).save());
    return 1;
}

// Listens with `state:on('command', fn, {owner})`, where `fn` receives the name and the arguments of every command, and the commands that ran before the first listener reach it at once.
int ConversationLua::on(lua_State* L) {
    const std::shared_ptr<ConversationState>& state = lua::Userdata::checkShared<ConversationState>(L, 1);
    const std::string_view event = lua::Stack::read<std::string_view>(L, 2);
    if (event != "command") {
        return luaL_error(L, "Unknown conversation event \"%s\". Conversations report \"command\".", std::string(event).c_str());
    }
    luaL_checktype(L, 3, LUA_TFUNCTION);
    int owner = 0;
    if (!lua_isnoneornil(L, 4)) {
        luaL_checktype(L, 4, LUA_TTABLE);
        lua::Table::checkFields(L, 4, {kListenerFields});
        if (lua_getfield(L, 4, "owner") != LUA_TNIL) {
            lua::Owners::checkOwner(L, -1);
            owner = lua_gettop(L);
        }
    }
    if (owner == 0) {
        owner = lua::Owners::pushDefault(L);
    }
    const std::weak_ptr<const void> lifetime = owner != 0 ? lua::Owners::getLifetime(L, owner) : std::weak_ptr<const void>();
    auto function = std::make_shared<lua::Owners::Function>(L, 3, owner);
    lua_State* main = lua::Runtime::getMainThread(L);

    // clang-format off
    core::Connection connection = state->commanded.connect([function, main](std::string_view name, const core::Json& arguments) {
        if (function->push(main)) {
            lua::Stack::push(main, name);
            lua::JsonConverter::push(main, arguments);
            lua::Runtime::protectedCall(main, 2, 0);
        }
    }, {.owner = lifetime});
    // clang-format on
    if (owner != 0) {
        lua::Owners::add(L, owner, connection);
    }
    lua::Userdata::emplace<core::Connection>(L, std::move(connection));
    deliver(*state);
    return 1;
}

int ConversationLua::open(lua_State* L) {
    const luaL_Reg functions[] = {
        {"new", &lua::Binding::native<&create>},
        {nullptr, nullptr},
    };
    lua::Binding::newModule(L, functions);
    return 1;
}

void ConversationLua::install(lua_State* L) {
    lua::ClassBuilder<Conversation>(L).function("start", &lua::Binding::native<&start>).function("restore", &lua::Binding::native<&restore>).property("texts", &lua::Binding::native<&texts>).property("nodes", &lua::Binding::native<&nodes>).install();
    lua::ClassBuilder<ConversationState>(L).property("line", &lua::Binding::native<&line>).property("choices", &lua::Binding::native<&choices>).property("finished", &lua::Binding::native<&finished>).property("variables", &lua::Binding::native<&variables>).property("node", &lua::Binding::native<&node>).function("advance", &lua::Binding::native<&advance>).function("choose", &lua::Binding::native<&choose>).function("setVariable", &lua::Binding::native<&setVariable>).function("visits", &lua::Binding::native<&visits>).function("save", &lua::Binding::native<&save>).function("on", &lua::Binding::native<&on>).install();
    lua::Binding::preload(L, "haylen.conversation", &open);
}

} // namespace haylen::conversation
