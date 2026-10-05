#include "io/ZipArchive.hpp"

#include <utility>

namespace haylen::io {

ZipArchive::ZipArchive(zip_t* openArchive, std::vector<std::uint8_t> archiveBytes, std::string archiveName) : archive(openArchive), bytes(std::move(archiveBytes)), name(std::move(archiveName)) {}

ZipArchive::~ZipArchive() {
    zip_discard(archive);
}

} // namespace haylen::io
