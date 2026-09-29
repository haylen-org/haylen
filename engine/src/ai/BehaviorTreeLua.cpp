#include "ai/BehaviorTreeLua.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "haylen/lua/Binding.hpp"
#include "haylen/lua/ClassBuilder.hpp"
#include "haylen/lua/Stack.hpp"
#include "haylen/lua/Type.hpp"
#include "haylen/lua/Userdata.hpp"

namespace haylen::lua {

template <> struct Type<ai::BehaviorTreeLua::Scripted> {
    static constexpr const char* name = "haylen.BehaviorTree";
    using Storage = ai::BehaviorTreeLua::Scripted;
};

} // namespace haylen::lua

namespace haylen::ai {

int BehaviorTreeLua::describe(lua_State* L, BehaviorTree::Kind kind, int children, int child, int count, int seconds) {
    lua_createtable(L, 0, 4);
    const int description = lua_gettop(L);
    lua::Stack::push(L, kKinds[static_cast<std::size_t>(kind)]);
    lua_setfield(L, description, "kind");
    if (children > 0) {
        luaL_checktype(L, children, LUA_TTABLE);
        lua_pushvalue(L, children);
        lua_setfield(L, description, "children");
    }
    if (child > 0) {
        luaL_checktype(L, child, LUA_TTABLE);
        lua_createtable(L, 1, 0);
        lua_pushvalue(L, child);
        lua_rawseti(L, -2, 1);
        lua_setfield(L, description, "children");
    }
    if (count > 0) {
        lua_pushinteger(L, luaL_optinteger(L, count, 0));
        lua_setfield(L, description, "count");
    }
    if (seconds > 0) {
        lua_pushnumber(L, luaL_checknumber(L, seconds));
        lua_setfield(L, description, "seconds");
    }
    return 1;
}

int BehaviorTreeLua::sequence(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Sequence, 1, 0, 0, 0);
}

int BehaviorTreeLua::selector(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Selector, 1, 0, 0, 0);
}

int BehaviorTreeLua::parallel(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Parallel, 1, 0, 2, 0);
}

int BehaviorTreeLua::inverter(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Inverter, 0, 1, 0, 0);
}

int BehaviorTreeLua::succeeder(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Succeeder, 0, 1, 0, 0);
}

int BehaviorTreeLua::failer(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Failer, 0, 1, 0, 0);
}

int BehaviorTreeLua::repeater(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Repeater, 0, 1, 2, 0);
}

int BehaviorTreeLua::retry(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Retry, 0, 1, 2, 0);
}

int BehaviorTreeLua::cooldown(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Cooldown, 0, 1, 0, 2);
}

int BehaviorTreeLua::timeout(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Timeout, 0, 1, 0, 2);
}

int BehaviorTreeLua::wait(lua_State* L) {
    return describe(L, BehaviorTree::Kind::Wait, 0, 0, 0, 1);
}

int BehaviorTreeLua::condition(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    describe(L, BehaviorTree::Kind::Condition, 0, 0, 0, 0);
    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "fn");
    return 1;
}

int BehaviorTreeLua::action(lua_State* L) {
    luaL_checktype(L, 1, LUA_TFUNCTION);
    describe(L, BehaviorTree::Kind::Action, 0, 0, 0, 0);
    lua_pushvalue(L, 1);
    lua_setfield(L, -2, "fn");
    return 1;
}

// Conditions pass when their function returns a true value. Actions return 'success', 'failure' or 'running', or true for success and false for failure, and nothing counts as success.
BehaviorTree::Status BehaviorTreeLua::callLeaf(Scripted& self, lua_Integer leaf, const float* deltaSeconds) {
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
    BehaviorTree::Status status = BehaviorTree::Status::Success;
    if (deltaSeconds == nullptr || type == LUA_TBOOLEAN) {
        status = lua_toboolean(L, -1) != 0 ? BehaviorTree::Status::Success : BehaviorTree::Status::Failure;
    } else if (type == LUA_TSTRING) {
        const std::optional<BehaviorTree::Status> named = BehaviorTree::statusFromName(lua_tostring(L, -1));
        if (!named) {
            luaL_error(L, "A behavior tree action returned the unknown status '%s'.", lua_tostring(L, -1));
        }
        status = *named;
    } else if (type != LUA_TNIL) {
        luaL_error(L, "A behavior tree action must return a status name, a boolean or nothing.");
    }
    lua_pop(L, 1);
    return status;
}

