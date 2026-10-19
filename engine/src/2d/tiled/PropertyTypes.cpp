#include "haylen/2d/tiled/PropertyTypes.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace haylen::tiled {

PropertyTypes PropertyTypes::parse(const core::Json& project) {
    PropertyTypes result;
    for (const core::Json& entry : project.value("propertyTypes", core::Json::array())) {
        if (entry.value("type", std::string{}) != "class") {
            continue;
        }
        Class type{.name = entry.value("name", std::string{})};
        if (type.name.empty()) {
            throw std::invalid_argument("A class of the Tiled project has no name.");
        }
        for (const core::Json& member : entry.value("members", core::Json::array())) {
            Member read{.name = member.value("name", std::string{}), .type = member.value("type", std::string("string")), .propertyType = member.value("propertyType", std::string{}), .value = member.value("value", core::Json())};
            if (read.name.empty()) {
                throw std::invalid_argument("A member of the class \"" + type.name + "\" of the Tiled project has no name.");
            }
            type.members.push_back(std::move(read));
        }
        result.classes.push_back(std::move(type));
    }
    return result;
}

const PropertyTypes::Class* PropertyTypes::findClass(std::string_view name) const noexcept {
    const auto found = std::ranges::find(classes, name, &Class::name);
    return found != classes.end() ? &*found : nullptr;
}

Properties PropertyTypes::complete(const Properties& properties, std::string_view className) const {
    std::vector<Property> items = properties.getItems();
    for (Property& property : items) {
        if (property.type == "class") {
            property.value = completeValue(property.propertyType, std::move(property.value), 0);
        } else if (property.type == "list") {
            property.value = completeItems(std::move(property.value), 0);
        }
    }
    if (const Class* type = findClass(className)) {
        for (const Member& member : type->members) {
            if (std::ranges::none_of(items, [&member](const Property& property) { return property.name == member.name; })) {
                items.push_back({.name = member.name, .type = member.type, .propertyType = member.propertyType, .value = member.type == "class" ? completeValue(member.propertyType, member.value, 0) : member.value});
            }
        }
    }
    return Properties(std::move(items));
}

// The items of a list name their own type, so class items complete like class values, and lists nested in lists complete their items too.
core::Json PropertyTypes::completeItems(core::Json items, int depth) const {
    if (!items.is_array() || depth >= kMaxDepth) {
        return items;
    }
    for (core::Json& item : items) {
        const std::string type = item.value("type", std::string("string"));
        if (type == "class") {
            item["value"] = completeValue(item.value("propertytype", std::string{}), item.value("value", core::Json()), depth + 1);
        } else if (type == "list") {
            item["value"] = completeItems(item.value("value", core::Json()), depth + 1);
        }
    }
    return items;
}

// A member that is a class holds its own members, so the value completes level by level until no class is left, which a class that holds itself never reaches.
core::Json PropertyTypes::completeValue(std::string_view className, core::Json value, int depth) const {
    const Class* type = findClass(className);
    if (type == nullptr) {
        return value;
    }
    if (depth >= kMaxDepth) {
        throw std::invalid_argument("The class \"" + type->name + "\" of the Tiled project holds itself.");
    }
    if (!value.is_object()) {
        value = core::Json::object();
    }
    for (const Member& member : type->members) {
        if (!value.contains(member.name)) {
            value[member.name] = member.type == "class" ? completeValue(member.propertyType, member.value, depth + 1) : member.value;
        } else if (member.type == "class") {
            value[member.name] = completeValue(member.propertyType, std::move(value[member.name]), depth + 1);
        }
    }
    return value;
}

} // namespace haylen::tiled
