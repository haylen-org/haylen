#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "haylen/core/Json.hpp"

namespace haylen::storage {

class UserStorage;

// Named save slots kept as JSON files in user storage. Each file holds the saved data, a small summary for load menus, the save version of the app, a revision that counts the writes of the slot and a checksum of the rest, and the previous file of a slot stays as its backup, which a read falls back to when the slot is damaged. Writes flush the file and its folder to the disk before and after they replace it, so a crash or a loss of power leaves the old slot or the new one. Slot names use letters, digits, dashes and underscores. Every method may run on any thread, and migrations run on the thread of the read.
class SaveSlots final {
  public:
    struct Info {
        std::string slot;
        std::int64_t savedAt = 0;
        core::Json summary;
        int version = 1;
        std::uint64_t revision = 0;
        bool recovered = false;
    };

    // A slot as it is stored, before any migration.
    struct Stored {
        core::Json data;
        Info info;
    };

    // Whether an imported slot replaces the slot of the device, or only when its revision is higher, or its save time when the revisions tie.
    enum class ImportPolicy : std::uint8_t {
        Replace,
        KeepNewer,
    };

    // Turns the data of one version into the data of the next one.
    using Migration = std::function<core::Json(core::Json data)>;

    // Returns the current time in Unix seconds. Without one, saves are stamped with the system clock.
    using Clock = std::function<std::int64_t()>;

    explicit SaveSlots(UserStorage& storage, std::string folder = "saves", Clock now = {});

    // The version that writes stamp and reads migrate to, 1 by default. Throws `std::invalid_argument` for a version below 1.
    void setVersion(int value);
    [[nodiscard]] int getVersion() const;

    // Registers the step from a version to the next one. Throws `std::invalid_argument` for a version below 1 or an empty migration.
    void addMigration(int fromVersion, Migration migration);
    void clearMigrations();

    // Throws `std::invalid_argument` for a bad slot name, a summary that is not an object, and a string or key that is not valid UTF-8 or a number that is not finite, which JSON cannot hold.
    void write(std::string_view slot, const core::Json& data, const core::Json& summary = core::Json::object());

    // Reads a slot and migrates its data to the version of the app. Throws `std::runtime_error` for a slot whose file and backup are damaged, a slot of a newer version and a version that no chain of migrations reaches.
    [[nodiscard]] std::optional<core::Json> read(std::string_view slot) const;
    [[nodiscard]] std::optional<Stored> readStored(std::string_view slot) const;
    [[nodiscard]] core::Json migrate(std::string_view slot, core::Json data, int fromVersion) const;

    [[nodiscard]] std::optional<Info> getInfo(std::string_view slot) const;
    [[nodiscard]] bool exists(std::string_view slot) const;

    // Removes the slot and its backup.
    bool remove(std::string_view slot);

    // Lists every slot, newest first.
    [[nodiscard]] std::vector<Info> list() const;

    // Returns the file of a slot as it is stored, for cloud saves to move it to another device.
    [[nodiscard]] std::optional<std::string> exportSlot(std::string_view slot) const;

    // Takes a file that `exportSlot` returned and returns whether it replaced the slot. Throws `std::invalid_argument` for bytes that are not a whole slot.
    bool importSlot(std::string_view slot, std::string_view bytes, ImportPolicy policy);

    [[nodiscard]] static std::optional<ImportPolicy> importPolicyFromName(std::string_view name) noexcept;
    [[nodiscard]] static std::string_view importPolicyName(ImportPolicy value) noexcept;

  private:
    static constexpr std::string_view kExtension = ".json";
    static constexpr std::string_view kBackupExtension = ".backup.json";
    static constexpr std::uint64_t kChecksumOffset = 14695981039346656037ULL;
    static constexpr std::uint64_t kChecksumPrime = 1099511628211ULL;
    static const std::array<std::pair<std::string_view, ImportPolicy>, 2> kImportPolicyNames;

    [[nodiscard]] static bool isValidSlot(std::string_view slot) noexcept;
    [[nodiscard]] static std::int64_t unixSeconds();
    [[nodiscard]] static std::string checksumOf(const core::Json& saved);

    // Parses a stored file and returns it when its fields and its checksum hold.
    [[nodiscard]] static std::optional<core::Json> parse(std::string_view text);
    [[nodiscard]] static Info infoOf(std::string_view slot, const core::Json& saved, bool recovered);

    [[nodiscard]] std::string pathOf(std::string_view slot) const;
    [[nodiscard]] std::string backupOf(std::string_view slot) const;
    [[nodiscard]] std::optional<std::string> readFile(const std::string& path) const;
    [[nodiscard]] std::optional<Stored> load(std::string_view slot) const;

    UserStorage& userStorage;
    std::string directory;
    Clock clock;
    mutable std::mutex mutex;
    int version = 1;
    std::map<int, Migration> migrations;
};

} // namespace haylen::storage
