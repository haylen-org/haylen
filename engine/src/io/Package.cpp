#include "haylen/io/Package.hpp"

#include <zip.h>

#include <algorithm>
#include <format>
#include <stdexcept>
#include <system_error>

#include "haylen/io/Path.hpp"
#include "io/DirectoryPackage.hpp"
#include "io/ZipArchive.hpp"
#include "io/ZipPackage.hpp"

namespace haylen::io {

std::unique_ptr<Package> Package::openDirectory(const std::filesystem::path& root) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        throw std::runtime_error("The package folder \"" + root.generic_string() + "\" was not found.");
    }
    return std::make_unique<DirectoryPackage>(std::filesystem::canonical(root));
}

std::unique_ptr<Package> Package::openZip(const std::filesystem::path& file) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        throw std::runtime_error("The package file \"" + file.generic_string() + "\" was not found.");
    }

    // The archive stays open on disk, and libzip reads the central directory now and each entry when it is asked for. It takes UTF-8 file names on every platform.
    const std::u8string path = file.u8string();
    int code = ZIP_ER_OK;
    zip_t* archive = zip_open(reinterpret_cast<const char*>(path.c_str()), ZIP_RDONLY, &code);
    if (archive == nullptr) {
        zip_error_t reason;
        zip_error_init_with_code(&reason, code);
        const std::string message = zip_error_strerror(&reason);
        zip_error_fini(&reason);
        throw std::runtime_error("The package archive \"" + file.filename().generic_string() + "\" is not a valid zip file, and the zip reader reported \"" + message + "\".");
    }
    return std::make_unique<ZipPackage>(std::make_shared<ZipArchive>(archive, std::vector<std::uint8_t>{}, file.filename().generic_string()));
}

std::unique_ptr<Package> Package::openZip(std::vector<std::uint8_t> bytes, std::string name) {
    zip_error_t error;
    zip_error_init(&error);

    // The source reads straight from the vector that the archive keeps alive for its whole lifetime.
    zip_source_t* source = zip_source_buffer_create(bytes.data(), bytes.size(), 0, &error);
    if (source == nullptr) {
        zip_error_fini(&error);
        throw std::runtime_error("The package archive \"" + name + "\" could not be read.");
    }

    zip_t* archive = zip_open_from_source(source, ZIP_RDONLY, &error);
    if (archive == nullptr) {
        const std::string reason = zip_error_strerror(&error);
        zip_source_free(source);
        zip_error_fini(&error);
        throw std::runtime_error("The package archive \"" + name + "\" is not a valid zip file, and the zip reader reported \"" + reason + "\".");
    }

    zip_error_fini(&error);
    return std::make_unique<ZipPackage>(std::make_shared<ZipArchive>(archive, std::move(bytes), std::move(name)));
}

std::unique_ptr<Package> Package::open(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::is_directory(path, error)) {
        return openDirectory(path);
    }
    return openZip(path);
}

std::vector<std::uint8_t> Package::read(std::string_view path) const {
    const std::unique_ptr<PackageReader> reader = openReader(path);
    const std::uint64_t size = reader->getSize();
    if (size > kMaxReadSize) {
        throw std::runtime_error(std::format("The package file \"{}/{}\" holds {} bytes, more than a whole read allows. Read it in ranges instead.", getName(), Path::normalize(path), size));
    }

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    reader->readExactly(0, bytes);
    return bytes;
}

std::vector<std::uint8_t> Package::readRange(std::string_view path, std::uint64_t offset, std::size_t size) const {
    const std::unique_ptr<PackageReader> reader = openReader(path);
    const std::uint64_t available = offset < reader->getSize() ? reader->getSize() - offset : 0;
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(std::min<std::uint64_t>(size, available)));
    reader->readExactly(offset, bytes);
    return bytes;
}

std::string Package::readText(std::string_view path) const {
    const std::vector<std::uint8_t> bytes = read(path);
    return std::string(bytes.begin(), bytes.end());
}

bool Package::assetExists(std::string_view path) const {
    return exists(Path::asset(path));
}

std::uint64_t Package::getAssetSize(std::string_view path) const {
    return getFileSize(Path::asset(path));
}

std::vector<std::uint8_t> Package::readAsset(std::string_view path) const {
    return read(Path::asset(path));
}

std::vector<std::uint8_t> Package::readAssetRange(std::string_view path, std::uint64_t offset, std::size_t size) const {
    return readRange(Path::asset(path), offset, size);
}

std::string Package::readAssetText(std::string_view path) const {
    return readText(Path::asset(path));
}

std::vector<std::string> Package::listAssets(std::string_view directory) const {
    // The folder is normalized on its own like every asset path, so `..` never climbs out of the content folder.
    const std::string root = std::string(Path::kContentDirectory) + "/";
    std::vector<std::string> assets = list(Path::join(Path::kContentDirectory, Path::normalize(directory)));
    for (std::string& asset : assets) {
        asset.erase(0, root.size());
    }
    return assets;
}

} // namespace haylen::io
