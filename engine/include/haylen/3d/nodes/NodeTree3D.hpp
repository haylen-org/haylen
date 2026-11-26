#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/3d/nodes/Inheritance3D.hpp"
#include "haylen/3d/nodes/Node3D.hpp"
#include "haylen/3d/nodes/NodeEvent3D.hpp"
#include "haylen/3d/nodes/NodeProperties3D.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Json.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/math/Mat4.hpp"
#include "haylen/math/Quat.hpp"
#include "haylen/math/Vec3.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::nodes3d {

// A retained tree of 3D nodes whose local transforms compose into world transforms, so children follow their parents, with visibility and enabled states that subtrees inherit. Nodes live in contiguous arrays in the order of a walk from the roots, and only the subtrees under a node that changed compute their world state again, at the first read of world state or at `update`, which spreads large changes over the job system. A tree lives in a shared pointer, which its node handles watch.
class NodeTree3D final : public std::enable_shared_from_this<NodeTree3D> {
    struct Passkey {
        explicit Passkey() = default;
    };

  public:
    // Updates of at least this many nodes run on the worker pool when `update` has a job system.
    static constexpr std::size_t kParallelNodes = 4096;
    static constexpr std::size_t kMaxNodes = std::size_t{1} << 24U;

    // The floats of one node in `writeTransforms` and `readTransforms`: the position and the rotation as `x`, `y`, `z` and `w`.
    static constexpr std::size_t kTransformFloats = 7;

    explicit NodeTree3D(Passkey) {}

    NodeTree3D(const NodeTree3D&) = delete;
    NodeTree3D& operator=(const NodeTree3D&) = delete;

    [[nodiscard]] static std::shared_ptr<NodeTree3D> create();

    // Adds a node at the root, after the roots the tree has, or under a parent of this tree. Throws `std::length_error` past `kMaxNodes` nodes and `std::invalid_argument` for a rotation without a length.
    Node3D add(const NodeProperties3D& properties);
    Node3D add(const Node3D& parent, const NodeProperties3D& properties);

    // Copies a node of this tree and its descendants under the parent, or at the root with an invalid parent, after the children the parent has: their names, local state, tags and data, not their listeners. The copy is made from the subtree as it was, so a parent inside the subtree works. The tree emits `cloned` for every node it copied, parents first.
    Node3D clone(const Node3D& source, const Node3D& parent);

    // Finds a node by a path of names from the roots separated by slashes, such as `tank/turret`, or returns an invalid node.
    [[nodiscard]] Node3D find(std::string_view path) const;
    [[nodiscard]] std::vector<Node3D> getRoots() const;

    // Returns every node with the tag, in no particular order.
    [[nodiscard]] std::vector<Node3D> findTagged(std::string_view tag) const;

    // Finds a node by the key of its handle, or returns an invalid node once that node is gone.
    [[nodiscard]] Node3D findByKey(std::uint64_t key) const;
    [[nodiscard]] std::size_t size() const noexcept {
        return count;
    }

    // Destroys every node.
    void clear();

    // Brings the world state of every node whose local state changed, and of its descendants, up to date. With a job system, an update of at least `kParallelNodes` nodes splits the subtrees that changed over the worker pool.
    void update(core::JobSystem* jobs = nullptr);

    // How many nodes the last update that had changes to apply computed again.
    [[nodiscard]] std::size_t getUpdatedCount() const noexcept {
        return updated;
    }

    // Sets the local position and rotation of each node from seven floats of the values, in the order of `kTransformFloats`, which moves many nodes in one call. A rotation is normalized. Throws `std::invalid_argument` unless the values hold seven floats for each node, more being left as they are, and `std::logic_error` for a node that is gone or of another tree, before it moves any node.
    void writeTransforms(std::span<const Node3D> nodes, std::span<const float> values);

    // Copies the world position and rotation of each node into seven floats of the values.
    void readTransforms(std::span<const Node3D> nodes, std::span<float> values);

