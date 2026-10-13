#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "haylen/2d/graphics/DrawOrder.hpp"
#include "haylen/2d/graphics/NineSlice.hpp"
#include "haylen/2d/graphics/SpriteInstance.hpp"
#include "haylen/2d/nodes/Node.hpp"
#include "haylen/2d/nodes/NodeContent.hpp"
#include "haylen/2d/nodes/NodeProperties.hpp"
#include "haylen/2d/physics/Body.hpp"
#include "haylen/core/Connection.hpp"
#include "haylen/core/Signal.hpp"
#include "haylen/graphics/Texture.hpp"
#include "haylen/math/Color.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Transform2D.hpp"
#include "haylen/math/Vec2.hpp"

namespace haylen::core {
class JobSystem;
}

namespace haylen::graphics2d {
class Renderer;
}

namespace haylen::nodes2d {

// A retained tree of 2D nodes whose local transforms compose into world transforms, so children follow their parents, with colors, visibility and layers that subtrees inherit. Nodes live in contiguous arrays in the order of a walk from the roots, so an update and a draw visit them in one pass without a call per node from scripts. A tree lives in a shared pointer, which its node handles watch.
class NodeTree final : public std::enable_shared_from_this<NodeTree> {
    struct Passkey {
        explicit Passkey() = default;
    };

  public:
    // Trees this large update the subtrees of their roots on the worker pool.
    static constexpr std::size_t kParallelNodes = 4096;
    static constexpr std::size_t kMaxNodes = std::size_t{1} << 24U;

    explicit NodeTree(Passkey) {}

    NodeTree(const NodeTree&) = delete;
    NodeTree& operator=(const NodeTree&) = delete;

    [[nodiscard]] static std::shared_ptr<NodeTree> create();

    // Adds a node at the root, after the roots the tree has, or under a parent of this tree. Throws `std::length_error` past `kMaxNodes` nodes.
    Node add(const NodeProperties& properties);
    Node add(const Node& parent, const NodeProperties& properties);

    // Finds a node by a path of names from the roots separated by slashes, such as `tank/turret`, or returns an invalid node.
    [[nodiscard]] Node find(std::string_view path) const;
    [[nodiscard]] std::vector<Node> getRoots() const;
    [[nodiscard]] std::size_t size() const noexcept {
        return count;
    }

    // Destroys every node.
    void clear();

    // Brings the world transform, color, visibility, layer and bounds of every node whose local state changed, or whose ancestor's did, up to date, moves the nodes that follow bodies, at `blend` between the last two steps of the worlds that interpolate, and advances the particles the nodes hold by the time.
    void update(float deltaSeconds, float blend = 1.0F, core::JobSystem* jobs = nullptr);

    // Draws the content of every visible node into the open canvas, parents before their children and siblings in order, and leaves out the nodes whose bounds lie outside the area the canvas shows. Particles are never left out. Nodes that changed since the last update take their new place first, without advancing particles.
    void draw(graphics2d::Renderer& renderer);

    // Sets the position and rotation of each node from three floats of the values, x, y and rotation, in order, which moves many nodes in one call. Throws `std::invalid_argument` unless the values hold three floats for each node.
    void writeTransforms(std::span<const Node> nodes, std::span<const float> values);

    // Copies the world position and rotation of each node into three floats of the values, as of the last update.
    void readTransforms(std::span<const Node> nodes, std::span<float> values) const;

    // The nodes a draw drew and left out last time.
    [[nodiscard]] std::size_t getDrawnCount() const noexcept {
        return drawn;
    }
    [[nodiscard]] std::size_t getCulledCount() const noexcept {
        return culled;
    }

    // Calls the slot once, right before the node goes, after the `destroyed` signal of the tree.
    core::Connection onDestroyed(const Node& node, core::Signal<const Node&>::Slot slot, core::Signal<const Node&>::Options options = {});

    // Emits every node right before it goes, descendants before their parents.
    core::Signal<const Node&> destroyed;

  private:
    friend class Node;

    static constexpr std::int32_t kNone = -1;

    // The state the update and the draw read for every node, next to each other.
    struct Slot {
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 scale{1.0F, 1.0F};
        math::Color color = math::Color::white();
        int layer = 0;
        float depth = 0.0F;
        float sortOffset = 0.0F;
        bool visible = true;
        bool alive = false;
        bool following = false;

        // The local state changed since the last update.
        bool changed = true;

        // The world state changed in the last update, which tells the children to follow.
        bool moved = false;
        bool worldVisible = true;
        bool bounded = false;
        int worldLayer = 0;
        math::Color worldColor = math::Color::white();
        math::Transform2D world{};

