#include "haylen/storage/Preferences.hpp"

#include <stdexcept>
#include <utility>

#include "haylen/storage/UserStorage.hpp"

namespace haylen::storage {

Preferences::Preferences(UserStorage& storage, std::string file) : userStorage(storage), path(std::move(file)) {}

std::vector<std::string> Preferences::splitKey(std::string_view key) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t dot = key.find('.', start);
        const std::string_view part = key.substr(start, dot == std::string_view::npos ? std::string_view::npos : dot - start);
        if (part.empty()) {
            throw std::invalid_argument("A preference key is a dotted path without empty parts: " + std::string(key));
        }
        parts.emplace_back(part);
        if (dot == std::string_view::npos) {
            return parts;
        }
        start = dot + 1;
    }
}

void Preferences::load() {
    dirty = false;
    if (!userStorage.exists(path)) {
        values = core::Json::object();
        return;
    }

    core::Json stored = core::Json::parse(userStorage.readText(path), nullptr, false);
    if (stored.is_discarded() || !stored.is_object()) {
        values = core::Json::object();
        throw std::runtime_error("The preferences file " + path + " is damaged.");
    }
    values = std::move(stored);
}

void Preferences::save() {
    userStorage.writeText(path, values.dump(2));
    userStorage.flush();
    dirty = false;
}

const core::Json* Preferences::find(std::string_view key) const {
    const core::Json* node = &values;
    for (const std::string& part : splitKey(key)) {
        if (!node->is_object()) {
            return nullptr;
        }
        const auto child = node->find(part);
        if (child == node->end()) {
            return nullptr;
        }
        node = &*child;
    }
    return node;
}

bool Preferences::has(std::string_view key) const {
    return find(key) != nullptr;
}

core::Json Preferences::get(std::string_view key, const core::Json& defaultValue) const {
    const core::Json* value = find(key);
    return value != nullptr ? *value : defaultValue;
}

void Preferences::set(std::string_view key, core::Json value) {
    if (value.is_null()) {
        remove(key);
        return;
    }

    const std::vector<std::string> parts = splitKey(key);
    core::Json* node = &values;
    for (std::size_t index = 0; index + 1 < parts.size(); ++index) {
        core::Json& child = (*node)[parts[index]];
        if (child.is_null()) {
            child = core::Json::object();
        }
        if (!child.is_object()) {
            throw std::invalid_argument("The preference key " + std::string(key) + " passes through " + parts[index] + ", which holds a value instead of a group.");
        }
        node = &child;
    }
    (*node)[parts.back()] = std::move(value);
    dirty = true;
}

bool Preferences::remove(std::string_view key) {
    const std::vector<std::string> parts = splitKey(key);
    core::Json* node = &values;
    for (std::size_t index = 0; index + 1 < parts.size(); ++index) {
        const auto child = node->find(parts[index]);
        if (child == node->end() || !child->is_object()) {
            return false;
        }
        node = &*child;
    }
    const bool removed = node->erase(parts.back()) > 0;
    dirty = dirty || removed;
    return removed;
}

void Preferences::clear() {
    values = core::Json::object();
    dirty = true;
}

} // namespace haylen::storage