    // Calls the slot on every event of the kind for the node, until the connection ends or the node goes.
    core::Connection on(const Node3D& node, NodeEvent3D event, core::Signal<const Node3D&>::Slot slot, core::Signal<const Node3D&>::Options options = {});

    // Emits every node right before it goes, descendants before their parents, before the listeners of the node hear it.
    core::Signal<const Node3D&> destroyed;

    // Emits every node that moved under another parent, after the move.
    core::Signal<const Node3D&> parentChanged;

    // Emits every node that turned on or off in the hierarchy.
    core::Signal<const Node3D&> enabledChanged;

    // Emits every node that a clone copied with its copy, parents before their children, once the whole copy is in the tree, so what keeps state for nodes copies its own.
    core::Signal<const Node3D&, const Node3D&> cloned;

  private:
    friend class Node3D;

    static constexpr std::int32_t kNone = -1;
    static constexpr std::size_t kEvents = 5;

    // The state that an update reads and writes for every node, next to each other.
    struct Slot {
        math::Vec3 position{};
        math::Quat rotation{};
        math::Vec3 scale{1.0F, 1.0F, 1.0F};
        math::Mat4 world{};
        math::Quat worldRotation{};
        math::Vec3 worldScale{1.0F, 1.0F, 1.0F};
        std::int32_t parent = kNone;
        std::uint32_t layers = 1;
        int renderOrder = 0;
        Inheritance3D inheritance{};
        bool visible = true;
        bool enabled = true;
        bool worldVisible = true;
        bool worldEnabled = true;
        bool isStatic = false;
        bool frustumCulling = true;
        bool alive = false;

        // The local state changed since the last update, so the subtree of the node computes its world state again.
        bool changed = false;
    };

    // The state of every node that an update never reads.
    struct Detail {
        std::string name{};
        core::Json data{};

        // The tags of the node, each with the place of the node among the members of the tag.
        std::vector<std::pair<std::uint32_t, std::uint32_t>> tags{};
        std::uint32_t generation = 1;
        std::int32_t firstChild = kNone;
        std::int32_t lastChild = kNone;
        std::int32_t next = kNone;
        std::int32_t previous = kNone;
    };

    // The listeners of one node, one signal for each event, which an emit holds while it runs, so a listener may destroy the node.
    struct Watchers {
        std::array<core::Signal<const Node3D&>, kEvents> signals;
    };

    // The nodes on their way out, each with the generation it had and whether its listeners heard of it, deepest last.
    struct Releasing {
        std::int32_t index = 0;
        std::uint32_t generation = 0;
        bool announced = false;
    };

    // Drops the entries a walk pushed when it ends, even when a listener throws, so a walk that a listener starts inside another one works above its entries.
    template <typename T> class StackScope final {
      public:
        StackScope(std::vector<T>& stack, std::size_t size) noexcept : entries(stack), base(size) {}
        ~StackScope() {
            entries.resize(base);
        }

        StackScope(const StackScope&) = delete;
        StackScope& operator=(const StackScope&) = delete;

      private:
        std::vector<T>& entries;
        std::size_t base;
    };

    // A node that a clone copies, with the place of the copy of its parent among the copies, or none for the root of the copy.
    struct Copy {
        std::uint32_t source = 0;
        std::int32_t parent = kNone;
        NodeProperties3D properties;
    };

    // A run of the order that updates as one piece: a whole subtree, whose root takes its parent from outside the run.
    struct Range {
        std::size_t begin = 0;
        std::size_t end = 0;
    };

    // Hashes tags as views, so a lookup never builds a string.
    struct NameHash {
        using is_transparent = void;

        [[nodiscard]] std::size_t operator()(std::string_view name) const noexcept {
            return std::hash<std::string_view>{}(name);
        }
    };

    // Pieces of at least this many nodes each update as one job.
    static constexpr std::size_t kParallelGrain = 1024;

    // Below one changed node in this many, the update sorts the changed nodes instead of scanning the whole order for them.
    static constexpr std::size_t kScanShare = 32;

