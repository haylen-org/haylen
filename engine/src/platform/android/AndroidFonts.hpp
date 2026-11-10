#pragma once

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>

#include "platform/SystemFont.hpp"

namespace haylen::platform {

// Finds the faces of the system that draw characters through the font matcher of the NDK, which Android 10 adds, so devices before it find no font of the system. The engine looks its functions up at run time, since the library of the engine also runs on Android 8.1.
class AndroidFonts final {
  public:
    [[nodiscard]] static std::optional<SystemFont> find(char32_t codePoint);

  private:
    // The functions of the font matcher, with its types as opaque pointers.
    struct Matcher {
        void* (*create)() = nullptr;
        void (*destroy)(void* matcher) = nullptr;
        void* (*match)(const void* matcher, const char* family, const std::uint16_t* text, std::uint32_t length, std::uint32_t* run) = nullptr;
        const char* (*getPath)(const void* font) = nullptr;
        std::size_t (*getIndex)(const void* font) = nullptr;
        void (*close)(void* font) = nullptr;
    };

    [[nodiscard]] static const Matcher* open();
    [[nodiscard]] static std::optional<Matcher> load();

    static std::once_flag& loaded;
    static std::optional<Matcher>& matcher;
};

} // namespace haylen::platform
