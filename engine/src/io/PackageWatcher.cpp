#include "haylen/io/PackageWatcher.hpp"

#include <algorithm>
#include <string_view>
#include <system_error>
#include <utility>

#include "haylen/io/Path.hpp"

namespace haylen::io {

PackageWatcher::PackageWatcher(std::filesystem::path folder) : root(std::move(folder)), files(snapshot()) {}

std::map<std::string, std::filesystem::file_time_type> PackageWatcher::snapshot() const {
    std::map<std::string, std::filesystem::file_time_type> found;
    std::error_code error;
    // clang-format off
    const auto record = [&](const std::filesystem::directory_entry& entry) {
        if (entry.is_regular_file(error)) {
            const std::filesystem::file_time_type time = entry.last_write_time(error);
            if (!error) {
                found.emplace(entry.path().lexically_relative(root).generic_string(), time);
            }
        }
    };
    // clang-format on

    // Only the package entries count, so the platform projects and build outputs that share the folder are never scanned.
    record(std::filesystem::directory_entry(root / Path::kAppConfigFile, error));
    const auto options = std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied;
    for (const std::string_view folder : {Path::kSourceDirectory, Path::kContentDirectory}) {
        for (auto iterator = std::filesystem::recursive_directory_iterator(root / folder, options, error); iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
            if (error) {
                break;
            }
            record(*iterator);
        }
    }
    return found;
}

std::vector<std::string> PackageWatcher::scan() {
    std::map<std::string, std::filesystem::file_time_type> current = snapshot();
    std::vector<std::string> changed;
    for (const auto& [path, time] : current) {
        const auto previous = files.find(path);
        if (previous == files.end() || previous->second != time) {
            changed.push_back(path);
        }
    }
    for (const auto& [path, time] : files) {
        if (!current.contains(path)) {
            changed.push_back(path);
        }
    }
    std::ranges::sort(changed);
    files = std::move(current);
    return changed;
}

} // namespace haylen::io