    // Returns the slot of a live node of this tree, and throws for a destroyed one.
    [[nodiscard]] std::uint32_t check(const Node3D& node) const;
    [[nodiscard]] Node3D handle(std::int32_t index) const;
    Node3D insert(std::int32_t parent, const NodeProperties3D& properties);
    [[nodiscard]] std::int32_t allocate();
    void link(std::int32_t index, std::int32_t parent);
    void unlink(std::int32_t index) noexcept;
    void release(std::int32_t index);
    void announce(std::int32_t index, std::uint32_t generation);
    void recycle(std::int32_t index);
    [[nodiscard]] bool holds(std::size_t slot, std::uint32_t generation) const noexcept;
    void touch(std::uint32_t index);
    void setParent(std::uint32_t index, std::int32_t parent, bool keepWorld);
    void setEnabled(std::uint32_t index, bool value);
    void setWorldPosition(std::uint32_t index, math::Vec3 value);
    void setWorldRotation(std::uint32_t index, math::Quat value);
    void setWorldScale(std::uint32_t index, math::Vec3 value);
    void addTag(std::uint32_t index, std::string_view tag);
    void removeTag(std::uint32_t index, std::string_view tag);
    void removeTagId(std::uint32_t index, std::uint32_t id) noexcept;
    void dropTags(std::uint32_t index) noexcept;
    [[nodiscard]] std::int32_t findChild(std::int32_t parent, std::string_view name) const;
    [[nodiscard]] std::int32_t findPath(std::int32_t from, std::string_view path) const;
    [[nodiscard]] bool isEnabledAt(std::int32_t index) const noexcept;

    // Calls the listeners of the node with the key for the event with the subject, and keeps them alive while they run. The key names the node as it was before a listener could change the tree, so a node that went and a slot that a new node took hear nothing.
    void notify(std::uint64_t key, NodeEvent3D event, const Node3D& subject);

    // Tells the nodes of the subtree that the hierarchy turned on or off whose state that changed, which are the nodes that are enabled themselves under nodes that are.
    void announceEnabled(std::uint32_t index);

    // Brings the world state of the changed subtrees up to date, on the job system when there is one and the subtrees are large.
    void refresh(core::JobSystem* jobs);

    // Walks the roots and their descendants into the order of the arrays, with the end of the subtree of each node.
    void rebuildOrder();

    // Finds the subtrees that hold changes, each from the highest node that changed.
    void collectRanges();

    // Splits the ranges into pieces of at most the grain, updating the root of each range it splits, and gathers the pieces into chunks of about the grain.
    void splitRanges();
    void updateRange(std::size_t begin, std::size_t end) noexcept;
    void updateNode(std::uint32_t index) noexcept;

    std::vector<Slot> slots;
    std::vector<Detail> details;
    std::vector<std::int32_t> freeSlots;
    std::unordered_map<std::uint64_t, std::shared_ptr<Watchers>> watchers;

    // The slots in the order of a walk from the roots, the end of the subtree that starts at each place of the order, and the place of each slot.
    std::vector<std::uint32_t> order;
    std::vector<std::uint32_t> subtreeEnds;
    std::vector<std::uint32_t> places;
    std::vector<std::int32_t> walk;

    // The slots that changed since the last update, which may repeat and may name slots that went since, and the places and ranges an update works through.
    std::vector<std::uint32_t> dirty;
    std::vector<std::uint32_t> dirtyPlaces;
    std::vector<Range> ranges;
    std::vector<Range> pieces;
    std::vector<Range> chunks;

    // The tags by name, the name of each tag and the slots of the nodes of each tag.
    std::unordered_map<std::string, std::uint32_t, NameHash, std::equal_to<>> tagIds;
    std::vector<std::string> tagNames;
    std::vector<std::vector<std::uint32_t>> tagMembers;

    std::vector<Releasing> releasing;
    std::vector<Node3D> announcing;
    std::int32_t firstRoot = kNone;
    std::int32_t lastRoot = kNone;
    std::size_t count = 0;
    std::size_t updated = 0;
    bool orderChanged = false;
};

} // namespace haylen::nodes3d
