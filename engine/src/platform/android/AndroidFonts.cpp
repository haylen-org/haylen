#include "platform/android/AndroidFonts.hpp"

#include <dlfcn.h>

#include <string>

namespace haylen::platform {

std::once_flag& AndroidFonts::loaded = *new std::once_flag();
std::optional<AndroidFonts::Matcher>& AndroidFonts::matcher = *new std::optional<Matcher>();

// The matcher picks, from the fallback chain of the system font, the face that draws the text in the locales of the device, and answers the first face of the chain when none draws it, which the engine never uses, since a face counts only for the characters it holds.
std::optional<SystemFont> AndroidFonts::find(char32_t codePoint) {
    const Matcher* functions = open();
    if (functions == nullptr) {
        return std::nullopt;
    }

    std::uint16_t units[2];
    std::uint32_t length = 1;
    if (codePoint > 0xFFFFU) {
        const char32_t shifted = codePoint - 0x10000U;
        units[0] = static_cast<std::uint16_t>(0xD800U + (shifted >> 10U));
        units[1] = static_cast<std::uint16_t>(0xDC00U + (shifted & 0x3FFU));
        length = 2;
    } else {
        units[0] = static_cast<std::uint16_t>(codePoint);
    }

    void* matching = functions->create();
    std::uint32_t run = 0;
    void* font = functions->match(matching, "sans-serif", units, length, &run);
    functions->destroy(matching);
    const char* path = functions->getPath(font);
    const int face = static_cast<int>(functions->getIndex(font));
    std::optional<SystemFont> found;
    if (path != nullptr) {
        found = SystemFont{.name = std::string(path) + "#" + std::to_string(face), .file = path, .face = face};
    }
    functions->close(font);
    return found;
}

const AndroidFonts::Matcher* AndroidFonts::open() {
    std::call_once(loaded, [] { matcher = load(); });
    return matcher ? &*matcher : nullptr;
}

std::optional<AndroidFonts::Matcher> AndroidFonts::load() {
    Matcher loading;
    loading.create = reinterpret_cast<void* (*)()>(dlsym(RTLD_DEFAULT, "AFontMatcher_create"));
    loading.destroy = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_DEFAULT, "AFontMatcher_destroy"));
    loading.match = reinterpret_cast<void* (*)(const void*, const char*, const std::uint16_t*, std::uint32_t, std::uint32_t*)>(dlsym(RTLD_DEFAULT, "AFontMatcher_match"));
    loading.getPath = reinterpret_cast<const char* (*)(const void*)>(dlsym(RTLD_DEFAULT, "AFont_getFontFilePath"));
    loading.getIndex = reinterpret_cast<std::size_t (*)(const void*)>(dlsym(RTLD_DEFAULT, "AFont_getCollectionIndex"));
    loading.close = reinterpret_cast<void (*)(void*)>(dlsym(RTLD_DEFAULT, "AFont_close"));
    const bool complete = loading.create != nullptr && loading.destroy != nullptr && loading.match != nullptr && loading.getPath != nullptr && loading.getIndex != nullptr && loading.close != nullptr;
    if (!complete) {
        return std::nullopt;
    }
    return loading;
}

} // namespace haylen::platform
