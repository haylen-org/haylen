#include "content/FolderStoreFiles.hpp"

#include <system_error>
#include <utility>

#include "haylen/content/Error.hpp"

namespace haylen::content {

FolderStoreFiles::FolderStoreFiles(std::filesystem::path storeFolder) : root(std::move(storeFolder)) {
    createFolder(root);
}

void FolderStoreFiles::createFolder(const std::filesystem::path& folder) const {
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    std::filesystem::permissions(folder, std::filesystem::perms::owner_all, error);
    if (!std::filesystem::is_directory(folder)) {
        throw Error(Error::Code::PermissionDenied, "The content store could not create the folder \"" + folder.generic_string() + "\".");
    }
}

std::shared_ptr<StoreFile> FolderStoreFiles::open(const std::string& name, bool create) const {
    std::scoped_lock lock(mutex);
    if (const auto found = files.find(name); found != files.end()) {
        return found->second;
    }
    const std::filesystem::path file = root / name;
    std::error_code error;
    if (!create && !std::filesystem::is_regular_file(file, error)) {
        return nullptr;
    }
    if (create) {
        createFolder(file.parent_path());
    }
    auto opened = std::make_shared<StoreFile>(file, StoreFile::Mode::Append);
    files.emplace(name, opened);
    return opened;
}

void FolderStoreFiles::forget(const std::string& name) {
    std::scoped_lock lock(mutex);
    files.erase(name);
}

std::optional<std::vector<std::uint8_t>> FolderStoreFiles::read(const std::string& name) const {
    const std::shared_ptr<StoreFile> file = open(name, false);
    if (!file) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(file->getSize()));
    file->readAt(0, bytes);
    return bytes;
}

void FolderStoreFiles::write(const std::string& name, std::span<const std::uint8_t> bytes) {
    forget(name);
    const std::filesystem::path file = root / name;
    createFolder(file.parent_path());
    StoreFile written(file, StoreFile::Mode::Replace);
    written.append(bytes);
    written.sync();
    StoreFile::syncFolder(file.parent_path());
}

std::uint64_t FolderStoreFiles::append(const std::string& name, std::span<const std::uint8_t> bytes) {
    return open(name, true)->append(bytes);
}

void FolderStoreFiles::sync(const std::string& name) {
    if (const std::shared_ptr<StoreFile> file = open(name, false)) {
        file->sync();
        StoreFile::syncFolder((root / name).parent_path());
    }
}

void FolderStoreFiles::truncate(const std::string& name, std::uint64_t size) {
    open(name, true)->truncate(size);
}

void FolderStoreFiles::remove(const std::string& name) {
    forget(name);
    std::error_code error;
    std::filesystem::remove(root / name, error);
}

std::uint64_t FolderStoreFiles::getSize(const std::string& name) const {
    const std::shared_ptr<StoreFile> file = open(name, false);
    return file ? file->getSize() : 0;
}

std::vector<std::string> FolderStoreFiles::list(const std::string& folder) const {
    std::vector<std::string> names;
    std::error_code error;
    for (std::filesystem::directory_iterator entry(root / folder, error), end; !error && entry != end; entry.increment(error)) {
        if (entry->is_regular_file(error)) {
            names.push_back(entry->path().filename().generic_string());
        }
    }
    return names;
}

void FolderStoreFiles::readRange(const std::string& name, std::uint64_t offset, std::span<std::uint8_t> target) const {
    const std::shared_ptr<StoreFile> file = open(name, false);
    if (!file) {
        throw Error(Error::Code::CacheCorrupt, "The content store has lost its file \"" + name + "\".");
    }
    file->readAt(offset, target);
}

std::uint64_t FolderStoreFiles::getAvailableSpace() const {
    std::error_code error;
    const std::filesystem::space_info space = std::filesystem::space(root, error);
    return error ? 0 : space.available;
}

} // namespace haylen::content
