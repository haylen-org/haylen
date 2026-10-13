#include "content/StoreFile.hpp"

#include <algorithm>
#include <string>

#include "haylen/content/Error.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#endif

namespace haylen::content {

#if defined(_WIN32)

StoreFile::StoreFile(const std::filesystem::path& file, Mode mode) : path(file) {
    const DWORD access = mode == Mode::Read ? GENERIC_READ : mode == Mode::Append ? FILE_APPEND_DATA | GENERIC_READ : GENERIC_WRITE;
    const DWORD disposition = mode == Mode::Read ? OPEN_EXISTING : mode == Mode::Append ? OPEN_ALWAYS : CREATE_ALWAYS;
    handle = CreateFileW(file.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        handle = nullptr;
        fail("open");
    }
}

StoreFile::~StoreFile() {
    if (handle != nullptr) {
        CloseHandle(handle);
    }
}

void StoreFile::readAt(std::uint64_t offset, std::span<std::uint8_t> target) const {
    std::size_t done = 0;
    while (done < target.size()) {
        OVERLAPPED position{};
        const std::uint64_t at = offset + done;
        position.Offset = static_cast<DWORD>(at);
        position.OffsetHigh = static_cast<DWORD>(at >> 32);
        const auto wanted = static_cast<DWORD>(std::min<std::size_t>(target.size() - done, 1U << 30));
        DWORD read = 0;
        if (!ReadFile(handle, target.data() + done, wanted, &read, &position) || read == 0) {
            throw Error(Error::Code::CacheCorrupt, "The content store file \"" + path.generic_string() + "\" ends before the bytes it should hold.");
        }
        done += read;
    }
}

std::uint64_t StoreFile::append(std::span<const std::uint8_t> bytes) {
    const std::uint64_t offset = getSize();
    std::size_t done = 0;
    while (done < bytes.size()) {
        const auto wanted = static_cast<DWORD>(std::min<std::size_t>(bytes.size() - done, 1U << 30));
        DWORD written = 0;
        if (!WriteFile(handle, bytes.data() + done, wanted, &written, nullptr)) {
            fail("write");
        }
        done += written;
    }
    return offset;
}

void StoreFile::sync() {
    if (!FlushFileBuffers(handle)) {
        fail("flush");
    }
}

// A handle that only appends may not move the end of its file, so the end moves through a handle of its own that writes.
void StoreFile::truncate(std::uint64_t size) {
    HANDLE writer = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (writer == INVALID_HANDLE_VALUE) {
        fail("truncate");
    }

    LARGE_INTEGER position{};
    position.QuadPart = static_cast<LONGLONG>(size);
    const bool truncated = SetFilePointerEx(writer, position, nullptr, FILE_BEGIN) && SetEndOfFile(writer);
    const DWORD code = GetLastError();
    CloseHandle(writer);
    if (!truncated) {
        SetLastError(code);
        fail("truncate");
    }
}

std::uint64_t StoreFile::getSize() const {
    LARGE_INTEGER size{};
    if (!GetFileSizeEx(handle, &size)) {
        fail("measure");
    }
    return static_cast<std::uint64_t>(size.QuadPart);
}

void StoreFile::syncFolder(const std::filesystem::path&) {}

void StoreFile::fail(const char* action) const {
    const DWORD code = GetLastError();
    const std::string message = "The content store could not " + std::string(action) + " the file \"" + path.generic_string() + "\" (system error " + std::to_string(code) + ").";
    if (code == ERROR_DISK_FULL || code == ERROR_HANDLE_DISK_FULL) {
        throw Error(Error::Code::DiskFull, message);
    }
    if (code == ERROR_ACCESS_DENIED || code == ERROR_WRITE_PROTECT) {
        throw Error(Error::Code::PermissionDenied, message);
    }
    throw Error(Error::Code::CacheCorrupt, message);
}

#else

StoreFile::StoreFile(const std::filesystem::path& file, Mode mode) : path(file) {
    const int flags = mode == Mode::Read ? O_RDONLY : mode == Mode::Append ? O_RDWR | O_CREAT | O_APPEND : O_WRONLY | O_CREAT | O_TRUNC;
    descriptor = ::open(file.c_str(), flags | O_CLOEXEC, 0600);
    if (descriptor < 0) {
        fail("open");
    }
}

StoreFile::~StoreFile() {
    if (descriptor >= 0) {
        ::close(descriptor);
    }
}

void StoreFile::readAt(std::uint64_t offset, std::span<std::uint8_t> target) const {
    std::size_t done = 0;
    while (done < target.size()) {
        const ssize_t read = ::pread(descriptor, target.data() + done, target.size() - done, static_cast<off_t>(offset + done));
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
        const ssize_t written = ::write(descriptor, bytes.data() + done, bytes.size() - done);
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
    if (::fcntl(descriptor, F_FULLFSYNC) == 0) {
        return;
    }
#endif
    if (::fsync(descriptor) != 0) {
        fail("flush");
    }
}

void StoreFile::truncate(std::uint64_t size) {
    if (::ftruncate(descriptor, static_cast<off_t>(size)) != 0) {
        fail("truncate");
    }
}

std::uint64_t StoreFile::getSize() const {
    struct stat status{};
    if (::fstat(descriptor, &status) != 0) {
        fail("measure");
    }
    return static_cast<std::uint64_t>(status.st_size);
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

#endif

} // namespace haylen::content
