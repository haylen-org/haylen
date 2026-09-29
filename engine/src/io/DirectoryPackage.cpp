#include "io/DirectoryPackage.hpp"

#include <algorithm>
#include <system_error>
#include <utility>

#include "haylen/io/Path.hpp"

namespace haylen::io {

DirectoryPackage::DirectoryPackage(std::filesystem::path folder) : root(std::move(folder)), name(root.filename().generic_string()) {}

bool DirectoryPackage::exists(std::string_view path) const {
    std::error_code error;
    return std::filesystem::is_regular_file(root / Path::normalize(path), error);
}

std::vector<std::uint8_t> DirectoryPackage::read(std::string_view path) const {
    return readFile(root / Path::normalize(path));
}

std::vector<std::string> DirectoryPackage::list(std::string_view directory) const {
    const std::string relative = Path::normalize(directory);
    const std::filesystem::path start = root / relative;
    std::vector<std::string> files;

    std::error_code error;
    if (!std::filesystem::is_directory(start, error)) {
        return files;
    }

    const auto options = std::filesystem::directory_options::follow_directory_symlink;
    for (auto iterator = std::filesystem::recursive_directory_iterator(start, options, error); iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
        if (error) {
            break;
        }
        if (iterator->is_regular_file(error)) {
            files.push_back(iterator->path().lexically_relative(root).generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace haylen::io
