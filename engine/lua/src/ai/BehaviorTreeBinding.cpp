#include "ai/BehaviorTreeBinding.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Table.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<lua::ai::BehaviorTreeBinding::Scripted> {
    static constexpr const char* name = "haylen.BehaviorTree";
    using Storage = lua::ai::BehaviorTreeBinding::Scripted;
};

} // namespace haylen::lua

namespace haylen::lua::ai {

// The arguments are read before the description goes on the stack, where it would take the place of an optional argument that was left out.
int BehaviorTreeBinding::describe(lua_State* L, haylen::ai::BehaviorTree::Kind kind, int children, int child, int count, int seconds) {
    if (children > 0) {
        luaL_checktype(L, children, LUA_TTABLE);
    }

    if (child > 0) {
        luaL_checktype(L, child, LUA_TTABLE);
    }

    std::optional<lua_Integer> times;
    if (count > 0 && !lua_isnoneornil(L, count)) {
        times = luaL_checkinteger(L, count);
    } else if (kind == haylen::ai::BehaviorTree::Kind::Parallel) {
        // A parallel node that names no count succeeds once every child succeeded.
        times = luaL_len(L, children);
    }

    const std::optional<lua_Number> duration = seconds > 0 ? std::optional(luaL_checknumber(L, seconds)) : std::nullopt;

    lua_createtable(L, 0, 4);
    const int description = lua_gettop(L);
    lua::Stack::push(L, kKinds[static_cast<std::size_t>(kind)]);
    lua_setfield(L, description, "kind");
    if (children > 0) {
        lua_pushvalue(L, children);
        lua_setfield(L, description, "children");
    }

    if (child > 0) {
        lua_createtable(L, 1, 0);
        lua_pushvalue(L, child);
        lua_rawseti(L, -2, 1);
        lua_setfield(L, description, "children");
    }

    if (times) {
        lua_pushinteger(L, *times);
        lua_setfield(L, description, "count");
    }

    if (duration) {
        lua_pushnumber(L, *duration);
        lua_setfield(L, description, "seconds");
    }

    return 1;
}

int BehaviorTreeBinding::sequence(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Sequence, 1, 0, 0, 0);
}

int BehaviorTreeBinding::selector(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Selector, 1, 0, 0, 0);
}

int BehaviorTreeBinding::parallel(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Parallel, 1, 0, 2, 0);
}

int BehaviorTreeBinding::inverter(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Inverter, 0, 1, 0, 0);
}

int BehaviorTreeBinding::succeeder(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Succeeder, 0, 1, 0, 0);
}

int BehaviorTreeBinding::failer(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Failer, 0, 1, 0, 0);
}

int BehaviorTreeBinding::repeater(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Repeater, 0, 1, 2, 0);
}

int BehaviorTreeBinding::retry(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Retry, 0, 1, 2, 0);
}

int BehaviorTreeBinding::cooldown(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Cooldown, 0, 1, 0, 2);
}

int BehaviorTreeBinding::timeout(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Timeout, 0, 1, 0, 2);
}

int BehaviorTreeBinding::wait(lua_State* L) {
    return describe(L, haylen::ai::BehaviorTree::Kind::Wait, 0, 0, 0, 1);
}

int BehaviorTreeBinding::condition(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    describe(L, haylen::ai::BehaviorTree::Kind::Condition, 0, 0, 0, 0);
    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "fn");
    return 1;
}

int BehaviorTreeBinding::action(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    describe(L, haylen::ai::BehaviorTree::Kind::Action, 0, 0, 0, 0);
    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "fn");
    return 1;
}

// Conditions pass when their function returns a true value. Actions return `'success'`, `'failure'` or `'running'`, or `true` for success and `false` for failure, and nothing counts as success.
haylen::ai::BehaviorTree::Status BehaviorTreeBinding::callLeaf(Scripted& self, lua_Integer leaf, const float* deltaSeconds) {
    lua_State* L = self.caller;
    lua::Userdata::pushField(L, 1, "leaves");
    lua_rawgeti(L, -1, leaf);
    lua_remove(L, -2);
    lua::Userdata::pushField(L, 1, "blackboard");
    if (deltaSeconds != nullptr) {
        lua_pushnumber(L, *deltaSeconds);
    }

    lua_call(L, deltaSeconds != nullptr ? 2 : 1, 1);

    const int type = lua_type(L, -1);
    haylen::ai::BehaviorTree::Status status = haylen::ai::BehaviorTree::Status::Success;
    if (deltaSeconds == nullptr || type == LUA_TBOOLEAN) {
        status = lua_toboolean(L, -1) != 0 ? haylen::ai::BehaviorTree::Status::Success : haylen::ai::BehaviorTree::Status::Failure;
    } else if (type == LUA_TSTRING) {
        const std::optional<haylen::ai::BehaviorTree::Status> named = haylen::ai::BehaviorTree::statusFromName(lua_tostring(L, -1));
        if (!named) {
            luaL_error(L, "A behavior tree action returned the unknown status \"%s\".", lua_tostring(L, -1));
        }

        status = *named;
    } else if (type != LUA_TNIL) {
        luaL_error(L, "A behavior tree action must return a status name, a boolean or nothing.");
    }

    lua_pop(L, 1);
    return status;
}

