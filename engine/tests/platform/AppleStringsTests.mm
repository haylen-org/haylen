#include <gtest/gtest.h>

#include <string_view>

#import "platform/apple/AppleStrings.hpp"

namespace haylen::platform {

// Foundation answers nil for bytes that are not UTF-8, which ends the app once an announcement or a dictionary holds it.
TEST(AppleStringsTest, NeverAnswersNilForBytesThatAreNotUtf8) {
    NSString* text = AppleStrings::toNSString(std::string_view("Go\xff on\xc3", 7));

    ASSERT_NE(text, nil);
    EXPECT_TRUE([text isEqualToString:@"Go� on�"]);
}

// The text keeps every character after a zero byte, which the conversions of C strings cut.
TEST(AppleStringsTest, KeepsTheCharactersAfterAZeroByte) {
    NSString* text = AppleStrings::toNSString(std::string_view("a\0b", 3));

    EXPECT_EQ(text.length, 3u);
    EXPECT_EQ([text characterAtIndex:2], u'b');
}

} // namespace haylen::platform
