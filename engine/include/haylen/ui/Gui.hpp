#pragma once

#include <cstddef>
#include <functional>
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

// A mounted tree of components. A node is an object with `kind`, an optional `id`, `children` and its properties, and ids are unique within the GUI. Events wait in a queue until the owner takes them.
class Gui final {
  public:
    // Hands over an event at once, for the unmount of nodes that leave, whose handlers end with them.
    using Delivery = std::function<void(Gui&, const Event&)>;

    static constexpr std::size_t kMaxDepth = 64;
    static constexpr std::size_t kMaxNodes = 20000;

    Gui(const ComponentRegistry& componentRegistry, const core::Json& tree, Placement where = Placement::Safe);

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

    // Makes the node report one of the events every kind reports, such as its presses, or stop reporting it.
    void listen(std::string_view id, Component::Notice notice, bool value);

    // The UI plugin attaches a GUI it mounts, whose nodes then report that they joined it the next time it draws, and detaches it when it unmounts, which returns the unmount events of its nodes for the plugin to deliver at once.
    void attach(Delivery deliver);
    [[nodiscard]] std::vector<Event> detach();

    void draw(Context& context, const math::Rect& area);

    // Tells the components of a GUI that the UI does not draw this frame, such as the GUI of a covered scene, that they stopped drawing.
    void skip(Context& context);
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

    static void collectUnmounts(Component& component, std::vector<Event>& found);
    void reportMounts(Component& component);
    void deliver(const std::vector<Event>& found);

    [[nodiscard]] Built build(const core::Json& node, std::size_t depth, std::size_t& count) const;
    [[nodiscard]] Component& require(std::string_view id) const;

    const ComponentRegistry& registry;
    std::unique_ptr<Component> root;
    std::map<std::string, Component*, std::less<>> ids;
    std::map<std::string, core::Json, std::less<>> properties;
    std::size_t nodeCount = 0;
    std::vector<Event> events;
    Delivery delivery;
    Placement placement;
    bool visible = true;

    // Whether nodes joined the attached GUI since it last drew, so they report it.
    bool arrived = false;
    debug::TrackedObject tracked{counter};
};

} // namespace haylen::ui
