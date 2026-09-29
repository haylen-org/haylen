#include "haylen/io/Package.hpp"

#include <zip.h>

#include <fstream>
#include <stdexcept>
#include <system_error>

#include "haylen/io/Path.hpp"
#include "io/DirectoryPackage.hpp"
#include "io/ZipPackage.hpp"

namespace haylen::io {

std::vector<std::uint8_t> Package::readFile(const std::filesystem::path& file) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) {
        throw std::runtime_error("Package file was not found: " + file.generic_string());
    }

    std::ifstream stream(file, std::ios::binary | std::ios::ate);
    if (!stream) {
        throw std::runtime_error("Package file could not be opened: " + file.generic_string());
    }

    const std::streamoff size = stream.tellg();
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    stream.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!stream) {
        throw std::runtime_error("Package file could not be read: " + file.generic_string());
    }
    return bytes;
}

std::unique_ptr<Package> Package::openDirectory(const std::filesystem::path& root) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) {
        throw std::runtime_error("Package folder was not found: " + root.generic_string());
    }
    return std::make_unique<DirectoryPackage>(std::filesystem::canonical(root));
}

std::unique_ptr<Package> Package::openZip(const std::filesystem::path& file) {
    return openZip(readFile(file), file.filename().generic_string());
}

std::unique_ptr<Package> Package::openZip(std::vector<std::uint8_t> bytes, std::string name) {
    zip_error_t error;
    zip_error_init(&error);

    // The source reads straight from the vector that the package keeps alive for the lifetime of the archive.
    zip_source_t* source = zip_source_buffer_create(bytes.data(), bytes.size(), 0, &error);
    if (source == nullptr) {
        zip_error_fini(&error);
        throw std::runtime_error("Package archive could not be read: " + name);
    }

    zip_t* archive = zip_open_from_source(source, ZIP_RDONLY, &error);
    if (archive == nullptr) {
        const std::string reason = zip_error_strerror(&error);
        zip_source_free(source);
        zip_error_fini(&error);
        throw std::runtime_error("Package archive is not a valid zip file: " + name + " (" + reason + ")");
    }

    zip_error_fini(&error);
    return std::make_unique<ZipPackage>(archive, std::move(bytes), std::move(name));
}

std::unique_ptr<Package> Package::open(const std::filesystem::path& path) {
    std::error_code error;
    if (std::filesystem::is_directory(path, error)) {
        return openDirectory(path);
    }
    return openZip(path);
}

std::string Package::readText(std::string_view path) const {
    const std::vector<std::uint8_t> bytes = read(path);
    return std::string(bytes.begin(), bytes.end());
}

bool Package::assetExists(std::string_view path) const {
    return exists(Path::asset(path));
}

std::vector<std::uint8_t> Package::readAsset(std::string_view path) const {
    return read(Path::asset(path));
}

std::string Package::readAssetText(std::string_view path) const {
    return readText(Path::asset(path));
}

std::vector<std::string> Package::listAssets(std::string_view directory) const {
    // The folder is normalized on its own like every asset path, so ".." never climbs out of the content folder.
    const std::string root = std::string(Path::kContentDirectory) + "/";
    std::vector<std::string> assets = list(Path::join(Path::kContentDirectory, Path::normalize(directory)));
    for (std::string& asset : assets) {
        asset.erase(0, root.size());
    }
    return assets;
}

} // namespace haylen::io