std::span<const std::string_view> BehaviorTreeBinding::getFields(haylen::ai::BehaviorTree::Kind kind) noexcept {
    switch (kind) {
    case haylen::ai::BehaviorTree::Kind::Parallel:
    case haylen::ai::BehaviorTree::Kind::Repeater:
    case haylen::ai::BehaviorTree::Kind::Retry:
        return kCountedFields;
    case haylen::ai::BehaviorTree::Kind::Cooldown:
    case haylen::ai::BehaviorTree::Kind::Timeout:
        return kTimedFields;
    case haylen::ai::BehaviorTree::Kind::Wait:
        return kWaitFields;
    case haylen::ai::BehaviorTree::Kind::Condition:
    case haylen::ai::BehaviorTree::Kind::Action:
        return kLeafFields;
    case haylen::ai::BehaviorTree::Kind::Sequence:
    case haylen::ai::BehaviorTree::Kind::Selector:
    case haylen::ai::BehaviorTree::Kind::Inverter:
    case haylen::ai::BehaviorTree::Kind::Succeeder:
    case haylen::ai::BehaviorTree::Kind::Failer:
        break;
    }
    return kCompositeFields;
}

std::string BehaviorTreeBinding::listKinds() {
    std::string list;
    for (std::size_t index = 0; index < kKinds.size(); ++index) {
        list += index == 0 ? "" : (index + 1 == kKinds.size() ? " or " : ", ");
        list += "\"" + std::string(kKinds[index]) + "\"";
    }

    return list;
}

haylen::ai::BehaviorTree::Node BehaviorTreeBinding::readNode(lua_State* L, int index, int leavesIndex, int pathIndex, Scripted& self, int depth) {
    luaL_checktype(L, index, LUA_TTABLE);
    if (depth > kMaxDepth) {
        luaL_error(L, "A behavior tree nests deeper than %d levels.", kMaxDepth);
    }

    luaL_checkstack(L, 4, "behavior tree too deep");
    const int description = lua_absindex(L, index);
    lua_pushvalue(L, description);
    if (lua_rawget(L, pathIndex) != LUA_TNIL) {
        luaL_error(L, "A behavior tree description contains itself.");
    }

    lua_pop(L, 1);

    lua_getfield(L, description, "kind");
    const auto found = lua_type(L, -1) == LUA_TSTRING ? std::find(kKinds.begin(), kKinds.end(), lua::Stack::read<std::string_view>(L, -1)) : kKinds.end();
    if (found == kKinds.end()) {
        luaL_error(L, "Unknown behavior tree node \"%s\". The kind of a node is %s.", luaL_tolstring(L, -1, nullptr), listKinds().c_str());
    }

    lua_pop(L, 1);

    haylen::ai::BehaviorTree::Node node{.kind = static_cast<haylen::ai::BehaviorTree::Kind>(found - kKinds.begin())};
    lua::Table::checkFields(L, description, {getFields(node.kind)});
    lua::Table::readField(L, description, "count", node.count);
    lua::Table::readField(L, description, "seconds", node.seconds);

    // The description stays in the path while its children are read, so a child that leads back to it is caught instead of recursing forever.
    if (lua_getfield(L, description, "children") == LUA_TTABLE) {
        lua_pushvalue(L, description);
        lua_pushboolean(L, 1);
        lua_rawset(L, pathIndex);
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer child = 1; child <= count; ++child) {
            lua_rawgeti(L, -1, child);
            node.children.push_back(readNode(L, -1, leavesIndex, pathIndex, self, depth + 1));
            lua_pop(L, 1);
        }

        lua_pushvalue(L, description);
        lua_pushnil(L);
        lua_rawset(L, pathIndex);
    }

    lua_pop(L, 1);

    if (node.kind == haylen::ai::BehaviorTree::Kind::Condition || node.kind == haylen::ai::BehaviorTree::Kind::Action) {
        const lua_Integer leaf = luaL_len(L, leavesIndex) + 1;
        if (lua_getfield(L, description, "fn") != LUA_TFUNCTION) {
            luaL_error(L, "A behavior tree leaf needs a function in \"fn\", not %s.", luaL_typename(L, -1));
        }

        lua_rawseti(L, leavesIndex, leaf);
        Scripted* owner = &self;
        if (node.kind == haylen::ai::BehaviorTree::Kind::Condition) {
            node.condition = [owner, leaf](haylen::ai::Blackboard&) { return callLeaf(*owner, leaf, nullptr) == haylen::ai::BehaviorTree::Status::Success; };
        } else {
            node.action = [owner, leaf](haylen::ai::Blackboard&, float deltaSeconds) { return callLeaf(*owner, leaf, &deltaSeconds); };
        }
    }

    return node;
}

