#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <vector>

#include "haylen/core/Engine.hpp"
#include "haylen/text/TrueTypeFont.hpp"
#include "platform/apple/AppleFonts.hpp"
#include "support/EngineFixture.hpp"

namespace haylen::platform {

// Every Apple system ships faces for Chinese, whose tables CoreText hands over as a font file that the engine draws.
TEST(AppleFontsTest, FindsTheFaceOfAChineseCharacterAndReadsItsTables) {
    const std::optional<SystemFont> found = AppleFonts::find(U'中');
    ASSERT_TRUE(found.has_value());
    EXPECT_FALSE(found->name.empty());
    EXPECT_TRUE(found->file.empty());

    test::EngineFixture fixture;
    text::TrueTypeFont font(fixture.engine().getGraphics(), AppleFonts::read(*found));
    EXPECT_TRUE(font.hasGlyph(U'中'));
}

// A character that no font of the system draws finds only the font of boxes, and an emoji only the face of bitmaps, which are no answers, and a name the system lacks has no tables.
TEST(AppleFontsTest, AnswersNoneForCharactersNoFontDraws) {
    EXPECT_FALSE(AppleFonts::find(U'\U0010FFFD').has_value());
    EXPECT_FALSE(AppleFonts::find(U'\U0001F600').has_value());
    EXPECT_THROW((void)AppleFonts::read({.name = "HaylenNoSuchFont"}), std::runtime_error);
}

} // namespace haylen::platform
