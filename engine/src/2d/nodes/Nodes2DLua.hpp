#pragma once

#include <array>
#include <memory>
#include <string_view>

#include "haylen/2d/nodes/Node.hpp"
#include "haylen/2d/nodes/NodeProperties.hpp"
#include "haylen/2d/nodes/NodeSerializer.hpp"
#include "haylen/lua/Type.hpp"

struct lua_State;

namespace haylen::nodes2d {

class NodeTree;
class Prefab;

} // namespace haylen::nodes2d

namespace haylen::lua {

template <> struct Type<nodes2d::NodeTree> {
    static constexpr const char* name = "haylen.NodeTree";
    using Storage = std::shared_ptr<nodes2d::NodeTree>;
};

// A node keeps the userdata of its tree as its user value, where the tree keeps the data and the listeners of its nodes.
template <> struct Type<nodes2d::Node> {
    static constexpr const char* name = "haylen.Node";
    using Storage = nodes2d::Node;
};

template <> struct Type<nodes2d::Prefab> {
    static constexpr const char* name = "haylen.Prefab";
    using Storage = std::shared_ptr<nodes2d::Prefab>;
};

} // namespace haylen::lua

namespace haylen::nodes2d {

// Installs `haylen.nodes2d` with the `NodeTree`, `Node` and `Prefab` classes.
class Nodes2DLua final {
  public:
    static void install(lua_State* L);

    // Pushes a loaded prefab, which the asset loader of `.prefab` files hands to Lua.
    static void pushPrefab(lua_State* L, std::shared_ptr<Prefab> prefab);

    // Returns the assets that descriptions name, loaded through the assets of the engine of the state.
    [[nodiscard]] static NodeSerializer::Assets getAssets(lua_State* L);

  private:
    static constexpr std::array<std::string_view, 18> kPropertyFields{"name", "x", "y", "rotation", "scaleX", "scaleY", "color", "visible", "layer", "depth", "sortOffset", "sprite", "text", "frame", "particles", "body", "data", "children"};
    static constexpr std::array<std::string_view, 4> kContentFields{"sprite", "text", "frame", "particles"};
    static constexpr std::array<std::string_view, 2> kTextFields{"font", "text"};
    static constexpr std::array<std::string_view, 6> kFrameFields{"slice", "width", "height", "pivotX", "pivotY", "borderScale"};
    static constexpr std::array<std::string_view, 1> kFrameColorFields{"color"};
    static constexpr std::array<std::string_view, 1> kListenerFields{"owner"};

    // The trees of the state by their address, held weakly, so a node of a tree that C++ hands out finds the userdata of its tree.
    static constexpr const char* kTrees = "haylen.nodeTrees";

    // The state of the Lua call that destroys nodes, where the listeners of their destruction run.
    inline static lua_State* destroying = nullptr;

    // Points `destroying` at a state while it lives, and back at the state before it.
    class DestroyScope final {
      public:
        explicit DestroyScope(lua_State* L) noexcept : previous(destroying) {
            destroying = L;
        }
        ~DestroyScope() {
            destroying = previous;
        }

        DestroyScope(const DestroyScope&) = delete;
        DestroyScope& operator=(const DestroyScope&) = delete;

      private:
        lua_State* previous;
    };

    [[nodiscard]] static NodeTree& checkTree(lua_State* L, int index);

    // Pushes the userdata of a tree, and returns `false` without pushing when the state holds none for it.
    static bool pushTree(lua_State* L, const NodeTree* tree);
    static void pushNode(lua_State* L, const Node& node);
    static void pushNodes(lua_State* L, const std::vector<Node>& nodes);

    // Pushes the table of the tree of the node at index under `field`, creating it when asked.
    static bool pushTreeTable(lua_State* L, int node, const char* field, bool create);
    [[nodiscard]] static NodeContent readContent(lua_State* L, int index);
    [[nodiscard]] static NodeProperties readProperties(lua_State* L, int index);

    // Builds the node of a properties table under the parent, with its body, data and children.
    static Node addNode(lua_State* L, NodeTree& tree, const Node& parent, int properties);
    static void clearData(lua_State* L, const NodeTree* tree, std::uint64_t key);
    [[nodiscard]] static std::vector<Node> readNodes(lua_State* L, int index);

    static int newTree(lua_State* L);
    static int newPrefab(lua_State* L);
    static int save(lua_State* L);

    static int treeAdd(lua_State* L);
    static int treeInstantiate(lua_State* L);
    static int treeLoad(lua_State* L);
    static int treeFind(lua_State* L);
    static int treeRoots(lua_State* L);
    static int treeUpdate(lua_State* L);
    static int treeDraw(lua_State* L);
    static int treeClear(lua_State* L);
    static int treeWriteTransforms(lua_State* L);
    static int treeReadTransforms(lua_State* L);
    static int treeSize(lua_State* L);
    static int treeDrawnCount(lua_State* L);
    static int treeCulledCount(lua_State* L);

    static int nodeValid(lua_State* L);
    static int nodeKey(lua_State* L);
    static int nodePath(lua_State* L);
    static int nodeChildCount(lua_State* L);
    static int nodeContent(lua_State* L);
    static int nodeGetParent(lua_State* L);
    static int nodeSetParent(lua_State* L);
    static int nodeGetSprite(lua_State* L);
    static int nodeSetSprite(lua_State* L);
    static int nodeGetText(lua_State* L);
    static int nodeSetText(lua_State* L);
    static int nodeGetBody(lua_State* L);
    static int nodeSetBody(lua_State* L);
    static int nodeData(lua_State* L);
    static int nodeAdd(lua_State* L);
    static int nodeFind(lua_State* L);
    static int nodeChildren(lua_State* L);
    static int nodeShow(lua_State* L);
    static int nodeToWorld(lua_State* L);
    static int nodeToLocal(lua_State* L);
    static int nodeWorldTransform(lua_State* L);
    static int nodeDestroy(lua_State* L);
    static int nodeOn(lua_State* L);

    static int prefabDescription(lua_State* L);

    static int open(lua_State* L);
};

} // namespace haylen::nodes2d
