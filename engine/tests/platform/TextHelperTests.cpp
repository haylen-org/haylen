#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "platform/TextHelper.hpp"

namespace haylen::platform {

// A Lua string may hold any bytes, which the systems that take only UTF-8 refuse, so each sequence that is not UTF-8 becomes U+FFFD as the engine draws it.
TEST(TextHelperTest, TurnsSequencesThatAreNotUtf8IntoReplacementCharacters) {
    EXPECT_EQ(TextHelper::toValidUtf8(std::string_view("Go\xff on\xc3", 7)), "Go\xEF\xBF\xBD on\xEF\xBF\xBD");
    EXPECT_EQ(TextHelper::toValidUtf8("over\xc0\xaflong"), "over\xEF\xBF\xBDlong");
}

TEST(TextHelperTest, KeepsValidTextWithItsZeroBytes) {
    EXPECT_EQ(TextHelper::toValidUtf8("Olá 😀"), "Olá 😀");
    EXPECT_EQ(TextHelper::toValidUtf8(std::string_view("a\0b", 3)), std::string("a\0b", 3));
    EXPECT_EQ(TextHelper::toValidUtf8(""), "");
}

} // namespace haylen::platform
