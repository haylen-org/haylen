#include "haylen/storage/SaveSlots.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <format>
#include <stdexcept>
#include <utility>

#include "haylen/core/Log.hpp"
#include "haylen/storage/UserStorage.hpp"
#include "storage/StoredJson.hpp"

namespace haylen::storage {

const std::array<std::pair<std::string_view, SaveSlots::ImportPolicy>, 2> SaveSlots::kImportPolicyNames{{{"replace", ImportPolicy::Replace}, {"keepNewer", ImportPolicy::KeepNewer}}};

SaveSlots::SaveSlots(UserStorage& storage, std::string folder, Clock now) : userStorage(storage), directory(std::move(folder)), clock(now ? std::move(now) : Clock(&unixSeconds)) {}

std::optional<SaveSlots::ImportPolicy> SaveSlots::importPolicyFromName(std::string_view name) noexcept {
    const auto found = std::ranges::find(kImportPolicyNames, name, &std::pair<std::string_view, ImportPolicy>::first);
    return found != kImportPolicyNames.end() ? std::optional(found->second) : std::nullopt;
}

std::string_view SaveSlots::importPolicyName(ImportPolicy value) noexcept {
    return std::ranges::find(kImportPolicyNames, value, &std::pair<std::string_view, ImportPolicy>::second)->first;
}

bool SaveSlots::isValidSlot(std::string_view slot) noexcept {
    const auto allowed = [](char character) { return std::isalnum(static_cast<unsigned char>(character)) != 0 || character == '-' || character == '_'; };
    return !slot.empty() && slot.size() <= 64 && std::ranges::all_of(slot, allowed);
}

std::int64_t SaveSlots::unixSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

void SaveSlots::setVersion(int value) {
    if (value < 1) {
        throw std::invalid_argument("A save version is a whole number from 1.");
    }
    const std::scoped_lock lock(mutex);
    version = value;
}

int SaveSlots::getVersion() const {
    const std::scoped_lock lock(mutex);
    return version;
}

void SaveSlots::addMigration(int fromVersion, Migration migration) {
    if (fromVersion < 1 || !migration) {
        throw std::invalid_argument("A save migration needs a version from 1 and a function.");
    }
    const std::scoped_lock lock(mutex);
    migrations.insert_or_assign(fromVersion, std::move(migration));
}

void SaveSlots::clearMigrations() {
    const std::scoped_lock lock(mutex);
    migrations.clear();
}

std::string SaveSlots::pathOf(std::string_view slot) const {
    if (!isValidSlot(slot)) {
        throw std::invalid_argument("The save slot name \"" + std::string(slot) + "\" must use 1 to 64 letters, digits, dashes or underscores.");
    }
    return directory + "/" + std::string(slot) + std::string(kExtension);
}

std::string SaveSlots::backupOf(std::string_view slot) const {
    return directory + "/" + std::string(slot) + std::string(kBackupExtension);
}

// The checksum covers every field but itself, in the order of the keys, which JSON objects keep sorted.
std::string SaveSlots::checksumOf(const core::Json& saved) {
    core::Json payload = saved;
    payload.erase("checksum");
    std::uint64_t hash = kChecksumOffset;
    for (const char character : payload.dump()) {
        hash = (hash ^ static_cast<unsigned char>(character)) * kChecksumPrime;
    }
    return std::format("{:016x}", hash);
}

std::optional<core::Json> SaveSlots::parse(std::string_view text) {
    core::Json saved = core::Json::parse(text, nullptr, false);
    const auto isInteger = [&saved](const char* key) { return saved.contains(key) && saved.at(key).is_number_integer(); };
    const bool valid = saved.is_object() && saved.contains("data") && saved.contains("summary") && saved.at("summary").is_object() && isInteger("savedAt") && isInteger("version") && saved.at("version").get<std::int64_t>() >= 1 && isInteger("revision") && saved.contains("checksum") && saved.at("checksum").is_string();
    if (!valid || saved.at("checksum").get<std::string>() != checksumOf(saved)) {
        return std::nullopt;
    }
    return saved;
}

SaveSlots::Info SaveSlots::infoOf(std::string_view slot, const core::Json& saved, bool recovered) {
    return {.slot = std::string(slot), .savedAt = saved.at("savedAt").get<std::int64_t>(), .summary = saved.at("summary"), .version = saved.at("version").get<int>(), .revision = saved.at("revision").get<std::uint64_t>(), .recovered = recovered};
}

// Another thread may remove the file after the check, which leaves it absent, while a file that is still there failed to read.
std::optional<std::string> SaveSlots::readFile(const std::string& path) const {
    if (!userStorage.exists(path)) {
        return std::nullopt;
    }
    try {
        return userStorage.readText(path);
    } catch (const std::runtime_error&) {
        if (userStorage.exists(path)) {
            throw;
        }
        return std::nullopt;
    }
}

// A damaged slot falls back to its backup, the file of the write before, and only a slot without a sound copy fails.
std::optional<SaveSlots::Stored> SaveSlots::load(std::string_view slot) const {
    const std::optional<std::string> text = readFile(pathOf(slot));
    if (!text) {
        return std::nullopt;
    }
    if (std::optional<core::Json> saved = parse(*text)) {
        Info info = infoOf(slot, *saved, false);
        return Stored{.data = std::move(saved->at("data")), .info = std::move(info)};
    }
    const std::optional<std::string> backup = readFile(backupOf(slot));
    std::optional<core::Json> previous = backup ? parse(*backup) : std::nullopt;
    if (!previous) {
        throw std::runtime_error("The save slot \"" + std::string(slot) + "\" is damaged.");
    }
    core::Log::warning("The save slot \"{}\" is damaged, and its backup of revision {} takes its place.", slot, previous->at("revision").get<std::uint64_t>());
    Info info = infoOf(slot, *previous, true);
    return Stored{.data = std::move(previous->at("data")), .info = std::move(info)};
}

void SaveSlots::write(std::string_view slot, const core::Json& data, const core::Json& summary) {
    if (!summary.is_object()) {
        throw std::invalid_argument("A save summary must be a JSON object.");
    }
    StoredJson::validate(data);
    StoredJson::validate(summary);
    const std::string path = pathOf(slot);

    // The file of the slot becomes its backup before the new one replaces it, unless it is damaged, which would replace a sound backup with a broken one.
    std::uint64_t revision = 0;
    const std::optional<std::string> current = readFile(path);
    if (current) {
        if (const std::optional<core::Json> saved = parse(*current)) {
            revision = saved->at("revision").get<std::uint64_t>();
            userStorage.writeText(backupOf(slot), *current, UserStorage::Durability::Durable);
        }
    }
    core::Json saved{{"version", getVersion()}, {"revision", revision + 1}, {"savedAt", clock()}, {"summary", summary}, {"data", data}};
    saved["checksum"] = checksumOf(saved);
    userStorage.writeText(path, saved.dump(), UserStorage::Durability::Durable);
}

std::optional<SaveSlots::Stored> SaveSlots::readStored(std::string_view slot) const {
    return load(slot);
}

core::Json SaveSlots::migrate(std::string_view slot, core::Json data, int fromVersion) const {
    std::map<int, Migration> steps;
    int target = 1;
    {
        const std::scoped_lock lock(mutex);
        steps = migrations;
        target = version;
    }
    if (fromVersion > target) {
        throw std::runtime_error(std::format("The save slot \"{}\" has version {}, which is newer than the version {} this app reads.", slot, fromVersion, target));
    }
    for (int current = fromVersion; current < target; ++current) {
        const auto step = steps.find(current);
        if (step == steps.end()) {
            throw std::runtime_error(std::format("The save slot \"{}\" has version {} and no migration reaches version {}.", slot, fromVersion, target));
        }
        data = step->second(std::move(data));
    }
    return data;
}

std::optional<core::Json> SaveSlots::read(std::string_view slot) const {
    std::optional<Stored> stored = load(slot);
    if (!stored) {
        return std::nullopt;
    }
    return migrate(slot, std::move(stored->data), stored->info.version);
}

std::optional<SaveSlots::Info> SaveSlots::getInfo(std::string_view slot) const {
    std::optional<Stored> stored = load(slot);
    return stored ? std::optional(std::move(stored->info)) : std::nullopt;
}

bool SaveSlots::exists(std::string_view slot) const {
    return userStorage.exists(pathOf(slot));
}

bool SaveSlots::remove(std::string_view slot) {
    const bool removed = userStorage.remove(pathOf(slot));
    userStorage.remove(backupOf(slot));
    return removed;
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

std::optional<std::string> SaveSlots::exportSlot(std::string_view slot) const {
    return readFile(pathOf(slot));
}

bool SaveSlots::importSlot(std::string_view slot, std::string_view bytes, ImportPolicy policy) {
    const std::optional<core::Json> imported = parse(bytes);
    if (!imported) {
        throw std::invalid_argument("The imported save slot \"" + std::string(slot) + "\" is damaged.");
    }
    const std::string path = pathOf(slot);
    if (policy == ImportPolicy::KeepNewer) {
        const std::optional<std::string> current = readFile(path);
        const std::optional<core::Json> saved = current ? parse(*current) : std::nullopt;
        if (saved) {
            const auto key = [](const core::Json& value) { return std::pair(value.at("revision").get<std::uint64_t>(), value.at("savedAt").get<std::int64_t>()); };
            if (key(*imported) <= key(*saved)) {
                return false;
            }
        }
    }
    if (const std::optional<std::string> current = readFile(path); current && parse(*current)) {
        userStorage.writeText(backupOf(slot), *current, UserStorage::Durability::Durable);
    }
    userStorage.writeText(path, std::string(bytes), UserStorage::Durability::Durable);
    return true;
}

} // namespace haylen::storage
