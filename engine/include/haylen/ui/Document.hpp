#pragma once

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "haylen/core/Json.hpp"
#include "haylen/debug/ObjectCounter.hpp"
#include "haylen/debug/TrackedObject.hpp"
#include "haylen/math/Rect.hpp"
#include "haylen/math/Vec2.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/Event.hpp"
#include "haylen/ui/Placement.hpp"

namespace haylen::ui {

class ComponentRegistry;
class Context;

// A mounted tree of components. A node is an object with kind, an optional id, children and its properties, and ids are unique within the document. Events wait in a queue until the owner takes them.
class Document final {
  public:
    static constexpr std::size_t kMaxDepth = 64;
    static constexpr std::size_t kMaxNodes = 20000;

    Document(const ComponentRegistry& componentRegistry, const core::Json& tree, Placement where = Placement::Safe);

    [[nodiscard]] Component& getRoot() noexcept {
        return *root;
    }
    [[nodiscard]] Component* find(std::string_view id) const;

    // Returns the properties a node was created with, updated by every set, or nothing for an unknown id.
    [[nodiscard]] const core::Json* getProperties(std::string_view id) const;
    [[nodiscard]] Placement getPlacement() const noexcept {
        return placement;
    }

    // Changes the given properties of a node and leaves the others as they are.
    void set(std::string_view id, const core::Json& changes);

    // Replaces the children of a node with new trees, checking every id before anything changes.
    void replaceChildren(std::string_view id, const core::Json& trees);

    void command(Context& context, std::string_view id, std::string_view name, const core::Json& arguments);

    void draw(Context& context, const math::Rect& area);
    [[nodiscard]] std::vector<Event> takeEvents();

    [[nodiscard]] bool isVisible() const noexcept {
        return visible;
    }
    void setVisible(bool value) noexcept {
        visible = value;
    }

  private:
    class EventScope;

    static debug::ObjectCounter& counter;

    struct Built {
        std::unique_ptr<Component> component;
        std::map<std::string, Component*, std::less<>> ids;
        std::map<std::string, core::Json, std::less<>> properties;
    };

    static void collectIds(const Component& component, std::vector<std::string>& found);
    [[nodiscard]] static std::size_t countNodes(const Component& component);
    [[nodiscard]] static std::optional<std::size_t> findDepth(const Component& component, const Component& target, std::size_t depth);
    [[nodiscard]] static math::Rect place(const Context& context, const Component& component, math::Vec2 size, const math::Rect& area);

    [[nodiscard]] Built build(const core::Json& node, std::size_t depth, std::size_t& count) const;
    [[nodiscard]] Component& require(std::string_view id) const;

    const ComponentRegistry& registry;
    std::unique_ptr<Component> root;
    std::map<std::string, Component*, std::less<>> ids;
    std::map<std::string, core::Json, std::less<>> properties;
    std::size_t nodeCount = 0;
    std::vector<Event> events;
    Placement placement;
    bool visible = true;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::ui
