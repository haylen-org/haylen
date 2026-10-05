#include "io/ZipReader.hpp"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace haylen::io {

ZipReader::ZipReader(std::shared_ptr<ZipArchive> openArchive, zip_uint64_t entryIndex, std::uint64_t entrySize, std::string entryName) : archive(std::move(openArchive)), index(entryIndex), size(entrySize), name(std::move(entryName)) {
    open();
}

ZipReader::~ZipReader() {
    std::scoped_lock lock(archive->getMutex());
    zip_fclose(file);
}

void ZipReader::open() {
    file = zip_fopen_index(archive->get(), index, 0);
    if (file == nullptr) {
        fail();
    }
    position = 0;
}

void ZipReader::fail() const {
    throw std::runtime_error("The package file \"" + archive->getName() + "/" + name + "\" could not be read.");
}

std::size_t ZipReader::read(std::uint64_t offset, std::span<std::uint8_t> target) {
    if (offset >= size || target.empty()) {
        return 0;
    }

    const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(target.size(), size - offset));
    std::scoped_lock lock(archive->getMutex());
    moveTo(offset);

    std::size_t filled = 0;
    while (filled < count) {
        const zip_int64_t received = zip_fread(file, target.data() + filled, count - filled);
        if (received <= 0) {
            fail();
        }
        filled += static_cast<std::size_t>(received);
    }
    position += count;
    return count;
}

void ZipReader::moveTo(std::uint64_t offset) {
    if (offset == position) {
        return;
    }
    if (zip_file_is_seekable(file) == 1) {
        if (zip_fseek(file, static_cast<zip_int64_t>(offset), SEEK_SET) != 0) {
            fail();
        }
        position = offset;
        return;
    }

    if (offset < position) {
        zip_fclose(file);
        file = nullptr;
        open();
    }

    std::vector<std::uint8_t> skipped(static_cast<std::size_t>(std::min<std::uint64_t>(kSkipBufferSize, offset - position)));
    while (position < offset) {
        const auto wanted = static_cast<zip_uint64_t>(std::min<std::uint64_t>(skipped.size(), offset - position));
        const zip_int64_t received = zip_fread(file, skipped.data(), wanted);
        if (received <= 0) {
            fail();
        }
        position += static_cast<std::uint64_t>(received);
    }
}

} // namespace haylen::io