        // The area the content covers around the node, and the same area in the world after the last update.
        math::Rect localBounds{};
        math::Rect bounds{};
        std::int32_t parent = kNone;
    };

    // The state of every node that the update and the draw rarely read.
    struct Detail {
        std::string name;
        NodeContent content;
        physics2d::Body followed;
        std::uint32_t generation = 1;
        std::int32_t firstChild = kNone;
        std::int32_t lastChild = kNone;
        std::int32_t next = kNone;
        std::int32_t previous = kNone;
    };

    // A transform split into the place, turn and scale that sprites and text take, which leaves out any shear.
    struct Placement {
        math::Vec2 position{};
        float rotation = 0.0F;
        math::Vec2 scale{1.0F, 1.0F};
    };

    // Groups of whole subtrees of roots with at least this many nodes each update as one job.
    static constexpr std::size_t kParallelGrain = 1024;

    [[nodiscard]] static Placement placementOf(const math::Transform2D& transform) noexcept;
    [[nodiscard]] static math::Rect transformBounds(const math::Transform2D& transform, const math::Rect& area) noexcept;
    [[nodiscard]] static bool isParticles(const NodeContent& content) noexcept;

    // Checks the content and returns the area it covers around its node, or nothing for content that is never left out.
    [[nodiscard]] static std::optional<math::Rect> measure(const NodeContent& content);

    // Returns the slot of a live node of this tree, and throws for a destroyed one.
    [[nodiscard]] std::uint32_t check(const Node& node) const;
    [[nodiscard]] Node handle(std::int32_t index) const;
    Node insert(std::int32_t parent, const NodeProperties& properties);
    [[nodiscard]] std::int32_t allocate();
    void link(std::int32_t index, std::int32_t parent);
    void unlink(std::int32_t index) noexcept;
    void release(std::int32_t index);
    void touch(std::uint32_t index) noexcept;
    void setContent(std::uint32_t index, NodeContent content);
    void setParent(std::uint32_t index, std::int32_t parent);
    [[nodiscard]] std::int32_t findChild(std::int32_t parent, std::string_view name) const;
    [[nodiscard]] std::int32_t findPath(std::int32_t from, std::string_view path) const;

    // Walks the roots and their descendants into the order of the arrays, with the end of the subtree of each node.
    void rebuildOrder();

    // Updates the nodes of the order from `begin` to `end`, whose parents come first.
    void updateRange(std::size_t begin, std::size_t end, float blend);
    void follow(Slot& slot, const physics2d::Body& body, float blend) const;
    void refresh(core::JobSystem* jobs);
    void advanceParticles(float deltaSeconds, core::JobSystem* jobs);
    void drawNode(graphics2d::Renderer& renderer, std::uint32_t index);

    // Draws the sprites gathered so far as one batch.
    void flushSprites(graphics2d::Renderer& renderer);
    static void drawParticles(graphics2d::Renderer& renderer, const NodeContent& content);

    std::vector<Slot> slots;
    std::vector<Detail> details;
    std::vector<std::int32_t> freeSlots;
    std::unordered_map<std::uint64_t, std::unique_ptr<core::Signal<const Node&>>> watchers;

    // The slots in the order of a walk from the roots, the end of the subtree that starts at each place of the order, and the place of each slot.
    std::vector<std::uint32_t> order;
    std::vector<std::uint32_t> subtreeEnds;
    std::vector<std::uint32_t> places;

    // The slots that hold particles, and the ranges of the order that update as one job each.
    std::vector<std::uint32_t> particles;
    std::vector<std::pair<std::size_t, std::size_t>> chunks;
    std::vector<std::int32_t> walk;

    // Reused by the draws, so drawing allocates nothing once they have grown: the pieces of frames, and the run of neighbouring sprites of one texture and order that draws as one batch.
    std::vector<graphics2d::NineSlice::Patch> patches;
    std::vector<graphics2d::SpriteInstance> pieces;
    std::vector<graphics2d::SpriteInstance> run;
    graphics::Texture runTexture;
    graphics2d::DrawOrder runOrder;
    std::int32_t firstRoot = kNone;
    std::int32_t lastRoot = kNone;
    std::size_t count = 0;
    std::size_t followers = 0;
    float lastBlend = 1.0F;
    bool orderChanged = false;

    // Some node changed since the last pass over the order.
    bool pending = false;
    std::size_t drawn = 0;
    std::size_t culled = 0;
};

} // namespace haylen::nodes2d
