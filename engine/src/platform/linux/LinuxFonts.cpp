#include "platform/linux/LinuxFonts.hpp"

#include <dlfcn.h>

#include <string>

#include "platform/linux/LinuxGlib.hpp"

namespace haylen::platform {

std::mutex& LinuxFonts::lock = *new std::mutex();
bool LinuxFonts::loaded = false;
std::optional<LinuxFonts::Library>& LinuxFonts::library = *new std::optional<Library>();
void* LinuxFonts::config = nullptr;

// The pattern asks for the character alone, so fontconfig fills in the languages of the person and the family of the system and picks the face that draws it best, and the face counts only when its character set holds the character.
std::optional<SystemFont> LinuxFonts::find(char32_t codePoint) {
    const std::scoped_lock guard(lock);
    const Library* fontconfig = open();
    if (fontconfig == nullptr) {
        return std::nullopt;
    }

    void* pattern = fontconfig->createPattern();
    void* characters = fontconfig->createCharSet();
    fontconfig->addChar(characters, codePoint);
    fontconfig->addCharSet(pattern, "charset", characters);
    fontconfig->destroyCharSet(characters);
    fontconfig->substitute(config, pattern, kMatchPattern);
    fillDefaults(*fontconfig, pattern);

    int result = 0;
    void* matched = fontconfig->match(config, pattern, &result);
    fontconfig->destroyPattern(pattern);
    if (matched == nullptr) {
        return std::nullopt;
    }

    unsigned char* file = nullptr;
    int face = 0;
    void* covered = nullptr;
    const bool named = fontconfig->getString(matched, "file", 0, &file) == kResultMatch;
    const bool listed = fontconfig->getCharSet(matched, "charset", 0, &covered) == kResultMatch;
    std::optional<SystemFont> font;
    if (named && listed && fontconfig->hasChar(covered, codePoint) != 0) {
        fontconfig->getInteger(matched, "index", 0, &face);
        const std::string path(reinterpret_cast<const char*>(file));
        font = SystemFont{.name = path + "#" + std::to_string(face), .file = path, .face = face};
    }
    fontconfig->destroyPattern(matched);
    return font;
}

void LinuxFonts::release() noexcept {
    const std::scoped_lock guard(lock);
    if (config != nullptr) {
        library->destroyConfig(config);
        config = nullptr;
    }
}

const LinuxFonts::Library* LinuxFonts::open() {
    if (!loaded) {
        library = load();
        loaded = true;
    }
    if (!library) {
        return nullptr;
    }

    if (config == nullptr) {
        config = library->createConfig();
    }
    return config != nullptr ? &*library : nullptr;
}

std::optional<LinuxFonts::Library> LinuxFonts::load() {
    void* handle = dlopen("libfontconfig.so.1", RTLD_NOW | RTLD_LOCAL);
    if (handle == nullptr) {
        return std::nullopt;
    }

    Library loading;
    bool complete = true;
    complete = LinuxGlib::find(handle, loading.createConfig, "FcInitLoadConfigAndFonts") && complete;
    complete = LinuxGlib::find(handle, loading.destroyConfig, "FcConfigDestroy") && complete;
    complete = LinuxGlib::find(handle, loading.createPattern, "FcPatternCreate") && complete;
    complete = LinuxGlib::find(handle, loading.destroyPattern, "FcPatternDestroy") && complete;
    complete = LinuxGlib::find(handle, loading.createCharSet, "FcCharSetCreate") && complete;
    complete = LinuxGlib::find(handle, loading.destroyCharSet, "FcCharSetDestroy") && complete;
    complete = LinuxGlib::find(handle, loading.addChar, "FcCharSetAddChar") && complete;
    complete = LinuxGlib::find(handle, loading.addCharSet, "FcPatternAddCharSet") && complete;
    complete = LinuxGlib::find(handle, loading.substitute, "FcConfigSubstitute") && complete;
    complete = LinuxGlib::find(handle, loading.substituteDefaults, "FcDefaultSubstitute") && complete;
    complete = LinuxGlib::find(handle, loading.match, "FcFontMatch") && complete;
    complete = LinuxGlib::find(handle, loading.getString, "FcPatternGetString") && complete;
    complete = LinuxGlib::find(handle, loading.getInteger, "FcPatternGetInteger") && complete;
    complete = LinuxGlib::find(handle, loading.getCharSet, "FcPatternGetCharSet") && complete;
    complete = LinuxGlib::find(handle, loading.hasChar, "FcCharSetHasChar") && complete;
    if (!complete) {
        dlclose(handle);
        return std::nullopt;
    }

    // Fontconfig adds this function in the version whose default substitution reads a configuration, so earlier versions lack it and read none there.
    LinuxGlib::find(handle, loading.substituteConfigDefaults, "FcConfigSetDefaultSubstitute");
    return loading;
}

// The default substitution of recent versions of fontconfig reads a configuration, and without one it loads the implicit configuration of the process, which nothing frees, so the lookup hands it its own.
void LinuxFonts::fillDefaults(const Library& fontconfig, void* pattern) {
    if (fontconfig.substituteConfigDefaults != nullptr) {
        fontconfig.substituteConfigDefaults(config, pattern);
        return;
    }
    fontconfig.substituteDefaults(pattern);
}

} // namespace haylen::platform
