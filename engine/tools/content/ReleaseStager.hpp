#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "content/ReleaseInspector.hpp"

namespace haylen::content {

// Splits a protected release into what a platform ships inside an app and what it leaves to a server or a store: the manifests, the shards of the app domain and the shards that the required files use go with the app, and the shards that only prefetched and on-demand files use stay behind, so an app starts with everything it needs and downloads the rest when the app asks for it.
class ReleaseStager final {
  public:
    struct Layout {
        std::vector<std::string> shipped;
        std::vector<std::string> deferred;
    };

    explicit ReleaseStager(const ReleaseInspector& releaseInspector);

    // Names the files of the release that ship and the ones that stay behind.
    [[nodiscard]] Layout plan(const std::filesystem::path& release) const;

    // Copies the files that ship into a new folder and, when a folder for them is given, the ones that stay behind into another, linking them where the file system can.
    Layout stage(const std::filesystem::path& release, const std::filesystem::path& shipped, const std::optional<std::filesystem::path>& deferred) const;

  private:
    static void place(const std::filesystem::path& source, const std::filesystem::path& destination);

    const ReleaseInspector& inspector;
};

} // namespace haylen::content
