#include "ui/components/ChoiceItem.hpp"

#include <stdexcept>

#include "haylen/core/JsonValidator.hpp"

namespace haylen::ui {

ChoiceItem ChoiceItem::read(const core::Json& value, const std::string& context, bool nested, std::set<std::string, std::less<>>& ids) {
    if (!value.is_object()) {
        throw std::invalid_argument(PropertyReader::describeProperty(context) + " must hold objects with an id.");
    }
    core::JsonValidator::requireKnownKeys(value, {"id", "text", "caption", "image", "enabled", "children"}, "\"" + context + "\"");
    const auto found = value.find("id");
    if (found == value.end() || !found->is_string() || found->get<std::string>().empty()) {
        throw std::invalid_argument(PropertyReader::describeProperty(context) + " needs a non-empty id for every item.");
    }

    ChoiceItem item{.id = found->get<std::string>()};
    if (!ids.insert(item.id).second) {
        throw std::invalid_argument(PropertyReader::describeProperty(context) + " uses the item id \"" + item.id + "\" more than once.");
    }
    if (const auto field = value.find("text"); field != value.end()) {
        item.text = TextValue::fromJson(*field, context + ".text");
    }
    if (const auto field = value.find("caption"); field != value.end()) {
        item.caption = TextValue::fromJson(*field, context + ".caption");
    }
    if (const auto field = value.find("image"); field != value.end()) {
        if (!field->is_string()) {
            throw std::invalid_argument(PropertyReader::describeProperty(context + ".image") + " must be a path.");
        }
        item.image = field->get<std::string>();
    }
    if (const auto field = value.find("enabled"); field != value.end()) {
        if (!field->is_boolean()) {
            throw std::invalid_argument(PropertyReader::describeProperty(context + ".enabled") + " must be \"true\" or \"false\".");
        }
        item.enabled = field->get<bool>();
    }
    if (const auto field = value.find("children"); field != value.end()) {
        if (!nested) {
            throw std::invalid_argument(PropertyReader::describeProperty(context) + " cannot nest items, which only a tree does.");
        }
        if (!PropertyReader::isList(*field)) {
            throw std::invalid_argument(PropertyReader::describeProperty(context + ".children") + " must be a list of items.");
        }
        for (const core::Json& child : *field) {
            item.children.push_back(read(child, context, nested, ids));
        }
    }
    return item;
}

void ChoiceItem::readList(PropertyReader& reader, std::string_view key, std::vector<ChoiceItem>& out, bool nested) {
    const core::Json* value = reader.take(key);
    if (value == nullptr) {
        return;
    }
    if (!PropertyReader::isList(*value)) {
        reader.fail(key, "must be a list of items");
    }

    std::vector<ChoiceItem> items;
    std::set<std::string, std::less<>> ids;
    for (const core::Json& entry : *value) {
        items.push_back(read(entry, reader.getQualifiedName(key), nested, ids));
    }
    out = std::move(items);
}

int ChoiceItem::indexOf(const std::vector<ChoiceItem>& items, std::string_view itemId) {
    for (std::size_t index = 0; index < items.size(); ++index) {
        if (items[index].id == itemId) {
            return static_cast<int>(index);
        }
    }
    return -1;
}

} // namespace haylen::ui
