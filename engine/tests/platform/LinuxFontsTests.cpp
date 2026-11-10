#include <gtest/gtest.h>

#include <dlfcn.h>

#include <optional>

#include "haylen/core/Engine.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "platform/SystemFontReader.hpp"
#include "platform/linux/LinuxFonts.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

// A system with fontconfig names the file and the face that draws a letter, which the engine draws, and so for Chinese where the system has a font for it. The release frees everything fontconfig allocated, which the address sanitizer checks, and the next lookup loads it again.
TEST(LinuxFontsTest, FindsTheFilesOfTheFacesOfCharacters) {
    void* fontconfig = dlopen("libfontconfig.so.1", RTLD_NOW | RTLD_LOCAL);
    if (fontconfig == nullptr) {
        GTEST_SKIP() << "This system has no fontconfig, so it finds no font of the system.";
    }
    dlclose(fontconfig);

    test::EngineFixture fixture;
    int found = 0;
    for (const char32_t character : {U'A', U'\u4E2D'}) {
        const std::optional<SystemFont> face = LinuxFonts::find(character);
        if (!face) {
            continue;
        }
        ++found;
        text::TrueTypeFont font(fixture.engine().getGraphics(), SystemFontReader::read(face->file), {.face = face->face});
        EXPECT_TRUE(font.hasGlyph(character)) << face->name;
    }

    const std::optional<SystemFont> before = LinuxFonts::find(U'A');
    LinuxFonts::release();
    const std::optional<SystemFont> after = LinuxFonts::find(U'A');
    LinuxFonts::release();
    EXPECT_EQ(before.has_value(), after.has_value());

    if (found == 0) {
        GTEST_SKIP() << "This system has fontconfig but no font with outlines for these characters.";
    }
}

} // namespace haylen::platform
