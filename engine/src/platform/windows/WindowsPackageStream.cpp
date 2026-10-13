#include "platform/windows/WindowsPackageStream.hpp"

#include <algorithm>
#include <exception>
#include <span>
#include <utility>

namespace haylen::platform {

WindowsPackageStream::WindowsPackageStream(std::shared_ptr<io::PackageReader> reader) : file(std::move(reader)) {}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::QueryInterface(REFIID id, void** object) {
    if (object == nullptr) {
        return E_POINTER;
    }
    if (id == __uuidof(IUnknown) || id == __uuidof(ISequentialStream) || id == __uuidof(IStream)) {
        *object = static_cast<IStream*>(this);
        AddRef();
        return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE WindowsPackageStream::AddRef() {
    return ++references;
}

ULONG STDMETHODCALLTYPE WindowsPackageStream::Release() {
    const ULONG left = --references;
    if (left == 0) {
        delete this;
    }
    return left;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::Read(void* target, ULONG size, ULONG* read) {
    const std::scoped_lock lock(mutex);
    try {
        const std::size_t count = file->read(position, std::span<std::uint8_t>(static_cast<std::uint8_t*>(target), size));
        position += count;
        if (read != nullptr) {
            *read = static_cast<ULONG>(count);
        }
        return count == size ? S_OK : S_FALSE;
    } catch (const std::exception&) {
        return STG_E_READFAULT;
    }
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::Write(const void*, ULONG, ULONG*) {
    return STG_E_ACCESSDENIED;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* result) {
    const std::scoped_lock lock(mutex);
    std::int64_t base = 0;
    if (origin == STREAM_SEEK_CUR) {
        base = static_cast<std::int64_t>(position);
    } else if (origin == STREAM_SEEK_END) {
        base = static_cast<std::int64_t>(file->getSize());
    } else if (origin != STREAM_SEEK_SET) {
        return STG_E_INVALIDFUNCTION;
    }
    const std::int64_t target = base + move.QuadPart;
    if (target < 0) {
        return STG_E_INVALIDFUNCTION;
    }
    position = static_cast<std::uint64_t>(target);
    if (result != nullptr) {
        result->QuadPart = position;
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::SetSize(ULARGE_INTEGER) {
    return STG_E_ACCESSDENIED;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::CopyTo(IStream*, ULARGE_INTEGER, ULARGE_INTEGER*, ULARGE_INTEGER*) {
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::Commit(DWORD) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::Revert() {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) {
    return STG_E_INVALIDFUNCTION;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) {
    return STG_E_INVALIDFUNCTION;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::Stat(STATSTG* stat, DWORD) {
    if (stat == nullptr) {
        return STG_E_INVALIDPOINTER;
    }
    *stat = {};
    stat->type = STGTY_STREAM;
    stat->cbSize.QuadPart = file->getSize();
    stat->grfMode = STGM_READ;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE WindowsPackageStream::Clone(IStream**) {
    return E_NOTIMPL;
}

} // namespace haylen::platform
