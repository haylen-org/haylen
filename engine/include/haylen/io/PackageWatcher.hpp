#pragma once

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace haylen::io {

// Notices the files of app.json, source and content that were added, changed or removed in a package folder since the previous scan, for reloading an app while it is being made.
class PackageWatcher final {
  public:
    explicit PackageWatcher(std::filesystem::path folder);

    // Returns the changed paths relative to the folder, in sorted order.
    [[nodiscard]] std::vector<std::string> scan();

  private:
    [[nodiscard]] std::map<std::string, std::filesystem::file_time_type> snapshot() const;

    std::filesystem::path root;
    std::map<std::string, std::filesystem::file_time_type> files;
};

} // namespace haylen::io
