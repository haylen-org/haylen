#include "content/StoreFile.hpp"

#include <windows.h>

#include <algorithm>
#include <string>

#include "haylen/content/Error.hpp"

namespace haylen::content {

// The handle of the file in Windows, or null once it failed to open.
struct StoreFile::Native {
    HANDLE handle = nullptr;
};

StoreFile::StoreFile(const std::filesystem::path& file, Mode mode) : path(file), native(std::make_unique<Native>()) {
    const DWORD access = mode == Mode::Read ? GENERIC_READ : mode == Mode::Append ? FILE_APPEND_DATA | GENERIC_READ : GENERIC_WRITE;
    const DWORD disposition = mode == Mode::Read ? OPEN_EXISTING : mode == Mode::Append ? OPEN_ALWAYS : CREATE_ALWAYS;
    native->handle = CreateFileW(file.c_str(), access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, disposition, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (native->handle == INVALID_HANDLE_VALUE) {
        native->handle = nullptr;
        fail("open");
    }
}

StoreFile::~StoreFile() {
    if (native->handle != nullptr) {
        CloseHandle(native->handle);
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
        if (!ReadFile(native->handle, target.data() + done, wanted, &read, &position) || read == 0) {
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
        if (!WriteFile(native->handle, bytes.data() + done, wanted, &written, nullptr)) {
            fail("write");
        }

        done += written;
    }

    return offset;
}

void StoreFile::sync() {
    if (!FlushFileBuffers(native->handle)) {
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
    if (!GetFileSizeEx(native->handle, &size)) {
        fail("measure");
    }

    return static_cast<std::uint64_t>(size.QuadPart);
}

bool StoreFile::isRemoved() const {
    FILE_STANDARD_INFO info{};
    if (!GetFileInformationByHandleEx(native->handle, FileStandardInfo, &info, sizeof(info))) {
        fail("measure");
    }

    return info.DeletePending != 0 || info.NumberOfLinks == 0;
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

} // namespace haylen::content
