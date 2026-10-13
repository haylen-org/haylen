#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <objidl.h>
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>

#include "haylen/io/PackageReader.hpp"

namespace haylen::platform {

// A COM stream that reads a file of the package by range, which Media Foundation reads a video through, so a video of a zip or of a protected release plays without a copy on disk. It only reads, and every method is thread-safe.
class WindowsPackageStream final : public IStream {
  public:
    explicit WindowsPackageStream(std::shared_ptr<io::PackageReader> file);

    WindowsPackageStream(const WindowsPackageStream&) = delete;
    WindowsPackageStream& operator=(const WindowsPackageStream&) = delete;

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id, void** object) override;
    ULONG STDMETHODCALLTYPE AddRef() override;
    ULONG STDMETHODCALLTYPE Release() override;

    HRESULT STDMETHODCALLTYPE Read(void* target, ULONG size, ULONG* read) override;
    HRESULT STDMETHODCALLTYPE Write(const void* source, ULONG size, ULONG* written) override;
    HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER move, DWORD origin, ULARGE_INTEGER* position) override;
    HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER size) override;
    HRESULT STDMETHODCALLTYPE CopyTo(IStream* target, ULARGE_INTEGER size, ULARGE_INTEGER* read, ULARGE_INTEGER* written) override;
    HRESULT STDMETHODCALLTYPE Commit(DWORD flags) override;
    HRESULT STDMETHODCALLTYPE Revert() override;
    HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type) override;
    HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER offset, ULARGE_INTEGER size, DWORD type) override;
    HRESULT STDMETHODCALLTYPE Stat(STATSTG* stat, DWORD flags) override;
    HRESULT STDMETHODCALLTYPE Clone(IStream** copy) override;

  private:
    ~WindowsPackageStream() = default;

    std::shared_ptr<io::PackageReader> file;
    std::atomic<ULONG> references{1};
    std::mutex mutex;
    std::uint64_t position = 0;
};

} // namespace haylen::platform
