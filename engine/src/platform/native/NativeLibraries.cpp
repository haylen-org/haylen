#include "haylen/platform/NativeLibraries.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>
#include <system_error>
#include <utility>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#elif !defined(__EMSCRIPTEN__)
#include <dlfcn.h>
#endif

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <mach-o/dyld.h>
#endif

namespace haylen::platform {

#if defined(__EMSCRIPTEN__)
bool NativeLibraries::isAvailable() noexcept {
    return false;
}
#else
bool NativeLibraries::isAvailable() noexcept {
    return true;
}
#endif

NativeLibraries::State& NativeLibraries::getState() {
    static State& state = *new State();
    return state;
}

void NativeLibraries::addSearchFolder(std::filesystem::path folder) {
    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    if (std::ranges::find(state.folders, folder) == state.folders.end()) {
        state.folders.push_back(std::move(folder));
    }
}

void NativeLibraries::registerLinked(std::string library, std::initializer_list<Symbol> symbols) {
    std::unordered_map<std::string, void*> table;
    for (const Symbol& symbol : symbols) {
        table.insert_or_assign(symbol.name, symbol.address);
    }

    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    state.linked.insert_or_assign(std::move(library), std::move(table));
}

std::vector<std::string> NativeLibraries::getFileNames(std::string_view name) {
    const std::string text(name);
    if (std::filesystem::path(text).has_extension()) {
        return {text};
    }
#if defined(_WIN32)
    return {text + ".dll"};
#elif defined(__APPLE__) && (TARGET_OS_OSX || TARGET_OS_MACCATALYST)
    return {"lib" + text + ".dylib", text + ".framework/" + text};
#elif defined(__APPLE__)
    return {text + ".framework/" + text};
#else
    return {"lib" + text + ".so"};
#endif
}

NativeLibraries::Library NativeLibraries::open(std::string_view name) {
    if (!isAvailable()) {
        throw std::runtime_error("Native libraries are not available in the browser. Call JavaScript through haylen.platform instead.");
    }
    if (name.empty()) {
        throw std::invalid_argument("A native library needs a name or a path.");
    }

    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    const std::filesystem::path given(name);
    std::vector<std::filesystem::path> candidates;
    if (given.has_parent_path()) {
        candidates.push_back(given);
    } else {
        std::vector<std::filesystem::path> folders = state.folders;
        std::ranges::copy(getPlatformFolders(), std::back_inserter(folders));
        for (const std::filesystem::path& folder : folders) {
            for (const std::string& file : getFileNames(name)) {
                candidates.push_back(folder / file);
            }
        }
    }

    std::string searched;
    for (const std::filesystem::path& candidate : candidates) {
        const std::string path = candidate.string();
        if (const auto found = std::ranges::find(state.loaded, path, &Library::path); found != state.loaded.end()) {
            return *found;
        }

        std::string failure;
        if (void* handle = load(candidate, failure)) {
            Library library{.name = std::string(name), .path = getLoadPath(candidate), .handle = handle};
            state.loaded.push_back(library);
            return library;
        }
        searched += "\n  " + path + ": " + failure;
    }

    if (!given.has_parent_path() && state.linked.contains(std::string(name))) {
        return {.name = std::string(name), .linked = true};
    }
    if (!given.has_parent_path()) {
        searched += "\n  the libraries linked into the app: not registered";
    }
    throw std::runtime_error("The native library " + std::string(name) + " could not be loaded. Searched:" + searched);
}

void* NativeLibraries::findSymbol(const Library& library, std::string_view name) {
    if (!library.linked) {
        return lookup(library.handle, std::string(name));
    }

    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    const auto table = state.linked.find(library.name);
    if (table == state.linked.end()) {
        return nullptr;
    }
    const auto symbol = table->second.find(std::string(name));
    return symbol != table->second.end() ? symbol->second : nullptr;
}

void* NativeLibraries::findSymbol(std::string_view name) {
    const std::string text(name);
    State& state = getState();
    const std::scoped_lock lock(state.mutex);
    for (const auto& [library, table] : state.linked) {
        if (const auto symbol = table.find(text); symbol != table.end()) {
            return symbol->second;
        }
    }
    for (const Library& library : state.loaded) {
        if (void* address = lookup(library.handle, text)) {
            return address;
        }
    }
    return lookupProcess(text);
}

// The engine loads the libraries of an app from inside its bundle or next to its executable, where make.py places them, so the system search paths never decide which file loads.
std::vector<std::filesystem::path> NativeLibraries::getPlatformFolders() {
#if defined(__ANDROID__)
    // The package manager extracts the libraries of the APK where the linker finds them by name.
    return {std::filesystem::path()};
#elif defined(__APPLE__) && (TARGET_OS_OSX || TARGET_OS_MACCATALYST)
    const std::filesystem::path executable = getExecutableFolder();
    return {executable.parent_path() / "Frameworks", executable};
#elif defined(__APPLE__)
    return {getExecutableFolder() / "Frameworks"};
#elif defined(_WIN32)
    return {getExecutableFolder()};
#elif defined(__EMSCRIPTEN__)
    return {};
#else
    const std::filesystem::path executable = getExecutableFolder();
    return {executable, executable / "lib"};
#endif
}

#if defined(_WIN32)
std::filesystem::path NativeLibraries::getExecutableFolder() {
    std::wstring buffer(MAX_PATH, L'\0');
    DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (length == buffer.size()) {
        buffer.resize(buffer.size() * 2);
        length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }
    buffer.resize(length);
    return std::filesystem::path(buffer).parent_path();
}

// Wide paths reach folders with any name, and the flags keep the dependencies of the library to its own folder and the system folders.
void* NativeLibraries::load(const std::filesystem::path& file, std::string& failure) {
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

void* NativeLibraries::lookup(void* handle, const std::string& name) {
    return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(handle), name.c_str()));
}

