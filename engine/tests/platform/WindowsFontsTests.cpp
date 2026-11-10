#include <gtest/gtest.h>

#include <optional>

#include "haylen/core/Engine.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "platform/SystemFontReader.hpp"
#include "platform/windows/WindowsFonts.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

// Every Windows system ships faces for Chinese in the fonts folder, which the fallback of DirectWrite names by file and index.
TEST(WindowsFontsTest, FindsTheFileOfTheFaceOfAChineseCharacter) {
    const std::optional<SystemFont> found = WindowsFonts::find(U'中');
    ASSERT_TRUE(found.has_value());
    EXPECT_TRUE(std::filesystem::exists(found->file));

    test::EngineFixture fixture;
    text::TrueTypeFont font(fixture.engine().getGraphics(), SystemFontReader::read(found->file), {.face = found->face});
    EXPECT_TRUE(font.hasGlyph(U'中'));
}

} // namespace haylen::platform
