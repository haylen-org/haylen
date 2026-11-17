#include "platform/native/LibraryLoader.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <array>
#include <system_error>

namespace haylen::platform {

bool LibraryLoader::isAvailable() noexcept {
    return true;
}

std::vector<std::string> LibraryLoader::getFileNames(std::string_view name) {
    return {std::string(name) + ".dll"};
}

// Windows apps keep their libraries next to their executable.
std::vector<std::filesystem::path> LibraryLoader::getFolders() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    buffer.resize(length);
    return {std::filesystem::path(buffer).parent_path()};
}

// Wide paths reach folders with any name, and the flags keep the dependencies of the library to its own folder and the system folders.
void* LibraryLoader::load(const std::filesystem::path& file, std::string& failure) {
    std::error_code error;
    if (!std::filesystem::exists(file, error)) {
        failure = "not found";
        return nullptr;
    }

    HMODULE module = LoadLibraryExW(file.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
    if (module == nullptr) {
        const DWORD code = GetLastError();
        std::array<char, 512> text{};
        const DWORD size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, code, 0, text.data(), static_cast<DWORD>(text.size()), nullptr);
        failure = size > 0 ? std::string(text.data(), size) : "Windows error " + std::to_string(code);
        while (!failure.empty() && (failure.back() == '\n' || failure.back() == '\r' || failure.back() == '.')) {
            failure.pop_back();
        }

        return nullptr;
    }

    return reinterpret_cast<void*>(module);
}

void* LibraryLoader::lookup(void* handle, const std::string& name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
}

void* LibraryLoader::lookupProcess(const std::string& name) {
    return reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(nullptr), name.c_str()));
}

} // namespace haylen::platform
