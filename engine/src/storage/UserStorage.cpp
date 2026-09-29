#include "haylen/storage/UserStorage.hpp"

#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <system_error>

#include "haylen/io/Path.hpp"

namespace haylen::storage {

UserStorage::UserStorage(std::filesystem::path folder, std::function<void()> onFlush) : root(std::move(folder)), persist(std::move(onFlush)) {
    std::filesystem::create_directories(root);
}

std::filesystem::path UserStorage::resolve(std::string_view path) const {
    const std::string relative = io::Path::normalize(path);
    if (relative.empty()) {
        throw std::invalid_argument("A storage path cannot be empty.");
    }
    return root / relative;
}

bool UserStorage::exists(std::string_view path) const {
    std::error_code error;
    return std::filesystem::is_regular_file(resolve(path), error);
}

std::vector<std::uint8_t> UserStorage::read(std::string_view path) const {
    const std::filesystem::path file = resolve(path);
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        throw std::runtime_error("Storage file was not found: " + std::string(path));
    }

    std::ifstream stream(file, std::ios::binary | std::ios::ate);
    const std::streamoff size = stream.tellg();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(std::max<std::streamoff>(0, size)));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("Storage file could not be read: " + std::string(path));
    }
    return bytes;
}

std::string UserStorage::readText(std::string_view path) const {
    const std::vector<std::uint8_t> bytes = read(path);
    return std::string(bytes.begin(), bytes.end());
}

void UserStorage::write(std::string_view path, std::span<const std::uint8_t> bytes) {
    const std::filesystem::path file = resolve(path);
    std::filesystem::create_directories(file.parent_path());

    // The data goes to a sibling file first so a crash mid-write never leaves a truncated save behind.
    std::filesystem::path temporary = file;
    temporary += ".tmp";
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    // Closing flushes the last buffered bytes, so only a stream that also closed cleanly may replace the file.
    stream.close();
    if (!stream) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("Storage file could not be written: " + std::string(path));
    }
    std::filesystem::rename(temporary, file);
}

void UserStorage::writeText(std::string_view path, std::string_view text) {
    write(path, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

bool UserStorage::remove(std::string_view path) {
    std::error_code error;
    return std::filesystem::remove(resolve(path), error);
}

std::vector<std::string> UserStorage::list(std::string_view directory) const {
    const std::string relative = io::Path::normalize(directory);
    const std::filesystem::path start = root / relative;
    std::vector<std::string> files;

    std::error_code error;
    if (!std::filesystem::is_directory(start, error)) {
        return files;
    }

    for (auto iterator = std::filesystem::recursive_directory_iterator(start, error); iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
        if (error) {
            break;
        }
        if (iterator->is_regular_file(error) && iterator->path().extension() != ".tmp") {
            files.push_back(std::filesystem::relative(iterator->path(), root, error).generic_string());
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

void UserStorage::flush() {
    if (persist) {
        persist();
    }
}

} // namespace haylen::storage
