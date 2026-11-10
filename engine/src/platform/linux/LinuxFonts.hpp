#pragma once

#include <mutex>
#include <optional>

#include "platform/SystemFont.hpp"

namespace haylen::platform {

// Finds the faces of the system that draw characters through fontconfig, which the engine loads at run time, so apps also run on systems without it, where they find no font of the system.
class LinuxFonts final {
  public:
    [[nodiscard]] static std::optional<SystemFont> find(char32_t codePoint);

  private:
    // The functions of fontconfig the lookup calls, with its types as opaque pointers.
    struct Library {
        void* config = nullptr;
        void* (*createPattern)() = nullptr;
        void (*destroyPattern)(void* pattern) = nullptr;
        void* (*createCharSet)() = nullptr;
        void (*destroyCharSet)(void* set) = nullptr;
        int (*addChar)(void* set, unsigned int codePoint) = nullptr;
        int (*addCharSet)(void* pattern, const char* object, const void* set) = nullptr;
        int (*substitute)(void* config, void* pattern, int kind) = nullptr;
        void (*substituteDefaults)(void* pattern) = nullptr;
        void* (*match)(void* config, void* pattern, int* result) = nullptr;
        int (*getString)(const void* pattern, const char* object, int index, unsigned char** value) = nullptr;
        int (*getInteger)(const void* pattern, const char* object, int index, int* value) = nullptr;
        int (*getCharSet)(const void* pattern, const char* object, int index, void** value) = nullptr;
        int (*hasChar)(const void* set, unsigned int codePoint) = nullptr;
    };

    // The values of `FcMatchPattern` and `FcResultMatch`.
    static constexpr int kMatchPattern = 0;
    static constexpr int kResultMatch = 0;

    // Loads fontconfig and its configuration the first time, and returns null on systems without it.
    [[nodiscard]] static const Library* open();
    [[nodiscard]] static std::optional<Library> load();

    static std::once_flag& loaded;
    static std::optional<Library>& library;
};

} // namespace haylen::platform
