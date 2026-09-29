#pragma once

#include <lua.hpp>

#include <array>
#include <optional>
#include <string_view>

#include "haylen/ai/BehaviorTree.hpp"

namespace haylen::ai {

// Installs the BehaviorTree class of haylen.ai and the functions that describe its nodes. Descriptions are plain tables, and leaves are Lua functions of the blackboard table.
class BehaviorTreeLua final {
  public:
    // A tree created from Lua. Its leaves run on the Lua thread that ticks it, with the tree at stack index 1.
    struct Scripted {
        std::optional<BehaviorTree> tree;
        lua_State* caller = nullptr;
        bool ticking = false;
    };

    static void install(lua_State* L);

    // Sets the node functions and newBehaviorTree on the module table at the top of the stack.
    static void addFunctions(lua_State* L);

  private:
    // The kind names of descriptions, in the order of BehaviorTree::Kind.
    static constexpr std::array<std::string_view, 13> kKinds{"sequence", "selector", "parallel", "inverter", "succeeder", "failer", "repeater", "retry", "cooldown", "timeout", "wait", "condition", "action"};

    // Reads the description at index, keeping its leaf functions in the leaves list at leavesIndex.
    [[nodiscard]] static BehaviorTree::Node readNode(lua_State* L, int index, int leavesIndex, Scripted& self);
    [[nodiscard]] static BehaviorTree::Status callLeaf(Scripted& self, lua_Integer leaf, const float* deltaSeconds);
    // Pushes a description of the kind, reading its children list, single child, count and seconds from the given argument positions, where 0 leaves a part out.
    static int describe(lua_State* L, BehaviorTree::Kind kind, int children, int child, int count, int seconds);

    static int sequence(lua_State* L);
    static int selector(lua_State* L);
    static int parallel(lua_State* L);
    static int inverter(lua_State* L);
    static int succeeder(lua_State* L);
    static int failer(lua_State* L);
    static int repeater(lua_State* L);
    static int retry(lua_State* L);
    static int cooldown(lua_State* L);
    static int timeout(lua_State* L);
    static int wait(lua_State* L);
    static int condition(lua_State* L);
    static int action(lua_State* L);

    static int newTree(lua_State* L);
    static int tick(lua_State* L);
    static int reset(lua_State* L);
    static int getStatus(lua_State* L);
    static int getTime(lua_State* L);
    static int getNodeCount(lua_State* L);
    static int getBlackboard(lua_State* L);
    static int setBlackboard(lua_State* L);
};

} // namespace haylen::ai
