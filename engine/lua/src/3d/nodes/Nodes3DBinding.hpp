#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "haylen/3d/nodes/Component3D.hpp"
#include "haylen/3d/nodes/ComponentType3D.hpp"
#include "haylen/3d/nodes/Node3D.hpp"
#include "haylen/3d/nodes/NodeProperties3D.hpp"
#include "haylen/lua/Type.hpp"
#include "runtime/Owners.hpp"

struct lua_State;

namespace haylen::nodes3d {
class NodeTree3D;
}

namespace haylen::lua {

template <> struct Type<haylen::nodes3d::NodeTree3D> {
    static constexpr const char* name = "haylen.NodeTree3D";
    using Storage = std::shared_ptr<haylen::nodes3d::NodeTree3D>;
};

// A node keeps the userdata of its tree as its user value, where the tree keeps the data tables of its nodes.
template <> struct Type<haylen::nodes3d::Node3D> {
    static constexpr const char* name = "haylen.Node3D";
    using Storage = haylen::nodes3d::Node3D;
};

template <> struct Type<haylen::nodes3d::Component3D> {
    static constexpr const char* name = "haylen.Component3D";
    using Storage = haylen::nodes3d::Component3D;
};

} // namespace haylen::lua

namespace haylen::lua::nodes3d {

// Installs `haylen.nodes3d` with the `NodeTree3D`, `Node3D` and `Component3D` classes.
class Nodes3DBinding final {
  public:
    static void install(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 16> kPropertyFields{"name", "position", "rotation", "scale", "visible", "enabled", "layers", "static", "frustumCulling", "renderOrder", "inheritPosition", "inheritRotation", "inheritScale", "tags", "data", "children"};
    static constexpr std::array<std::string_view, 1> kCloneFields{"parent"};
    static constexpr std::array<std::string_view, 1> kParentFields{"keepWorld"};

    // The trees of the state by their address, held weakly, so a node that C++ hands out finds the userdata of its tree.
    static constexpr const char* kTrees = "haylen.nodeTrees3D";

    // The state of the Lua call that changes the tree, where the listeners that the change wakes run, so an error reaches the caller.
    inline static lua_State* calling = nullptr;

    // Points `calling` at a state while it lives, and back at the state before it.
    class CallScope final {
      public:
        explicit CallScope(lua_State* L) noexcept : previous(calling) {
            calling = L;
        }
        ~CallScope() {
            calling = previous;
        }

        CallScope(const CallScope&) = delete;
        CallScope& operator=(const CallScope&) = delete;

      private:
        lua_State* previous;
    };

    [[nodiscard]] static haylen::nodes3d::NodeTree3D& checkTree(lua_State* L, int index);
    [[nodiscard]] static haylen::nodes3d::Node3D& checkNode(lua_State* L, int index);

    // Pushes the userdata of a tree, and returns `false` without pushing when the state holds none for it.
    static bool pushTree(lua_State* L, const haylen::nodes3d::NodeTree3D* tree);
    static void pushNode(lua_State* L, const haylen::nodes3d::Node3D& node);
    static void pushNodes(lua_State* L, const std::vector<haylen::nodes3d::Node3D>& nodes);
    [[nodiscard]] static std::vector<haylen::nodes3d::Node3D> readNodes(lua_State* L, int index);

    // Pushes the data tables of the tree at index, creating them when asked, and returns whether it pushed them.
    static bool pushDataTables(lua_State* L, const haylen::nodes3d::NodeTree3D* tree, bool create);
    static void forgetData(lua_State* L, const haylen::nodes3d::NodeTree3D* tree, std::uint64_t key);
    static void copyData(lua_State* L, const haylen::nodes3d::NodeTree3D* tree, std::uint64_t source, std::uint64_t copy);

    [[nodiscard]] static haylen::nodes3d::NodeProperties3D readProperties(lua_State* L, int index);

    // Returns the registered component type with the name, or raises an error that names it.
    [[nodiscard]] static std::shared_ptr<const haylen::nodes3d::ComponentType3D> checkType(lua_State* L, int index);

    // Converts between Lua values and the JSON of a property of a component, where a `Vec3` is the list of its three numbers and a `Quat` the list of its four.
    [[nodiscard]] static haylen::core::Json readValue(lua_State* L, int index, const haylen::nodes3d::ComponentType3D::Property* property);
    [[nodiscard]] static bool isNumbers(const haylen::core::Json& value, std::size_t count);
    static void pushValue(lua_State* L, const haylen::core::Json& value, const haylen::nodes3d::ComponentType3D::Property* property);
    [[nodiscard]] static haylen::core::Json readOptions(lua_State* L, int index, const haylen::nodes3d::ComponentType3D& type);
    static void pushComponent(lua_State* L, const haylen::nodes3d::Component3D& component);

    // Builds the node of a properties table under the parent, with its data and children, and pushes it.
    static haylen::nodes3d::Node3D addNode(lua_State* L, haylen::nodes3d::NodeTree3D& tree, const haylen::nodes3d::Node3D& parent, int properties);

    // Calls a listener with a node: on the Lua call that changed the tree, so an error reaches its caller, or reported from the main thread.
    static void callListener(const lua::Owners::Function& function, lua_State* main, const haylen::nodes3d::Node3D& subject);

    static int newTree(lua_State* L);

    static int treeAdd(lua_State* L);
    static int treeFind(lua_State* L);
    static int treeFindByKey(lua_State* L);
    static int treeRoots(lua_State* L);
    static int treeFindTagged(lua_State* L);
    static int treeUpdate(lua_State* L);
    static int treeClear(lua_State* L);
    static int treeWriteTransforms(lua_State* L);
    static int treeReadTransforms(lua_State* L);
    static int treeSize(lua_State* L);
    static int treeUpdatedCount(lua_State* L);
    static int treeComponentCount(lua_State* L);
    static int treeGetProcessMode(lua_State* L);
    static int treeSetProcessMode(lua_State* L);
    static int componentTypes(lua_State* L);

    static int nodeValid(lua_State* L);
    static int nodeKey(lua_State* L);
    static int nodePath(lua_State* L);
    static int nodeChildCount(lua_State* L);
    static int nodeData(lua_State* L);
    static int nodeGetParent(lua_State* L);
    static int nodeSetParentProperty(lua_State* L);
    static int nodeGetEnabled(lua_State* L);
    static int nodeSetEnabled(lua_State* L);
    template <bool haylen::nodes3d::Inheritance3D::* Part> static int nodeGetInherit(lua_State* L);
    template <bool haylen::nodes3d::Inheritance3D::* Part> static int nodeSetInherit(lua_State* L);
    static int nodeAdd(lua_State* L);
    static int nodeFind(lua_State* L);
    static int nodeChildren(lua_State* L);
    static int nodeClone(lua_State* L);
    static int nodeSetParent(lua_State* L);
    static int nodeLookAt(lua_State* L);
    static int nodeDestroy(lua_State* L);
    static int nodeOn(lua_State* L);
    static int nodeAddComponent(lua_State* L);
    static int nodeComponent(lua_State* L);
    static int nodeComponents(lua_State* L);
    static int nodeRemoveComponent(lua_State* L);
    static int nodeSaveComponents(lua_State* L);
    static int nodeLoadComponents(lua_State* L);

    static int componentGet(lua_State* L);
    static int componentSet(lua_State* L);
    static int componentSave(lua_State* L);
    static int componentRemove(lua_State* L);
    static int componentType(lua_State* L);
    static int componentNode(lua_State* L);
    static int componentValid(lua_State* L);

    static int open(lua_State* L);
};

} // namespace haylen::lua::nodes3d
