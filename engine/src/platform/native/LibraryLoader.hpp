#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace haylen::platform {

// Loads native libraries at run time through the dynamic linker of the system of the build: the one of POSIX in `posix/LibraryLoader.cpp`, the one of Windows in `windows/LibraryLoader.cpp`, or none in `native/unavailable/LibraryLoader.cpp`, where a platform does not allow `dynamicLibraries` and the app reaches only the libraries it links.
class LibraryLoader final {
  public:
    // Whether the platform loads libraries at run time.
    [[nodiscard]] static bool isAvailable() noexcept;

    // The file names a library of a name without an extension has on the platform, such as `libname.so` on Linux.
    [[nodiscard]] static std::vector<std::string> getFileNames(std::string_view name);

    // The folders where the backend of the target places the libraries of an app, inside its bundle or next to its executable, so the search paths of the system never decide which file loads.
    [[nodiscard]] static std::vector<std::filesystem::path> getFolders();

    // Opens a file, or returns null and describes why it failed.
    [[nodiscard]] static void* load(const std::filesystem::path& file, std::string& failure);
    [[nodiscard]] static void* lookup(void* handle, const std::string& name);
    [[nodiscard]] static void* lookupProcess(const std::string& name);
};

} // namespace haylen::platform
