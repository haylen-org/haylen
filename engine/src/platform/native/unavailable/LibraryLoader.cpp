#include "platform/native/LibraryLoader.hpp"

namespace haylen::platform {

// A platform that does not allow `dynamicLibraries`, such as a page, loads nothing at run time, so the app reaches only the libraries that it links and registers.
bool LibraryLoader::isAvailable() noexcept {
    return false;
}

std::vector<std::string> LibraryLoader::getFileNames(std::string_view) {
    return {};
}

std::vector<std::filesystem::path> LibraryLoader::getFolders() {
    return {};
}

void* LibraryLoader::load(const std::filesystem::path&, std::string& failure) {
    failure = "this platform loads no library at run time";
    return nullptr;
}

void* LibraryLoader::lookup(void*, const std::string&) {
    return nullptr;
}

void* LibraryLoader::lookupProcess(const std::string&) {
    return nullptr;
}

} // namespace haylen::platform