// Builds a tree with `newBehaviorTree(root, blackboard)`, where the blackboard table is optional.
int BehaviorTreeBinding::newTree(lua_State* L) {
    luaL_checktype(L, 1, LUA_TTABLE);
    if (!lua_isnoneornil(L, 2)) {
        luaL_checktype(L, 2, LUA_TTABLE);
    } else {
        lua_settop(L, 1);
        lua_newtable(L);
    }

    Scripted& self = lua::Userdata::emplace<Scripted>(L);
    const int tree = lua_gettop(L);
    lua::Userdata::setField(L, tree, "blackboard", 2);
    lua_newtable(L);
    lua::Userdata::setField(L, tree, "leaves", -1);
    const int leaves = lua_gettop(L);
    lua_newtable(L);
    self.tree.emplace(readNode(L, 1, leaves, lua_gettop(L), self, 1));
    lua_settop(L, tree);
    return 1;
}

// A leaf that ticks its tree again fails, and the outer tick keeps calling its leaves on its own thread.
int BehaviorTreeBinding::tick(lua_State* L) {
    Scripted& self = lua::Userdata::check<Scripted>(L, 1);
    const auto deltaSeconds = lua::Stack::read<float>(L, 2);
    lua_settop(L, 1);

    struct CallerScope {
        Scripted& owner;
        lua_State* outer;
        ~CallerScope() {
            owner.caller = outer;
        }
    } scope{self, std::exchange(self.caller, L)};
    lua::Stack::push(L, haylen::ai::BehaviorTree::statusName(self.tree->tick(deltaSeconds)));
    return 1;
}

int BehaviorTreeBinding::reset(lua_State* L) {
    lua::Userdata::check<Scripted>(L, 1).tree->reset();
    return 0;
}

int BehaviorTreeBinding::getStatus(lua_State* L) {
    lua::Stack::push(L, haylen::ai::BehaviorTree::statusName(lua::Userdata::check<Scripted>(L, 1).tree->getStatus()));
    return 1;
}

int BehaviorTreeBinding::getTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Scripted>(L, 1).tree->getTime());
    return 1;
}

int BehaviorTreeBinding::getNodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Scripted>(L, 1).tree->getNodeCount());
    return 1;
}

int BehaviorTreeBinding::getBlackboard(lua_State* L) {
    (void)lua::Userdata::check<Scripted>(L, 1);
    lua::Userdata::pushField(L, 1, "blackboard");
    return 1;
}

int BehaviorTreeBinding::setBlackboard(lua_State* L) {
    (void)lua::Userdata::check<Scripted>(L, 1);
    luaL_checktype(L, 3, LUA_TTABLE);
    lua::Userdata::setField(L, 1, "blackboard", 3);
    return 0;
}

void BehaviorTreeBinding::install(lua_State* L) {
    lua::ClassBuilder<Scripted>(L).function("tick", &lua::Binding::native<&tick>).function("reset", &lua::Binding::native<&reset>).property("status", &getStatus).property("time", &getTime).property("nodeCount", &getNodeCount).property("blackboard", &getBlackboard, &setBlackboard).install();
}

void BehaviorTreeBinding::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"sequence", &sequence}, {"selector", &selector}, {"parallel", &parallel}, {"inverter", &inverter}, {"succeeder", &succeeder}, {"failer", &failer}, {"repeater", &repeater}, {"retry", &retry}, {"cooldown", &cooldown}, {"timeout", &timeout}, {"wait", &wait}, {"condition", &condition}, {"action", &action}, {"newBehaviorTree", &lua::Binding::native<&newTree>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::lua::ai
