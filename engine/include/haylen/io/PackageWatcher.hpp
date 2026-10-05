#pragma once

#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::io {

// Notices the files of `app.json`, `source`, `content` and the `plugin.json` and `source` folder of every plugin that were added, changed or removed in a package folder since the previous scan, for reloading an app while it is being made. Hidden files, backup files and files under the source folders that are not Lua never count, so the swap and backup files of editors change nothing.
class PackageWatcher final {
  public:
    // How long a scan that found changes waits before it looks at them again, so a file that an editor or a copy is still writing waits for the next scan.
    static constexpr std::chrono::milliseconds kSettleTime{50};

    // Takes the snapshot that changes count from.
    explicit PackageWatcher(std::filesystem::path folder);

    // Returns the changed paths relative to the folder, in sorted order, leaving out the files that were still changing.
    [[nodiscard]] std::vector<std::string> scan();

    // Tells whether a package path is one whose changes count: `app.json`, a file under `content`, a Lua module under `source`, and the `plugin.json` and the Lua modules under `source` of a plugin, none of them hidden or a backup.
    [[nodiscard]] static bool isWatched(std::string_view path);

  private:
    static constexpr std::array<std::string_view, 6> kBackupExtensions{".swp", ".swo", ".swx", ".tmp", ".bak", ".orig"};

    struct Stamp {
        std::filesystem::file_time_type time;
        std::uintmax_t size = 0;

        friend bool operator==(const Stamp&, const Stamp&) = default;
    };

    [[nodiscard]] std::map<std::string, Stamp> snapshot() const;
    [[nodiscard]] Stamp stamp(const std::string& path) const;

    std::filesystem::path root;
    std::map<std::string, Stamp> files;
};

} // namespace haylen::io