void* NativeLibraries::lookupProcess(const std::string& name) {
    return reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(nullptr), name.c_str()));
}

// Varn opens libraries with an ANSI path, so it gets the file name, which Windows resolves to the module this class already loaded from any path.
std::string NativeLibraries::getLoadPath(const std::filesystem::path& file) {
    return file.filename().string();
}
#elif defined(__EMSCRIPTEN__)
std::filesystem::path NativeLibraries::getExecutableFolder() {
    return {};
}

void* NativeLibraries::load(const std::filesystem::path&, std::string& failure) {
    failure = "not available in the browser";
    return nullptr;
}

void* NativeLibraries::lookup(void*, const std::string&) {
    return nullptr;
}

void* NativeLibraries::lookupProcess(const std::string&) {
    return nullptr;
}

std::string NativeLibraries::getLoadPath(const std::filesystem::path& file) {
    return file.string();
}
#else
#if defined(__APPLE__)
std::filesystem::path NativeLibraries::getExecutableFolder() {
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    _NSGetExecutablePath(buffer.data(), &size);
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::canonical(buffer.c_str(), error);
    return (error ? std::filesystem::path(buffer.c_str()) : resolved).parent_path();
}
#else
std::filesystem::path NativeLibraries::getExecutableFolder() {
    std::error_code error;
    return std::filesystem::read_symlink("/proc/self/exe", error).parent_path();
}
#endif

// A candidate without a folder is a name that the dynamic linker looks up itself, which is how Android finds the libraries of the APK.
void* NativeLibraries::load(const std::filesystem::path& file, std::string& failure) {
    std::error_code error;
    if (file.has_parent_path() && !std::filesystem::exists(file, error)) {
        failure = "not found";
        return nullptr;
    }
    void* handle = dlopen(file.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        const char* text = dlerror();
        failure = text != nullptr ? text : "dlopen failed";
    }
    return handle;
}

void* NativeLibraries::lookup(void* handle, const std::string& name) {
    return dlsym(handle, name.c_str());
}

void* NativeLibraries::lookupProcess(const std::string& name) {
    return dlsym(RTLD_DEFAULT, name.c_str());
}

std::string NativeLibraries::getLoadPath(const std::filesystem::path& file) {
    return file.string();
}
#endif

} // namespace haylen::platform
