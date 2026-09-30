#include "haylen/storage/SaveSlots.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <stdexcept>
#include <utility>

#include "haylen/storage/UserStorage.hpp"

namespace haylen::storage {

SaveSlots::SaveSlots(UserStorage& storage, std::string folder, Clock now) : userStorage(storage), directory(std::move(folder)), clock(now ? std::move(now) : Clock(&unixSeconds)) {}

bool SaveSlots::isValidSlot(std::string_view slot) noexcept {
    const auto allowed = [](char character) { return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' || character == '_'; };
    return !slot.empty() && slot.size() <= 64 && std::ranges::all_of(slot, allowed);
}

std::int64_t SaveSlots::unixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string SaveSlots::pathOf(std::string_view slot) const {
    if (!isValidSlot(slot)) {
        throw std::invalid_argument("The save slot name \"" + std::string(slot) + "\" must use 1 to 64 letters, digits, dashes or underscores.");
    }
    return directory + "/" + std::string(slot) + std::string(kExtension);
}

void SaveSlots::write(std::string_view slot, const core::Json& data, const core::Json& summary) {
    if (!summary.is_object()) {
        throw std::invalid_argument("A save summary must be a JSON object.");
    }
    const core::Json saved{{"savedAt", clock()}, {"summary", summary}, {"data", data}};
    userStorage.writeText(pathOf(slot), saved.dump());
}

std::optional<core::Json> SaveSlots::document(std::string_view slot) const {
    const std::string path = pathOf(slot);
    if (!userStorage.exists(path)) {
        return std::nullopt;
    }

    core::Json saved = core::Json::parse(userStorage.readText(path), nullptr, false);
    const bool valid = saved.is_object() && saved.contains("data") && saved.contains("summary") && saved.at("summary").is_object() && saved.contains("savedAt") && saved.at("savedAt").is_number_integer();
    if (!valid) {
        throw std::runtime_error("The save slot \"" + std::string(slot) + "\" is damaged.");
    }
    return saved;
}

std::optional<core::Json> SaveSlots::read(std::string_view slot) const {
    std::optional<core::Json> saved = document(slot);
    if (!saved) {
        return std::nullopt;
    }
    return std::move(saved->at("data"));
}

std::optional<SaveSlots::Info> SaveSlots::getInfo(std::string_view slot) const {
    const std::optional<core::Json> saved = document(slot);
    if (!saved) {
        return std::nullopt;
    }
    return Info{.slot = std::string(slot), .savedAt = saved->at("savedAt").get<std::int64_t>(), .summary = saved->at("summary")};
}

bool SaveSlots::exists(std::string_view slot) const {
    return userStorage.exists(pathOf(slot));
}

bool SaveSlots::remove(std::string_view slot) {
    return userStorage.remove(pathOf(slot));
}

std::vector<SaveSlots::Info> SaveSlots::list() const {
    std::vector<Info> saves;
    const std::string prefix = directory + "/";
    for (const std::string& file : userStorage.list(directory)) {
        if (!file.starts_with(prefix) || !file.ends_with(kExtension)) {
            continue;
        }
        const std::string_view slot = std::string_view(file).substr(prefix.size(), file.size() - prefix.size() - kExtension.size());
        if (!isValidSlot(slot)) {
            continue;
        }

        // A slot removed after the folder was listed is left out.
        std::optional<Info> info = getInfo(slot);
        if (info) {
            saves.push_back(std::move(*info));
        }
    }

    // clang-format off
    std::ranges::sort(saves, [](const Info& lhs, const Info& rhs) {
        return lhs.savedAt != rhs.savedAt ? lhs.savedAt > rhs.savedAt : lhs.slot < rhs.slot;
    });
    // clang-format on
    return saves;
}

} // namespace haylen::storage
