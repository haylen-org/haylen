#include "haylen/storage/ResumeState.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <utility>

#include "haylen/storage/UserStorage.hpp"
#include "storage/StoredJson.hpp"

namespace haylen::storage {

ResumeState::ResumeState(UserStorage& storage, std::string file) : userStorage(storage), path(std::move(file)) {}

void ResumeState::load() {
    restored.reset();
    if (!userStorage.exists(path)) {
        return;
    }

    const core::Json stored = core::Json::parse(userStorage.readText(path), nullptr, false);
    userStorage.remove(path);
    userStorage.flush();
    const auto savedAt = stored.is_object() ? stored.find("savedAt") : stored.end();
    if (stored.is_discarded() || !stored.is_object() || savedAt == stored.end() || !savedAt->is_number() || !stored.contains("value")) {
        throw std::runtime_error("The resume state \"" + path + "\" is damaged, so the app starts without it.");
    }
    const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    restored = Restored{.value = stored.at("value"), .age = std::max(0.0, now - savedAt->get<double>())};
}

void ResumeState::set(core::Json state) {
    StoredJson::validate(state);
    value = std::move(state);
}

void ResumeState::save() {
    if (value.is_null()) {
        discard();
        return;
    }
    const double now = std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count();
    userStorage.writeText(path, core::Json{{"savedAt", now}, {"value", value}}.dump(), UserStorage::Durability::Durable);
    userStorage.flush();
    written = true;
}

void ResumeState::discard() {
    if (!std::exchange(written, false)) {
        return;
    }
    userStorage.remove(path);
    userStorage.flush();
}

} // namespace haylen::storage