BehaviorTree::Node BehaviorTreeLua::readNode(lua_State* L, int index, int leavesIndex, Scripted& self) {
    luaL_checktype(L, index, LUA_TTABLE);
    const int description = lua_absindex(L, index);
    lua_getfield(L, description, "kind");
    const auto found = std::find(kKinds.begin(), kKinds.end(), lua::Stack::read<std::string_view>(L, -1));
    if (found == kKinds.end()) {
        luaL_error(L, "Unknown behavior tree node '%s'.", lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    BehaviorTree::Node node{.kind = static_cast<BehaviorTree::Kind>(found - kKinds.begin())};
    lua_getfield(L, description, "count");
    node.count = static_cast<int>(luaL_optinteger(L, -1, 0));
    lua_getfield(L, description, "seconds");
    node.seconds = static_cast<float>(luaL_optnumber(L, -1, 0.0));
    lua_pop(L, 2);

    if (lua_getfield(L, description, "children") == LUA_TTABLE) {
        const lua_Integer count = luaL_len(L, -1);
        for (lua_Integer child = 1; child <= count; ++child) {
            lua_rawgeti(L, -1, child);
            node.children.push_back(readNode(L, -1, leavesIndex, self));
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    if (node.kind == BehaviorTree::Kind::Condition || node.kind == BehaviorTree::Kind::Action) {
        const lua_Integer leaf = luaL_len(L, leavesIndex) + 1;
        lua_getfield(L, description, "fn");
        luaL_checktype(L, -1, LUA_TFUNCTION);
        lua_rawseti(L, leavesIndex, leaf);
        Scripted* owner = &self;
        if (node.kind == BehaviorTree::Kind::Condition) {
            node.condition = [owner, leaf](Blackboard&) { return callLeaf(*owner, leaf, nullptr) == BehaviorTree::Status::Success; };
        } else {
            node.action = [owner, leaf](Blackboard&, float deltaSeconds) { return callLeaf(*owner, leaf, &deltaSeconds); };
        }
    }
    return node;
}

// Builds a tree with newBehaviorTree(root, blackboard), where the blackboard table is optional.
int BehaviorTreeLua::newTree(lua_State* L) {
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
    self.tree.emplace(readNode(L, 1, lua_gettop(L), self));
    lua_settop(L, tree);
    return 1;
}

int BehaviorTreeLua::tick(lua_State* L) {
    Scripted& self = lua::Userdata::check<Scripted>(L, 1);
    const auto deltaSeconds = lua::Stack::read<float>(L, 2);
    if (self.ticking) {
        return luaL_error(L, "The behavior tree is already ticking.");
    }
    lua_settop(L, 1);

    struct TickScope {
        Scripted& owner;
        ~TickScope() {
            owner.ticking = false;
            owner.caller = nullptr;
        }
    } scope{self};
    self.ticking = true;
    self.caller = L;
    lua::Stack::push(L, BehaviorTree::statusName(self.tree->tick(deltaSeconds)));
    return 1;
}

int BehaviorTreeLua::reset(lua_State* L) {
    lua::Userdata::check<Scripted>(L, 1).tree->reset();
    return 0;
}

int BehaviorTreeLua::getStatus(lua_State* L) {
    lua::Stack::push(L, BehaviorTree::statusName(lua::Userdata::check<Scripted>(L, 1).tree->getStatus()));
    return 1;
}

int BehaviorTreeLua::getTime(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Scripted>(L, 1).tree->getTime());
    return 1;
}

int BehaviorTreeLua::getNodeCount(lua_State* L) {
    lua::Stack::push(L, lua::Userdata::check<Scripted>(L, 1).tree->getNodeCount());
    return 1;
}

int BehaviorTreeLua::getBlackboard(lua_State* L) {
    (void)lua::Userdata::check<Scripted>(L, 1);
    lua::Userdata::pushField(L, 1, "blackboard");
    return 1;
}

int BehaviorTreeLua::setBlackboard(lua_State* L) {
    (void)lua::Userdata::check<Scripted>(L, 1);
    luaL_checktype(L, 3, LUA_TTABLE);
    lua::Userdata::setField(L, 1, "blackboard", 3);
    return 0;
}

void BehaviorTreeLua::install(lua_State* L) {
    lua::ClassBuilder<Scripted>(L).function("tick", &lua::Binding::native<&tick>).function("reset", &lua::Binding::native<&reset>).property("status", &getStatus).property("time", &getTime).property("nodeCount", &getNodeCount).property("blackboard", &getBlackboard, &setBlackboard).install();
}

void BehaviorTreeLua::addFunctions(lua_State* L) {
    const luaL_Reg functions[] = {
        {"sequence", &sequence}, {"selector", &selector}, {"parallel", &parallel}, {"inverter", &inverter}, {"succeeder", &succeeder}, {"failer", &failer}, {"repeater", &repeater}, {"retry", &retry}, {"cooldown", &cooldown}, {"timeout", &timeout}, {"wait", &wait}, {"condition", &condition}, {"action", &action}, {"newBehaviorTree", &lua::Binding::native<&newTree>}, {nullptr, nullptr},
    };
    luaL_setfuncs(L, functions, 0);
}

} // namespace haylen::ai
