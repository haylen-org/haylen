#include "io/ZipPackage.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "haylen/io/Path.hpp"
#include "io/ZipReader.hpp"

namespace haylen::io {

ZipPackage::ZipPackage(std::shared_ptr<ZipArchive> openArchive) : archive(std::move(openArchive)) {}

zip_stat_t ZipPackage::find(const std::string& entry) const {
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat(archive->get(), entry.c_str(), 0, &stat) != 0 || (stat.valid & ZIP_STAT_SIZE) == 0 || (stat.valid & ZIP_STAT_INDEX) == 0) {
        throw std::runtime_error("The package file \"" + archive->getName() + "/" + entry + "\" was not found.");
    }
    return stat;
}

bool ZipPackage::exists(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    std::scoped_lock lock(archive->getMutex());
    return zip_name_locate(archive->get(), entry.c_str(), 0) >= 0;
}

std::uint64_t ZipPackage::getFileSize(std::string_view path) const {
    const std::string entry = Path::normalize(path);
    std::scoped_lock lock(archive->getMutex());
    return find(entry).size;
}

std::unique_ptr<PackageReader> ZipPackage::openReader(std::string_view path) const {
    std::string entry = Path::normalize(path);
    std::scoped_lock lock(archive->getMutex());
    const zip_stat_t stat = find(entry);
    return std::make_unique<ZipReader>(archive, stat.index, stat.size, std::move(entry));
}

std::vector<std::string> ZipPackage::list(std::string_view directory) const {
    const std::string prefix = Path::normalize(directory);
    std::vector<std::string> files;

    std::scoped_lock lock(archive->getMutex());
    const zip_int64_t count = zip_get_num_entries(archive->get(), 0);
    for (zip_int64_t index = 0; index < count; ++index) {
        const char* entry = zip_get_name(archive->get(), static_cast<zip_uint64_t>(index), 0);
        const std::string_view entryName = entry == nullptr ? std::string_view{} : std::string_view(entry);
        if (!entryName.empty() && !entryName.ends_with('/') && Path::isInside(entryName, prefix)) {
            files.emplace_back(entryName);
        }
    }
    std::sort(files.begin(), files.end());
    return files;
}

} // namespace haylen::io
