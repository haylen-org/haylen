#include "platform/native/LibraryLoader.hpp"

#include <dlfcn.h>

#include <cstdint>
#include <system_error>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#include <mach-o/dyld.h>
#endif

namespace haylen::platform {

bool LibraryLoader::isAvailable() noexcept {
    return true;
}

std::vector<std::string> LibraryLoader::getFileNames(std::string_view name) {
    const std::string text(name);
#if defined(__APPLE__) && (TARGET_OS_OSX || TARGET_OS_MACCATALYST)
    return {"lib" + text + ".dylib", text + ".framework/" + text};
#elif defined(__APPLE__)
    return {text + ".framework/" + text};
#else
    return {"lib" + text + ".so"};
#endif
}

// Android extracts the libraries of the APK where the linker finds them by name, Apple apps keep theirs in the frameworks of their bundle, and Linux apps next to their executable and in its folder "lib". A port of another POSIX system loads the libraries that the app names by their paths and the ones of the search folders.
std::vector<std::filesystem::path> LibraryLoader::getFolders() {
#if defined(__ANDROID__)
    return {std::filesystem::path()};
#elif defined(__APPLE__)
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buffer(size, '\0');
    _NSGetExecutablePath(buffer.data(), &size);
    std::error_code error;
    const std::filesystem::path resolved = std::filesystem::canonical(buffer.c_str(), error);
    const std::filesystem::path executable = (error ? std::filesystem::path(buffer.c_str()) : resolved).parent_path();
#if TARGET_OS_OSX || TARGET_OS_MACCATALYST
    return {executable.parent_path() / "Frameworks", executable};
#else
    return {executable / "Frameworks"};
#endif
#elif defined(__linux__)
    std::error_code error;
    const std::filesystem::path executable = std::filesystem::read_symlink("/proc/self/exe", error).parent_path();
    return {executable, executable / "lib"};
#else
    return {};
#endif
}

// A candidate without a folder is a name that the dynamic linker looks up itself, which is how Android finds the libraries of the APK.
void* LibraryLoader::load(const std::filesystem::path& file, std::string& failure) {
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

void* LibraryLoader::lookup(void* handle, const std::string& name) {
    return dlsym(handle, name.c_str());
}

void* LibraryLoader::lookupProcess(const std::string& name) {
    return dlsym(RTLD_DEFAULT, name.c_str());
}

} // namespace haylen::platform
