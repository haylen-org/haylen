#include "io/ZipPackage.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/io/Path.hpp"

namespace haylen::io {

ZipPackage::ZipPackage(zip_t* openArchive, std::vector<std::uint8_t> archiveBytes, std::string archiveName) : archive(openArchive), bytes(std::move(archiveBytes)), name(std::move(archiveName)) {}

ZipPackage::~ZipPackage() {
    zip_discard(archive);
}

bool ZipPackage::exists(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    std::scoped_lock lock(mutex);
    return zip_name_locate(archive, entry.c_str(), 0) >= 0;
}

std::vector<std::uint8_t> ZipPackage::read(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    std::scoped_lock lock(mutex);

    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(archive, entry.c_str(), 0, &stat) != 0 || (stat.valid & ZIP_STAT_SIZE) == 0) {
        throw std::runtime_error("The package file '" + name + "/" + entry + "' was not found.");
    }

    zip_file_t* file = zip_fopen_index(archive, stat.index, 0);
    if (file == nullptr) {
        throw std::runtime_error("The package file '" + name + "/" + entry + "' could not be opened.");
    }

    std::vector<std::uint8_t> content(static_cast<std::size_t>(stat.size));
    const zip_int64_t count = zip_fread(file, content.data(), content.size());
    zip_fclose(file);
    if (count < 0 || static_cast<std::uint64_t>(count) != stat.size) {
        throw std::runtime_error("The package file '" + name + "/" + entry + "' could not be read.");
    }
    return content;
}

std::vector<std::string> ZipPackage::list(std::string_view directory) const {
    const std::string prefix = Path::normalize(directory);
    std::vector<std::string> files;

    std::scoped_lock lock(mutex);
    const zip_int64_t count = zip_get_num_entries(archive, 0);
    for (zip_int64_t index = 0; index < count; ++index) {
        const char* entry = zip_get_name(archive, static_cast<zip_uint64_t>(index), 0);
        const std::string_view entryName = entry == nullptr ? std::string_view{} : std::string_view(entry);
        if (!entryName.empty() && !entryName.ends_with('/') && Path::isInside(entryName, prefix)) {
            files.emplace_back(entryName);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace haylen::io
