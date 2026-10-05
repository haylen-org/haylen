#include "io/DirectoryPackage.hpp"

#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "haylen/io/Path.hpp"
#include "io/FileReader.hpp"

namespace haylen::io {

DirectoryPackage::DirectoryPackage(std::filesystem::path folder) : root(std::move(folder)), name(root.filename().generic_string()) {}

bool DirectoryPackage::exists(std::string_view path) const {
    std::error_code error;
    return std::filesystem::is_regular_file(root / Path::normalize(path), error);
}

std::uint64_t DirectoryPackage::getFileSize(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    const std::filesystem::path file = root / entry;
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        throw std::runtime_error("The package file \"" + name + "/" + entry + "\" was not found.");
    }
    return std::filesystem::file_size(file);
}

std::unique_ptr<PackageReader> DirectoryPackage::openReader(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    return std::make_unique<FileReader>(root / entry, name + "/" + entry);
}

std::vector<std::string> DirectoryPackage::list(std::string_view directory) const {
    const std::string relative = Path::normalize(directory);
    const std::filesystem::path start = root / relative;
    std::vector<std::string> files;

    std::error_code error;
    if (!std::filesystem::is_directory(start, error)) {
        return files;
    }

    // A folder that cannot be read fails the listing, so no caller mistakes a part of the files for all of them.
    const auto options = std::filesystem::directory_options::follow_directory_symlink;
    for (auto iterator = std::filesystem::recursive_directory_iterator(start, options, error); !error && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
        std::error_code status;
        if (iterator->is_regular_file(status)) {
            files.push_back(iterator->path().lexically_relative(root).generic_string());
        }
    }
    if (error) {
        throw std::runtime_error("The package folder \"" + (relative.empty() ? name : name + "/" + relative) + "\" could not be listed, and the file system reported \"" + error.message() + "\".");
    }
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace haylen::io
