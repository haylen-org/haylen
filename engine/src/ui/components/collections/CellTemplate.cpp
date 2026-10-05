#include "ui/components/collections/CellTemplate.hpp"

#include <algorithm>
#include <exception>
#include <stdexcept>
#include <utility>

#include "haylen/ui/CollectionCell.hpp"
#include "haylen/ui/CollectionSource.hpp"
#include "haylen/ui/Component.hpp"
#include "haylen/ui/ComponentRegistry.hpp"
#include "haylen/ui/Gui.hpp"
#include "haylen/ui/PropertyReader.hpp"

namespace haylen::ui {

CellTemplate::CellTemplate(const ComponentRegistry& components, std::string typeName, const core::Json& definition) : registry(components), name(std::move(typeName)) {
    readDefinition(definition);
}

void CellTemplate::readDefinition(const core::Json& definition) {
    if (!definition.is_object() || !definition.contains("template")) {
        throw std::invalid_argument("The type \"" + name + "\" of a \"collection\" needs a \"template\" node.");
    }
    for (const auto& [key, value] : definition.items()) {
        if (std::ranges::find(kKeys, key) == kKeys.end()) {
            throw std::invalid_argument("Unknown key \"" + key + "\" in \"collection.types." + name + "\".");
        }
    }
    if (const auto size = definition.find("estimatedSize"); size != definition.end()) {
        if (!size->is_number() || size->get<double>() < 0.0 || size->get<double>() > kMaxEstimatedSize) {
            throw std::invalid_argument("The \"estimatedSize\" of the type \"" + name + "\" must be a number from 0 to 100000.");
        }
        estimatedSize = size->get<float>();
    }
    if (const auto lanes = definition.find("span"); lanes != definition.end()) {
        const bool full = lanes->is_string() && lanes->get_ref<const std::string&>() == "full";
        if (!full && (!lanes->is_number_integer() || lanes->get<int>() < 1 || lanes->get<int>() > kMaxSpan)) {
            throw std::invalid_argument("The \"span\" of the type \"" + name + "\" must be a whole number from 1 to 64 or \"full\".");
        }
        span = full ? 0 : static_cast<std::uint16_t>(lanes->get<int>());
    }
    for (const auto& [key, target] : {std::pair<const char*, bool*>{"sticky", &sticky}, std::pair<const char*, bool*>{"interactive", &interactive}}) {
        if (const auto flag = definition.find(key); flag != definition.end()) {
            if (!flag->is_boolean()) {
                throw std::invalid_argument("The \"" + std::string(key) + "\" of the type \"" + name + "\" must be \"true\" or \"false\".");
            }
            *target = flag->get<bool>();
        }
    }

    std::set<std::string, std::less<>> parts;
    flatten(definition.at("template"), kNoParent, 0, 0, parts);

    // A prototype checks every property of every node the way a GUI checks its nodes, and tells which nodes take the focus.
    std::vector<Component*> created(nodes.size());
    try {
        const std::unique_ptr<Component> prototype = create(0, created);
        for (std::size_t node = 0; node < nodes.size(); ++node) {
            nodes[node].focusable = created[node]->isFocusable();
        }
    } catch (const std::exception& error) {
        throw std::invalid_argument("The template of the type \"" + name + "\" has a problem. " + error.what());
    }

    // A cell takes the focus as a whole unless a part of it takes the focus itself.
    if (!definition.contains("interactive")) {
        interactive = std::ranges::none_of(nodes, &Node::focusable);
    }
}

void CellTemplate::flatten(const core::Json& node, std::size_t parent, std::size_t slot, std::size_t depth, std::set<std::string, std::less<>>& parts) {
    const std::string problem = "The template of the type \"" + name + "\" has a problem. ";
    const auto kind = node.is_object() ? node.find("kind") : node.end();
    if (!node.is_object() || kind == node.end() || !kind->is_string()) {
        throw std::invalid_argument(problem + "A UI node needs a kind.");
    }
    const std::string& kindName = kind->get_ref<const std::string&>();
    if (!registry.contains(kindName)) {
        throw std::invalid_argument(problem + "There is no UI component kind named \"" + kindName + "\".");
    }
    if (depth >= Gui::kMaxDepth) {
        throw std::invalid_argument(problem + "A template is limited to " + std::to_string(Gui::kMaxDepth) + " levels.");
    }
    if (std::ranges::find(kRejectedKinds, kindName) != kRejectedKinds.end()) {
        throw std::invalid_argument("The template of the type \"" + name + "\" cannot hold a \"scroll\" or a \"collection\".");
    }
    if (node.contains("id")) {
        throw std::invalid_argument("A node inside a collection template names itself with \"part\", not \"id\".");
    }

    std::string part;
    if (const auto named = node.find("part"); named != node.end()) {
        if (!named->is_string() || named->get_ref<const std::string&>().empty()) {
            throw std::invalid_argument("The \"part\" of " + PropertyReader::describeKind(kindName) + " in the template of the type \"" + name + "\" must be a non-empty string.");
        }
        part = named->get<std::string>();
        if (!parts.insert(part).second) {
            throw std::invalid_argument("The part \"" + part + "\" is used more than once in the template of the type \"" + name + "\".");
        }
    }

    core::Json properties = node;
    for (const std::string_view key : {"kind", "children", "part", "bind"}) {
        properties.erase(std::string(key));
    }
    const std::size_t index = nodes.size();
    nodes.push_back({.kind = kindName, .properties = std::move(properties), .part = std::move(part), .parent = parent, .slot = slot, .end = 0, .focusable = false});
    if (const auto bind = node.find("bind"); bind != node.end()) {
        readBindings(*bind, index);
    }

    if (const auto children = node.find("children"); children != node.end()) {
        if (!PropertyReader::isList(*children)) {
            throw std::invalid_argument(problem + "The children of " + PropertyReader::describeKind(kindName) + " must be a list.");
        }
        std::size_t position = 0;
        for (const core::Json& child : *children) {
            flatten(child, index, position++, depth + 1, parts);
        }
    }
    nodes[index].end = nodes.size();
}

// Bindings keep the order of the nodes, so binding a cell applies the values of each node at once.
void CellTemplate::readBindings(const core::Json& bind, std::size_t node) {
    bool valid = bind.is_object();
    for (auto entry = bind.begin(); valid && entry != bind.end(); ++entry) {
        valid = entry->is_string() && !entry->get_ref<const std::string&>().empty() && std::ranges::find(kReservedProperties, entry.key()) == kReservedProperties.end();
    }
    if (!valid) {
        throw std::invalid_argument("The \"bind\" of " + PropertyReader::describeKind(nodes[node].kind) + " in the template of the type \"" + name + "\" must map property names to field names.");
    }
    for (const auto& [property, field] : bind.items()) {
        const std::string& fieldName = field.get_ref<const std::string&>();
        const Source source = fieldName == "$index" ? Source::Index : fieldName == "$selected" ? Source::Selected : Source::Field;
        indexBound = indexBound || source == Source::Index;
        selectionBound = selectionBound || source == Source::Selected;
        bindings.push_back({.node = node, .property = property, .field = fieldName, .source = source});
    }
}

std::unique_ptr<Component> CellTemplate::create(std::size_t node, std::vector<Component*>& created) const {
    const Node& entry = nodes[node];
    std::unique_ptr<Component> component = registry.create(entry.kind);
    component->id = entry.part;
    component->identity = static_cast<int>(node);
    component->apply(entry.properties);
    component->compile(registry);
    created[node] = component.get();
    for (std::size_t child = node + 1; child < entry.end; child = nodes[child].end) {
        component->children.push_back(create(child, created));
    }
    if (component->children.size() > component->getChildLimit()) {
        throw std::invalid_argument("The component kind \"" + entry.kind + "\" takes at most " + std::to_string(component->getChildLimit()) + " children.");
    }
    return component;
}

std::shared_ptr<CollectionCell> CellTemplate::build() const {
    auto cell = std::make_shared<CollectionCell>(*this);
    cell->nodes.resize(nodes.size());
    cell->touched.assign(nodes.size(), 0);
    cell->changedKeys.resize(nodes.size());
    cell->transformed.assign(nodes.size(), 0);
    cell->root = create(0, cell->nodes);
    return cell;
}

void CellTemplate::rebuild(CollectionCell& cell, std::size_t node) const {
    std::unique_ptr<Component> fresh = create(node, cell.nodes);
    const Node& entry = nodes[node];
    if (entry.parent == kNoParent) {
        cell.root = std::move(fresh);
    } else {
        cell.nodes[entry.parent]->children[entry.slot] = std::move(fresh);
    }
    for (std::size_t inside = node; inside < entry.end; ++inside) {
        cell.touched[inside] = 0;
        cell.changedKeys[inside].clear();
        cell.transformed[inside] = 0;
    }
}

// Keys that `set` changed go back to their template values, and a node that the player changed or that lost a key the template does not set is built again, which only allocates in those cases.
void CellTemplate::reset(CollectionCell& cell) const {
    for (std::size_t node = 0; node < nodes.size(); ++node) {
        if (cell.touched[node] != 0) {
            rebuild(cell, node);
            continue;
        }
        std::vector<std::string>& keys = cell.changedKeys[node];
        if (!keys.empty()) {
            core::Json restored = core::Json::object();
            const bool restorable = std::ranges::all_of(keys, [&](const std::string& key) { return nodes[node].properties.contains(key); });
            if (!restorable) {
                rebuild(cell, node);
                continue;
            }
            for (const std::string& key : keys) {
                restored[key] = nodes[node].properties.at(key);
            }
            keys.clear();
            cell.nodes[node]->apply(restored);
        }
        if (cell.transformed[node] != 0) {
            cell.nodes[node]->transform = std::make_shared<Transform>();
            cell.transformed[node] = 0;
        }
    }
}

void CellTemplate::bind(CollectionCell& cell, const CollectionSource& source, std::size_t index, bool selected) const {
    if (cell.generation > 0) {
        reset(cell);
    }
    cell.index = index;
    cell.item = source.getId(index);
    cell.stale = false;
    ++cell.generation;
    applyBindings(cell, &source, index, selected, std::nullopt);
}

void CellTemplate::bindIndex(CollectionCell& cell, std::size_t index) const {
    cell.index = index;
    if (indexBound) {
        applyBindings(cell, nullptr, index, false, Source::Index);
    }
}

void CellTemplate::bindSelection(CollectionCell& cell, bool selected) const {
    if (selectionBound) {
        applyBindings(cell, nullptr, cell.index, selected, Source::Selected);
    }
}

std::string CellTemplate::describeNode(std::size_t node) const {
    return nodes[node].part.empty() ? PropertyReader::describeKind(nodes[node].kind) + " of its cell" : "the part \"" + nodes[node].part + "\"";
}

// A field the item lacks takes the template value, and a node without one is built again, so no value of the previous item stays.
void CellTemplate::applyBindings(CollectionCell& cell, const CollectionSource* source, std::size_t index, bool selected, std::optional<Source> only) const {
    std::size_t current = kNoParent;
    core::Json values;
    bool fresh = false;
    // clang-format off
    const auto flush = [&] {
        if (current == kNoParent) {
            return;
        }
        if (fresh) {
            rebuild(cell, current);
        }
        try {
            cell.nodes[current]->apply(values);
        } catch (const std::exception& error) {
            throw std::invalid_argument("The item \"" + cell.item + "\" gives " + describeNode(current) + " a value it does not accept. " + error.what());
        }
    };
    // clang-format on

    for (const Binding& binding : bindings) {
        if (only && binding.source != *only) {
            continue;
        }
        if (binding.node != current) {
            flush();
            current = binding.node;
            values = core::Json::object();
            fresh = false;
        }
        core::Json value;
        if (binding.source == Source::Index) {
            value = index + 1;
        } else if (binding.source == Source::Selected) {
            value = selected;
        } else {
            value = source->getValue(index, binding.field);
        }
        if (value.is_null()) {
            const core::Json& properties = nodes[binding.node].properties;
            const auto found = properties.find(binding.property);
            if (found == properties.end()) {
                fresh = true;
                continue;
            }
            value = *found;
        }
        values[binding.property] = std::move(value);
    }
    flush();
}

void CellTemplate::writeBack(CollectionCell& cell, const Component& component, CollectionSource& source) const {
    core::Json changed = core::Json::object();
    component.collectPlayerValues(changed);
    if (changed.empty()) {
        return;
    }
    const auto node = static_cast<std::size_t>(component.identity);
    cell.touched[node] = 1;
    for (const Binding& binding : bindings) {
        if (binding.node == node && binding.source == Source::Field && changed.contains(binding.property)) {
            source.setValue(cell.index, binding.field, changed.at(binding.property));
        }
    }
}

void CellTemplate::set(CollectionCell& cell, std::size_t node, const core::Json& properties) const {
    const auto reserved = [&properties](std::string_view key) { return properties.contains(key); };
    if (!properties.is_object() || std::ranges::any_of(kReservedProperties, reserved)) {
        throw std::invalid_argument("The \"set\" method of a cell changes properties only, so it takes an object without \"kind\", \"id\", \"children\", \"part\" or \"bind\".");
    }

    // A fresh component of the same kind checks the template values with the changes first, so a bad value never leaves the part half updated.
    core::Json merged = nodes[node].properties;
    merged.update(properties);
    registry.create(nodes[node].kind)->apply(merged);
    cell.nodes[node]->apply(properties);
    std::vector<std::string>& keys = cell.changedKeys[node];
    for (const auto& [key, value] : properties.items()) {
        if (std::ranges::find(keys, key) == keys.end()) {
            keys.push_back(key);
        }
    }
}

const std::shared_ptr<Transform>& CellTemplate::getTransform(CollectionCell& cell, std::size_t node) const {
    cell.transformed[node] = 1;
    return cell.nodes[node]->getTransform();
}

std::optional<std::size_t> CellTemplate::findPart(std::string_view part) const {
    const auto found = std::ranges::find(nodes, part, &Node::part);
    if (part.empty() || found == nodes.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - nodes.begin());
}

} // namespace haylen::ui
