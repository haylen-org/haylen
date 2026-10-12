#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "content/ReleaseInspector.hpp"
#include "haylen/core/AppConfig.hpp"

namespace haylen::content {

// Splits a protected release into what a platform ships inside an app and what it leaves to a server or a store: the manifests, the shards of the app domain and the shards that the required files use go with the app, the shards of each named pack go to the store that delivers the pack, and the shards that only prefetched and on-demand files use stay behind, so an app starts with everything it needs and downloads the rest when the app asks for it.
class ReleaseStager final {
  public:
    // The files that ship with the app, the shards that stay for the server, and the shards of each named pack, which a store delivers on its own.
    struct Layout {
        std::vector<std::string> shipped;
        std::vector<std::string> deferred;
        std::map<std::string, std::vector<std::string>> packs;
    };

    // The rules of `app.json` name the pack of each asset, and without them no asset belongs to a pack.
    ReleaseStager(const ReleaseInspector& releaseInspector, core::AppConfig::Content rules);

    // Which content ships besides the packs: the shards the required files use, every shard, for an app without a server, or none, as for a web page, which downloads the manifests and the app domain before the app starts and streams all of its content.
    enum class Content : std::uint8_t {
        Required,
        All,
        None,
    };

    // Names the files of the release that ship and the ones that stay behind.
    [[nodiscard]] Layout plan(const std::filesystem::path& release, Content content = Content::Required) const;

    // Copies the files that ship into a new folder, and, when folders for them are given, the ones that stay behind into another and the shards of each pack into a folder of the pack in a third, linking them where the file system can.
    Layout stage(const std::filesystem::path& release, const std::filesystem::path& shipped, const std::optional<std::filesystem::path>& deferred, const std::optional<std::filesystem::path>& packs, Content content = Content::Required) const;

  private:
    static void place(const std::filesystem::path& source, const std::filesystem::path& destination);

    const ReleaseInspector& inspector;
    core::AppConfig::Content settings;
};

} // namespace haylen::content
