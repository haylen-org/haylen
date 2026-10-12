#include "haylen/storage/UserStorage.hpp"

#include <algorithm>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>

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
        throw std::runtime_error("The storage file \"" + std::string(path) + "\" was not found.");
    }

    const std::shared_lock lock(mutex);
    std::ifstream stream(file, std::ios::binary | std::ios::ate);
    const std::streamoff size = stream.tellg();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(std::max<std::streamoff>(0, size)));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("The storage file \"" + std::string(path) + "\" could not be read.");
    }
    return bytes;
}

std::string UserStorage::readText(std::string_view path) const {
    const std::vector<std::uint8_t> bytes = read(path);
    return std::string(bytes.begin(), bytes.end());
}

// A failed write leaves the previous file and no temporary file behind, and its error names the relative path rather than the folder of the user.
void UserStorage::write(std::string_view path, std::span<const std::uint8_t> bytes) {
    const std::filesystem::path file = resolve(path);

    // The data goes to a sibling file first so a crash mid-write never leaves a truncated save behind, and each write has a sibling of its own, so writes of the same file on two threads never mix.
    std::filesystem::path temporary = file;
    temporary += "." + std::to_string(++writeCount) + ".tmp";
    if (!replace(file, temporary, bytes)) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw std::runtime_error("The storage file \"" + std::string(path) + "\" could not be written.");
    }
}

bool UserStorage::replace(const std::filesystem::path& file, const std::filesystem::path& temporary, std::span<const std::uint8_t> bytes) {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    // Closing flushes the last buffered bytes, so only a stream that also closed cleanly may replace the file.
    stream.close();
    if (!stream) {
        return false;
    }

    // Windows refuses to replace or remove a file that another thread is replacing or reading, so those steps wait for each other. Another program, such as a virus scanner or a search indexer, may also hold the file for a moment after it changes, so the replace tries again for a short while before it fails.
    for (int attempt = 1;; ++attempt) {
        {
            const std::scoped_lock lock(mutex);
            std::filesystem::rename(temporary, file, error);
        }
        if (!error || attempt == kReplaceAttempts) {
            return !error;
        }
        std::this_thread::sleep_for(kReplaceDelay);
    }
}

void UserStorage::writeText(std::string_view path, std::string_view text) {
    write(path, std::span(reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

bool UserStorage::remove(std::string_view path) {
    const std::filesystem::path file = resolve(path);
    std::error_code error;
    const std::scoped_lock lock(mutex);
    return std::filesystem::remove(file, error);
}

std::vector<std::string> UserStorage::list(std::string_view directory) const {
    const std::string relative = io::Path::normalize(directory);
    const std::filesystem::path start = root / relative;
    std::vector<std::string> files;

    std::error_code error;
    if (!std::filesystem::is_directory(start, error)) {
        return files;
    }

    // A folder that cannot be read fails the listing, so no caller mistakes a part of the files for all of them.
    for (auto iterator = std::filesystem::recursive_directory_iterator(start, error); !error && iterator != std::filesystem::recursive_directory_iterator(); iterator.increment(error)) {
        std::error_code status;
        if (iterator->is_regular_file(status) && iterator->path().extension() != ".tmp") {
            files.push_back(iterator->path().lexically_relative(root).generic_string());
        }
    }
    if (error) {
        const std::string folder = relative.empty() ? std::string("The user storage") : "The storage folder \"" + relative + "\"";
        throw std::runtime_error(folder + " could not be listed, and the file system reported \"" + error.message() + "\".");
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
