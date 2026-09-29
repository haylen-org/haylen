#include "haylen/ui/Document.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/ui/ComponentRegistry.hpp"
#include "haylen/ui/Context.hpp"
#include "haylen/ui/FocusNavigator.hpp"

namespace haylen::ui {

debug::ObjectCounter Document::counter("UiDocument", debug::ObjectCounter::Kind::Native);

// Points the context at the event queue of the document while it draws, and away from it afterwards even when drawing fails.
class Document::EventScope final {
  public:
    EventScope(Context& drawing, std::vector<Event>& queue) : context(drawing) {
        context.setEventQueue(&queue);
    }
    ~EventScope() {
        context.setEventQueue(nullptr);
    }

    EventScope(const EventScope&) = delete;
    EventScope& operator=(const EventScope&) = delete;

  private:
    Context& context;
};

Document::Document(const ComponentRegistry& componentRegistry, const core::Json& tree, Placement where) : registry(componentRegistry), placement(where) {
    std::size_t count = 0;
    Built built = build(tree, 0, count);
    root = std::move(built.component);
    ids = std::move(built.ids);
    properties = std::move(built.properties);
}

void Document::collectIds(const Component& component, std::vector<std::string>& found) {
    if (!component.getId().empty()) {
        found.push_back(component.getId());
    }
    for (const auto& child : component.getChildren()) {
        collectIds(*child, found);
    }
}

math::Rect Document::place(const Component& component, math::Vec2 size, const math::Rect& area) {
    if (component.getAlignment() == Alignment::Stretch) {
        return area;
    }
    return {Component::align(component.getAlignment(), area.x, area.width, size.x), Component::align(component.getAlignment(), area.y, area.height, size.y), size.x, size.y};
}

Document::Built Document::build(const core::Json& node, std::size_t depth, std::size_t& count) const {
    if (!node.is_object()) {
        throw std::invalid_argument("A UI node must be an object with a kind.");
    }
    if (depth > kMaxDepth || ++count > kMaxNodes) {
        throw std::invalid_argument("A UI document is limited to " + std::to_string(kMaxDepth) + " levels and " + std::to_string(kMaxNodes) + " nodes.");
    }
    const auto kind = node.find("kind");
    if (kind == node.end() || !kind->is_string()) {
        throw std::invalid_argument("A UI node needs a kind.");
    }

    Built built{.component = registry.create(kind->get<std::string>()), .ids = {}, .properties = {}};
    Component& component = *built.component;
    if (const auto id = node.find("id"); id != node.end()) {
        if (!id->is_string() || id->get<std::string>().empty()) {
            throw std::invalid_argument("The id of a " + kind->get<std::string>() + " must be a non-empty string.");
        }
        component.id = id->get<std::string>();
        built.ids.emplace(component.id, &component);

        core::Json values = node;
        values.erase("kind");
        values.erase("id");
        values.erase("children");
        built.properties.emplace(component.id, std::move(values));
    }
    component.apply(node);

    const auto children = node.find("children");
    if (children == node.end()) {
        return built;
    }
    if (!children->is_array()) {
        throw std::invalid_argument("The children of a " + kind->get<std::string>() + " must be a list.");
    }
    if (children->size() > component.getChildLimit()) {
        throw std::invalid_argument("A " + kind->get<std::string>() + " takes at most " + std::to_string(component.getChildLimit()) + " children.");
    }
    for (const core::Json& child : *children) {
        Built nested = build(child, depth + 1, count);
        for (auto& [nestedId, target] : nested.ids) {
            if (!built.ids.emplace(nestedId, target).second) {
                throw std::invalid_argument("The UI id " + nestedId + " is used more than once.");
            }
        }
        built.properties.merge(nested.properties);
        component.children.push_back(std::move(nested.component));
    }
    return built;
}

Component* Document::find(std::string_view id) const {
    const auto found = ids.find(id);
    return found != ids.end() ? found->second : nullptr;
}

const core::Json* Document::getProperties(std::string_view id) const {
    const auto found = properties.find(id);
    return found != properties.end() ? &found->second : nullptr;
}

Component& Document::require(std::string_view id) const {
    Component* component = find(id);
    if (component == nullptr) {
        throw std::invalid_argument("The UI document has no node with the id " + std::string(id) + ".");
    }
    return *component;
}

void Document::set(std::string_view id, const core::Json& changes) {
    Component& component = require(id);
    if (!changes.is_object() || changes.contains("kind") || changes.contains("id") || changes.contains("children")) {
        throw std::invalid_argument("set changes properties only, so it takes an object without kind, id or children.");
    }

    // A fresh component of the same kind checks the whole merged state first, so a bad value never leaves the node half updated.
    core::Json merged = properties.at(std::string(id));
    merged.update(changes);
    registry.create(component.getKind())->apply(merged);
    component.apply(changes);
    properties.insert_or_assign(std::string(id), std::move(merged));
}

void Document::replaceChildren(std::string_view id, const core::Json& trees) {
    Component& component = require(id);
    if (!trees.is_array()) {
        throw std::invalid_argument("replaceChildren takes a list of nodes.");
    }
    if (trees.size() > component.getChildLimit()) {
        throw std::invalid_argument("A " + std::string(component.getKind()) + " takes at most " + std::to_string(component.getChildLimit()) + " children.");
    }

    std::vector<std::string> removed;
    for (const auto& child : component.getChildren()) {
        collectIds(*child, removed);
    }
    std::map<std::string, Component*, std::less<>> remainingIds = ids;
    std::map<std::string, core::Json, std::less<>> remainingProperties = properties;
    for (const std::string& gone : removed) {
        remainingIds.erase(gone);
        remainingProperties.erase(gone);
    }

    std::vector<std::unique_ptr<Component>> built;
    std::size_t count = ids.size();
    for (const core::Json& child : trees) {
        Built nested = build(child, 1, count);
        for (auto& [childId, target] : nested.ids) {
            if (!remainingIds.emplace(childId, target).second) {
                throw std::invalid_argument("The UI id " + childId + " is used more than once.");
            }
        }
        remainingProperties.merge(nested.properties);
        built.push_back(std::move(nested.component));
    }

    component.children = std::move(built);
    ids = std::move(remainingIds);
    properties = std::move(remainingProperties);
}

void Document::command(Context& context, std::string_view id, std::string_view name, const core::Json& arguments) {
    require(id).command(context, name, arguments);
}

void Document::draw(Context& context, const math::Rect& area) {
    const EventScope scope(context, events);
    if (visible) {
        context.getFocus().beginDocument(*this);
        root->draw(context, root->getCommon().anchor ? root->getAnchoredBounds(context) : place(*root, root->measure(context, area.width), area));
    }
    root->noticeStoppedDrawing(context);
}

std::vector<Event> Document::takeEvents() {
    return std::exchange(events, {});
}

} // namespace haylen::ui
