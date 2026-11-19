#include "support/ZipArchives.hpp"

#include <zip.h>

#include <memory>
#include <stdexcept>

namespace haylen::test {

void ZipArchives::write(const std::filesystem::path& file, const std::vector<Entry>& entries) {
    int error = 0;
    zip_t* opened = zip_open(file.string().c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error);
    if (opened == nullptr) {
        throw std::runtime_error("The test could not create the zip archive \"" + file.string() + "\".");
    }

    // An archive that fails before it closes is discarded, so no half-written file stays behind.
    std::unique_ptr<zip_t, decltype(&zip_discard)> archive(opened, &zip_discard);
    for (const Entry& entry : entries) {
        zip_source_t* source = zip_source_buffer(archive.get(), entry.bytes.data(), entry.bytes.size(), 0);
        if (source == nullptr) {
            throw std::runtime_error("The test could not hand the entry \"" + entry.name + "\" to the zip archive.");
        }

        const zip_int64_t index = zip_file_add(archive.get(), entry.name.c_str(), source, ZIP_FL_ENC_UTF_8);
        if (index < 0) {
            zip_source_free(source);
            throw std::runtime_error("The test could not add the entry \"" + entry.name + "\" to the zip archive.");
        }

        if (entry.deflate && zip_set_file_compression(archive.get(), static_cast<zip_uint64_t>(index), ZIP_CM_DEFLATE, 0) != 0) {
            throw std::runtime_error("The test could not compress the entry \"" + entry.name + "\" of the zip archive.");
        }
    }

    if (zip_close(archive.get()) != 0) {
        throw std::runtime_error("The test could not write the zip archive \"" + file.string() + "\".");
    }

    (void)archive.release();
}

} // namespace haylen::test
