#include "content/StoreFile.hpp"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <string>

#include "haylen/content/Error.hpp"

namespace haylen::content {

// The descriptor of the file, or -1 once it failed to open.
struct StoreFile::Native {
    int descriptor = -1;
};

StoreFile::StoreFile(const std::filesystem::path& file, Mode mode) : path(file), native(std::make_unique<Native>()) {
    const int flags = mode == Mode::Read ? O_RDONLY : mode == Mode::Append ? O_RDWR | O_CREAT | O_APPEND : O_WRONLY | O_CREAT | O_TRUNC;
    native->descriptor = ::open(file.c_str(), flags | O_CLOEXEC, 0600);
    if (native->descriptor < 0) {
        fail("open");
    }
}

StoreFile::~StoreFile() {
    if (native->descriptor >= 0) {
        ::close(native->descriptor);
    }
}

void StoreFile::readAt(std::uint64_t offset, std::span<std::uint8_t> target) const {
    std::size_t done = 0;
    while (done < target.size()) {
        const ssize_t read = ::pread(native->descriptor, target.data() + done, target.size() - done, static_cast<off_t>(offset + done));
        if (read < 0 && errno == EINTR) {
            continue;
        }

        if (read <= 0) {
            throw Error(Error::Code::CacheCorrupt, "The content store file \"" + path.generic_string() + "\" ends before the bytes it should hold.");
        }

        done += static_cast<std::size_t>(read);
    }
}

std::uint64_t StoreFile::append(std::span<const std::uint8_t> bytes) {
    const std::uint64_t offset = getSize();
    std::size_t done = 0;
    while (done < bytes.size()) {
        const ssize_t written = ::write(native->descriptor, bytes.data() + done, bytes.size() - done);
        if (written < 0 && errno == EINTR) {
            continue;
        }

        if (written <= 0) {
            fail("write");
        }

        done += static_cast<std::size_t>(written);
    }

    return offset;
}

void StoreFile::sync() {
#if defined(__APPLE__)
    // Only a full flush makes the drive itself write its cache on Apple platforms, and a file system that cannot falls back to the ordinary one.
    if (::fcntl(native->descriptor, F_FULLFSYNC) == 0) {
        return;
    }
#endif
    if (::fsync(native->descriptor) != 0) {
        fail("flush");
    }
}

void StoreFile::truncate(std::uint64_t size) {
    if (::ftruncate(native->descriptor, static_cast<off_t>(size)) != 0) {
        fail("truncate");
    }
}

std::uint64_t StoreFile::getSize() const {
    struct stat status{};
    if (::fstat(native->descriptor, &status) != 0) {
        fail("measure");
    }

    return static_cast<std::uint64_t>(status.st_size);
}

bool StoreFile::isRemoved() const {
    struct stat status{};
    if (::fstat(native->descriptor, &status) != 0) {
        fail("measure");
    }

    return status.st_nlink == 0;
}

void StoreFile::syncFolder(const std::filesystem::path& folder) {
    const int handle = ::open(folder.c_str(), O_RDONLY | O_CLOEXEC);
    if (handle >= 0) {
        ::fsync(handle);
        ::close(handle);
    }
}

void StoreFile::fail(const char* action) const {
    const int code = errno;
    const std::string message = "The content store could not " + std::string(action) + " the file \"" + path.generic_string() + "\": " + std::strerror(code) + ".";
    if (code == ENOSPC || code == EDQUOT) {
        throw Error(Error::Code::DiskFull, message, {.platformMessage = std::strerror(code)});
    }

    if (code == EACCES || code == EPERM || code == EROFS) {
        throw Error(Error::Code::PermissionDenied, message, {.platformMessage = std::strerror(code)});
    }

    throw Error(Error::Code::CacheCorrupt, message, {.platformMessage = std::strerror(code)});
}

} // namespace haylen::content
