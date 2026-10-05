#include "haylen/io/PackageWatcher.hpp"

#include <algorithm>
#include <system_error>
#include <thread>
#include <utility>

#include "haylen/io/Path.hpp"

namespace haylen::io {

PackageWatcher::PackageWatcher(std::filesystem::path folder) : root(std::move(folder)), files(snapshot()) {}

bool PackageWatcher::isWatched(std::string_view path) {
    std::size_t start = 0;
    while (start < path.size()) {
        const std::size_t end = std::min(path.find('/', start), path.size());
        const std::string_view segment = path.substr(start, end - start);
        if (segment.starts_with('.') || segment.starts_with('#') || segment.ends_with('~')) {
            return false;
        }
        start = end + 1;
    }
    const std::string_view extension = Path::extension(path);
    if (std::ranges::find(kBackupExtensions, extension) != kBackupExtensions.end()) {
        return false;
    }

    if (path == Path::kAppConfigFile || Path::isInside(path, Path::kContentDirectory)) {
        return true;
    }
    if (Path::isInside(path, Path::kSourceDirectory)) {
        return extension == ".lua";
    }
    if (!Path::isInside(path, Path::kPluginsDirectory)) {
        return false;
    }

    // A plugin counts with its manifest and the Lua modules of its source folder, never with its native parts.
    const std::string_view inPlugin = path.substr(Path::kPluginsDirectory.size() + 1);
    const std::size_t slash = inPlugin.find('/');
    if (slash == std::string_view::npos) {
        return false;
    }
    const std::string_view relative = inPlugin.substr(slash + 1);
    return relative == Path::kPluginManifestFile || (Path::isInside(relative, Path::kSourceDirectory) && extension == ".lua");
}

PackageWatcher::Stamp PackageWatcher::stamp(const std::string& path) const {
    std::error_code error;
    const std::filesystem::path file = root / path;
    Stamp result{.time = std::filesystem::last_write_time(file, error)};
    if (!error) {
        result.size = std::filesystem::file_size(file, error);
    }
    return error ? Stamp{} : result;
}

std::map<std::string, PackageWatcher::Stamp> PackageWatcher::snapshot() const {
    std::map<std::string, Stamp> found;
    std::error_code error;
    // clang-format off
    const auto record = [&](const std::filesystem::directory_entry& entry) {
        if (!entry.is_regular_file(error)) {
            return;
        }
        std::string path = entry.path().lexically_relative(root).generic_string();
        if (!isWatched(path)) {
            return;
        }
        const Stamp current{.time = entry.last_write_time(error), .size = error ? 0 : entry.file_size(error)};
        if (!error) {
            found.emplace(std::move(path), current);
        }
    };
    // clang-format on

    const auto options = std::filesystem::directory_options::follow_directory_symlink | std::filesystem::directory_options::skip_permission_denied;
    // clang-format off
    const auto recordFolder = [&](const std::filesystem::path& folder) {
        for (auto iterator = std::filesystem::recursive_directory_iterator(folder, options, error); iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
            if (error) {
                break;
            }
            record(*iterator);
        }
    };
    // clang-format on

    // Only the package entries count, so the platform projects and build outputs that share the folder, and the native parts of the plugins, are never scanned.
    record(std::filesystem::directory_entry(root / Path::kAppConfigFile, error));
    recordFolder(root / Path::kSourceDirectory);
    recordFolder(root / Path::kContentDirectory);
    for (auto plugin = std::filesystem::directory_iterator(root / Path::kPluginsDirectory, options, error); plugin != std::filesystem::directory_iterator(); plugin.increment(error)) {
        if (error) {
            break;
        }
        const std::filesystem::path folder = plugin->path();
        record(std::filesystem::directory_entry(folder / Path::kPluginManifestFile, error));
        recordFolder(folder / Path::kSourceDirectory);
    }
    return found;
}

// A file whose size or time moved while the scan waited is still being written, so it keeps its previous stamp and the next scan reports it once it settled.
std::vector<std::string> PackageWatcher::scan() {
    std::map<std::string, Stamp> current = snapshot();
    std::vector<std::string> changed;
    for (const auto& [path, found] : current) {
        const auto previous = files.find(path);
        if (previous == files.end() || previous->second != found) {
            changed.push_back(path);
        }
    }
    for (const auto& [path, found] : files) {
        if (!current.contains(path)) {
            changed.push_back(path);
        }
    }
    if (changed.empty()) {
        return changed;
    }

    std::this_thread::sleep_for(kSettleTime);
    // clang-format off
    std::erase_if(changed, [&](const std::string& path) {
        const auto seen = current.find(path);
        const Stamp now = stamp(path);
        const bool settled = seen == current.end() ? now == Stamp{} : now == seen->second;
        if (settled) {
            return false;
        }
        if (const auto previous = files.find(path); previous != files.end()) {
            current.insert_or_assign(path, previous->second);
        } else {
            current.erase(path);
        }
        return true;
    });
    // clang-format on
    std::ranges::sort(changed);
    files = std::move(current);
    return changed;
}

} // namespace haylen::io
